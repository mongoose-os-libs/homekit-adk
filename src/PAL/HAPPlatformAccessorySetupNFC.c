// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include "mgos.h"
#include "mgos_hap.h"

#include "HAPPlatform.h"

#ifndef HAVE_NFC
#define HAVE_NFC 0
#endif

#if HAVE_NFC
#include <errno.h>
#include <unistd.h>
#endif

void HAPPlatformAccessorySetupNFCUpdateSetupPayload(
        HAPPlatformAccessorySetupNFCRef setupNFC,
        const HAPSetupPayload* setupPayload,
        bool isPairable) {
    struct mgos_hap_nfc_update_setup_payload_arg arg = {
        .setupNFC = setupNFC,
        .setupPayload = setupPayload,
        .isPairable = isPairable,
    };
    mgos_event_trigger(MGOS_HAP_EV_NFC_UPDATE_SETUP_PAYLOAD, &arg);
}
