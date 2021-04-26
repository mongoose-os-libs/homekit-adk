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
#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "HAP.h"

namespace mgos {
namespace hap {

    class Service;

    class Characteristic {
    public:
        Characteristic(
                uint16_t iid,
                HAPCharacteristicFormat format,
                const HAPUUID* type,
                const char* debug_description = nullptr);
        virtual ~Characteristic();

        const Service* parent() const;
        void set_parent(const Service* parent);

        virtual const HAPCharacteristic* GetHAPCharacteristic() {
            return hap_charactristic();
        }

        const HAPCharacteristic* hap_charactristic();

        void RaiseEvent();

    protected:
        struct HAPCharacteristicWithInstance {
            union AllHAPCHaracteristicTypes {
                HAPDataCharacteristic data;
                HAPBoolCharacteristic bool_;
                HAPUInt8Characteristic uint8;
                HAPUInt16Characteristic uint16;
                HAPUInt32Characteristic uint32;
                HAPUInt64Characteristic uint64;
                HAPIntCharacteristic int_;
                HAPFloatCharacteristic float_;
                HAPStringCharacteristic string;
                HAPTLV8Characteristic tlv8;
            } char_;
            Characteristic* inst; // Pointer back to the instance.
        } hap_char_;

    private:
        const Service* parent_ = nullptr;

        Characteristic(const Characteristic& other) = delete;
    };

    class StringCharacteristic : public Characteristic {
    public:
        StringCharacteristic(
                uint16_t iid,
                const HAPUUID* type,
                uint16_t max_length,
                const std::string& initial_value,
                const char* debug_description = nullptr);
        virtual ~StringCharacteristic();

        const std::string& value() const;
        void set_value(const std::string& value);

    private:
        static HAPError HandleReadCB(
                HAPAccessoryServerRef* server,
                const HAPStringCharacteristicReadRequest* request,
                char* value,
                size_t maxValueBytes,
                void* context);

        std::string value_;
    };

    // Template class that can be used to create scalar-value characteristics.
    template <class ValType, class HAPBaseClass, class HAPReadRequestType, class HAPWriteRequestType>
    class ScalarCharacteristic : public Characteristic {
    public:
        typedef std::function<
                HAPError(HAPAccessoryServerRef* server, const HAPReadRequestType* request, ValType* value)>
                ReadHandler;
        typedef std::function<
                HAPError(HAPAccessoryServerRef* server, const HAPWriteRequestType* request, ValType value)>
                WriteHandler;

        ScalarCharacteristic(
                HAPCharacteristicFormat format,
                uint16_t iid,
                const HAPUUID* type,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : Characteristic(iid, format, type, debug_description)
            , read_handler_(read_handler)
            , write_handler_(write_handler) {
            HAPBaseClass* c = reinterpret_cast<HAPBaseClass*>(&hap_char_.char_);
            if (read_handler) {
                c->properties.readable = true;
                c->callbacks.handleRead = ScalarCharacteristic::HandleReadCB;
                c->properties.supportsEventNotification = supports_notification;
                c->properties.ble.supportsBroadcastNotification = true;
                c->properties.ble.supportsDisconnectedNotification = true;
            }
            if (write_handler) {
                c->properties.writable = true;
                c->callbacks.handleWrite = ScalarCharacteristic::HandleWriteCB;
            }
        }

        virtual ~ScalarCharacteristic() {
        }

    private:
        static HAPError HandleReadCB(
                HAPAccessoryServerRef* server,
                const HAPReadRequestType* request,
                ValType* value,
                void* context) {
            auto* hci = reinterpret_cast<const HAPCharacteristicWithInstance*>(request->characteristic);
            auto* c = static_cast<const ScalarCharacteristic*>(hci->inst);
            (void) context;
            return const_cast<ScalarCharacteristic*>(c)->read_handler_(server, request, value);
        }
        static HAPError HandleWriteCB(
                HAPAccessoryServerRef* server,
                const HAPWriteRequestType* request,
                ValType value,
                void* context) {
            auto* hci = reinterpret_cast<const HAPCharacteristicWithInstance*>(request->characteristic);
            auto* c = static_cast<const ScalarCharacteristic*>(hci->inst);
            (void) context;
            return const_cast<ScalarCharacteristic*>(c)->write_handler_(server, request, value);
        }

        const ReadHandler read_handler_;
        const WriteHandler write_handler_;
    };

    class BoolCharacteristic : public ScalarCharacteristic<
                                        bool,
                                        HAPBoolCharacteristic,
                                        HAPBoolCharacteristicReadRequest,
                                        HAPBoolCharacteristicWriteRequest> {
    public:
        BoolCharacteristic(
                uint16_t iid,
                const HAPUUID* type,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_Bool,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
        }
        virtual ~BoolCharacteristic() {
        }
    };

    class IntCharacteristic : public ScalarCharacteristic<
                                      int32_t,
                                      HAPIntCharacteristic,
                                      HAPIntCharacteristicReadRequest,
                                      HAPIntCharacteristicWriteRequest> {
    public:
        IntCharacteristic(
                uint16_t iid,
                const HAPUUID* type,
                int32_t min,
                int32_t max,
                int32_t step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_Int,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPIntCharacteristic* c = &hap_char_.char_.int_;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~IntCharacteristic() {
        }
    };

    class FloatCharacteristic : public ScalarCharacteristic<
                                        float,
                                        HAPFloatCharacteristic,
                                        HAPFloatCharacteristicReadRequest,
                                        HAPFloatCharacteristicWriteRequest> {
    public:
        FloatCharacteristic(
                uint16_t iid,
                const HAPUUID* type,
                float min,
                float max,
                float step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_UInt8,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPFloatCharacteristic* c = &hap_char_.char_.float_;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~FloatCharacteristic() {
        }
    };

    class UInt8Characteristic : public ScalarCharacteristic<
                                        uint8_t,
                                        HAPUInt8Characteristic,
                                        HAPUInt8CharacteristicReadRequest,
                                        HAPUInt8CharacteristicWriteRequest> {
    public:
        UInt8Characteristic(
                uint16_t iid,
                const HAPUUID* type,
                uint8_t min,
                uint8_t max,
                uint8_t step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_UInt8,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPUInt8Characteristic* c = &hap_char_.char_.uint8;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~UInt8Characteristic() {
        }
    };

    class UInt16Characteristic : public ScalarCharacteristic<
                                         uint16_t,
                                         HAPUInt16Characteristic,
                                         HAPUInt16CharacteristicReadRequest,
                                         HAPUInt16CharacteristicWriteRequest> {
    public:
        UInt16Characteristic(
                uint16_t iid,
                const HAPUUID* type,
                uint16_t min,
                uint16_t max,
                uint16_t step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_UInt16,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPUInt16Characteristic* c = &hap_char_.char_.uint16;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~UInt16Characteristic() {
        }
    };

    class UInt32Characteristic : public ScalarCharacteristic<
                                         uint32_t,
                                         HAPUInt32Characteristic,
                                         HAPUInt32CharacteristicReadRequest,
                                         HAPUInt32CharacteristicWriteRequest> {
    public:
        UInt32Characteristic(
                uint16_t iid,
                const HAPUUID* type,
                uint32_t min,
                uint32_t max,
                uint32_t step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_UInt32,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPUInt32Characteristic* c = &hap_char_.char_.uint32;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~UInt32Characteristic() {
        }
    };

    class UInt64Characteristic : public ScalarCharacteristic<
                                         uint64_t,
                                         HAPUInt64Characteristic,
                                         HAPUInt64CharacteristicReadRequest,
                                         HAPUInt64CharacteristicWriteRequest> {
    public:
        UInt64Characteristic(
                uint16_t iid,
                const HAPUUID* type,
                uint64_t min,
                uint64_t max,
                uint64_t step,
                ReadHandler read_handler,
                bool supports_notification,
                WriteHandler write_handler = nullptr,
                const char* debug_description = nullptr)
            : ScalarCharacteristic(
                      kHAPCharacteristicFormat_UInt64,
                      iid,
                      type,
                      read_handler,
                      supports_notification,
                      write_handler,
                      debug_description) {
            HAPUInt64Characteristic* c = &hap_char_.char_.uint64;
            c->constraints.minimumValue = min;
            c->constraints.maximumValue = max;
            c->constraints.stepValue = step;
        }
        virtual ~UInt64Characteristic() {
        }
    };

} // namespace hap
} // namespace mgos

#ifdef __clang__
#pragma clang diagnostic pop
#endif
