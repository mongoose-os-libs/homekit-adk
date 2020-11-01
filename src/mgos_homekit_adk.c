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
#include "HAPAccessorySetup.h"

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

bool mgos_hap_setup_id_from_string(HAPSetupID* setupID, const char* id) {

    int config_level = 2;
    int id_len = strlen(id != NULL ? id : "");

    // when no setup id was provided, generate an id
    if (id_len == 0 || id_len > sizeof(HAPSetupID)) {

        HAPAccessorySetupGenerateRandomSetupID(setupID);

        struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));

        // load configuration for requested level
        if (!mgos_sys_config_load_level(cfg, (enum mgos_config_level) config_level)) {
            LOG(LL_ERROR, ("%s: failed to load config.", __func__));
            goto out;
        }

        mgos_sys_config_set_hap_setupid(setupID->stringValue);
        mgos_conf_set_str(&cfg->hap.setupid, setupID->stringValue);

        // save configuration
        char* msg = NULL;
        if (!mgos_sys_config_save_level(cfg, (enum mgos_config_level) config_level, false, &msg)) {
            LOG(LL_ERROR, ("%s: Error saving config: %s", __func__, (msg ? msg : "")));
            free(msg);
        }

    out:
        if (cfg != NULL) {
            mgos_conf_free(mgos_config_schema(), cfg);
            free(cfg);
        }

        // generated id is always valid.
        return true;
    }

    // check if provided id is valid
    if (HAPAccessorySetupIsValidSetupID(id)) {
        // copy valid id string to stringValue
        strncpy(setupID->stringValue, id, sizeof(HAPSetupID));
        return true;
    } // ending here ... if a non valid setup id was provided don't overrule it. Chosen fate.

    // no valid setup id was generated or read
    return false;
}

#ifdef MGOS_HAP_SIMPLE_CONFIG
static void load_setup_info_cb(int ev, void* ev_data, void* userdata) {
    struct mgos_hap_load_setup_info_arg* arg = (struct mgos_hap_load_setup_info_arg*) ev_data;
    if (!mgos_hap_setup_info_from_string(
                arg->setupInfo, mgos_sys_config_get_hap_salt(), mgos_sys_config_get_hap_verifier())) {
        LOG(LL_ERROR, ("Failed to load HAP accessory info from config!"));
    }
    (void) ev;
    (void) userdata;
}

static void load_setup_id_cb(int ev, void* ev_data, void* userdata) {

    LOG(LL_INFO, ("%s: Loading setup identifier...", __func__));

    struct mgos_hap_load_setup_id_arg* arg = (struct mgos_hap_load_setup_id_arg*) ev_data;

    if (!mgos_hap_setup_id_from_string(arg->setupID, mgos_sys_config_get_hap_setupid())) {
        LOG(LL_ERROR, ("Failed to load or generate HAP accessory setup identifier!"));
        *arg->valid = false;
    } else {
        *arg->valid = true;
        LOG(LL_INFO, ("Success loading setup id. (Identifier is \"%s\")", arg->setupID->stringValue));
    }

    (void) ev;
    (void) userdata;
}

bool mgos_hap_config_valid(void) {
    HAPSetupInfo setupInfo;
    HAPSetupID setupID;
    return mgos_hap_setup_info_from_string(
                   &setupInfo, mgos_sys_config_get_hap_salt(), mgos_sys_config_get_hap_verifier()) &&
           mgos_hap_setup_id_from_string(&setupID, mgos_sys_config_get_hap_setupid());
}
#endif

bool mgos_homekit_adk_init(void) {
#ifdef MGOS_HAP_SIMPLE_CONFIG
    mgos_event_add_handler(MGOS_HAP_EV_LOAD_SETUP_INFO, load_setup_info_cb, NULL);
    mgos_event_add_handler(MGOS_HAP_EV_LOAD_SETUP_ID, load_setup_id_cb, NULL);
#endif
    mgos_event_register_base(MGOS_HAP_EV_BASE, "HAP");
    return true;
}
