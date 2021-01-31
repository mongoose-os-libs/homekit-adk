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

#include "mgos.h"
#include "mgos_hap.h"

#include "HAPPlatformAccessorySetup.h"
#include "HAPPlatformAccessorySetup+Init.h"

void HAPPlatformAccessorySetupCreate(
        HAPPlatformAccessorySetupRef accessorySetup HAP_UNUSED,
        const HAPPlatformAccessorySetupOptions* options HAP_UNUSED) {
}

void HAPPlatformAccessorySetupLoadSetupInfo(HAPPlatformAccessorySetupRef accessorySetup, HAPSetupInfo* setupInfo) {
    struct mgos_hap_load_setup_info_arg arg = {
        .accessorySetup = accessorySetup,
        .setupInfo = setupInfo,
    };
    mgos_event_trigger(MGOS_HAP_EV_LOAD_SETUP_INFO, &arg);
}

void HAPPlatformAccessorySetupLoadSetupCode(HAPPlatformAccessorySetupRef accessorySetup, HAPSetupCode* setupCode) {
    struct mgos_hap_load_setup_code_arg arg = {
        .accessorySetup = accessorySetup,
        .setupCode = setupCode,
    };
    mgos_event_trigger(MGOS_HAP_EV_LOAD_SETUP_CODE, &arg);
}

void HAPPlatformAccessorySetupLoadSetupID(
        HAPPlatformAccessorySetupRef accessorySetup,
        bool* valid,
        HAPSetupID* setupID) {
    struct mgos_hap_load_setup_id_arg arg = {
        .accessorySetup = accessorySetup,
        .valid = valid,
        .setupID = setupID,
    };
    mgos_event_trigger(MGOS_HAP_EV_LOAD_SETUP_ID, &arg);
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
