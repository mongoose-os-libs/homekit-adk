#ifdef MGOS_HAVE_BT_COMMON
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

// vim: tabstop=4 expandtab shiftwidth=4 ai cin smarttab

#include "HAPPlatformBLEPeripheralManager+Init.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mgos.hpp"
#include "mgos_bt.h"
#include "mgos_bt_gap.h"
#include "mgos_bt_gatts.h"

#include "HAPPlatformBLEPeripheralManager.h"

namespace mgos {
namespace hap {

    std::string HAPPlatformBLEPeripheralManagerUUIDToStr(const HAPPlatformBLEPeripheralManagerUUID* type) {
        struct mgos_bt_uuid uuid;
        char buf[MGOS_BT_UUID_STR_LEN];
        mgos_bt_uuid128_from_bytes(type->bytes, false /* reverse */, &uuid);
        mgos_bt_uuid_to_str(&uuid, buf);
        return std::string(buf);
    }

    class BLEPeripheralManagerImpl {
    public:
        BLEPeripheralManagerImpl(HAPPlatformBLEPeripheralManagerRef bpm)
            : bpm_(bpm) {
        }
        BLEPeripheralManagerImpl(const BLEPeripheralManagerImpl& other) = delete;

        void SetDelegate(const HAPPlatformBLEPeripheralManagerDelegate* delegate);

        void RemoveAllServices();

        HAPError AddService(const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type, bool isPrimary);

        HAPError AddCharacteristic(
                const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
                HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
                const void* _Nullable constBytes,
                size_t constNumBytes,
                HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull valueHandle,
                HAPPlatformBLEPeripheralManagerAttributeHandle* _Nullable cccDescriptorHandle);

        HAPError AddDescriptor(
                const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
                HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
                const void* _Nullable constBytes,
                size_t constNumBytes,
                HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull descriptorHandle);

        void PublishServices();

        void CancelCentralConnection(HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle);

    private:
        class Char {
        public:
            Char(const HAPPlatformBLEPeripheralManagerUUID* type,
                 HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
                 const void* _Nullable constBytes,
                 size_t constNumBytes,
                 uint16_t handle);
            Char(const HAPPlatformBLEPeripheralManagerUUID* type,
                 HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
                 const void* _Nullable constBytes,
                 size_t constNumBytes,
                 uint16_t handle);

            const struct mgos_bt_uuid* uuid() const {
                return &uuid_;
            }

            struct mgos_bt_gatts_char_def* def() {
                def_.uuid_bin = uuid_;
                return &def_;
            }

            uint16_t handle() const {
                return handle_;
            }

        private:
            struct mgos_bt_gatts_char_def def_ = {};
            struct mgos_bt_uuid uuid_;
            uint16_t handle_;
        };

        class Service {
        public:
            Service(const HAPPlatformBLEPeripheralManagerUUID* type)
                : uuid_str_(HAPPlatformBLEPeripheralManagerUUIDToStr(type)) {
                uuid_str_.shrink_to_fit();
            }

            const std::string& uuid_str() {
                return uuid_str_;
            }

            void AddCharacteristic(const Char& ch) {
                chars_.push_back(ch);
            }

            void Publish(mgos_bt_gatts_ev_handler_t handler, void* handler_arg) {
                std::vector<struct mgos_bt_gatts_char_def> defs;
                for (auto& ch : chars_) {
                    defs.push_back(*ch.def());
                }
                defs.push_back({});
                bool res = mgos_bt_gatts_register_service(
                        uuid_str_.c_str(), MGOS_BT_GATT_SEC_LEVEL_NONE, defs.data(), handler, handler_arg);
                LOG(LL_INFO, ("RES %d", res));
            }

            uint16_t GetHandleByUUID(const struct mgos_bt_uuid* uuid) const {
                for (const auto& ch : chars_) {
                    if (mgos_bt_uuid_cmp(ch.uuid(), uuid) == 0) {
                        return ch.handle();
                    }
                }
                return 0;
            }

        private:
            std::string uuid_str_;
            std::vector<Char> chars_;
        };

        static enum mgos_bt_gatt_status ConnEvHandlerCB(
                struct mgos_bt_gatts_conn* c,
                enum mgos_bt_gatts_ev ev,
                void* ev_arg,
                void* handler_arg);

        enum mgos_bt_gatt_status ConnEvHandler(struct mgos_bt_gatts_conn* c, enum mgos_bt_gatts_ev ev, void* ev_arg);

        uint16_t GetHandleByUUID(const struct mgos_bt_uuid* uuid);

        HAPPlatformBLEPeripheralManagerRef bpm_;

        HAPPlatformBLEPeripheralManagerDelegate delegate_ = {};

        std::vector<Service> services_;

        std::multimap<uint16_t, struct mgos_bt_gatts_conn*> conns_;

        // Handles are assigned only after service registration but we have to return them immediately
        // so we have to assign our own and perform translation.
        uint16_t next_handle_ = 1;
    };

    BLEPeripheralManagerImpl::Char::Char(
            const HAPPlatformBLEPeripheralManagerUUID* type,
            HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
            const void* _Nullable constBytes,
            size_t constNumBytes,
            uint16_t handle)
        : handle_(handle) {
        mgos_bt_uuid128_from_bytes(type->bytes, false /* reverse */, &uuid_);
        def_.is_uuid_bin = true;
        def_.prop = MGOS_BT_GATT_PROP_RWNI(properties.read, properties.write, properties.notify, properties.indicate);
        def_.handler_arg = nullptr;
        switch (constNumBytes) {
            case 0:
                break;
            case 1:
                def_.handler = mgos_bt_gatts_read_1;
                memcpy(&def_.handler_arg, constBytes, 1);
                break;
            case 2:
                def_.handler = mgos_bt_gatts_read_2;
                memcpy(&def_.handler_arg, constBytes, 2);
                break;
            case 4:
                def_.handler = mgos_bt_gatts_read_4;
                memcpy(&def_.handler_arg, constBytes, 4);
                break;
            default:
                assert(0);
        }
    }
    BLEPeripheralManagerImpl::Char::Char(
            const HAPPlatformBLEPeripheralManagerUUID* type,
            HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
            const void* _Nullable constBytes,
            size_t constNumBytes,
            uint16_t handle)
        : Char(type,
               HAPPlatformBLEPeripheralManagerCharacteristicProperties {
                       .read = properties.read,
                       .writeWithoutResponse = false,
                       .write = properties.write,
                       .notify = false,
                       .indicate = false,
               },
               constBytes,
               constNumBytes,
               handle) {
        def_.is_desc = true;
    }

    void BLEPeripheralManagerImpl::SetDelegate(const HAPPlatformBLEPeripheralManagerDelegate* delegate) {
        LOG(LL_INFO, ("BPM %p delegate=%p", this, delegate));
        if (delegate != nullptr) {
            delegate_ = *delegate;
        } else {
            delegate_ = HAPPlatformBLEPeripheralManagerDelegate {};
        }
    }

    void BLEPeripheralManagerImpl::RemoveAllServices() {
        LOG(LL_INFO, ("BPM %p remove all", this));
        next_handle_ = 1;
    }

    HAPError BLEPeripheralManagerImpl::AddService(
            const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
            bool isPrimary) {
        Service svc(type);
        LOG(LL_INFO, ("BPM %p addsvc %s pri %d", this, svc.uuid_str().c_str(), isPrimary));
        services_.push_back(svc);
        return kHAPError_None;
    }

    HAPError BLEPeripheralManagerImpl::AddCharacteristic(
            const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
            HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
            const void* _Nullable constBytes,
            size_t constNumBytes,
            HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull valueHandle,
            HAPPlatformBLEPeripheralManagerAttributeHandle* _Nullable cccDescriptorHandle) {
        Char ch(type, properties, constBytes, constNumBytes, next_handle_);
        const auto* def = ch.def();
        char buf[MGOS_BT_UUID_STR_LEN];
        mgos_bt_uuid_to_str(ch.uuid(), buf);
        LOG(LL_INFO,
            ("BPM %p add_char %s props %02x cb %p/%d h %u",
             this,
             buf,
             def->prop,
             constBytes,
             (int) constNumBytes,
             ch.handle()));
        services_.back().AddCharacteristic(ch);
        *valueHandle = next_handle_++;
        if (cccDescriptorHandle != nullptr) {
            if (properties.notify || properties.indicate) {
                *cccDescriptorHandle = next_handle_++;
            } else {
                *cccDescriptorHandle = 0;
            }
        }
        return kHAPError_None;
    }

    HAPError BLEPeripheralManagerImpl::AddDescriptor(
            const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
            HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
            const void* _Nullable constBytes,
            size_t constNumBytes,
            HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull descriptorHandle) {
        Char ch(type, properties, constBytes, constNumBytes, next_handle_);
        auto* def = ch.def();
        char buf[MGOS_BT_UUID_STR_LEN];
        mgos_bt_uuid_to_str(ch.uuid(), buf);
        LOG(LL_INFO,
            ("BPM %p add_desc %s props %02x cb %p/%d %02x%02x h %u",
             this,
             buf,
             def->prop,
             constBytes,
             (int) constNumBytes,
             ((uint8_t*) constBytes)[1],
             ((uint8_t*) constBytes)[0],
             next_handle_));
        services_.back().AddCharacteristic(ch);
        *descriptorHandle = next_handle_++;
        return kHAPError_None;
    }

    void BLEPeripheralManagerImpl::PublishServices() {
        LOG(LL_INFO, ("BPM %p publish", this));
        for (auto& svc : services_) {
            svc.Publish(&BLEPeripheralManagerImpl::ConnEvHandlerCB, this);
        }
    }

    void BLEPeripheralManagerImpl::CancelCentralConnection(
            HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle) {
        LOG(LL_INFO, ("BPM %p cancel_conn %u", this, connectionHandle));
        // Delay until next event loop, as it may be invoked while handler is running.
        mgos::InvokeCB([this, connectionHandle]() {
            const auto& conns = conns_.equal_range(connectionHandle);
            for (auto it = conns.first; it != conns.second; it++) {
                mgos_bt_gatts_disconnect(it->second);
            }
        });
    }

    // static
    enum mgos_bt_gatt_status BLEPeripheralManagerImpl::ConnEvHandlerCB(
            struct mgos_bt_gatts_conn* c,
            enum mgos_bt_gatts_ev ev,
            void* ev_arg,
            void* handler_arg) {
        return static_cast<BLEPeripheralManagerImpl*>(handler_arg)->ConnEvHandler(c, ev, ev_arg);
    }

    uint16_t BLEPeripheralManagerImpl::GetHandleByUUID(const struct mgos_bt_uuid* uuid) {
        for (const auto& svc : services_) {
            uint16_t handle = svc.GetHandleByUUID(uuid);
            if (handle != 0)
                return handle;
        }
        return 0;
    }

    enum mgos_bt_gatt_status BLEPeripheralManagerImpl::ConnEvHandler(
            struct mgos_bt_gatts_conn* c,
            enum mgos_bt_gatts_ev ev,
            void* ev_arg) {
        if (delegate_.handleConnectedCentral == nullptr) {
            return MGOS_BT_GATT_STATUS_UNLIKELY_ERROR;
        }
        uint16_t conn_id = c->gc.conn_id;
        switch (ev) {
            case MGOS_BT_GATTS_EV_CONNECT: {
                LOG(LL_INFO, ("BPM %p c %p/%u conn", this, c, conn_id));
                conns_.insert(std::make_pair(conn_id, c));
                if (conns_.count(conn_id) == 1) {
                    delegate_.handleConnectedCentral(bpm_, conn_id, delegate_.context);
                }
                return MGOS_BT_GATT_STATUS_OK;
            }
            case MGOS_BT_GATTS_EV_DISCONNECT: {
                LOG(LL_INFO, ("BPM %p c %p/%u disconn", this, c, conn_id));
                if (conns_.erase(conn_id) > 0) {
                    delegate_.handleDisconnectedCentral(bpm_, conn_id, delegate_.context);
                }
                break;
            }
            case MGOS_BT_GATTS_EV_READ: {
                struct mgos_bt_gatts_read_arg* arg = (struct mgos_bt_gatts_read_arg*) ev_arg;
                if (arg->offset != 0) {
                    return MGOS_BT_GATT_STATUS_INVALID_OFFSET;
                }
                uint16_t handle = GetHandleByUUID(&arg->uuid);
                if (handle == 0) {
                    return MGOS_BT_GATT_STATUS_INVALID_HANDLE;
                }
                std::unique_ptr<char> buf(new char[kHAPPlatformBLEPeripheralManager_MaxAttributeBytes]);
                if (buf == nullptr) {
                    return MGOS_BT_GATT_STATUS_INSUF_RESOURCES;
                }
                size_t num_bytes = 0;
                HAPError res = delegate_.handleReadRequest(
                        bpm_,
                        conn_id,
                        handle,
                        buf.get(),
                        kHAPPlatformBLEPeripheralManager_MaxAttributeBytes,
                        &num_bytes,
                        delegate_.context);
                LOG(LL_INFO,
                    ("BPM %p c %p/%u read h %u -> %d %d", this, c, conn_id, handle, res, (int) num_bytes));
                switch (res) {
                    case kHAPError_None:
                        mgos_bt_gatts_send_resp_data(c, arg, mg_mk_str_n(buf.get(), num_bytes));
                        return MGOS_BT_GATT_STATUS_OK;
                    case kHAPError_OutOfResources:
                        return MGOS_BT_GATT_STATUS_INSUF_RESOURCES;
                    case kHAPError_InvalidState:
                        return MGOS_BT_GATT_STATUS_READ_NOT_PERMITTED;
                    default:
                        return MGOS_BT_GATT_STATUS_UNLIKELY_ERROR;
                }
                break;
            }
            case MGOS_BT_GATTS_EV_WRITE: {
                struct mgos_bt_gatts_write_arg *arg = (struct mgos_bt_gatts_write_arg*) ev_arg;
                if (arg->offset != 0) {
                    return MGOS_BT_GATT_STATUS_INVALID_OFFSET;
                }
                if (arg->data.len > kHAPPlatformBLEPeripheralManager_MaxAttributeBytes) {
                    return MGOS_BT_GATT_STATUS_INVALID_ATT_VAL_LENGTH;
                }
                uint16_t handle = GetHandleByUUID(&arg->uuid);
                if (handle == 0) {
                    return MGOS_BT_GATT_STATUS_INVALID_HANDLE;
                }
                HAPError res = delegate_.handleWriteRequest(
                        bpm_,
                        conn_id,
                        handle,
                        (void *) arg->data.p,
                        arg->data.len,
                        delegate_.context);
                LOG(LL_INFO,
                    ("BPM %p c %p/%u write h %u len %u -> %d", this, c, conn_id, handle, (int) arg->data.len, res));
                switch (res) {
                    case kHAPError_None:
                        return MGOS_BT_GATT_STATUS_OK;
                    case kHAPError_InvalidState:
                        return MGOS_BT_GATT_STATUS_READ_NOT_PERMITTED;
                    case kHAPError_OutOfResources:
                        return MGOS_BT_GATT_STATUS_INSUF_RESOURCES;
                    default:
                        return MGOS_BT_GATT_STATUS_UNLIKELY_ERROR;
                }
                break;
            }
            case MGOS_BT_GATTS_EV_NOTIFY_MODE: {
                struct mgos_bt_gatts_notify_mode_arg *arg = (struct mgos_bt_gatts_notify_mode_arg*) ev_arg;
                uint16_t handle = GetHandleByUUID(&arg->uuid);
                LOG(LL_INFO,
                    ("BPM %p c %p/%u nm h %u m %u", this, c, conn_id, handle, arg->mode));
                return MGOS_BT_GATT_STATUS_OK;
            }
            case MGOS_BT_GATTS_EV_IND_CONFIRM: {
                return MGOS_BT_GATT_STATUS_OK;
            }
        }
        return MGOS_BT_GATT_STATUS_REQUEST_NOT_SUPPORTED;
    }

} // namespace hap
} // namespace mgos

// Implementation of the PAL API.
extern "C" {

static mgos::hap::BLEPeripheralManagerImpl* BPMI(HAPPlatformBLEPeripheralManagerRef bpm) {
    return static_cast<mgos::hap::BLEPeripheralManagerImpl*>(
            static_cast<struct HAPPlatformBLEPeripheralManager*>(bpm)->impl);
}

void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerOptions* _Nonnull options) {
    struct HAPPlatformBLEPeripheralManager* bpm = (struct HAPPlatformBLEPeripheralManager*) blePeripheralManager;
    bpm->impl = new mgos::hap::BLEPeripheralManagerImpl(blePeripheralManager);
    LOG(LL_INFO, ("BPM %p create", bpm->impl));
}

void HAPPlatformBLEPeripheralManagerSetDelegate(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDelegate* _Nullable delegate) {
    BPMI(blePeripheralManager)->SetDelegate(delegate);
}

void HAPPlatformBLEPeripheralManagerSetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerDeviceAddress* _Nonnull deviceAddress) {
    // Not required.
}

void HAPPlatformBLEPeripheralManagerSetDeviceName(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const char* _Nonnull deviceName) {
    struct HAPPlatformBLEPeripheralManager* bpm = (struct HAPPlatformBLEPeripheralManager*) blePeripheralManager;
    LOG(LL_INFO, ("BPM %p name=%s", bpm, deviceName));
}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    BPMI(blePeripheralManager)->RemoveAllServices();
}

HAPError HAPPlatformBLEPeripheralManagerAddService(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        bool isPrimary) {
    return BPMI(blePeripheralManager)->AddService(type, isPrimary);
}

HAPError HAPPlatformBLEPeripheralManagerAddCharacteristic(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull valueHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nullable cccDescriptorHandle) {
    return BPMI(blePeripheralManager)
            ->AddCharacteristic(type, properties, constBytes, constNumBytes, valueHandle, cccDescriptorHandle);
}

HAPError HAPPlatformBLEPeripheralManagerAddDescriptor(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerUUID* _Nonnull type,
        HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        HAPPlatformBLEPeripheralManagerAttributeHandle* _Nonnull descriptorHandle) {
    return BPMI(blePeripheralManager)->AddDescriptor(type, properties, constBytes, constNumBytes, descriptorHandle);
}

void HAPPlatformBLEPeripheralManagerPublishServices(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    BPMI(blePeripheralManager)->PublishServices();
}

void HAPPlatformBLEPeripheralManagerStartAdvertising(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPBLEAdvertisingInterval advertisingInterval,
        const void* _Nonnull advertisingBytes,
        size_t numAdvertisingBytes,
        const void* _Nullable scanResponseBytes,
        size_t numScanResponseBytes) {
    struct HAPPlatformBLEPeripheralManager* bpm = (struct HAPPlatformBLEPeripheralManager*) blePeripheralManager;
    LOG(LL_INFO,
        ("BPM %p start_adv intvl %u ab %d sr %d",
         bpm,
         advertisingInterval,
         (int) numAdvertisingBytes,
         (int) numScanResponseBytes));
    mgos_bt_gap_set_adv_data(mg_mk_str_n((const char*) advertisingBytes, numAdvertisingBytes));
    mgos_bt_gap_set_scan_rsp_data(mg_mk_str_n((const char*) scanResponseBytes, numScanResponseBytes));
    mgos_bt_gap_set_adv_enable(true);
}

void HAPPlatformBLEPeripheralManagerStopAdvertising(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    struct HAPPlatformBLEPeripheralManager* bpm = (struct HAPPlatformBLEPeripheralManager*) blePeripheralManager;
    LOG(LL_INFO, ("BPM %p stop_adv", bpm));
    mgos_bt_gap_set_adv_enable(false);
}

bool HAPPlatformBLEPeripheralManagerIsAdvertising(HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager) {
    struct HAPPlatformBLEPeripheralManager* bpm = (struct HAPPlatformBLEPeripheralManager*) blePeripheralManager;
    bool res = mgos_bt_gap_get_adv_enable();
    LOG(LL_INFO, ("BPM %p is_adv? %d", bpm, res));
    return res;
}

#if 0 // Only used in tests, not required.
void HAPPlatformBLEPeripheralManagerGetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerDeviceAddress* _Nonnull deviceAddress) {
    (void) blePeripheralManager;
    (void) deviceAddress;
}

HAPError HAPPlatformBLEPeripheralManagerGetAdvertisingData(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        void* _Nonnull advertisingBytes,
        size_t maxAdvertisingBytes,
        size_t* _Nonnull numAdvertisingBytes,
        void* _Nonnull scanResponseBytes,
        size_t maxScanResponseBytes,
        size_t* _Nonnull numScanResponseBytes) {
    (void) blePeripheralManager;
    (void) advertisingBytes;
    (void) maxAdvertisingBytes;
    (void) numAdvertisingBytes;
    (void) scanResponseBytes;
    (void) maxScanResponseBytes;
    (void) numScanResponseBytes;
    return kHAPError_Unknown;
}
#endif

void HAPPlatformBLEPeripheralManagerCancelCentralConnection(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle) {
    BPMI(blePeripheralManager)->CancelCentralConnection(connectionHandle);
}

HAPError HAPPlatformBLEPeripheralManagerSendHandleValueIndication(
        HAPPlatformBLEPeripheralManagerRef _Nonnull blePeripheralManager,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle,
        HAPPlatformBLEPeripheralManagerAttributeHandle valueHandle,
        const void* _Nullable bytes,
        size_t numBytes) {
    (void) blePeripheralManager;
    (void) connectionHandle;
    (void) valueHandle;
    (void) bytes;
    (void) numBytes;
    return kHAPError_Unknown;
}

} // extern "C"
#endif // MGOS_HAVE_BT_COMMON
