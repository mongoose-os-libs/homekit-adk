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

#include <cstdlib>
#include <cstring>
#include <string>

#include "mgos.hpp"

class KVStore {
public:
    explicit KVStore(const char* fileName);
    ~KVStore();
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
                size_t numBytes,
                bool save = true);
    HAPError Remove(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key, bool save = true);
    HAPError Enumerate(
            HAPPlatformKeyValueStoreRef keyValueStore,
            HAPPlatformKeyValueStoreDomain domain,
            HAPPlatformKeyValueStoreEnumerateCallback callback,
            void* _Nullable context) const;
    HAPError PurgeDomain(HAPPlatformKeyValueStoreDomain domain);

private:
    struct Item {
        Item* next;
        HAPPlatformKeyValueStoreDomain dom;
        HAPPlatformKeyValueStoreKey key;
        uint16_t len;
        uint8_t data[];

        static Item*
                New(HAPPlatformKeyValueStoreDomain domain,
                    HAPPlatformKeyValueStoreKey key,
                    const void* data,
                    uint8_t len) {
            Item* itm = reinterpret_cast<Item*>(new uint8_t[sizeof(Item) + len]);
            itm->next = nullptr;
            itm->dom = domain;
            itm->key = key;
            itm->len = len;
            std::memcpy(itm->data, data, len);
            return itm;
        }

        static void Delete(Item* itm) {
            delete[] reinterpret_cast<uint8_t*>(itm);
        }
    };

    static uint16_t KVSKey(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key);
    void Load();
    void Clear();
    HAPError Save() const;

    const std::string fileName_;
    Item* items_ = nullptr;
};

KVStore::KVStore(const char* fileName)
    : fileName_(fileName) {
    Load();
}

KVStore::~KVStore() {
    Clear();
}

void KVStore::Clear() {
    Item *itm = items_, *next;
    while (itm != nullptr) {
        next = itm->next;
        Item::Delete(itm);
        itm = next;
    }
    items_ = nullptr;
}

void KVStore::Load() {
    Clear();
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
    mgos::ScopedCPtr data_owner(data);
    int num_keys = 0;
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
            HAPPlatformKeyValueStoreDomain dd = (HAPPlatformKeyValueStoreDomain)(k >> 8);
            HAPPlatformKeyValueStoreKey kk = (HAPPlatformKeyValueStoreKey) k;
            Set(dd, kk, v, vs, false /* save */);
            num_keys++;
            free(v);
        }
    }
    LOG(LL_DEBUG, ("Loaded %d keys from %s ss %d", num_keys, fileName_.c_str(), (int) sizeof(Item)));
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
    const Item* itm;
    json_printf(&out, "{");
    for (itm = items_, num = 0; itm != nullptr; itm = itm->next, num++) {
        if (num != 0) {
            json_printf(&out, ",");
        }
        uint16_t fkey = ((((uint16_t) itm->dom) << 8) | ((uint16_t) itm->key));
        if (json_printf(&out, "\n  \"%u\": %V", fkey, &itm->data[0], itm->len) < 0) {
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
    if (fp != NULL) {
        fclose(fp);
    }
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
    *found = false;
    for (const Item* itm = items_; itm != nullptr; itm = itm->next) {
        if (itm->dom != domain || itm->key != key) {
            continue;
        }
        *numBytes = std::min((size_t) itm->len, maxBytes);
        std::memcpy(bytes, &itm->data[0], *numBytes);
        *found = true;
        break;
    }
    return kHAPError_None;
}

HAPError KVStore::Set(
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes,
        bool save) {
    Item *prev = nullptr, *itm = items_, *next = nullptr;
    while (itm != nullptr) {
        if (itm->dom == domain && itm->key == key) {
            break;
        }
        prev = itm;
        itm = itm->next;
    }
    if (itm != nullptr) {
        next = itm->next;
    }
    if (itm != nullptr) {
        Item::Delete(itm);
    }
    bool changed;
    if (bytes != nullptr) {
        itm = Item::New(domain, key, bytes, numBytes);
        if (itm == nullptr) {
            return kHAPError_OutOfResources;
        }
        changed = true;
    } else {
        changed = (itm != nullptr);
        itm = next;
        next = nullptr;
    }
    if (prev != nullptr) {
        prev->next = itm;
    } else {
        items_ = itm;
    }
    if (next != nullptr) {
        itm->next = next;
    }
    return (changed && save ? Save() : kHAPError_None);
}

HAPError KVStore::Remove(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key, bool save) {
    return Set(domain, key, nullptr, 0, save);
}

HAPError KVStore::Enumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) const {
    HAPError err = kHAPError_None;
    for (const Item* itm = items_; itm != nullptr; itm = itm->next) {
        if (itm->dom != domain) {
            continue;
        }
        bool shouldContinue = true;
        err = callback(context, keyValueStore, itm->dom, itm->key, &shouldContinue);
        if (err != kHAPError_None || !shouldContinue) {
            break;
        }
    }
    return err;
}

HAPError KVStore::PurgeDomain(HAPPlatformKeyValueStoreDomain domain) {
    bool changed = false;
    while (true) {
        Item* itm = items_;
        while (itm != nullptr) {
            if (itm->dom == domain) {
                break;
            }
            itm = itm->next;
        }
        if (itm == nullptr) {
            break;
        }
        Remove(itm->dom, itm->key, false /* save */);
        changed = true;
    }
    return (changed ? Save() : kHAPError_None);
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

void HAPPlatformKeyValueStoreRelease(HAPPlatformKeyValueStoreRef keyValueStore) {
    auto* kvs = static_cast<KVStore*>(keyValueStore->ctx);
    keyValueStore->ctx = nullptr;
    delete kvs;
}

} // extern "C"
