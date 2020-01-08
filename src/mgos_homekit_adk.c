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

#include "mgos_hap.h"

#include "mgos.h"

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

void HAPPlatformAccessorySetupDisplayUpdateSetupPayload(
        HAPPlatformAccessorySetupDisplayRef setupDisplay,
        const HAPSetupPayload* _Nullable setupPayload,
        const HAPSetupCode* _Nullable setupCode) {

    struct mgos_hap_display_update_setup_payload_arg arg = {
        .setupDisplay = setupDisplay,
        .setupPayload = setupPayload,
        .setupCode = setupCode,
    };
    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_UPDATE_SETUP_PAYLOAD, &arg);
}

void HAPPlatformAccessorySetupDisplayHandleStartPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    struct mgos_hap_display_start_pairing_arg arg = {
        .setupDisplay = setupDisplay,
    };
    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_START_PAIRING, &arg);
}

void HAPPlatformAccessorySetupDisplayHandleStopPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    struct mgos_hap_display_stop_pairing_arg arg = {
        .setupDisplay = setupDisplay,
    };
    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_STOP_PAIRING, &arg);
}

void HAPPlatformAccessorySetupNFCUpdateSetupPayload(
        HAPPlatformAccessorySetupNFCRef setupNFC,
        const HAPSetupPayload* setupPayload,
        bool isPairable) {
    struct mgos_hap_nfc_update_setup_payload_arg arg = {
        .setupNFC = setupNFC,
        .setupPayload = setupPayload,
        .isPairable = isPairable,
    };
    mgos_event_trigger(MGOS_HAP_EV_NFC_UPDATE_SETUP_PAYLOAD, &arg);
}

bool mgos_hap_setup_info_from_string(HAPSetupInfo* setupInfo, const char* salt, const char* verifier) {
    int salt_len = strlen(salt != NULL ? salt : "");
    int verifier_len = strlen(verifier != NULL ? verifier : "");
    int d = 0;
    switch (salt_len) {
        case 32:
            cs_hex_decode(salt, salt_len, setupInfo->salt, &d);
            break;
        case 24:
            cs_base64_decode((const void*) salt, salt_len, (void*) setupInfo->salt, &d);
            break;
    }
    if (d != (int) sizeof(setupInfo->salt)) {
        return false;
    }
    switch (verifier_len) {
        case 768:
            cs_hex_decode(verifier, verifier_len, setupInfo->verifier, &d);
            break;
        case 512:
            cs_base64_decode((const void*) verifier, verifier_len, (void*) setupInfo->verifier, &d);
            break;
    }
    return (d == (int) sizeof(setupInfo->verifier));
}

#ifdef MGOS_HAP_SIMPLE_CONFIG
static void setup_info_cb(int ev, void* ev_data, void* userdata) {
    struct mgos_hap_load_setup_info_arg* arg = (struct mgos_hap_load_setup_info_arg*) ev_data;
    if (!mgos_hap_setup_info_from_string(
                arg->setupInfo, mgos_sys_config_get_hap_salt(), mgos_sys_config_get_hap_verifier())) {
        LOG(LL_ERROR, ("Failed to load HAP accessory info from config!"));
    }
    (void) ev;
    (void) userdata;
}
#endif

bool mgos_homekit_adk_init(void) {
#ifdef MGOS_HAP_SIMPLE_CONFIG
    mgos_event_add_handler(MGOS_HAP_EV_LOAD_SETUP_INFO, setup_info_cb, NULL);
#endif
    mgos_event_register_base(MGOS_HAP_EV_BASE, "HAP");
    return true;
}
