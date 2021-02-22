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

static bool set_salt_and_verfier(const char* salt, const char* verifier, int config_level) {

    struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));

    if (!mgos_sys_config_load_level(cfg, (enum mgos_config_level) config_level)) {
        LOG(LL_ERROR, ("failed to load config"));
        goto out;
    }

    mgos_conf_set_str(&cfg->hap.salt, salt);
    mgos_conf_set_str(&cfg->hap.verifier, verifier);

    char* msg = NULL;
    if (!mgos_sys_config_save_level(cfg, (enum mgos_config_level) config_level, false, &msg)) {
        LOG(LL_ERROR, ("error saving config: %s", (msg ? msg : "")));
        free(msg);
        goto out;
    }

    mgos_sys_config_set_hap_salt(salt);
    mgos_sys_config_set_hap_verifier(verifier);

out:
    if (cfg != NULL) {
        mgos_conf_free(mgos_config_schema(), cfg);
        free(cfg);
    }
    return true;
}

static void mgos_hap_setup_handler(
        struct mg_rpc_request_info* ri,
        void* cb_arg,
        struct mg_rpc_frame_info* fi,
        struct mg_str args) {
    char* code = NULL;
    char *salt = NULL, *verifier = NULL;
    int config_level = 2;
    bool start_server = true;
    HAPSetupInfo setupInfo;

    json_scanf(args.p, args.len, ri->args_fmt, &code, &salt, &verifier, &config_level, &start_server);

    if (code != NULL && (salt == NULL && verifier == NULL)) {
        if (!HAPAccessorySetupIsValidSetupCode(code)) {
            mg_rpc_send_errorf(ri, 400, "invalid code");
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
    } else if (code == NULL && (salt != NULL && verifier != NULL)) {
        if (!mgos_hap_setup_info_from_string(&setupInfo, salt, verifier)) {
            mg_rpc_send_errorf(ri, 400, "invalid salt + verifier");
            ri = NULL;
            goto out;
        }
    } else {
        mg_rpc_send_errorf(ri, 400, "either code or salt + verifier required");
        ri = NULL;
        goto out;
    }

    if (!set_salt_and_verfier(salt, verifier, config_level)) {
        mg_rpc_send_errorf(ri, 500, "failed to set code");
        ri = NULL;
        goto out;
    }

    mg_rpc_send_responsef(ri, NULL);

    if (start_server && HAPAccessoryServerGetState(s_server) == kHAPAccessoryServerState_Idle) {
        s_start_cb(s_server);
    }

out:
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
                if (!set_salt_and_verfier(NULL, NULL, 2)) {
                    res = false;
                }
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
            "{code: %Q, salt: %Q, verifier: %Q, config_level: %d, start_server: %B}",
            mgos_hap_setup_handler,
            NULL);
    mg_rpc_add_handler(
            mgos_rpc_get_global(), "HAP.Reset", "{reset_server: %B, reset_code: %B}", mgos_hap_reset_handler, NULL);
}

#endif // defined(MGOS_HAVE_RPC_COMMON) && defined(MGOS_HAP_SIMPLE_CONFIG)
