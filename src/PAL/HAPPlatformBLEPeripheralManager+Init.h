/*
 * Copyright (c) 2021 Deomid "rojer" Ryabkov
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

// vim: tabstop=4 expandtab shiftwidth=4 ai cin smarttab:

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    bool isPrimary;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
} HAPPlatformBLEPeripheralManagerService;

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    HAPPlatformBLEPeripheralManagerCharacteristicProperties properties;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
    HAPPlatformBLEPeripheralManagerAttributeHandle valueHandle;
    HAPPlatformBLEPeripheralManagerAttributeHandle cccDescriptorHandle;
} HAPPlatformBLEPeripheralManagerCharacteristic;

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    HAPPlatformBLEPeripheralManagerDescriptorProperties properties;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
} HAPPlatformBLEPeripheralManagerDescriptor;

HAP_ENUM_BEGIN(uint8_t, HAPPlatformBLEPeripheralManagerAttributeType) {
    kHAPPlatformBLEPeripheralManagerAttributeType_None,
    kHAPPlatformBLEPeripheralManagerAttributeType_Service,
    kHAPPlatformBLEPeripheralManagerAttributeType_Characteristic,
    kHAPPlatformBLEPeripheralManagerAttributeType_Descriptor
} HAP_ENUM_END(uint8_t, HAPPlatformBLEPeripheralManagerAttributeType);

typedef struct {
    HAPPlatformBLEPeripheralManagerAttributeType type;
    union {
        HAPPlatformBLEPeripheralManagerDescriptor descriptor;
        HAPPlatformBLEPeripheralManagerCharacteristic characteristic;
        HAPPlatformBLEPeripheralManagerService service;
    } _;
} HAPPlatformBLEPeripheralManagerAttribute;

/**
 * BLE peripheral manager initialization options.
 */
typedef struct {
} HAPPlatformBLEPeripheralManagerOptions;

/**
 * BLE peripheral manager.
 */
struct HAPPlatformBLEPeripheralManager {
    void* impl; // Pointer to the BLEPeripheralManagerImpl instance.
};

/**
 * Initializes the BLE peripheral manager.
 *
 * @param[out] blePeripheralManager Pointer to an allocated but uninitialized HAPPlatformBLEPeripheralManager structure.
 * @param      options              Initialization options.
 */
void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerOptions* options);

#ifdef __cplusplus
}
#endif
