/*
 * Copyright (c) 2020 Deomid "rojer" Ryabkov
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

#include "mgos_hap_accessory.hpp"

#include "mgos.hpp"

#include "HAPIPAccessoryProtocol.h"

namespace mgos {
namespace hap {

    Accessory::Accessory(
            uint64_t aid,
            HAPAccessoryCategory category,
            const std::string& name,
            const IdentifyCB& identify_cb,
            HAPAccessoryServerRef* server,
            const std::string& serial_number)
        : name_(name)
        , identify_cb_(identify_cb)
        , server_(server)
        , serial_number_(serial_number)
        , hai_({}) {
        HAPAccessory* a = &hai_.acc;
        a->aid = aid;
        a->category = category;
        a->name = name_.c_str();
        a->manufacturer = CS_STRINGIFY_MACRO(PRODUCT_VENDOR);
        a->model = CS_STRINGIFY_MACRO(PRODUCT_MODEL);
        if (serial_number_.length() == 0) {
            char sn[13] = "????????????";
            if (!mgos_conf_str_empty(mgos_sys_config_get_device_sn())) {
                serial_number_ = mgos_sys_config_get_device_sn();
            } else {
                mgos_expand_mac_address_placeholders(sn);
                serial_number_ = sn;
            }
            // To avoid duplicate serial numbers, append AID for accessories other than primary.
            if (aid != kHAPIPAccessoryProtocolAID_PrimaryAccessory) {
                snprintf(sn, sizeof(sn) - 1, "-%d", (int) aid);
                serial_number_.append(sn);
            }
        }
        serial_number_.shrink_to_fit();
        a->serialNumber = serial_number_.c_str();
        // Sanitize firmware version for HAP, it must be x.y.z and nothing else.
        // Strip any additional components after '-'.
        static char hap_fw_version[12];
        const char* p = mgos_sys_ro_vars_get_fw_version();
        size_t i = 0;
        while (*p != '\0' && (isdigit(*p) || *p == '.') && i < sizeof(hap_fw_version) - 1) {
            hap_fw_version[i++] = *p++;
        }
        a->firmwareVersion = hap_fw_version;
        a->hardwareVersion = CS_STRINGIFY_MACRO(PRODUCT_HW_REV);
        a->callbacks.identify = &Accessory::Identify;
        hai_.inst = this;
    }

    Accessory::~Accessory() {
    }

    HAPAccessoryServerRef* Accessory::server() const {
        return server_;
    }
    void Accessory::set_server(HAPAccessoryServerRef* server) {
        server_ = server;
    }

    void Accessory::SetName(const std::string& name) {
        name_ = name;
        name_.shrink_to_fit();
        hai_.acc.name = name_.c_str();
    }

    void Accessory::SetCategory(HAPAccessoryCategory category) {
        hai_.acc.category = category;
    }

    void Accessory::SetSerialNumber(const std::string& sn) {
        serial_number_ = sn;
        serial_number_.shrink_to_fit();
        hai_.acc.serialNumber = serial_number_.c_str();
    }

    void Accessory::AddService(Service* svc) {
        if (svc == nullptr)
            return;
        svc->set_parent(this);
        AddHAPService(svc->GetHAPService());
    }

    void Accessory::AddService(std::unique_ptr<Service> svc) {
        if (svc == nullptr)
            return;
        AddService(svc.get());
        svcs_.emplace_back(std::move(svc));
    }

    void Accessory::AddHAPService(const HAPService* svc) {
        if (svc == nullptr)
            return;
        if (!hap_svcs_.empty())
            hap_svcs_.pop_back();
        hap_svcs_.push_back(svc);
        hap_svcs_.push_back(nullptr);
        hap_svcs_.shrink_to_fit();
        hai_.acc.services = hap_svcs_.data();
    }

    const HAPAccessory* Accessory::GetHAPAccessory() const {
        if (hap_svcs_.empty())
            return nullptr;
        return &hai_.acc;
    }

    // static
    HAPError Accessory::Identify(
            HAPAccessoryServerRef* server,
            const HAPAccessoryIdentifyRequest* request,
            void* context) {
        auto* ai = reinterpret_cast<const HAPAccessoryWithInstance*>(request->accessory);
        auto* a = ai->inst;
        if (a->identify_cb_ == nullptr) {
            return kHAPError_Unknown;
        }
        return a->identify_cb_(request);
        (void) server;
        (void) context;
    }

} // namespace hap
} // namespace mgos
