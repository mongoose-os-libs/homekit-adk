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

#include "HAPPlatformServiceDiscovery.h"
#include "HAPPlatformServiceDiscovery+Init.h"

#include "mgos.h"
#include "mgos_dns_sd.h"

void HAPPlatformServiceDiscoveryRegister(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        const char* name,
        const char* protocol,
        HAPNetworkPort port,
        HAPPlatformServiceDiscoveryTXTRecord* txtRecords,
        size_t numTXTRecords) {
    struct mgos_dns_sd_txt_entry txt[numTXTRecords + 1];
    memset(txt, 0, sizeof(txt));
    for (size_t i = 0; i < numTXTRecords; i++) {
        txt[i].key = txtRecords[i].key;
        txt[i].value = mg_mk_str_n(txtRecords[i].value.bytes, txtRecords[i].value.numBytes);
    }
    if (!mgos_dns_sd_add_service_instance(name, protocol, port, txt)) {
        LOG(LL_ERROR, ("Failed to advertise %s.%s!", name, protocol));
    }
    serviceDiscovery->name = name;
    serviceDiscovery->protocol = protocol;
    serviceDiscovery->port = port;
}

void HAPPlatformServiceDiscoveryUpdateTXTRecords(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        HAPPlatformServiceDiscoveryTXTRecord* txtRecords,
        size_t numTXTRecords) {
    HAPPlatformServiceDiscoveryRegister(
            serviceDiscovery,
            serviceDiscovery->name,
            serviceDiscovery->protocol,
            serviceDiscovery->port,
            txtRecords,
            numTXTRecords);
}

void HAPPlatformServiceDiscoveryStop(HAPPlatformServiceDiscoveryRef serviceDiscovery) {
    mgos_dns_sd_remove_service_instance(serviceDiscovery->name, serviceDiscovery->protocol, serviceDiscovery->port);
}

void HAPPlatformServiceDiscoveryCreate(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        const HAPPlatformServiceDiscoveryOptions* options HAP_UNUSED) {
    memset(serviceDiscovery, 0, sizeof(*serviceDiscovery));
}
