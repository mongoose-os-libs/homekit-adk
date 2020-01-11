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

#include "HAPPlatformTCPStreamManager.h"
#include "HAPPlatformTCPStreamManager+Init.h"

#include "mgos.h"
#include "mgos_mongoose.h"

#define HAP_F_CONN_ACCEPTED MG_F_USER_1
#define HAP_F_READ_PENDING MG_F_USER_2
#define HAP_F_WRITE_PENDING MG_F_USER_3

HAPNetworkPort HAPPlatformTCPStreamManagerGetListenerPort(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    return tcpStreamManager->port;
}

bool HAPPlatformTCPStreamManagerIsListenerOpen(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    return (tcpStreamManager->listener != NULL);
}

static void HAPMGListenerHandler(struct mg_connection* nc, int ev, void* ev_data, void* userdata) {
    HAPPlatformTCPStreamManagerRef tcpStreamManager = (HAPPlatformTCPStreamManagerRef) userdata;
    switch (ev) {
        case MG_EV_ACCEPT:
            tcpStreamManager->listenerCallback(tcpStreamManager, tcpStreamManager->listenerCallbackContext);
            break;
        case MG_EV_CLOSE:
            break;
    }
    (void) ev_data;
    (void) nc;
}

void HAPPlatformTCPStreamManagerOpenListener(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamListenerCallback callback,
        void* _Nullable context) {
    char buf[32];
    snprintf(buf, sizeof(buf), "tcp://:%u", tcpStreamManager->port);
    tcpStreamManager->listenerCallback = callback;
    tcpStreamManager->listenerCallbackContext = context;
    tcpStreamManager->listener = mg_bind(mgos_get_mgr(), buf, HAPMGListenerHandler, tcpStreamManager);
    if (tcpStreamManager->listener == NULL) {
        LOG(LL_ERROR, ("Failed to create listener on %s!", buf));
        return;
    }
}

void HAPPlatformTCPStreamManagerCloseListener(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    if (tcpStreamManager->listener == NULL)
        return;
    tcpStreamManager->listener->flags |= MG_F_CLOSE_IMMEDIATELY;
    tcpStreamManager->listener = NULL;
}

static void HAPMGConnHandler(struct mg_connection* nc, int ev, void* ev_data HAP_UNUSED, void* userdata) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) userdata;
    HAPPlatformTCPStreamEvent hapEvent = { 0 };
    switch (ev) {
        case MG_EV_POLL:
            // fallthrough
        case MG_EV_RECV:
            // fallthrough
        case MG_EV_SEND:
            // fallthrough
        case MG_EV_TIMER:
            if (ts->interests.hasBytesAvailable && nc->recv_mbuf.len > 0 && !(nc->flags & HAP_F_READ_PENDING)) {
                hapEvent.hasBytesAvailable = true;
            }
            if (ts->interests.hasSpaceAvailable && nc->send_mbuf.len == 0 && !(nc->flags & HAP_F_WRITE_PENDING)) {
                hapEvent.hasSpaceAvailable = true;
            }
            break;
        case MG_EV_CLOSE:
            ts->nc = NULL;
            if (ts->interests.hasBytesAvailable) {
                // Deliver EOF via read.
                hapEvent.hasBytesAvailable = true;
            } else if (ts->interests.hasBytesAvailable) {
                // Deliver EOF via write.
                hapEvent.hasSpaceAvailable = true;
            } else {
                // Shouldn't happen.
                LOG(LL_ERROR, ("Orphan stream: %p", ts));
                free(ts);
            }
            break;
    }
    if (hapEvent.hasBytesAvailable || hapEvent.hasSpaceAvailable) {
        if (hapEvent.hasBytesAvailable) {
            nc->flags |= HAP_F_READ_PENDING;
        }
        if (hapEvent.hasSpaceAvailable) {
            nc->flags |= HAP_F_WRITE_PENDING;
        }
        ts->callback(
                (HAPPlatformTCPStreamManagerRef) nc->listener->user_data,
                (HAPPlatformTCPStreamRef) ts,
                hapEvent,
                ts->context);
    }
}

HAPError HAPPlatformTCPStreamManagerAcceptTCPStream(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamRef* tcpStream) {
    struct mg_mgr* mgr = tcpStreamManager->listener->mgr;
    struct mg_connection* nc = NULL;
    for (nc = mg_next(mgr, NULL); nc != NULL; nc = mg_next(mgr, nc)) {
        if (nc->listener != tcpStreamManager->listener)
            continue;
        if (!(nc->flags & HAP_F_CONN_ACCEPTED)) {
            HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) calloc(1, sizeof(*ts));
            if (tcpStream == NULL) {
                return kHAPError_OutOfResources;
            }
            ts->nc = nc;
            nc->flags |= HAP_F_CONN_ACCEPTED;
            nc->handler = HAPMGConnHandler;
            nc->user_data = ts;
            *tcpStream = (HAPPlatformTCPStreamRef) ts;
            return kHAPError_None;
        }
    }
    return kHAPError_Unknown;
}

void HAPPlatformTCPStreamCloseOutput(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream HAP_UNUSED) {
    // Mongoose doesn't have the concept of half-closed connections.
}

void HAPPlatformTCPStreamClose(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) tcpStream;
    if (ts == NULL || ts->nc == NULL) {
        return;
    }
    ts->nc->flags |= MG_F_SEND_AND_CLOSE;
    // Do not deliver further events.
    HAPPlatformTCPStreamEvent zero = { 0 };
    ts->interests = zero;
}

void HAPPlatformTCPStreamUpdateInterests(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream,
        HAPPlatformTCPStreamEvent interests,
        HAPPlatformTCPStreamEventCallback _Nullable callback,
        void* _Nullable context) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) tcpStream;
    if (ts == NULL) {
        return;
    }
    ts->interests = interests;
    ts->callback = callback;
    ts->context = context;
}

HAPError HAPPlatformTCPStreamRead(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream,
        void* bytes,
        size_t maxBytes,
        size_t* numBytes) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) tcpStream;
    if (ts == NULL) {
        return kHAPError_Unknown;
    }
    struct mg_connection* nc = ts->nc;
    if (nc == NULL) { // EOF
        *numBytes = 0;
        return kHAPError_None;
    }
    nc->flags &= ~HAP_F_READ_PENDING;
    if (nc->recv_mbuf.len == 0) {
        *numBytes = 0;
        return kHAPError_Busy;
    }
    *numBytes = MIN(maxBytes, nc->recv_mbuf.len);
    memcpy(bytes, nc->recv_mbuf.buf, *numBytes);
    mbuf_remove(&nc->recv_mbuf, *numBytes);
    // Actively push data.
    if (nc->recv_mbuf.len > 0) {
        nc->ev_timer_time = mg_time() + 0.01;
    }
    return kHAPError_None;
}

HAPError HAPPlatformTCPStreamWrite(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream,
        const void* bytes,
        size_t maxBytes,
        size_t* numBytes) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) tcpStream;
    if (ts == NULL || ts->nc == NULL) {
        return kHAPError_Unknown;
    }
    struct mg_connection* nc = ts->nc;
    nc->flags &= ~HAP_F_WRITE_PENDING;
    if (nc->send_mbuf.len >= 1024) {
        *numBytes = 0;
        return kHAPError_Busy;
    }
    *numBytes = MIN(maxBytes, (1024 - nc->send_mbuf.len));
    mbuf_append(&nc->send_mbuf, bytes, *numBytes);
    return kHAPError_None;
}

void HAPPlatformTCPStreamManagerCreate(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        const HAPPlatformTCPStreamManagerOptions* options) {
    static HAPNetworkPort anyPort = 9000; // Kinda hacky, but hey...
    memset(tcpStreamManager, 0, sizeof(*tcpStreamManager));
    tcpStreamManager->maxTCPStreams = options->maxConcurrentTCPStreams;
    if (options->port != kHAPNetworkPort_Any) {
        tcpStreamManager->port = options->port;
    } else {
        tcpStreamManager->port = anyPort++;
    }
}

void HAPPlatformTCPStreamManagerRelease(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    (void) tcpStreamManager;
}
