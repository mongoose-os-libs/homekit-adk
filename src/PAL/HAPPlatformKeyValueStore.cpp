/*
 * Copyright (c) 2019 Deomid "rojer" Ryabkov
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HAPPlatformKeyValueStore.h"
#include "HAPPlatformKeyValueStore+Init.h"

#include <stdio.h>

#include <map>
#include <cstdlib>
#include <cstring>
#include <string>

#include "mgos.h"

class KVStore {
public:
    explicit KVStore(const char* fileName);
    HAPError
            Get(HAPPlatformKeyValueStoreDomain domain,
                HAPPlatformKeyValueStoreKey key,
                void* _Nullable bytes,
                size_t maxBytes,
                size_t* _Nullable numBytes,
                bool* found) const;
    HAPError
            Set(HAPPlatformKeyValueStoreDomain domain,
                HAPPlatformKeyValueStoreKey key,
                const void* bytes,
                size_t numBytes);
    HAPError Remove(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key);
    HAPError Enumerate(
            HAPPlatformKeyValueStoreRef keyValueStore,
            HAPPlatformKeyValueStoreDomain domain,
            HAPPlatformKeyValueStoreEnumerateCallback callback,
            void* _Nullable context) const;
    HAPError PurgeDomain(HAPPlatformKeyValueStoreDomain domain);

private:
    static uint16_t KVSKey(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key);
    void Load();
    HAPError Save() const;

    const std::string fileName_;
    std::map<uint16_t, std::string> kvs_;
};

KVStore::KVStore(const char* fileName)
    : fileName_(fileName) {
    Load();
}

void KVStore::Load() {
    void* h = NULL;
    size_t size = 0;
    char* data = cs_read_file(fileName_.c_str(), &size);
    if (data == NULL) {
        // In case Save() was interrupted at the final stage.
        std::string tmpFileName = fileName_ + ".tmp";
        data = cs_read_file(tmpFileName.c_str(), &size);
        if (data != NULL) {
            // Finish the job.
            rename(tmpFileName.c_str(), fileName_.c_str());
        }
    }
    if (data == NULL) {
        LOG(LL_WARN, ("No KVStore data"));
        goto out;
    }

    kvs_.clear();
    struct json_token key, val;
    while ((h = json_next_key(data, size, h, "", &key, &val)) != NULL) {
        char* v = NULL;
        int vs = 0;
        // NUL-terminate the key.
        std::string ks(key.ptr, key.len);
        int k = std::atoi(ks.c_str());
        // Include quotes.
        val.ptr--;
        val.len += 2;
        if (json_scanf(val.ptr - 1, val.len + 2, "%V", &v, &vs) == 1) {
            kvs_[k] = std::string(v, vs);
            free(v);
        }
    }

    LOG(LL_DEBUG, ("Loaded %d keys from %s", (int) kvs_.size(), fileName_.c_str()));

out:
    free(data);
}

HAPError KVStore::Save() const {
    HAPError err = kHAPError_Unknown;
    std::string tmpFileName = fileName_ + ".tmp";
    FILE* fp = fopen(tmpFileName.c_str(), "w");
    if (fp == NULL) {
        LOG(LL_ERROR, ("Failed to open %s for writing", tmpFileName.c_str()));
        goto out;
    }
    struct json_out out;
    out = JSON_OUT_FILE(fp);
    int num;
    num = 0;
    json_printf(&out, "{");
    for (auto it = kvs_.begin(); it != kvs_.end(); it++, num++) {
        if (num != 0) {
            json_printf(&out, ",");
        }
        if (json_printf(&out, "\n  \"%d\": %V", it->first, it->second.data(), (int) it->second.size()) < 0) {
            goto out;
        }
    }
    json_printf(&out, "\n}\n");
    fclose(fp);
    fp = NULL;
    remove(fileName_.c_str());
    if (rename(tmpFileName.c_str(), fileName_.c_str()) != 0) {
        goto out;
    }
    LOG(LL_DEBUG, ("Saved %d keys to %s", num, fileName_.c_str()));

    err = kHAPError_None;

out:
    if (fp != NULL)
        fclose(fp);
    return err;
}

// static
uint16_t KVStore::KVSKey(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key) {
    return ((static_cast<uint16_t>(domain) << 8) | static_cast<uint16_t>(key));
}

HAPError KVStore::Get(
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        void* _Nullable bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* found) const {
    auto it = kvs_.find(KVSKey(domain, key));
    if (it == kvs_.end()) {
        *found = false;
        *numBytes = 0;
    } else {
        *numBytes = std::min(it->second.size(), maxBytes);
        std::memcpy(bytes, it->second.data(), *numBytes);
        *found = true;
    }
    return kHAPError_None;
}

HAPError KVStore::Set(
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes) {
    kvs_[KVSKey(domain, key)] = std::string(static_cast<const char*>(bytes), numBytes);
    return Save();
}

HAPError KVStore::Remove(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key) {
    size_t numErased = kvs_.erase(KVSKey(domain, key));
    return (numErased == 0 ? kHAPError_None : Save());
}

HAPError KVStore::Enumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) const {
    HAPError err = kHAPError_None;
    const auto from_it = kvs_.lower_bound(KVSKey(domain, 0x00));
    const auto to_it = kvs_.lower_bound(KVSKey(domain, 0xff));
    for (auto it = from_it; it != to_it; it++) {
        HAPPlatformKeyValueStoreDomain kd = static_cast<HAPPlatformKeyValueStoreDomain>(it->first >> 8);
        HAPPlatformKeyValueStoreKey kk = static_cast<HAPPlatformKeyValueStoreKey>(it->first);
        bool shouldContinue = true;
        err = callback(context, keyValueStore, kd, kk, &shouldContinue);
        if (err != kHAPError_None || !shouldContinue) {
            break;
        }
    }
    return err;
}

HAPError KVStore::PurgeDomain(HAPPlatformKeyValueStoreDomain domain) {
    const auto from_it = kvs_.lower_bound(KVSKey(domain, 0x00));
    const auto to_it = kvs_.lower_bound(KVSKey(domain, 0xff));
    size_t numErased = 0;
    for (auto it = from_it; it != to_it;) {
        auto it2 = it++;
        kvs_.erase(it2);
        numErased++;
    }
    return (numErased == 0 ? kHAPError_None : Save());
}

extern "C" {

HAPError HAPPlatformKeyValueStoreGet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        void* _Nullable bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* found) {
    return static_cast<KVStore*>(keyValueStore->ctx)->Get(domain, key, bytes, maxBytes, numBytes, found);
}

HAPError HAPPlatformKeyValueStoreSet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes) {
    return static_cast<KVStore*>(keyValueStore->ctx)->Set(domain, key, bytes, numBytes);
}

HAPError HAPPlatformKeyValueStoreRemove(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key) {
    return static_cast<KVStore*>(keyValueStore->ctx)->Remove(domain, key);
}

HAPError HAPPlatformKeyValueStoreEnumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) {
    return static_cast<KVStore*>(keyValueStore->ctx)->Enumerate(keyValueStore, domain, callback, context);
}

HAPError HAPPlatformKeyValueStorePurgeDomain(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain) {
    return static_cast<KVStore*>(keyValueStore->ctx)->PurgeDomain(domain);
}

void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options) {
    keyValueStore->ctx = static_cast<void*>(new KVStore(options->fileName));
}

} // extern "C"
