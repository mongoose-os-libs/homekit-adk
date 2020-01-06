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

#include "HAPPlatformAccessorySetup.h"
#include "HAPPlatformAccessorySetup+Init.h"

void HAPPlatformAccessorySetupCreate(
        HAPPlatformAccessorySetupRef accessorySetup HAP_UNUSED,
        const HAPPlatformAccessorySetupOptions* options HAP_UNUSED) {
}

HAPPlatformAccessorySetupCapabilities
        HAPPlatformAccessorySetupGetCapabilities(HAPPlatformAccessorySetupRef accessorySetup) {
    HAPPrecondition(accessorySetup);

    // Deprecated. Return false and use HAPPlatformAccessorySetupDisplay / HAPPlatformAccessorySetupNFC instead.
    return (HAPPlatformAccessorySetupCapabilities) { .supportsDisplay = false, .supportsProgrammableNFC = false };
}

void HAPPlatformAccessorySetupUpdateSetupPayload(
        HAPPlatformAccessorySetupRef accessorySetup HAP_UNUSED,
        const HAPSetupPayload* _Nullable setupPayload HAP_UNUSED,
        const HAPSetupCode* _Nullable setupCode HAP_UNUSED) {
}
