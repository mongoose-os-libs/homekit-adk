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

#include "HAPPlatformTimer.h"

#include "mgos.h"

struct hap_timer_ctx {
    mgos_timer_id timer_id;
    HAPPlatformTimerCallback cb;
    void* cb_arg;
};

static void hap_timer_cb(void* arg) {
    struct hap_timer_ctx* ctx = (struct hap_timer_ctx*) arg;
    ctx->cb((HAPPlatformTimerRef) ctx, ctx->cb_arg);
    free(ctx);
}

HAPError HAPPlatformTimerRegister(
        HAPPlatformTimerRef* timer,
        HAPTime deadline,
        HAPPlatformTimerCallback callback,
        void* _Nullable context) {
    struct hap_timer_ctx* ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return kHAPError_OutOfResources;
    }
    int64_t duration = ((int64_t) deadline - (int64_t) HAPPlatformClockGetCurrent());
    ctx->timer_id = mgos_set_timer((int) duration, 0, hap_timer_cb, ctx);
    if (ctx->timer_id == MGOS_INVALID_TIMER_ID) {
        return kHAPError_OutOfResources;
    }
    ctx->cb = callback;
    ctx->cb_arg = context;
    *timer = (HAPPlatformTimerRef) ctx;
    return kHAPError_None;
}

void HAPPlatformTimerDeregister(HAPPlatformTimerRef timer) {
    struct hap_timer_ctx* ctx = (struct hap_timer_ctx*) timer;
    mgos_clear_timer(ctx->timer_id);
    free(ctx);
}
