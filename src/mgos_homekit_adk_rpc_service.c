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

#if defined(MGOS_HAVE_RPC_COMMON) && defined(MGOS_HAP_SIMPLE_CONFIG)

#include "mgos_rpc.h"

#include "HAPAccessoryServer+Internal.h"
#include "HAPAccessorySetup.h"
#include "HAPCrypto.h"

static const HAPAccessory* s_acc = NULL;
static HAPAccessoryServerRef* s_server = NULL;
static void (*s_start_cb)(HAPAccessoryServerRef* _Nonnull server) = NULL;

static void clear_provision_info(int config_level) {

    struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));

    // load configuration for requested level
    if (!mgos_sys_config_load_level(cfg, (enum mgos_config_level) config_level)) {
        LOG(LL_ERROR, ("%s: failed to load config.", __func__));
        goto out;
    }

    mgos_sys_config_set_hap_salt(NULL);
    mgos_sys_config_set_hap_verifier(NULL);
    mgos_sys_config_set_hap_setupid(NULL);

    mgos_conf_set_str(&cfg->hap.salt, NULL);
    mgos_conf_set_str(&cfg->hap.verifier, NULL);
    mgos_conf_set_str(&cfg->hap.setupid, NULL);

    // save configuration
    char* msg = NULL;
    if (!mgos_sys_config_save_level(cfg, (enum mgos_config_level) config_level, false, &msg)) {
        LOG(LL_ERROR, ("Error saving config: %s", (msg ? msg : "")));
        free(msg);
    }

out:
    if (cfg != NULL) {
        mgos_conf_free(mgos_config_schema(), cfg);
        free(cfg);
    }

    return;
}

static bool provision(HAPSetupInfo* info, HAPSetupID* id, int config_level) {

    char* salt = NULL;
    char* verifier = NULL;

    bool provisioned = false;

    struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));

    // load configuration for requested level
    if (!mgos_sys_config_load_level(cfg, (enum mgos_config_level) config_level)) {
        LOG(LL_ERROR, ("failed to load config"));
        goto out;
    }

    // HAPSetupID provided, provision for dynamic setup info pairing (R14, setupDisplay, setupNFC)
    if (id != NULL) {
        // set it in configuration
        mgos_conf_set_str(&cfg->hap.setupid, id->stringValue);
        mgos_sys_config_set_hap_setupid(id->stringValue);
    }

    // HAPSetupInfo provided
    if (info != NULL) {
        salt = calloc(1, 24 + 1);
        cs_base64_encode(info->salt, 16, salt);

        verifier = calloc(1, 512 + 1);
        cs_base64_encode(info->verifier, 384, verifier);

        // set salted verifier in configuration
        mgos_conf_set_str(&cfg->hap.salt, salt);
        mgos_conf_set_str(&cfg->hap.verifier, verifier);

        mgos_sys_config_set_hap_salt(salt);
        mgos_sys_config_set_hap_verifier(verifier);
    }

    // save configuration
    char* msg = NULL;
    if (!mgos_sys_config_save_level(cfg, (enum mgos_config_level) config_level, false, &msg)) {
        LOG(LL_ERROR, ("Error saving config: %s", (msg ? msg : "")));
        free(msg);
        provisioned = false;
    } else {
        provisioned = true;
    }

out:
    if (cfg != NULL) {
        mgos_conf_free(mgos_config_schema(), cfg);
        free(cfg);
    }

    free(salt);
    free(verifier);

    return provisioned;
}

static void mgos_hap_setup_handler(
        struct mg_rpc_request_info* ri,
        void* cb_arg,
        struct mg_rpc_frame_info* fi,
        struct mg_str args) {

    char* setup_id = NULL;
    char* code = NULL;
    char *salt = NULL, *verifier = NULL;

    int config_level = 2;
    bool start_server = false;

    if (HAPAccessoryServerIsPaired(s_server)) {

        mg_rpc_send_error_jsonf(
                ri, 400, "{ err: %d, msg: %Q}", 100, "Accessory is paired. Unpair or reset server first.");
        ri = NULL;
        goto out;
    }

    json_scanf(args.p, args.len, ri->args_fmt, &setup_id, &code, &salt, &verifier, &config_level, &start_server);

    // Setup identifier
    HAPSetupID setupID;
    bool setup_id_is_set = false;

    // if a previous setup identifier exists, use it
    if (mgos_hap_setup_id_from_string(&setupID, mgos_sys_config_get_hap_setupid())) {
        setup_id_is_set = true;
    }

    // override if a new setup identifier is provided, hash is already deployed
    if (setup_id != NULL) {
        if (HAPAccessorySetupIsValidSetupID(setup_id)) {
            HAPRawBufferCopyBytes(setupID.stringValue, setup_id, sizeof setupID.stringValue);
            setup_id_is_set = true;
        } else {
            // invalid setup identifier provided
            mg_rpc_send_error_jsonf(ri, 400, "{ err: %d, msg: %Q}", 101, "Invalid setup identifier");
            ri = NULL;
            goto out;
        }

    } else if (!setup_id_is_set) { // don't override a saved identifier.
        HAPAccessorySetupGenerateRandomSetupID(&setupID);
        setup_id_is_set = true;
    }

    // Setup info
    HAPSetupInfo setupInfo;
    bool setup_info_is_set = false;

    if (salt != NULL) {
        if (verifier != NULL) {
            // fill setup info with legacy pairing info
            if (mgos_hap_setup_info_from_string(&setupInfo, salt, verifier)) {
                setup_info_is_set = true; // legacy provisioning
            }
        }
    }

    // if a setup code is provided, provided salt and verifier are dismissed.
    // if either a code was provided nor salted verifier, create a valid code.
    HAPSetupCode setupCode;
    bool code_is_set = false;

    if (code != NULL) {
        if (HAPAccessorySetupIsValidSetupCode(code)) {
            HAPRawBufferCopyBytes(setupCode.stringValue, code, sizeof setupCode.stringValue);
            code_is_set = true;
        }
    } else if (!setup_info_is_set) {

        // create a valid code
        HAPAccessorySetupGenerateRandomSetupCode(&setupCode);
        code_is_set = true;
    }

    // the code was set if no legacy pairing info was provided (salted verifier), check it
    if (code_is_set && !setup_info_is_set) {

        // create salted verifier
        HAPPlatformRandomNumberFill(setupInfo.salt, sizeof setupInfo.salt);

        const uint8_t srpUserName[] = "Pair-Setup";
        HAP_srp_verifier(
                setupInfo.verifier,
                setupInfo.salt,
                srpUserName,
                sizeof(srpUserName) - 1,
                (const uint8_t*) setupCode.stringValue,
                sizeof setupCode.stringValue - 1);

        setup_info_is_set = true;
    }

    bool provisioned_successful = false;

    if (setup_info_is_set) {

        provisioned_successful = provision(&setupInfo, &setupID, config_level);

        if (!provisioned_successful) {
            mg_rpc_send_error_jsonf(ri, 400, "{ err: %d, msg: %Q}", 102, "Failed to save setup info.");
            ri = NULL;
            goto out;
        } else {
            start_server = true;
        }
    }

    // Setup payload.
    HAPSetupPayload setupPayload;
    bool setup_payload_is_set = false;

    if (setup_id_is_set && code_is_set) {
        // Derive setup payload flags and category.
        HAPAccessoryServer* server = (HAPAccessoryServer*) s_server;
        HAPAccessorySetupSetupPayloadFlags flags = { .isPaired = HAPAccessoryServerIsPaired(s_server),
                                                     .ipSupported = (server->transports.ip != NULL),
                                                     .bleSupported = (server->transports.ble != NULL) };
        HAPAccessoryCategory category = s_acc->category;

        LOG(LL_INFO,
            ("Creating payload with code: %s, id: %s, category: %d",
             setupCode.stringValue,
             setupID.stringValue,
             category));
        HAPAccessorySetupGetSetupPayload(&setupPayload, &setupCode, &setupID, flags, category);
        setup_payload_is_set = true;
    }

    char* successMessage = "{code: %Q, payload: %Q}";

    mg_rpc_send_responsef(
            ri,
            successMessage,
            (code_is_set ? setupCode.stringValue : ""),
            (setup_payload_is_set ? setupPayload.stringValue : ""));

    if (start_server && HAPAccessoryServerGetState(s_server) == kHAPAccessoryServerState_Idle) {
        s_start_cb(s_server);
    }

out:
    free(setup_id);
    free(code);
    free(salt);
    free(verifier);
    (void) cb_arg;
    (void) fi;
}

struct reset_ctx {
    struct mg_rpc_request_info* ri;
    bool reset_server;
    bool reset_code;
    bool stopped_server;
};

static void stop_and_reset(void* arg) {
    struct reset_ctx* ctx = arg;
    switch (HAPAccessoryServerGetState(s_server)) {
        case kHAPAccessoryServerState_Running:
            ctx->stopped_server = true;
            LOG(LL_INFO, ("Stopping server for reset"));
            HAPAccessoryServerStop(s_server);
            // fallthrough
        case kHAPAccessoryServerState_Stopping:
            // Wait some more.
            mgos_set_timer(100, 0, stop_and_reset, arg);
            break;
        case kHAPAccessoryServerState_Idle: {
            bool res = true;
            HAPError err = kHAPError_None;
            if (ctx->reset_server) {
                LOG(LL_INFO, ("Resetting server"));
                HAPAccessoryServer* server = (HAPAccessoryServer*) s_server;
                err = HAPRestoreFactorySettings(server->platform.keyValueStore);
                if (err != kHAPError_None) {
                    res = false;
                }
            }
            if (ctx->reset_code) {
                LOG(LL_INFO, ("Resetting code"));
                // How can we determine the right level?
                clear_provision_info(2);
            }
            if (res && ctx->stopped_server && mgos_hap_config_valid()) {
                // We stopped server for reset, restart it.
                s_start_cb(s_server);
            }
            if (res) {
                mg_rpc_send_responsef(ctx->ri, NULL);
            } else {
                mg_rpc_send_errorf(ctx->ri, 500, "error %d", err);
            }
            free(ctx);
        }
    }
}

static void mgos_hap_reset_handler(
        struct mg_rpc_request_info* ri,
        void* cb_arg,
        struct mg_rpc_frame_info* fi,
        struct mg_str args) {
    bool reset_code = false, reset_server = false;
    json_scanf(args.p, args.len, ri->args_fmt, &reset_server, &reset_code);

    if (!reset_server && !reset_code) {
        mg_rpc_send_errorf(ri, 400, "Reset what?");
        return;
    }

    struct reset_ctx* ctx = calloc(1, sizeof(*ctx));
    ctx->ri = ri;
    ctx->reset_server = reset_server;
    ctx->reset_code = reset_code;
    stop_and_reset(ctx);

    (void) cb_arg;
    (void) fi;
}

static void simple_start_cb(HAPAccessoryServerRef* _Nonnull server) {
    HAPAccessoryServerStart(server, s_acc);
}

void mgos_hap_add_rpc_service(HAPAccessoryServerRef* server, const HAPAccessory* _Nonnull acc) {
    s_acc = acc;
    mgos_hap_add_rpc_service_cb(server, simple_start_cb);
}

void mgos_hap_add_rpc_service_cb(
        HAPAccessoryServerRef* _Nonnull server,
        void(server_start_cb)(HAPAccessoryServerRef* _Nonnull server)) {
    s_server = server;
    s_start_cb = server_start_cb;
    mg_rpc_add_handler(
            mgos_rpc_get_global(),
            "HAP.Setup",
            "{setup_id: %Q, code: %Q, salt: %Q, verifier: %Q, config_level: %d, start_server: %B}",
            mgos_hap_setup_handler,
            NULL);
    mg_rpc_add_handler(
            mgos_rpc_get_global(), "HAP.Reset", "{reset_server: %B, reset_code: %B}", mgos_hap_reset_handler, NULL);
}

#endif // defined(MGOS_HAVE_RPC_COMMON) && defined(MGOS_HAP_SIMPLE_CONFIG)
