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
static void (*s_stop_cb)(HAPAccessoryServerRef* _Nonnull server) = NULL;
static void (*s_start_cb)(HAPAccessoryServerRef* _Nonnull server) = NULL;

static void mgos_hap_stop_and_reset_server(
        struct mg_rpc_request_info* ri,
        bool reset_code,
        bool reset_server,
        char* setup_code);

static void hap_setup_response(struct mg_rpc_request_info* ri, const char* setup_code, const char* setup_id) {
    HAPAccessoryServer* server = (HAPAccessoryServer*) s_server;
    if (setup_code == NULL || setup_id == NULL || server == NULL || server->primaryAccessory == NULL) {
        mg_rpc_send_responsef(ri, "{}");
        return;
    }
    HAPSetupCode sc = { 0 };
    strncpy(sc.stringValue, setup_code, sizeof(sc.stringValue) - 1);
    HAPSetupID si = { 0 };
    strncpy(si.stringValue, setup_id, sizeof(si.stringValue) - 1);
    HAPSetupPayload payload = { 0 };
    HAPAccessorySetupSetupPayloadFlags flags = {
        .isPaired = false,
        .ipSupported = (server->transports.ip != NULL),
        .bleSupported = (server->transports.ble != NULL),
    };
    HAPAccessorySetupGetSetupPayload(&payload, &sc, &si, flags, server->primaryAccessory->category);
    mg_rpc_send_responsef(ri, "{code: %Q, id: %Q, url: %Q}", setup_code, setup_id, payload.stringValue);
}

#define SET_SETUP_INFO (1 << 0)
#define SET_SETUP_ID (1 << 2)
#define SET_MFI_AUTH (1 << 3)
static bool set_hap_config(
        const char* salt,
        const char* verifier,
        const char* setup_id,
        const char* mfi_uuid,
        const char* mfi_token,
        uint8_t flags) {

    struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));

    enum mgos_config_level config_level = (enum mgos_config_level) MGOS_HAP_SIMPLE_CONFIG_LEVEL;

    if (!mgos_sys_config_load_level(cfg, config_level)) {
        LOG(LL_ERROR, ("failed to load config"));
        goto out;
    }

    if ((flags & SET_SETUP_INFO) != 0) {
        mgos_conf_set_str(&cfg->hap.salt, salt);
        mgos_conf_set_str(&cfg->hap.verifier, verifier);
    }
    if ((flags & SET_SETUP_ID) != 0) {
        mgos_conf_set_str(&cfg->hap.setup_id, setup_id);
    }
    if ((flags & SET_MFI_AUTH) != 0) {
        mgos_conf_set_str(&cfg->hap.mfi_uuid, mfi_uuid);
        mgos_conf_set_str(&cfg->hap.mfi_token, mfi_token);
    }

    char* msg = NULL;
    if (!mgos_sys_config_save_level(cfg, config_level, false, &msg)) {
        LOG(LL_ERROR, ("error saving config: %s", (msg ? msg : "")));
        free(msg);
        goto out;
    }

    mgos_conf_free(mgos_config_schema(), cfg);
    free(cfg);
    cfg = NULL;

    if ((flags & SET_SETUP_INFO) != 0) {
        mgos_sys_config_set_hap_salt(salt);
        mgos_sys_config_set_hap_verifier(verifier);
    }
    if ((flags & SET_SETUP_ID) != 0) {
        mgos_sys_config_set_hap_setup_id(setup_id);
    }
    if ((flags & SET_MFI_AUTH) != 0) {
        mgos_sys_config_set_hap_mfi_uuid(mfi_uuid);
        mgos_sys_config_set_hap_mfi_token(mfi_token);
    }

out:
    if (cfg != NULL) {
        mgos_conf_free(mgos_config_schema(), cfg);
        free(cfg);
    }
    return true;
}

bool mgos_hap_config_reset(void) {
    return set_hap_config(NULL, NULL, NULL, NULL, NULL, (SET_SETUP_INFO | SET_SETUP_ID));
}

static void mgos_hap_setup_handler(
        struct mg_rpc_request_info* ri,
        void* cb_arg,
        struct mg_rpc_frame_info* fi,
        struct mg_str args) {
    char *code = NULL, *id = NULL;
    char *salt = NULL, *verifier = NULL;
    char *mfi_uuid = NULL, *mfi_token = NULL;
    int8_t start_server = true, reset_server = true;
    HAPSetupInfo setupInfo;
    HAPSetupID setup_id;
    uint8_t set_flags = 0;

    json_scanf(
            args.p,
            args.len,
            ri->args_fmt,
            &code,
            &id,
            &salt,
            &verifier,
            &mfi_uuid,
            &mfi_token,
            &start_server,
            &reset_server);

    if (code != NULL && (salt == NULL && verifier == NULL)) {
        if (strcmp(code, "RANDOMCODE") == 0) {
            HAPSetupCode random_code;
            HAPAccessorySetupGenerateRandomSetupCode(&random_code);
            strncpy(code, random_code.stringValue, 10);
        } else if (!HAPAccessorySetupIsValidSetupCode(code)) {
            mg_rpc_send_errorf(ri, 400, "invalid %s", "code");
            ri = NULL;
            goto out;
        }
        HAPPlatformRandomNumberFill(setupInfo.salt, sizeof setupInfo.salt);
        const uint8_t srpUserName[] = "Pair-Setup";
        HAP_srp_verifier(
                setupInfo.verifier,
                setupInfo.salt,
                srpUserName,
                sizeof(srpUserName) - 1,
                (const uint8_t*) code,
                strlen(code));
        salt = calloc(1, 24 + 1);
        cs_base64_encode(setupInfo.salt, 16, salt, NULL);
        verifier = calloc(1, 512 + 1);
        cs_base64_encode(setupInfo.verifier, 384, verifier, NULL);
        set_flags |= SET_SETUP_INFO;
    } else if (code == NULL && (salt != NULL && verifier != NULL)) {
        if (!mgos_hap_setup_info_from_string(&setupInfo, salt, verifier)) {
            mg_rpc_send_errorf(ri, 400, "invalid %s", "salt + verifier");
            ri = NULL;
            goto out;
        }
        set_flags |= SET_SETUP_INFO;
    }

    if (id != NULL) {
        if (strlen(id) == 0) {
            memset(&setup_id, 0, sizeof(setup_id));
        } else if (strcmp(id, "RANDOMID") == 0) {
            HAPAccessorySetupGenerateRandomSetupID(&setup_id);
        } else if (HAPAccessorySetupIsValidSetupID(id)) {
            memcpy(setup_id.stringValue, id, sizeof(setup_id.stringValue) - 1);
        } else {
            mg_rpc_send_errorf(ri, 400, "invalid %s '%s'", "id", id);
            ri = NULL;
            goto out;
        }
        set_flags |= SET_SETUP_ID;
    }

    if (mfi_uuid != NULL && mfi_token != NULL) {
        set_flags |= SET_MFI_AUTH;
    }

    if (set_flags == 0) {
        mg_rpc_send_errorf(ri, 400, "nothing to set");
        ri = NULL;
        goto out;
    }

    if (!set_hap_config(salt, verifier, setup_id.stringValue, mfi_uuid, mfi_token, set_flags)) {
        mg_rpc_send_errorf(ri, 500, "failed to set code");
        ri = NULL;
        goto out;
    }

    if (start_server) {
        switch (HAPAccessoryServerGetState(s_server)) {
            case kHAPAccessoryServerState_Idle:
                s_start_cb(s_server);
                hap_setup_response(ri, code, mgos_sys_config_get_hap_setup_id());
                break;
            case kHAPAccessoryServerState_Running:
                // Restart server without resetting code (which we've just set).
                mgos_hap_stop_and_reset_server(ri, false /* reset_code */, reset_server, code);
                code = NULL;
                break;
            default:
                mg_rpc_send_responsef(ri, NULL, mgos_sys_config_get_hap_setup_id());
                break;
        }
    }

out:
    free(id);
    free(code);
    free(salt);
    free(verifier);
    free(mfi_uuid);
    free(mfi_token);
    (void) cb_arg;
    (void) fi;
}

struct reset_ctx {
    struct mg_rpc_request_info* ri;
    bool reset_server;
    bool reset_code;
    bool stopped_server;
    char* setup_code;
};

static void stop_and_reset(void* arg) {
    struct reset_ctx* ctx = arg;
    switch (HAPAccessoryServerGetState(s_server)) {
        case kHAPAccessoryServerState_Running:
            ctx->stopped_server = true;
            LOG(LL_INFO, ("Stopping server for reset"));
            s_stop_cb(s_server);
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
                if (!mgos_hap_config_reset()) {
                    res = false;
                }
            }
            if (res && ctx->stopped_server && mgos_hap_config_valid()) {
                // We stopped server for reset, restart it.
                s_start_cb(s_server);
            }
            if (res) {
                if (ctx->setup_code != NULL) {
                    hap_setup_response(ctx->ri, ctx->setup_code, mgos_sys_config_get_hap_setup_id());
                    free(ctx->setup_code);
                } else {
                    mg_rpc_send_responsef(ctx->ri, NULL);
                }
            } else {
                mg_rpc_send_errorf(ctx->ri, 500, "error %d", err);
            }
            free(ctx);
        }
    }
}

static void mgos_hap_stop_and_reset_server(
        struct mg_rpc_request_info* ri,
        bool reset_code,
        bool reset_server,
        char* setup_code) {
    struct reset_ctx* ctx = calloc(1, sizeof(*ctx));
    ctx->ri = ri;
    ctx->reset_server = reset_server;
    ctx->reset_code = reset_code;
    ctx->setup_code = setup_code;
    stop_and_reset(ctx);
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

    mgos_hap_stop_and_reset_server(ri, true /* reset_code */, true /* reset_server */, NULL /* setup_code */);

    (void) cb_arg;
    (void) fi;
}

static void simple_stop_cb(HAPAccessoryServerRef* _Nonnull server) {
    HAPAccessoryServerStop(server);
}

static void simple_start_cb(HAPAccessoryServerRef* _Nonnull server) {
    HAPAccessoryServerStart(server, s_acc);
}

void mgos_hap_add_rpc_service(HAPAccessoryServerRef* server, const HAPAccessory* _Nonnull acc) {
    s_acc = acc;
    mgos_hap_add_rpc_service_cb(server, simple_stop_cb, simple_start_cb);
}

void mgos_hap_add_rpc_service_cb(
        HAPAccessoryServerRef* _Nonnull server,
        void(server_stop_cb)(HAPAccessoryServerRef* _Nonnull server),
        void(server_start_cb)(HAPAccessoryServerRef* _Nonnull server)) {
    s_server = server;
    s_stop_cb = server_stop_cb;
    s_start_cb = server_start_cb;
    mg_rpc_add_handler(
            mgos_rpc_get_global(),
            "HAP.Setup",
            "{code: %Q, id: %Q, salt: %Q, verifier: %Q, uuid: %Q, token: %Q, "
            "start_server: %B, reset_server: %B}",
            mgos_hap_setup_handler,
            NULL);
    mg_rpc_add_handler(
            mgos_rpc_get_global(), "HAP.Reset", "{reset_server: %B, reset_code: %B}", mgos_hap_reset_handler, NULL);
}

#endif // defined(MGOS_HAVE_RPC_COMMON) && defined(MGOS_HAP_SIMPLE_CONFIG)
