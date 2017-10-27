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

#include "util.h"

//#define USE_XSUB
#ifdef USE_XSUB
#define  ZMQ_PUB_TYPE   ZMQ_XPUB
#define  ZMQ_SUB_TYPE   ZMQ_XSUB
#else
#define  ZMQ_PUB_TYPE   ZMQ_PUB
#define  ZMQ_SUB_TYPE   ZMQ_SUB
#endif

//
// threads
#define OPENMAMA_ZMQ_THREAD_SAFE
#ifdef  OPENMAMA_ZMQ_THREAD_SAFE
#define WLOCK_LOCK      wlock_lock
#define WLOCK_UNLOCK    wlock_unlock
#else
#define WLOCK_LOCK      wlock_noop
#define WLOCK_UNLOCK    wlock_noop
#endif

// call a function that returns mama_status -- log an error and return if not MAMA_STATUS_OK
#define CALL_MAMA_FUNC(x)                                                                  \
   do {                                                                                    \
      mama_status s = (x);                                                                 \
      if (s != MAMA_STATUS_OK) {                                                           \
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", s, mamaStatus_stringForStatus(s)); \
         return s;                                                                         \
      }                                                                                    \
   } while(0)

// call a function that returns an int -- log an error and return if < 0
#define CALL_ZMQ_FUNC(x)                                                             \
   do {                                                                              \
      int rc = (x);                                                                  \
      if (rc < 0) {                                                                  \
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", errno, zmq_strerror(errno)); \
         return MAMA_STATUS_PLATFORM;                                                \
      }                                                                              \
   } while(0)



/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

#include <sys/types.h>
#include <regex.h>

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
#define     MAX_SUBJECT_LENGTH               256         // topic size
#define     ZMQ_MAX_NAMING_URIS              8           // proxy processes for naming messages
#define     ZMQ_MAX_INCOMING_URIS            256         // incoming connections from other processes
#define     ZMQ_MAX_OUTGOING_URIS            256         // outgoing connections to other processes
#define     ZMQ_MAX_ENDPOINT_LENGTH          256         // enpoint string


/* Message types */
typedef enum zmqMsgType_ {
   ZMQ_MSG_PUB_SUB        =              0x00,
   ZMQ_MSG_INBOX_REQUEST,
   ZMQ_MSG_INBOX_RESPONSE,
   ZMQ_MSG_SUB_REQUEST
} zmqMsgType;

typedef enum zmqTransportType_ {
   ZMQ_TPORT_TYPE_TCP,
   ZMQ_TPORT_TYPE_IPC,
   ZMQ_TPORT_TYPE_PGM,
   ZMQ_TPORT_TYPE_EPGM,
   ZMQ_TPORT_TYPE_UNKNOWN = 99
} zmqTransportType;

typedef enum zmqTransportDirection_ {
   ZMQ_TPORT_DIRECTION_INCOMING,
   ZMQ_TPORT_DIRECTION_OUTGOING
} zmqTransportDirection;


#define ZMQ_CONTROL_ENDPOINT  "inproc://control"

typedef struct zmqSocket_ {
   void*       mSocket;
   wLock       mLock;
} zmqSocket;


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

typedef struct zmqTransportBridge_ {
   int                     mIsValid;
   mamaTransport           mTransport;
   msgBridge               mMsg;
   void*                   mZmqContext;
   int                     mIsNaming;
   const char*             mPublishAddress;

   // inproc socket for inter-thread commands
   zmqSocket               mZmqControlSubscriber;
   zmqSocket               mZmqControlPublisher;

   // naming transports only
   const char*             mIncomingNamingAddress[ZMQ_MAX_NAMING_URIS];
   const char*             mOutgoingNamingAddress[ZMQ_MAX_NAMING_URIS];
   const char*             mPubEndpoint;          // endpoint address for naming
   const char*             mSubEndpoint;          // endpoint address for naming
   zmqSocket               mZmqNamingPublisher;   // outgoing connections to proxy
   zmqSocket               mZmqNamingSubscriber;  // incoming connections from proxy

   // "data" sockets for normal messaging
   zmqSocket               mZmqDataPublisher;
   zmqSocket               mZmqDataSubscriber;
   const char*             mIncomingAddress[ZMQ_MAX_INCOMING_URIS];
   const char*             mOutgoingAddress[ZMQ_MAX_OUTGOING_URIS];

   const char*             mName;
   wthread_t               mOmzmqDispatchThread;
   uint32_t                mIsDispatching;
   mama_status             mOmzmqDispatchStatus;
   endpointPool_t          mSubEndpoints;
   wList                   mWcEndpoints;
   wtable_t                mInboxes;
   long int                mMemoryPoolSize;
   long int                mMemoryNodeSize;

   // inbox support
   const char*             mInboxSubject;         // one subject per transport

   // misc stats
   long int                mNormalMessages;
   long int                mNamingMessages;
   long int                mSubMessages;
   long int                mInboxMessages;
   long int                mControlMessages;
   long int                mPolls;
   long int                mNoPolls;

} zmqTransportBridge;

typedef struct zmqSubscription_ {
   mamaMsgCallbacks        mMamaCallback;
   mamaSubscription        mMamaSubscription;
   mamaQueue               mMamaQueue;
   void*                   mZmqQueue;
   zmqTransportBridge*     mTransport;
   char*                   mSubjectKey;
   void*                   mClosure;
   int                     mIsNotMuted;
   int                     mIsValid;
   int                     mIsTportDisconnected;
   msgBridge               mMsg;
   const char*             mEndpointIdentifier;
   int                     mIsWildcard;
   const char*             mOrigRegex;
   regex_t*                mRegexTopic;
} zmqSubscription;

typedef struct zmqTransportMsg_ {
   zmqTransportBridge*     mTransport;
   endpointPool_t          mSubEndpoints;
   char*                   mEndpointIdentifier;
   union {
      const char*          mSubject;
      uint8_t*             mNodeBuffer;
   };
   size_t                  mNodeSize;
} zmqTransportMsg;

typedef struct zmqQueueBridge {
   mamaQueue               mParent;
   wombatQueue             mQueue;
   void*                   mEnqueueClosure;
   uint8_t                 mHighWaterFired;
   size_t                  mHighWatermark;
   size_t                  mLowWatermark;
   uint32_t                mIsDispatching;
   mamaQueueEnqueueCB      mEnqueueCallback;
   void*                   mClosure;
   wthread_mutex_t         mDispatchLock;
   zmqQueueClosureCleanup  mClosureCleanupCb;
} zmqQueueBridge;

#define ZMQ_NAMING_PREFIX            "_NAMING"
typedef struct zmqNamingMsg {
   char                    mTopic[MAX_SUBJECT_LENGTH];
   unsigned char           mType;
   char                    mProgName[256];
   char                    mHost[MAXHOSTNAMELEN + 1];
   int                     mPid;
   char                    mPubEndpoint[ZMQ_MAX_ENDPOINT_LENGTH];
   char                    mSubEndpoint[ZMQ_MAX_ENDPOINT_LENGTH];
} zmqNamingMsg;

typedef struct zmqControlMsg {
   char     command;
   char     arg1[256];
} zmqControlMsg;


#define ZMQ_REPLYHANDLE_PREFIX            "_INBOX"
// full reply handle is "_INBOX.<replyAddr>.<inboxName>" where:
// replyAddr is a uuid string (36 bytes)
// inboxName is a uuid string (36 bytes)
// so the whole thing is 6+1+36+1+36 = 80 (+1 for trailing null)
// e.g., "_INBOX.15BD6852-91B0-4F4A-A2FB-5E63A1D24561.B68A5F70-A7FB-4816-B3E1-1DF7438E2FD2"
#define ZMQ_REPLYHANDLE_SIZE              81
// offset of inboxName in the string
#define ZMQ_REPLYHANDLE_INBOXNAME_INDEX   43

typedef struct zmqInboxImpl {
   void*                           mClosure;
   mamaQueue                       mMamaQueue;
   void*                           mZmqQueue;
   zmqTransportBridge*             mTransport;
   const char*                     mReplyHandle;
   mamaInboxMsgCallback            mMsgCB;
   mamaInboxErrorCallback          mErrCB;
   mamaInboxDestroyCallback        mOnInboxDestroyed;
   mamaInbox                       mParent;
} zmqInboxImpl;



#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_ZMQDEFS_H__ */
