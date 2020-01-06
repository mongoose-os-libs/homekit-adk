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

#include "HAPPlatform.h"

#include "mgos_ro_vars.h"

uint32_t HAPPlatformGetCompatibilityVersion(void) {
    // HAP_PLATFORM_COMPATIBILITY_VERSION is 7 at the time of implementation.
    return 7;
}

const char* HAPPlatformGetIdentification(void) {
    return "Mongoose OS";
}

const char* HAPPlatformGetVersion(void) {
    return mgos_sys_ro_vars_get_fw_version();
}

const char* HAPPlatformGetBuild(void) {
    return mgos_sys_ro_vars_get_fw_id();
}
