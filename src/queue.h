/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MAMA_BRIDGE_ZMQ_QUEUE_H__
#define MAMA_BRIDGE_ZMQ_QUEUE_H__

#include "zmqdefs.h"

struct zmqTransportMsg_;

#define     CHECK_QUEUE(IMPL)                                          \
   do {                                                                \
      if (IMPL == NULL)              return MAMA_STATUS_NULL_ARG;      \
      if (IMPL->mQueue == NULL)      return MAMA_STATUS_NULL_ARG;      \
   } while(0)

/* Timeout is in milliseconds */
#define     ZMQ_QUEUE_DISPATCH_TIMEOUT     500
#define     ZMQ_QUEUE_MAX_SIZE             WOMBAT_QUEUE_MAX_SIZE
#define     ZMQ_QUEUE_CHUNK_SIZE           WOMBAT_QUEUE_CHUNK_SIZE
#define     ZMQ_QUEUE_INITIAL_SIZE         WOMBAT_QUEUE_CHUNK_SIZE

#if defined(__cplusplus)
extern "C" {
#endif

mama_status zmqBridgeMamaQueue_enqueueMsg(queueBridge queue, mamaQueueEnqueueCB callback, struct zmqTransportMsg_ *msg);

#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_QUEUE_H__ */
