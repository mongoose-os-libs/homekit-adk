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
    HAPPrecondition(byteBuffer->data);
    HAPPrecondition(byteBuffer->position <= byteBuffer->limit);
    HAPPrecondition(byteBuffer->limit <= byteBuffer->capacity);

    HAPError err;

    va_list args;
    va_start(args, format);
    err = HAPStringWithFormatAndArguments(
            &byteBuffer->data[byteBuffer->position], byteBuffer->limit - byteBuffer->position, format, args);
    va_end(args);
    if (err) {
        HAPAssert(kHAPError_OutOfResources);
        return err;
    }
    byteBuffer->position += HAPStringGetNumBytes(&byteBuffer->data[byteBuffer->position]);
    return kHAPError_None;
}

HAPError HAPIPByteBufferSetCapacity(HAPIPByteBuffer* byteBuffer, size_t newCapacity) {
    char* newData = realloc(byteBuffer->data, newCapacity);
    HAPLogDebug(
            &logObject,
            "BUF %p: %s %u -> %u, %p -> %p",
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
    if (byteBuffer->limit == byteBuffer->capacity) {
        byteBuffer->limit = newCapacity;
    }
    byteBuffer->capacity = newCapacity;
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPIPByteBufferEnsureHeadroom(HAPIPByteBuffer* byteBuffer, size_t numBytes) {
    HAPPrecondition(byteBuffer);
    size_t newCapacity = byteBuffer->position + numBytes;
    if (newCapacity <= byteBuffer->capacity) {
        return kHAPError_None;
    }
    if (!byteBuffer->isDynamic) {
        return kHAPError_OutOfResources;
    }
    return HAPIPByteBufferSetCapacity(byteBuffer, newCapacity);
}

void HAPIPByteBufferTrim(HAPIPByteBuffer* byteBuffer) {
    HAPPrecondition(byteBuffer);
    HAPPrecondition(byteBuffer->limit <= byteBuffer->capacity);
    if (!byteBuffer->isDynamic || byteBuffer->limit == byteBuffer->capacity) {
        return;
    }
    HAPIPByteBufferSetCapacity(byteBuffer, byteBuffer->limit);
}
