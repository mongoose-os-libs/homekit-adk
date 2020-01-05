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

#include "HAPPlatformMFiHWAuth.h"
#include "HAPPlatformMFiHWAuth+Init.h"

// TODO(rojer): implement this.

bool HAPPlatformMFiHWAuthIsPoweredOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    (void) mfiHWAuth;
    return false;
}

HAPError HAPPlatformMFiHWAuthPowerOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    (void) mfiHWAuth;
    return kHAPError_Unknown;
}

void HAPPlatformMFiHWAuthPowerOff(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    (void) mfiHWAuth;
}

HAPError HAPPlatformMFiHWAuthWrite(HAPPlatformMFiHWAuthRef mfiHWAuth, const void* bytes, size_t numBytes) {
    (void) mfiHWAuth;
    (void) bytes;
    (void) numBytes;
    return kHAPError_Unknown;
}

HAPError HAPPlatformMFiHWAuthRead(
        HAPPlatformMFiHWAuthRef mfiHWAuth,
        uint8_t registerAddress,
        void* bytes,
        size_t numBytes) {
    (void) mfiHWAuth;
    (void) registerAddress;
    (void) bytes;
    (void) numBytes;
    return kHAPError_Unknown;
}

void HAPPlatformMFiHWAuthCreate(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    (void) mfiHWAuth;
}

void HAPPlatformMFiHWAuthRelease(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    (void) mfiHWAuth;
}
