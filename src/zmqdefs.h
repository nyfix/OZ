/*
 * The MIT License (MIT)
 *
 * Original work Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
 * Modified work Copyright (c) 2018 Bill Torpey (http://btorpey.github.io)
 * and assigned to Ullink, Inc.
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

#define ONE_MILLION           1000000


///////////////////////////////////////////////////////////////////////
// the following definitions control how the library is built

// for debugging inbox create/destroy
//#define MAMA_LOG_INBOX_MSGS MAMA_LOG_LEVEL_NORMAL
#define MAMA_LOG_INBOX_MSGS MAMA_LOG_LEVEL_FINER

// Note that hash table size is actually 10x value specified in wtable_create
// (to reduce collisions), and that there is no limit to # of entries
// So a table of size 1024 will use 8MB (1024*10*sizeof(void*))
#define     INBOX_TABLE_SIZE                 1024

#define     PEER_TABLE_SIZE                  1024


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
//#define     ZMQ_MAX_ENDPOINT_LENGTH          sizeof("tcp://255.255.255.255:65536")
#define     ZMQ_MAX_ENDPOINT_LENGTH          256
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
#include <wlock.h>
#include <wombat/queue.h>
#include <wombat/mempool.h>
#include <mama/integration/types.h>

// required for definition of ZMQ_CLIENT, ZMQ_SERVER
#define ZMQ_BUILD_DRAFT_API
#include <zmq.h>

// zmq bridge includes
#include "queue.h"
#include "util.h"
#include "uqueue.h"

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
#define ZMQ_MONITOR_ENDPOINT  "inproc://monitor"

typedef struct zmqSocket_ {
   void*       mSocket;        // the zmq socket
   wLock       mLock;          // mutex to control access to socket across threads
   int         mMonitor;       // if non-zero, socket is to be monitored
} zmqSocket;


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/


// main data structure for the transport
typedef struct zmqTransportBridge_ {
   const char*             mName;               // select from mama.properties: mama.<middleware>.transport.<name>.<property>
   wsem_t                  mIsReady;            // prevents shutdown from proceeding until startup has completed
   uint32_t                mNamingConnected;    // signals that we've received our own discovery msg
   int                     mIsValid;            // required by Mama API
   mamaTransport           mTransport;          // parent Mama transport
   void*                   mZmqContext;
   int                     mIsNaming;           // whether transport is a "naming" transport
   const char*             mPublishAddress;     // publish_address from mama.properties (e.g., "eth0")
   const char*             mUuid;               // unique id of this transport object

   // inproc socket for inter-thread commands
   zmqSocket               mZmqControlSub;
   zmqSocket               mZmqControlPub;

   // naming transports only
   zmqSocket               mZmqNamingPub;             // outgoing connections to proxy
   zmqSocket               mZmqNamingSub;             // incoming connections from proxy
   const char*             mPubEndpoint;              // endpoint address for naming
   const char*             mNamingAddress[ZMQ_MAX_NAMING_URIS];
   int                     mNamingReconnect;          // enable auto-reconnect for naming sockets?
   int                     mNamingReconnectInterval;
   int                     mNamingWaitForConnect;     // wait until connected to proxy at startup/abort if failed?
   int                     mNamingConnectRetries;     // max number of proxy connect attempts
   int                     mNamingConnectInterval;    // interval between proxy connect attempts (in micros, as per usleep)
   uint32_t                mBeaconInterval;           // interval between beacons (in millis, as per zmq_poll), or -1 to disable beaconing

   // "data" sockets for normal messaging
   zmqSocket               mZmqDataPub;
   zmqSocket               mZmqDataSub;
   const char*             mIncomingAddress[ZMQ_MAX_INCOMING_URIS];
   const char*             mOutgoingAddress[ZMQ_MAX_OUTGOING_URIS];
   int                     mDataReconnect;
   int                     mDataReconnectInterval;

   // main dispatch thread
   wthread_t               mOmzmqDispatchThread;
   uint32_t                mIsDispatching;
   mama_status             mOmzmqDispatchStatus;

   // for zmq_socket_monitor
   zmqSocket               mZmqMonitorPub;
   zmqSocket               mZmqMonitorSub;
   wthread_t               mOmzmqMonitorThread;
   uint32_t                mIsMonitoring;
   int                     mSocketMonitor;

   // peers
   wtable_t                mPeers;

   // subscription handling
   endpointPool_t          mSubEndpoints;         // regular subscription endpoints
   wLock                   mSubsLock;             // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....
   unsigned long long      mSubUid;               // unique ID of (non-wildcard) subscription
   wList                   mWcEndpoints;          // wildcard endpoints
   wLock                   mWcsLock;              // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....
   unsigned long long      mWcsUid;               // unique ID of wildcard subscription

   // inbox support
   const char*             mInboxSubject;         // one subject per transport
   wtable_t                mInboxes;              // collection of inboxes
   wLock                   mInboxesLock;          // NOTE: this lock protects ONLY the collection, NOT the individual objects contained in it....
   unsigned long long      mInboxUid;             // unique ID of inbox

   // misc stats
   long int                mNormalMessages;        // msgs received over dataSubscriber socket
   long int                mNamingMessages;        // msgs received over namingSubscriber socket
   long int                mControlMessages;       // msgs received over controlSubscriber socket
   long int                mSubMessages;           // subscription (as opposed to inbox) messages
   long int                mInboxMessages;         // inbox (as opposed to subscription) messages
   long int                mPolls;                 // msgs read after calling zmq_poll

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


typedef struct zmqQueueBridge {
   mamaQueue               mParent;
   uQueue                  mQueue;
   void*                   mEnqueueClosure;
   uint8_t                 mHighWaterFired;
   size_t                  mHighWatermark;
   size_t                  mLowWatermark;
   uint32_t                mIsDispatching;
   uint32_t                mIsActive;
   mamaQueueEnqueueCB      mEnqueueCallback;
   wthread_mutex_t         mDispatchLock;
} zmqQueueBridge;

#define ZMQ_NAMING_PREFIX            "_NAMING"
// Note: 0mq doesn't guarantee that messages will be aligned on any particular boundary, so use
// pragma to ensure compiler knows that struct is unaligned
#pragma pack(push, 1)
// defines discovery (naming) msgs sent by transport on startup and received by other transports
// naming msgs use namingSubscriber/namingPublisher, which is connected to one or more zmq_proxy processes
typedef struct zmqNamingMsg {
   char                    mTopic[MAX_SUBJECT_LENGTH +1];               // w/zmq, topic string must be first part of msg
   unsigned char           mType;                                       // "C"=connect, "D"=disconnect, "W"=welcome
   char                    mProgName[256 +1];                           // executable name
   char                    mHost[MAXHOSTNAMELEN + 1];                   // (short) hostname
   long                    mPid;                                        // process ID
   char                    mUuid[UUID_STRING_SIZE +1];                  // uuid of transport
   char                    mEndPointAddr[ZMQ_MAX_ENDPOINT_LENGTH +1];   // dataSub socket connects to this endpoint
}  zmqNamingMsg;
#pragma pack(pop)


#pragma pack(push, 1)
// defines control msg sent to main dispatch thread via inproc transport
typedef struct zmqControlMsg {
   char     command;                         // "S"=subscribe, "U"=unsubscribe, "X"=exit
   char     arg1[MAX_SUBJECT_LENGTH +1];     // for subscribe & unsubscribe this is the topic
} zmqControlMsg;
#pragma pack(pop)


// full reply handle is "_INBOX.<replyAddr>.<inboxID>" where:
// replyAddr is a uuid string (36 bytes)
// inboxID is a long long encoded as a hex string (16 bytes)
// so the whole thing is 6+1+36+1+16 = 60 (+1 for trailing null)
// e.g., "_INBOX.d4ac532a-224f-11e8-a178-082e5f19101.000000000000000A"
#define ZMQ_REPLYHANDLE_PREFIX            "_INBOX"
#define ZMQ_INBOX_SUBJECT_SIZE            6+1+UUID_STRING_SIZE                // _INBOX.<UUID>
#define ZMQ_REPLYHANDLE_INBOXNAME_INDEX   ZMQ_INBOX_SUBJECT_SIZE              // offset of inboxName in the string
#define ZMQ_REPLYHANDLE_INBOXNAME_SIZE    16                                  // long long in hex format
#define ZMQ_REPLYHANDLE_SIZE              ZMQ_INBOX_SUBJECT_SIZE+1+ZMQ_REPLYHANDLE_INBOXNAME_SIZE

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


// created by the dispatch thread (thread that reads zmq directly), and enqueued to the callback thread
// it has everything the callback thread needs to process the message
typedef struct zmqTransportMsg_ {
    zmqTransportBridge*     mTransport;
    char                    mEndpointIdentifier[ZMQ_REPLYHANDLE_INBOXNAME_SIZE+1];    // UUID that uniquely identifies a specific subscriber
    zmq_msg_t               mZmsg;
} zmqTransportMsg;


// this is the internal msg structure implemented in msg.c
typedef struct zmqBridgeMsgImpl {
   mamaMsg             mParent;
   uint8_t             mMsgType;                               // pub/sub, request or reply
   char                mReplyHandle[ZMQ_REPLYHANDLE_SIZE +1];  // for a request msg, unique identifier of the sending inbox
   char                mSendSubject[MAX_SUBJECT_LENGTH +1];    // topic on which the msg is sent
} zmqBridgeMsgImpl;



#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_ZMQDEFS_H__ */
