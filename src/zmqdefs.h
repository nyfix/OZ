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


#define USE_XSUB



/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

#include <wombat/wSemaphore.h>
#include <wombat/wtable.h>
#include <list.h>
#include <wombat/queue.h>
#include <wombat/mempool.h>
#include "endpointpool.h"
#include "queue.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Maximum topic length */
#define     MAX_SUBJECT_LENGTH              256
#define     ZMQ_MAX_INCOMING_URIS           64
#define     ZMQ_MAX_OUTGOING_URIS           64

#define     ZMQ_MSG_PROPERTY_LEN     1024


typedef struct zmqBridgeMsgReplyHandle
{
    char                        mInboxName[ZMQ_MSG_PROPERTY_LEN];
    char                        mReplyTo[ZMQ_MSG_PROPERTY_LEN];
} zmqBridgeMsgReplyHandle;



/* Message types */
typedef enum zmqMsgType_
{
    ZMQ_MSG_PUB_SUB        =              0x00,
    ZMQ_MSG_INBOX_REQUEST,
    ZMQ_MSG_INBOX_RESPONSE,
    ZMQ_MSG_SUB_REQUEST
} zmqMsgType;

typedef enum zmqTransportType_
{
    ZMQ_TPORT_TYPE_TCP,
    ZMQ_TPORT_TYPE_IPC,
    ZMQ_TPORT_TYPE_PGM,
    ZMQ_TPORT_TYPE_EPGM,
    ZMQ_TPORT_TYPE_UNKNOWN = 99
} zmqTransportType;

typedef enum zmqTransportDirection_
{
    ZMQ_TPORT_DIRECTION_INCOMING,
    ZMQ_TPORT_DIRECTION_OUTGOING
} zmqTransportDirection;


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

    const char*             mIncomingAddress[ZMQ_MAX_INCOMING_URIS];
    const char*             mOutgoingAddress[ZMQ_MAX_OUTGOING_URIS];
    const char*             mName;
    wthread_t               mOmzmqDispatchThread;
    int                     mIsDispatching;
    mama_status             mOmzmqDispatchStatus;
    endpointPool_t          mSubEndpoints;
    endpointPool_t          mPubEndpoints;
    long int                mMemoryPoolSize;
    long int                mMemoryNodeSize;
} zmqTransportBridge;

typedef struct zmqSubscription_
{
    mamaMsgCallbacks        mMamaCallback;
    mamaSubscription        mMamaSubscription;
    mamaQueue               mMamaQueue;
    void*                   mZmqQueue;
    zmqTransportBridge*     mTransport;
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
    endpointPool_t          mSubEndpoints;
    char*                   mEndpointIdentifier;
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
    zmqQueueClosureCleanup  mClosureCleanupCb;
    void*                   mZmqContext;
    void*                   mZmqSocketWorker;
    void*                   mZmqSocketDealer;
} zmqQueueBridge;


#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_ZMQDEFS_H__ */
