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

#pragma once

#include "HAP.h"

#include "mgos_event.h"

#define MGOS_HAP_EV_BASE MGOS_EVENT_BASE('H', 'A', 'P')

/*
 * Note: If your accessory has no display and no NFC,
 * handle MGOS_HAP_EV_LOAD_SETUP_ID and load static salt + verifier.
 * MGOS_HAP_EV_LOAD_SETUP_CODE will not work, it will only be invoked
 * if the accessory has NFC.
 * For other cases, see ADK documentation.
 */

/* HAP events */
enum mgos_hap_event {
    /* This event corresponds to HAPPlatformAccessorySetupLoadSetupInfo.
     * Arg: struct mgos_hap_load_setup_info_arg */
    MGOS_HAP_EV_LOAD_SETUP_INFO = MGOS_HAP_EV_BASE,
    /* This event corresponds to HAPPlatformAccessorySetupLoadSetupCode.
     * Arg: struct mgos_hap_load_setup_code_arg */
    MGOS_HAP_EV_LOAD_SETUP_CODE,
    /* This event corresponds to HAPPlatformAccessorySetupLoadSetupID.
     * Arg: struct mgos_hap_load_setup_id_arg */
    MGOS_HAP_EV_LOAD_SETUP_ID,
    /* This event corresponds to HAPPlatformAccessorySetupDisplayUpdateSetupPayload.
     * Arg: struct mgos_hap_display_update_setup_payload_arg */
    MGOS_HAP_EV_DISPLAY_UPDATE_SETUP_PAYLOAD,
    /* This event corresponds to HAPPlatformAccessorySetupDisplayHandleStartPairing.
     * Arg: struct mgos_hap_display_start_pairing_arg */
    MGOS_HAP_EV_DISPLAY_START_PAIRING,
    /* This event corresponds to HAPPlatformAccessorySetupDisplayHandleStopPairing.
     * Arg: struct mgos_hap_display_stop_pairing_arg */
    MGOS_HAP_EV_DISPLAY_STOP_PAIRING,
    /* This event corresponds to HAPPlatformAccessorySetupNFCUpdateSetupPayload.
     * Arg: struct mgos_hap_nfc_update_setup_payload_arg */
    MGOS_HAP_EV_NFC_UPDATE_SETUP_PAYLOAD,
};

struct mgos_hap_load_setup_info_arg {
    HAPPlatformAccessorySetupRef _Nonnull accessorySetup;
    HAPSetupInfo* _Nonnull setupInfo;
};

struct mgos_hap_load_setup_code_arg {
    HAPPlatformAccessorySetupRef _Nonnull accessorySetup;
    HAPSetupCode* _Nonnull setupCode;
};

struct mgos_hap_load_setup_id_arg {
    HAPPlatformAccessorySetupRef _Nonnull accessorySetup;
    bool* _Nonnull valid;
    HAPSetupID* _Nonnull setupID;
};

struct mgos_hap_display_update_setup_payload_arg {
    HAPPlatformAccessorySetupDisplayRef _Nonnull setupDisplay;
    const HAPSetupPayload* _Nullable setupPayload;
    const HAPSetupCode* _Nullable setupCode;
};

struct mgos_hap_display_start_pairing_arg {
    HAPPlatformAccessorySetupDisplayRef _Nonnull setupDisplay;
};

struct mgos_hap_display_stop_pairing_arg {
    HAPPlatformAccessorySetupDisplayRef _Nonnull setupDisplay;
};

struct mgos_hap_nfc_update_setup_payload_arg {
    HAPPlatformAccessorySetupNFCRef _Nonnull setupNFC;
    const HAPSetupPayload* _Nonnull setupPayload;
    bool isPairable;
};

/*
 * Load salt and verifier from strings.
 * Both can be either hex or base4 encoded.
 */
bool mgos_hap_setup_info_from_string(
        HAPSetupInfo* _Nonnull setupInfo,
        const char* _Nonnull salt,
        const char* _Nonnull verifier);

#ifdef MGOS_HAVE_RPC_COMMON
void mgos_hap_add_rpc_service(void);
#endif

/*
 * Some common definitions for reuse.
 */

/*
 * Accessory information Service.
 * Uses IDs 0x1000 - 0x1008.
 */
extern const HAPService mgos_hap_accessory_information_service;

/*
 * Protocol Information Service.
 * Uses IDs 0x1010 - 0x1012.
 */
extern const HAPService mgos_hap_protocol_information_service;

/*
 * Pairing Service.
 * Uses IDs 0x1020 - 0x1024.
 */
extern const HAPService mgos_hap_pairing_service;
