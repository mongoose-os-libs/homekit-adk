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

#include "HAPCrypto.h"

static void mgos_hap_setup_info_handler(
        struct mg_rpc_request_info* ri,
        void* cb_arg,
        struct mg_rpc_frame_info* fi,
        struct mg_str args) {
    char* code = NULL;
    char *salt = NULL, *verifier = NULL;
    int config_level = 2;
    bool reboot = false;
    HAPSetupInfo setupInfo;

    json_scanf(args.p, args.len, ri->args_fmt, &code, &salt, &verifier, &config_level, &reboot);

    if (code != NULL && (salt == NULL && verifier == NULL)) {
        if (strlen(code) != 10) {
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
        cs_base64_encode(setupInfo.salt, 16, salt);
        verifier = calloc(1, 512 + 1);
        cs_base64_encode(setupInfo.verifier, 384, verifier);
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

    struct mgos_config* cfg = (struct mgos_config*) calloc(1, sizeof(*cfg));
    if (!mgos_sys_config_load_level(cfg, (enum mgos_config_level) config_level)) {
        mg_rpc_send_errorf(ri, 500, "failed to load config");
        goto out;
    }

    cfg->hap.salt = salt;
    cfg->hap.verifier = verifier;

    char* msg = NULL;
    if (!mgos_sys_config_save_level(cfg, (enum mgos_config_level) config_level, false, &msg)) {
        mg_rpc_send_errorf(ri, -1, "error saving config: %s", (msg ? msg : ""));
        ri = NULL;
        free(msg);
        goto out;
    }

    cfg->hap.salt = NULL;
    cfg->hap.verifier = NULL;

    mgos_conf_free(mgos_config_schema(), cfg);

    mgos_sys_config_set_hap_salt(salt);
    salt = NULL;
    mgos_sys_config_set_hap_verifier(verifier);
    verifier = NULL;

    mg_rpc_send_responsef(ri, NULL);

    if (reboot) {
        mgos_system_restart_after(500);
    }

out:
    free(code);
    free(salt);
    free(verifier);
    (void) cb_arg;
    (void) fi;
}

void mgos_hap_add_rpc_service(void) {
    struct mg_rpc* c = mgos_rpc_get_global();
    mg_rpc_add_handler(
            c,
            "HAP.SetupInfo",
            "{code: %Q, salt: %Q, verifier: %Q, config_level: %d, reboot: %B}",
            mgos_hap_setup_info_handler,
            NULL);
}

#endif // defined(MGOS_HAVE_RPC_COMMON) && defined(MGOS_HAP_SIMPLE_CONFIG)
