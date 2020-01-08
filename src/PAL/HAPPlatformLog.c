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

#include "HAPPlatformLog.h"

#include "mgos.h"
#include "mongoose.h"

int HAPPlatformLogLevel(void) {
    int l = mgos_sys_config_get_debug_level();
    if (l < 0) {
        return kHAPPlatformLogEnabledTypes_None;
    }
    if (l > 3) {
        return kHAPPlatformLogEnabledTypes_Debug;
    };
    return (HAPPlatformLogEnabledTypes) l;
}

HAPPlatformLogEnabledTypes HAPPlatformLogGetEnabledTypes(const HAPLogObject* log HAP_UNUSED) {
    return (HAPPlatformLogEnabledTypes) HAPPlatformLogLevel();
}

void HAPPlatformLogCapture(
        const HAPLogObject* log,
        HAPLogType type HAP_UNUSED,
        const char* message,
        const void* _Nullable bufferBytes,
        size_t numBufferBytes) {
    const char* category = log->category;
    if (category == NULL) {
        category = "HAP";
    }
    fprintf(stderr, "%s %s\n", category, message);
    if (numBufferBytes > 0) {
        mg_hexdumpf(stderr, bufferBytes, numBufferBytes);
    }
}
