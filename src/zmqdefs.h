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

#ifndef MAMA_BRIDGE_ZMQ_ZMQDEFS_H__
#define MAMA_BRIDGE_ZMQ_ZMQDEFS_H__


/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

#include <wombat/wSemaphore.h>
#include <wombat/wtable.h>
#include <list.h>
#include <wombat/queue.h>
#include <wombat/mempool.h>
#include "endpointpool.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Maximum topic length */
#define     MAX_SUBJECT_LENGTH              256

/* Message types */
typedef enum zmqMsgType_
{
    ZMQ_MSG_PUB_SUB        =              0x00,
    ZMQ_MSG_INBOX_REQUEST,
    ZMQ_MSG_INBOX_RESPONSE,
    ZMQ_MSG_SUB_REQUEST
} zmqMsgType;


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

typedef struct zmqTransportBridge_
{
    int                     mIsValid;
    mamaTransport           mTransport;
    msgBridge               mMsg;
    void*                   mZmqContext;
    void*                   mZmqSocketSubscriber;
    void*                   mZmqSocketPublisher;
    void*                   mZmqSocketDispatcher;
    const char*             mIncomingAddress;
    const char*             mOutgoingAddress;
    const char*             mName;
    wthread_t               mOmzmqDispatchThread;
    int                     mIsDispatching;
    mama_status             mOmzmqDispatchStatus;
    endpointPool_t          mSubEndpoints;
    endpointPool_t          mPubEndpoints;
    memoryPool*             mMemoryNodePool;
} zmqTransportBridge;

typedef struct zmqSubscription_
{
    mamaMsgCallbacks        mMamaCallback;
    mamaSubscription        mMamaSubscription;
    mamaQueue               mMamaQueue;
    void*                   mQpidQueue;
    zmqTransportBridge*   mTransport;
    const char*             mSymbol;
    char*                   mSubjectKey;
    void*                   mClosure;
    int                     mIsNotMuted;
    int                     mIsValid;
    int                     mIsTportDisconnected;
    msgBridge               mMsg;
    const char*             mEndpointIdentifier;
} zmqSubscription;

typedef struct zmqTransportMsg_
{
    size_t                  mNodeSize;
    size_t                  mNodeCapacity;
    zmqSubscription*      mSubscription;
    uint8_t*                mNodeBuffer;
} zmqTransportMsg;

typedef struct zmqQueueBridge {
    mamaQueue               mParent;
    wombatQueue             mQueue;
    void*                   mEnqueueClosure;
    uint8_t                 mHighWaterFired;
    size_t                  mHighWatermark;
    size_t                  mLowWatermark;
    uint8_t                 mIsDispatching;
    mamaQueueEnqueueCB      mEnqueueCallback;
    void*                   mClosure;
    wthread_mutex_t         mDispatchLock;
} zmqQueueBridge;

#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_ZMQDEFS_H__ */
