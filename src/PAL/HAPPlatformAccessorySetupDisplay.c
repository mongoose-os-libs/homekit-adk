// Copyright (c) 2015-2019 The HomeKit ADK Contributors
// Copyright (c) 2019 Deomid "rojer" Ryabkov
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

#include "mgos.h"
#include "mgos_hap.h"

#include "HAPPlatform.h"
#include "HAPPlatformAccessorySetupDisplay+Init.h"

void HAPPlatformAccessorySetupDisplayCreate(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    HAPPrecondition(HAVE_DISPLAY);
    HAPPrecondition(setupDisplay);

    HAPRawBufferZero(setupDisplay, sizeof *setupDisplay);
}

void HAPPlatformAccessorySetupDisplayUpdateSetupPayload(
        HAPPlatformAccessorySetupDisplayRef setupDisplay,
        const HAPSetupPayload* _Nullable setupPayload,
        const HAPSetupCode* _Nullable setupCode) {

    struct mgos_hap_display_update_setup_payload_arg arg = {
        .setupDisplay = setupDisplay,
        .setupPayload = setupPayload,
        .setupCode = setupCode,
    };

    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_UPDATE_SETUP_PAYLOAD, &arg);
}

void HAPPlatformAccessorySetupDisplayHandleStartPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    struct mgos_hap_display_start_pairing_arg arg = {
        .setupDisplay = setupDisplay,
    };
    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_START_PAIRING, &arg);
}

void HAPPlatformAccessorySetupDisplayHandleStopPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    struct mgos_hap_display_stop_pairing_arg arg = {
        .setupDisplay = setupDisplay,
    };
    mgos_event_trigger(MGOS_HAP_EV_DISPLAY_STOP_PAIRING, &arg);
}
