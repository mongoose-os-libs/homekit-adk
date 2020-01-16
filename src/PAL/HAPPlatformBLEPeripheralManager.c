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

#include "HAPPlatformBLEPeripheralManager.h"
//#include "HAPPlatformBLEPeripheralManager+Init.h"

// TODO: Implement.

#if 0
void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerOptions* _Nonnull options) {
    HAPPrecondition(blePeripheralManager);
    HAPPrecondition(options);
    HAPPrecondition(options->attributes);
    HAPPrecondition(options->numAttributes <= SIZE_MAX / sizeof options->attributes[0]);

    HAPRawBufferZero(options->attributes, options->numAttributes * sizeof options->attributes[0]);

    HAPRawBufferZero(blePeripheralManager, sizeof *blePeripheralManager);
    blePeripheralManager->attributes = options->attributes;
    blePeripheralManager->numAttributes = options->numAttributes;
}
#endif

void HAPPlatformBLEPeripheralManagerSetDelegate(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDelegate* _Nullable delegate) {
    (void) blePeripheralManager;
    (void) delegate;
}

void HAPPlatformBLEPeripheralManagerSetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDeviceAddress* _Nonnull deviceAddress) {
    (void) blePeripheralManager;
    (void) deviceAddress;
}

void HAPPlatformBLEPeripheralManagerSetDeviceName(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const char* _Nonnull deviceName) {
    (void) blePeripheralManager;
    (void) deviceName;
}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    (void) blePeripheralManager;
}

HAPError HAPPlatformBLEPeripheralManagerAddService(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        bool isPrimary) {
    (void) blePeripheralManager;
    (void) type;
    (void) isPrimary;
    return kHAPError_Unknown;
}

HAPError HAPPlatformBLEPeripheralManagerAddCharacteristic(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull valueHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nullable cccDescriptorHandle) {
    (void) blePeripheralManager;
    (void) type;
    (void) properties;
    (void) constBytes;
    (void) constNumBytes;
    (void) valueHandle;
    (void) cccDescriptorHandle;
    return kHAPError_Unknown;
}

HAPError HAPPlatformBLEPeripheralManagerAddDescriptor(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull descriptorHandle) {
    (void) blePeripheralManager;
    (void) type;
    (void) properties;
    (void) constBytes;
    (void) constNumBytes;
    (void) descriptorHandle;
    return kHAPError_Unknown;
}

void HAPPlatformBLEPeripheralManagerPublishServices(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    (void) blePeripheralManager;
}

void HAPPlatformBLEPeripheralManagerStartAdvertising(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPBLEAdvertisingInterval advertisingInterval,
        const void* _Nonnull advertisingBytes,
        size_t numAdvertisingBytes,
        const void* _Nullable scanResponseBytes,
        size_t numScanResponseBytes) {
    (void) blePeripheralManager;
    (void) advertisingInterval;
    (void) advertisingBytes;
    (void) numAdvertisingBytes;
    (void) scanResponseBytes;
    (void) numScanResponseBytes;
}

void HAPPlatformBLEPeripheralManagerStopAdvertising(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    (void) blePeripheralManager;
}

bool HAPPlatformBLEPeripheralManagerIsAdvertising(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    (void) blePeripheralManager;
    return false;
}

void HAPPlatformBLEPeripheralManagerGetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerDeviceAddress* _Nonnull deviceAddress) {
    (void) blePeripheralManager;
    (void) deviceAddress;
}

HAPError HAPPlatformBLEPeripheralManagerGetAdvertisingData(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        void* _Nonnull advertisingBytes,
        size_t maxAdvertisingBytes,
        size_t* _Nonnull numAdvertisingBytes,
        void* _Nonnull scanResponseBytes,
        size_t maxScanResponseBytes,
        size_t* _Nonnull numScanResponseBytes) {
    (void) blePeripheralManager;
    (void) advertisingBytes;
    (void) maxAdvertisingBytes;
    (void) numAdvertisingBytes;
    (void) scanResponseBytes;
    (void) maxScanResponseBytes;
    (void) numScanResponseBytes;
    return kHAPError_Unknown;
}

void HAPPlatformBLEPeripheralManagerCancelCentralConnection(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle) {
    (void) blePeripheralManager;
    (void) connectionHandle;
}

HAPError HAPPlatformBLEPeripheralManagerSendHandleValueIndication(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle valueHandle,
        const void* _Nullable bytes,
        size_t numBytes) {
    (void) blePeripheralManager;
    (void) connectionHandle;
    (void) valueHandle;
    (void) bytes;
    (void) numBytes;
    return kHAPError_Unknown;
}
