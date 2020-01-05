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

#include <map>
#include <cstring>
#include <string>

class KVStore {
public:
    explicit KVStore(const char* fileName);
    HAPError
            Get(HAPPlatformKeyValueStoreDomain domain,
                HAPPlatformKeyValueStoreKey key,
                void* _Nullable bytes,
                size_t maxBytes,
                size_t* _Nullable numBytes,
                bool* found);
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
            void* _Nullable context);
    HAPError PurgeDomain(HAPPlatformKeyValueStoreDomain domain);

private:
    static uint16_t KVSKey(HAPPlatformKeyValueStoreDomain domain, HAPPlatformKeyValueStoreKey key);
    void Load();
    HAPError Save();

    const std::string fileName_;
    std::map<uint16_t, std::string> kvs_;
};

KVStore::KVStore(const char* fileName)
    : fileName_(fileName) {
    Load();
}

void KVStore::Load() {
    // TODO
}

HAPError KVStore::Save() {
    // TODO
    return kHAPError_None;
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
        bool* found) {
    auto it = kvs_.find(KVSKey(domain, key));
    if (it == kvs_.end()) {
        *found = false;
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
        void* _Nullable context) {
    HAPError err = kHAPError_None;
    const auto from_it = kvs_.lower_bound(KVSKey(domain, 0x00));
    const auto to_it = kvs_.lower_bound(KVSKey(domain, 0xff));
    for (auto it = from_it; it != to_it; it++) {
        HAPPlatformKeyValueStoreDomain kd = static_cast<HAPPlatformKeyValueStoreDomain>(it->first >> 8);
        HAPPlatformKeyValueStoreKey kk = static_cast<HAPPlatformKeyValueStoreKey>(it->first);
        bool shouldContinue = false;
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
