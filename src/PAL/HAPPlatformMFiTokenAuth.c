/*
 * Copyright (c) 2021 Deomid "rojer" Ryabkov
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

#include <stdio.h>

#include "HAPPlatform.h"
#include "HAPPlatformKeyValueStore+SDKDomains.h"
#include "HAPPlatformMFiTokenAuth+Init.h"

#include "mgos.h"
#include "mgos_hap.h"

void HAPPlatformMFiTokenAuthCreate(
        HAPPlatformMFiTokenAuthRef mfiTokenAuth,
        const HAPPlatformMFiTokenAuthOptions* options) {
    mfiTokenAuth->keyValueStore = NULL;
    (void) options;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiTokenAuthLoad(
        HAPPlatformMFiTokenAuthRef mfiTokenAuth,
        bool* valid,
        HAPPlatformMFiTokenAuthUUID* _Nullable mfiTokenUUID,
        void* _Nullable mfiTokenBytes,
        size_t maxMFiTokenBytes,
        size_t* _Nullable numMFiTokenBytes) {
    HAPPrecondition(mfiTokenAuth);
    HAPPrecondition(valid);
    HAPPrecondition((mfiTokenUUID == NULL) == (mfiTokenBytes == NULL));
    HAPPrecondition(!maxMFiTokenBytes || mfiTokenBytes);
    HAPPrecondition((mfiTokenBytes == NULL) == (numMFiTokenBytes == NULL));

    *valid = false;

    const char* uuid_str = mgos_sys_config_get_hap_mfi_uuid();
    const char* token_str = mgos_sys_config_get_hap_mfi_token();
    if (mgos_conf_str_empty(uuid_str) || mgos_conf_str_empty(token_str)) {
        return kHAPError_None;
    }

    size_t uuid_str_len = strlen(uuid_str);
    size_t token_str_len = strlen(token_str);
    if (uuid_str_len != 36 || token_str_len > 1368) {
        return kHAPError_Unknown;
    }

    if (mfiTokenUUID != NULL) {
        unsigned int uuid_bytes[32];
        if (sscanf(uuid_str,
                   "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   &uuid_bytes[15],
                   &uuid_bytes[14],
                   &uuid_bytes[13],
                   &uuid_bytes[12],
                   &uuid_bytes[11],
                   &uuid_bytes[10],
                   &uuid_bytes[9],
                   &uuid_bytes[8],
                   &uuid_bytes[7],
                   &uuid_bytes[6],
                   &uuid_bytes[5],
                   &uuid_bytes[4],
                   &uuid_bytes[3],
                   &uuid_bytes[2],
                   &uuid_bytes[1],
                   &uuid_bytes[0]) != 16) {
            return kHAPError_Unknown;
        }
        for (int i = 0; i < 16; i++) {
            mfiTokenUUID->bytes[i] = (uint8_t) uuid_bytes[i];
        }
    }

    if (mfiTokenBytes != NULL && maxMFiTokenBytes > 0) {
        int dlen = 0;
        cs_base64_decode((const void*) token_str, (int) token_str_len, mfiTokenBytes, &dlen);
        *numMFiTokenBytes = dlen;
    }

    *valid = true;

    return kHAPError_None;
}

bool HAPPlatformMFiTokenAuthIsProvisioned(HAPPlatformMFiTokenAuthRef mfiTokenAuth) {
    bool valid;
    return (HAPPlatformMFiTokenAuthLoad(mfiTokenAuth, &valid, NULL, NULL, 0, NULL) == kHAPError_None && valid);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiTokenAuthUpdate(
        HAPPlatformMFiTokenAuthRef mfiTokenAuth,
        const void* mfiTokenBytes,
        size_t numMFiTokenBytes) {
    HAPPrecondition(mfiTokenAuth);
    HAPPrecondition(mfiTokenBytes);
    HAPPrecondition(numMFiTokenBytes <= kHAPPlatformMFiTokenAuth_MaxMFiTokenBytes);

    char* token_str = calloc((numMFiTokenBytes * 8) / 6 + 10, 1);
    if (token_str == NULL) {
        return kHAPError_Unknown;
    }

    cs_base64_encode(mfiTokenBytes, numMFiTokenBytes, token_str, NULL);

    bool res = mgos_hap_config_set(
            NULL, NULL, NULL, mgos_sys_config_get_hap_mfi_uuid(), token_str, MGOS_HAP_CONFIG_SET_MFI_AUTH);

    LOG(LL_INFO, ("Updated device token: %s", token_str));

    free(token_str);
    return (res ? kHAPError_None : kHAPError_Unknown);
}
