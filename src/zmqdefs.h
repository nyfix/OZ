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

///////////////////////////////////////////////////////////////////////
// the following definitions control how the library is built

// if defined, a separate thread is created to log output from zmq_socket_monitor
#define MONITOR_SOCKETS

// Note that hash table size is actually 10x value specified in wtable_create
// (to reduce collisions), and that there is no limit to # of entries
// So a table of size 1024 will use 8MB (1024*10*sizeof(void*))
#define     INBOX_TABLE_SIZE                 1024

// zmq has two ways to manage subscriptions
// w/XSUB subscriptions messages can be made visible to the application
//#define USE_XSUB
#ifdef USE_XSUB
#define  ZMQ_PUB_TYPE   ZMQ_XPUB
#define  ZMQ_SUB_TYPE   ZMQ_XSUB
#else
#define  ZMQ_PUB_TYPE   ZMQ_PUB
#define  ZMQ_SUB_TYPE   ZMQ_SUB
#endif

#define     MAX_SUBJECT_LENGTH               256         // topic size
#define     ZMQ_MAX_NAMING_URIS              8           // proxy processes for naming messages
// TODO: is 256 enough? what happens if exceeded?
#define     ZMQ_MAX_INCOMING_URIS            256         // incoming connections from other processes
#define     ZMQ_MAX_OUTGOING_URIS            256         // outgoing connections to other processes
#define     ZMQ_MAX_ENDPOINT_LENGTH          256         // enpoint string
///////////////////////////////////////////////////////////////////////

/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

// system includes
#include <regex.h>

// Mama/Wombat includes
#include <wombat/wSemaphore.h>
#include <wombat/wtable.h>
#include <list.h>
#include <wombat/queue.h>
#include <wombat/mempool.h>
#include <endpointpool.h>

// zmq bridge includes
#include "queue.h"
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Message types */
typedef enum zmqMsgType_ {
   ZMQ_MSG_UNKNOWN            = 0x00,
   ZMQ_MSG_PUB_SUB,
   ZMQ_MSG_INBOX_REQUEST,
   ZMQ_MSG_INBOX_RESPONSE,
} zmqMsgType;

typedef enum zmqTransportType_ {
   ZMQ_TPORT_TYPE_UNKNOWN = 0,
   ZMQ_TPORT_TYPE_TCP,
   ZMQ_TPORT_TYPE_IPC,
   ZMQ_TPORT_TYPE_PGM,
   ZMQ_TPORT_TYPE_EPGM
} zmqTransportType;

typedef enum zmqTransportDirection_ {
   ZMQ_TPORT_DIRECTION_UNKNOWN = 0,
   ZMQ_TPORT_DIRECTION_INCOMING,
   ZMQ_TPORT_DIRECTION_OUTGOING
} zmqTransportDirection;


#define ZMQ_CONTROL_ENDPOINT  "inproc://control"

typedef struct zmqSocket_ {
   void*       mSocket;        // the zmq socket
   wLock       mLock;          // mutex to control access to socket across threads
} zmqSocket;


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/


// main data structure for the transport
typedef struct zmqTransportBridge_ {
   const char*             mName;               // select from mama.properties: mama.<middleware>.transport.<name>.<property>
   wsem_t                  mIsReady;            // prevents shutdown from proceeding until startup has completed
   wsem_t                  mNamingConnected;    // signals that we've received our own discovery msg
   int                     mIsValid;            // required by Mama API
   mamaTransport           mTransport;          // parent Mama transport
   void*                   mZmqContext;
   int                     mIsNaming;           // whether transport is a "naming" transport
   const char*             mPublishAddress;     // publish_address from mama.properties (e.g., "eth0")

   // inproc socket for inter-thread commands
   zmqSocket               mZmqControlSubscriber;
   zmqSocket               mZmqControlPublisher;

   // naming transports only
   zmqSocket               mZmqNamingPublisher;   // outgoing connections to proxy
   zmqSocket               mZmqNamingSubscriber;  // incoming connections from proxy
   const char*             mPubEndpoint;          // endpoint address for naming
   const char*             mSubEndpoint;          // endpoint address for naming
   const char*             mIncomingNamingAddress[ZMQ_MAX_NAMING_URIS];
   const char*             mOutgoingNamingAddress[ZMQ_MAX_NAMING_URIS];

   // "data" sockets for normal messaging
   zmqSocket               mZmqDataPublisher;
   zmqSocket               mZmqDataSubscriber;
   const char*             mIncomingAddress[ZMQ_MAX_INCOMING_URIS];
   const char*             mOutgoingAddress[ZMQ_MAX_OUTGOING_URIS];

   // main dispatch thread
   wthread_t               mOmzmqDispatchThread;
   uint32_t                mIsDispatching;
   mama_status             mOmzmqDispatchStatus;

   // for zmq_socket_monitor
   wthread_t               mOmzmqMonitorThread;
   uint32_t                mIsMonitoring;

   // subscription handling
   endpointPool_t          mSubEndpoints;         // regular subscription endpoints
   wLock                   mSubsLock;             // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....
   wList                   mWcEndpoints;          // wildcard endpoints
   wLock                   mWcsLock;              // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....

   // inbox support
   const char*             mInboxSubject;         // one subject per transport
   wtable_t                mInboxes;              // collection of inboxes
   wLock                   mInboxesLock;          // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....

   long int                mMemoryPoolSize;
   long int                mMemoryNodeSize;

   // misc stats
   long int                mNormalMessages;        // msgs received over dataSubscriber socket
   long int                mNamingMessages;        // msgs received over namingSubscriber socket
   long int                mControlMessages;       // msgs received over controlSubscriber socket
   long int                mSubMessages;           // subscription (as opposed to inbox) messages
   long int                mInboxMessages;         // inbox (as opposed to subscription) messages
   long int                mPolls;                 // msgs read after calling zmq_poll
   long int                mNoPolls;               // msgs read w/o needing to call zmq_poll (i.e., immediately available)

} zmqTransportBridge;


// defines a subscriber (either "normal" or wildcard)
typedef struct zmqSubscription_ {
   mamaMsgCallbacks        mMamaCallback;
   mamaSubscription        mMamaSubscription;
   mamaQueue               mMamaQueue;
   void*                   mZmqQueue;
   void*                   mClosure;
   int                     mIsNotMuted;
   int                     mIsValid;
   int                     mIsTportDisconnected;
   zmqTransportBridge*     mTransport;             // the transport that owns this subscription
   char*                   mSubjectKey;            // the topic subscribed to
   const char*             mEndpointIdentifier;    // UUID that uniquely identifies a specific subscriber
   int                     mIsWildcard;            // is this a wildcard subscription?
   const char*             mOrigRegex;             // for wildcards, original regex
   regex_t*                mCompRegex;             // for wildcards, compiled regex
} zmqSubscription;


// created by the dispatch thread (thread that reads zmq directly), and enqueued to the callback thread
// it has everything the callback thread needs to process the message
typedef struct zmqTransportMsg_ {
   zmqTransportBridge*     mTransport;
   char*                   mEndpointIdentifier;    // UUID that uniquely identifies a specific subscriber
   union {
      uint8_t*             mNodeBuffer;
      const char*          mSubject;               // for convenience -- w/zmq subject must be first part of message
   };
   size_t                  mNodeSize;              // size of mNodeBuffer
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
// defines discovery (naming) msgs sent by transport on startup and received by other transports
// naming msgs use namingSubscriber/namingPublisher, which is connected to one or more zmq_proxy processes
typedef struct zmqNamingMsg {
   char                    mTopic[MAX_SUBJECT_LENGTH];               // w/zmq, topic string must be first part of msg
   unsigned char           mType;                                    // "C"=connect, "D"=disconnect
   char                    mProgName[256];                           // executable name
   char                    mHost[MAXHOSTNAMELEN + 1];                // (short) hostname
   int                     mPid;                                     // process ID
   char                    mPubEndpoint[ZMQ_MAX_ENDPOINT_LENGTH];    // dataSubscriber connects to this endpoint
   char                    mSubEndpoint[ZMQ_MAX_ENDPOINT_LENGTH];    // dataPublisher connects to this endpoint
} zmqNamingMsg;


// defines control msg sent to main dispatch thread via inproc transport
typedef struct zmqControlMsg {
   char     command;                   // "S"=subscribe, "U"=unsubscribe, "X"=exit
   char     arg1[256];                 // for subscribe & unsubscribe this is the topic
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

// defines internal structure of an "inbox" for request/reply messaging
typedef struct zmqInboxImpl {
   void*                           mClosure;
   mamaQueue                       mMamaQueue;
   void*                           mZmqQueue;
   zmqTransportBridge*             mTransport;
   mamaInboxMsgCallback            mMsgCB;
   mamaInboxErrorCallback          mErrCB;
   mamaInboxDestroyCallback        mOnInboxDestroyed;
   mamaInbox                       mParent;
   const char*                     mReplyHandle;               // unique reply address for this inbox
} zmqInboxImpl;


// this is the internal msg structure implemented in msg.c
typedef struct zmqBridgeMsgImpl {
   mamaMsg             mParent;
   uint8_t             mMsgType;                               // pub/sub, request or reply
   char                mReplyHandle[ZMQ_REPLYHANDLE_SIZE];     // for a request msg, unique identifier of the sending inbox
   char                mSendSubject[MAX_SUBJECT_LENGTH];       // topic on which the msg is sent
   void*               mSerializedBuffer;                      // flattened buffer in wire format
   size_t              mSerializedBufferSize;                  // size of buffer
} zmqBridgeMsgImpl;



#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_ZMQDEFS_H__ */
