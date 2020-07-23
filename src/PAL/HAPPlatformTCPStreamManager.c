/* vim: tabstop=4 expandtab shiftwidth=4
 *
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

#define HAP_F_CONN_PENDING MG_F_USER_1
#define HAP_F_CONN_ACCEPTED MG_F_USER_2
#define HAP_F_READ_PENDING MG_F_USER_3
#define HAP_F_WRITE_PENDING MG_F_USER_4

#define HAP_MAX_PENDING_CONNECTIONS 5
#define HAP_EVICT_MIN_IDLE_SECONDS 3

HAPNetworkPort HAPPlatformTCPStreamManagerGetListenerPort(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    return tcpStreamManager->actualPort;
}

bool HAPPlatformTCPStreamManagerIsListenerOpen(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    return (tcpStreamManager->listener != NULL);
}

static struct mg_connection* HAPMGListenerGetNextPendingConnection(HAPPlatformTCPStreamManagerRef tm) {
    struct mg_mgr* mgr = tm->listener->mgr;
    struct mg_connection *nc = NULL, *result = NULL;
    for (nc = mg_next(mgr, NULL); nc != NULL; nc = mg_next(mgr, nc)) {
        if (nc->listener != tm->listener) {
            continue;
        }
        if ((nc->flags & (MG_F_CLOSE_IMMEDIATELY | MG_F_SEND_AND_CLOSE)) != 0) {
            continue;
        }
        if ((nc->flags & HAP_F_CONN_PENDING) != 0) {
            result = nc;
        }
    }
    return result;
}

static void HAPMGListenerEvictOldestConnection(HAPPlatformTCPStreamManagerRef tm) {
    struct mg_mgr* mgr = tm->listener->mgr;
    struct mg_connection *nc = NULL, *oldest = NULL;
    HAPPlatformTCPStream* tsOldest = NULL;
    for (nc = mg_next(mgr, NULL); nc != NULL; nc = mg_next(mgr, nc)) {
        if (nc->listener != tm->listener) {
            continue;
        }
        if (!(nc->flags & HAP_F_CONN_ACCEPTED)) {
            continue;
        }
        if (oldest == NULL) {
            oldest = nc;
            tsOldest = (HAPPlatformTCPStream*) nc->user_data;
        } else {
            HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) nc->user_data;
            if (ts->lastRead < tsOldest->lastRead) {
                oldest = nc;
                tsOldest = ts;
            }
        }
    }
    if (oldest == NULL)
        return;
    if ((oldest->flags & MG_F_SEND_AND_CLOSE) != 0) {
        // Already closing.
        return;
    }
    // Wait until connection is inactive for at least 5 seconds.
    if (mgos_uptime_micros() - tsOldest->lastRead < HAP_EVICT_MIN_IDLE_SECONDS * 1000000) {
        return;
    }
    char addr[32];
    mg_sock_addr_to_str(&oldest->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    LOG(LL_WARN, ("%p %s Evicting HAP connection", oldest, addr));
    oldest->flags |= MG_F_SEND_AND_CLOSE;
}

static void HAPMGListenerConnectionClosed(HAPPlatformTCPStreamManagerRef tm, struct mg_connection* nc) {
    char addr[32];
    if (nc->listener != tm->listener)
        return;
    if ((nc->flags & HAP_F_CONN_PENDING) != 0) {
        tm->numPendingTCPStreams--;
    }
    mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    LOG(LL_INFO,
        ("%p %s HAP connection closed, ns %u/%u/%u",
         nc,
         addr,
         (unsigned) tm->numPendingTCPStreams,
         (unsigned) tm->numActiveTCPStreams,
         (unsigned) tm->maxNumTCPStreams));
}

static void HAPMGListenerHandler(struct mg_connection* nc, int ev, void* ev_data, void* userdata) {
    char addr[32];
    mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    HAPPlatformTCPStreamManagerRef tm = (HAPPlatformTCPStreamManagerRef) userdata;
    switch (ev) {
        case MG_EV_ACCEPT: {
            LOG(LL_INFO,
                ("%p %s Incoming HAP connection, ns %u/%u/%u",
                 nc,
                 addr,
                 (unsigned) tm->numPendingTCPStreams,
                 (unsigned) tm->numActiveTCPStreams,
                 (unsigned) tm->maxNumTCPStreams));
            if (tm->numPendingTCPStreams >= HAP_MAX_PENDING_CONNECTIONS) {
                LOG(LL_ERROR, ("%p %s Too many pending connections, dropping", nc, addr));
                nc->flags |= MG_F_CLOSE_IMMEDIATELY;
                break;
            }
            nc->recv_mbuf_limit = kHAPIPAccessoryServerMaxIOSize;
            nc->flags |= HAP_F_CONN_PENDING;
            tm->numPendingTCPStreams++;
            if (tm->numActiveTCPStreams < tm->maxNumTCPStreams) {
                tm->listenerCallback(tm, tm->listenerCallbackContext);
            } else {
                HAPMGListenerEvictOldestConnection(tm);
            }
            break;
        }
        case MG_EV_POLL:
            // fallthrough
        case MG_EV_TIMER: {
            if (tm->numPendingTCPStreams == 0)
                break;
            if (tm->numActiveTCPStreams < tm->maxNumTCPStreams) {
                // We need to give session GC a chance to run.
                if (mgos_uptime_micros() - tm->lastCloseTS > 50000) {
                    tm->listenerCallback(tm, tm->listenerCallbackContext);
                }
            } else {
                HAPMGListenerEvictOldestConnection(tm);
            }
            break;
        }
        case MG_EV_CLOSE: {
            HAPMGListenerConnectionClosed(tm, nc);
            break;
        }
    }
    (void) ev_data;
    (void) nc;
}

void HAPPlatformTCPStreamManagerOpenListener(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamListenerCallback callback,
        void* _Nullable context) {
    char buf[32];
    HAPNetworkPort port = tcpStreamManager->port;
    if (port == kHAPNetworkPort_Any) {
        static HAPNetworkPort anyPort = 9000; // Kinda hacky, but hey...
        port = anyPort++;
    }
    snprintf(buf, sizeof(buf), "tcp://:%u", port);
    tcpStreamManager->listenerCallback = callback;
    tcpStreamManager->listenerCallbackContext = context;
    tcpStreamManager->listener = mg_bind(mgos_get_mgr(), buf, HAPMGListenerHandler, tcpStreamManager);
    if (tcpStreamManager->listener == NULL) {
        LOG(LL_ERROR, ("Failed to create listener on %s!", buf));
        return;
    }
    LOG(LL_INFO, ("Listening on %d", port));
    tcpStreamManager->actualPort = port;
}

void HAPPlatformTCPStreamManagerCloseListener(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    if (tcpStreamManager->listener == NULL)
        return;
    tcpStreamManager->listener->flags |= MG_F_CLOSE_IMMEDIATELY;
    tcpStreamManager->listener = NULL;
}

static void HAPMGConnHandler(struct mg_connection* nc, int ev, void* ev_data HAP_UNUSED, void* userdata) {
    HAPPlatformTCPStreamManagerRef tm = (HAPPlatformTCPStreamManagerRef) nc->listener->user_data;
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) userdata;
    HAPPlatformTCPStreamEvent hapEvent = { 0 };
    if (ev == MG_EV_CLOSE) {
        HAPMGListenerConnectionClosed(tm, nc);
    }
    if (ts == NULL) {
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        return;
    }
    switch (ev) {
        case MG_EV_RECV:
            // fallthrough
        case MG_EV_POLL:
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
        case MG_EV_CLOSE: {
            ts->nc = NULL;
            if (ts->interests.hasBytesAvailable) {
                // Deliver EOF via read.
                hapEvent.hasBytesAvailable = true;
            } else if (ts->interests.hasBytesAvailable) {
                // Deliver EOF via write.
                hapEvent.hasSpaceAvailable = true;
            } else {
                // Release the stream context.
                free(ts);
            }
            break;
        }
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
        HAPPlatformTCPStreamManagerRef tm,
        HAPPlatformTCPStreamRef* tcpStream) {
    struct mg_connection* nc = HAPMGListenerGetNextPendingConnection(tm);
    if (nc == NULL) {
        return kHAPError_Unknown;
    }
    char addr[32];
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) calloc(1, sizeof(*ts));
    if (tcpStream == NULL) {
        return kHAPError_OutOfResources;
    }
    ts->nc = nc;
    nc->flags &= ~HAP_F_CONN_PENDING;
    nc->flags |= HAP_F_CONN_ACCEPTED;
    tm->numPendingTCPStreams--;
    tm->numActiveTCPStreams++;
    nc->handler = HAPMGConnHandler;
    nc->user_data = ts;
    *tcpStream = (HAPPlatformTCPStreamRef) ts;
    mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    LOG(LL_INFO,
        ("%p %s Accepted HAP connection, ns %u/%u/%u ts %p",
         nc,
         addr,
         (unsigned) tm->numPendingTCPStreams,
         (unsigned) tm->numActiveTCPStreams,
         (unsigned) tm->maxNumTCPStreams,
         ts));
    ts->lastRead = mgos_uptime_micros();
    return kHAPError_None;
}

void HAPPlatformTCPStreamCloseOutput(
        HAPPlatformTCPStreamManagerRef tcpStreamManager HAP_UNUSED,
        HAPPlatformTCPStreamRef tcpStream HAP_UNUSED) {
    // Mongoose doesn't have the concept of half-closed connections.
}

void HAPPlatformTCPStreamClose(HAPPlatformTCPStreamManagerRef tm, HAPPlatformTCPStreamRef tcpStream) {
    HAPPlatformTCPStream* ts = (HAPPlatformTCPStream*) tcpStream;
    tm->numActiveTCPStreams--;
    tm->lastCloseTS = mgos_uptime_micros();
    tm->listener->ev_timer_time = mg_time() + 0.06;
    LOG(LL_INFO,
        ("HAPPlatformTCPStreamClose ts %p nc %p %u/%u/%u",
         ts,
         (ts ? ts->nc : NULL),
         (unsigned) tm->numPendingTCPStreams,
         (unsigned) tm->numActiveTCPStreams,
         (unsigned) tm->maxNumTCPStreams));
    if (ts == NULL) {
        return;
    }
    if (ts->nc != NULL) {
        ts->nc->flags |= MG_F_SEND_AND_CLOSE;
        ts->nc->user_data = NULL;
        ts->nc = NULL;
    }
    free(ts);
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
    ts->lastRead = mgos_uptime_micros();
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
    memset(tcpStreamManager, 0, sizeof(*tcpStreamManager));
    tcpStreamManager->maxNumTCPStreams = options->maxConcurrentTCPStreams;
    tcpStreamManager->port = options->port;
}

void HAPPlatformTCPStreamManagerRelease(HAPPlatformTCPStreamManagerRef tcpStreamManager) {
    (void) tcpStreamManager;
}

HAPError HAPPlatformTCPStreamManagerGetStats(
        HAPPlatformTCPStreamManagerRef tcpStreamManager,
        HAPPlatformTCPStreamManagerStats* stats) {
    stats->maxNumTCPStreams = tcpStreamManager->maxNumTCPStreams;
    stats->numPendingTCPStreams = tcpStreamManager->numPendingTCPStreams;
    stats->numActiveTCPStreams = tcpStreamManager->numActiveTCPStreams;
    return kHAPError_None;
}
