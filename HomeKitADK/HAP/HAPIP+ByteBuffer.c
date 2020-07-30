// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include "HAP+Internal.h"

#include <stdlib.h>

static const HAPLogObject logObject = { .subsystem = kHAP_LogSubsystem, .category = "HAPIPByteBuffer" };

void HAPIPByteBufferClear(HAPIPByteBuffer* byteBuffer) {
    HAPPrecondition(byteBuffer);

    byteBuffer->position = 0;
    if (byteBuffer->isDynamic) {
        byteBuffer->limit = 0;
        HAPIPByteBufferTrim(byteBuffer);
    }
    byteBuffer->limit = byteBuffer->capacity;
}

void HAPIPByteBufferFlip(HAPIPByteBuffer* byteBuffer) {
    HAPPrecondition(byteBuffer);

    byteBuffer->limit = byteBuffer->position;
    byteBuffer->position = 0;
}

void HAPIPByteBufferShiftLeft(HAPIPByteBuffer* byteBuffer, size_t numBytes) {
    HAPPrecondition(byteBuffer);
    HAPPrecondition(byteBuffer->data);
    HAPPrecondition(numBytes <= byteBuffer->position);
    HAPPrecondition(byteBuffer->position <= byteBuffer->limit);
    HAPPrecondition(byteBuffer->limit <= byteBuffer->capacity);

    HAPRawBufferCopyBytes(byteBuffer->data, &byteBuffer->data[numBytes], byteBuffer->position - numBytes);
    byteBuffer->position -= numBytes;
    byteBuffer->limit -= numBytes;
    HAPIPByteBufferTrim(byteBuffer);
}

HAP_PRINTFLIKE(2, 3)
HAP_RESULT_USE_CHECK
HAPError HAPIPByteBufferAppendStringWithFormat(HAPIPByteBuffer* byteBuffer, const char* format, ...) {
    HAPPrecondition(byteBuffer);
    HAPPrecondition(byteBuffer->data || byteBuffer->isDynamic);
    HAPPrecondition(byteBuffer->position <= byteBuffer->limit);
    HAPPrecondition(byteBuffer->limit <= byteBuffer->capacity);

    HAPError err;
    size_t headroom = 64;
    do {
        va_list args;
        va_start(args, format);
        err = HAPIPByteBufferEnsureHeadroom(byteBuffer, headroom);
        if (err) {
            break;
        }
        err = HAPStringWithFormatAndArguments(
                &byteBuffer->data[byteBuffer->position], byteBuffer->limit - byteBuffer->position, format, args);
        va_end(args);
        if (!err) {
            break;
        }
        HAPAssert(kHAPError_OutOfResources);
        if (byteBuffer->isDynamic) {
            headroom += 64;
        }
    } while (true);
    if (!err) {
        byteBuffer->position += HAPStringGetNumBytes(&byteBuffer->data[byteBuffer->position]);
    }
    return err;
}

HAPError HAPIPByteBufferSetCapacity(HAPIPByteBuffer* byteBuffer, size_t newCapacity) {
    HAPPrecondition(byteBuffer);
    char* newData = realloc(byteBuffer->data, newCapacity);
    HAPLogDebug(
            &logObject,
            "%p: %s %u -> %u, %p -> %p",
            byteBuffer,
            (newCapacity > byteBuffer->capacity ? " grow " : "shrink"),
            (unsigned) byteBuffer->capacity,
            (unsigned) newCapacity,
            byteBuffer->data,
            newData);
    if (newData == NULL && newCapacity != 0) {
        return kHAPError_OutOfResources;
    }
    byteBuffer->data = newData;
    byteBuffer->capacity = newCapacity;
    return kHAPError_None;
}

HAPError HAPIPByteBufferEnsureHeadroom(HAPIPByteBuffer* byteBuffer, size_t numBytes) {
    HAPPrecondition(byteBuffer);
    size_t newCapacity = byteBuffer->position + numBytes;
    bool pullUpLimit = (byteBuffer->limit == byteBuffer->capacity);
    // Grow in chunks of 64 bytes.
    // > 1 is because of the inboundBuffer case that allocates + 1 byte.
    if (newCapacity % 64 > 1) {
        newCapacity = (newCapacity + 64) & ~((size_t) 63);
    }
    HAPError err = HAPIPByteBufferEnsureCapacity(byteBuffer, newCapacity);
    if (!err && pullUpLimit && newCapacity > byteBuffer->limit) {
        byteBuffer->limit = newCapacity;
    }
    return err;
}

HAPError HAPIPByteBufferEnsureCapacity(HAPIPByteBuffer* byteBuffer, size_t numBytes) {
    if (numBytes <= byteBuffer->capacity) {
        return kHAPError_None;
    }
    if (!byteBuffer->isDynamic) {
        return kHAPError_OutOfResources;
    }
    return HAPIPByteBufferSetCapacity(byteBuffer, numBytes);
}

void HAPIPByteBufferTrim(HAPIPByteBuffer* byteBuffer) {
    HAPPrecondition(byteBuffer);
    HAPPrecondition(byteBuffer->limit <= byteBuffer->capacity);
    if (!byteBuffer->isDynamic || byteBuffer->limit == byteBuffer->capacity) {
        return;
    }
    HAPIPByteBufferSetCapacity(byteBuffer, byteBuffer->limit);
}
