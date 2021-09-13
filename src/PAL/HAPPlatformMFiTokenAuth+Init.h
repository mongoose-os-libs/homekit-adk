// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

typedef struct {
    uint8_t dummy;
} HAPPlatformMFiTokenAuthOptions;

struct HAPPlatformMFiTokenAuth {
    uint8_t dummy;
    HAPPlatformKeyValueStoreRef keyValueStore;
};

/**
 * Initializes Software Token provider.
 *
 * @param[out] mfiTokenAuth         Software Token provider.
 * @param      options              Initialization options.
 */
void HAPPlatformMFiTokenAuthCreate(
        HAPPlatformMFiTokenAuthRef mfiTokenAuth,
        const HAPPlatformMFiTokenAuthOptions* options);

/**
 * Returns whether a Software Token is provisioned.
 *
 * @param      mfiTokenAuth         Software Token provider.
 *
 * @return true                     If a Software Token is provisioned.
 * @return false                    Otherwise.
 */
bool HAPPlatformMFiTokenAuthIsProvisioned(HAPPlatformMFiTokenAuthRef mfiTokenAuth);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif
