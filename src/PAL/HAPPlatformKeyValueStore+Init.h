// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#ifndef HAP_PLATFORM_KEY_VALUE_STORE_INIT_H
#define HAP_PLATFORM_KEY_VALUE_STORE_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**@file
 * File system based key-value store.
 *
 * The implementation uses a JSON file to store data.
 *
 * **Example**

   @code{.c}

   // Allocate key-value store.
   static HAPPlatformKeyValueStore keyValueStore;

   // Initialize key-value store.
   HAPPlatformKeyValueStoreCreate(&keyValueStore,
       &(const HAPPlatformKeyValueStoreOptions) {
           .fileName = "kv.json"
       });

   @endcode
 */

/**
 * Key-value store initialization options.
 */
typedef struct {
    const char* fileName;
} HAPPlatformKeyValueStoreOptions;

/**
 * Key-value store.
 */
struct HAPPlatformKeyValueStore {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    void* ctx;
    /**@endcond */
};

/**
 * Initializes the key-value store.
 *
 * @param[out] keyValueStore        Pointer to an allocated but uninitialized HAPPlatformKeyValueStore structure.
 * @param      options              Initialization options.
 */
void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
