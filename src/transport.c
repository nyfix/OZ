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

/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

// required for definition of program_invocation_short_name, which is used for
// naming messages
#define _GNU_SOURCE

// system includes
#include <stdio.h>
#include <errno.h>
#include <assert.h>

// MAMA includes
#include <mama/mama.h>
#include <wombat/wInterlocked.h>
#include <wombat/mempool.h>
#include <wombat/memnode.h>
#include <wombat/queue.h>
#include <queueimpl.h>
#include <msgimpl.h>
#include <subscriptionimpl.h>
#include <transportimpl.h>
#include <timers.h>
#include <wombat/wUuid.h>

// local includes
#include "subscription.h"
#include "msg.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include "util.h"
#include "inbox.h"
#include "params.h"

#include "transport.h"

const char* zmqBridgeMamaTransport_generateInboxSubject()
{
   wUuid tempUuid;
   wUuid_generate_random(tempUuid);
   char uuidStringBuffer[UUID_STRING_SIZE+1];
   wUuid_unparse(tempUuid, uuidStringBuffer);

   return strdup(uuidStringBuffer);
}


///////////////////////////////////////////////////////////////////////////////
// following functions are defined in the Mama API
int zmqBridgeMamaTransport_isValid(transportBridge transport)
{
   zmqTransportBridge*    impl   = (zmqTransportBridge*) transport;
   int                    status = 0;

   if (NULL != impl) {
      status = impl->mIsValid;
   }
   return status;
}


mama_status zmqBridgeMamaTransport_create(transportBridge* result, const char* name, mamaTransport parent)
{
   if (NULL == result || NULL == name || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }
   mama_status status = MAMA_STATUS_OK;

   zmqTransportBridge* impl = (zmqTransportBridge*) calloc(1, sizeof(zmqTransportBridge));

   /* Back reference the MAMA transport */
   impl->mTransport           = parent;

   /* Initialize the dispatch thread pointer */
   impl->mOmzmqDispatchThread  = 0;
   impl->mOmzmqDispatchStatus  = MAMA_STATUS_OK;
   impl->mName                 = name;

   wsem_init(&impl->mIsReady, 0, 0);

   // initialize counters
   impl->mNamingMessages       = 0;
   impl->mNormalMessages       = 0;
   impl->mSubMessages          = 0;
   impl->mInboxMessages        = 0;
   impl->mControlMessages      = 0;
   impl->mPolls                = 0;
   impl->mNoPolls              = 0;

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Initializing transport with name %s", impl->mName);

   // parse params
   zmqBridgeMamaTransportImpl_parseCommonParams(impl);
   if (impl->mIsNaming == 1) {
      zmqBridgeMamaTransportImpl_parseNamingParams(impl);
   }
   else {
      zmqBridgeMamaTransportImpl_parseNonNamingParams(impl);
   }

   // create wildcard endpoints
   impl->mWcEndpoints = list_create(sizeof(zmqSubscription*));
   if (impl->mWcEndpoints == INVALID_LIST) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create wildcard endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }
   impl->mWcsLock = wlock_create();
   __sync_fetch_and_and(&impl->mWcsUid, 0);

   // create inboxes
   impl->mInboxes = wtable_create("inboxes", INBOX_TABLE_SIZE);
   if (impl->mInboxes == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create inbox endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }
   impl->mInboxesLock = wlock_create();
   __sync_fetch_and_and(&impl->mInboxUid, 0);

   // create sub endpoints
   status = endpointPool_create(&impl->mSubEndpoints, "mSubEndpoints");
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create subscribing endpoints");
      free(impl);
      return status;
   }
   impl->mSubsLock = wlock_create();
   __sync_fetch_and_and(&impl->mSubUid, 0);

   // generate inbox subject
   const char* inboxSubject = zmqBridgeMamaTransport_generateInboxSubject();
   char temp[ZMQ_INBOX_SUBJECT_SIZE];
   snprintf(temp, sizeof(temp) - 1, "%s.%s", ZMQ_REPLYHANDLE_PREFIX, inboxSubject);
   free((void*) inboxSubject);
   impl->mInboxSubject = strdup(temp);

   // connect/bind/subscribe/etc. all sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_init(impl));

   // start the dispatch thread
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_start(impl));

   if (impl->mIsNaming) {
      // publish connection information
      #if 0
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_publishEndpoints(impl));
      #else
      /* Initialize dispatch thread */
      wthread_t publishThread;
      int rc = wthread_create(&publishThread, NULL, zmqBridgeMamaTransportImpl_publishEndpoints, impl);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "create of endpoint publish thread failed %d(%s)", rc, strerror(rc));
         return MAMA_STATUS_PLATFORM;
      }
      #endif

   }

   impl->mIsValid = 1;
   *result = (transportBridge) impl;

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransport_destroy(transportBridge transport)
{
   zmqTransportBridge*    impl    = NULL;
   mama_status            status  = MAMA_STATUS_OK;

   if (NULL == transport) {
      return MAMA_STATUS_NULL_ARG;
   }

   impl  = (zmqTransportBridge*) transport;

   // stop the dispatcher(s)
   status = zmqBridgeMamaTransportImpl_stop(impl);
   wsem_destroy(&impl->mIsReady);
   wsem_destroy(&impl->mNamingConnected);

   // close sockets
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataPublisher);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataSubscriber);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlPublisher);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlSubscriber);
   if (impl->mIsNaming) {
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingPublisher);
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingSubscriber);
   }

   // stop the monitor thread
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_stopMonitor(impl));

   // shutdown zmq
   zmq_ctx_shutdown(impl->mZmqContext);
   zmq_ctx_term(impl->mZmqContext);

   // free memory
   wlock_destroy(impl->mSubsLock);
   endpointPool_destroy(impl->mSubEndpoints);

   wlock_destroy(impl->mInboxesLock);
   wtable_destroy(impl->mInboxes);

   wlock_destroy(impl->mWcsLock);
   list_destroy(impl->mWcEndpoints, NULL, NULL);

   free((void*) impl->mInboxSubject);
   free((void*) impl->mPubEndpoint);
   free((void*) impl->mSubEndpoint);

   for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
      free((void*) impl->mOutgoingNamingAddress[i]);
      free((void*) impl->mIncomingNamingAddress[i]);
   }

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Naming messages = %ld", impl->mNamingMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Normal messages = %ld", impl->mNormalMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Subscription messages = %ld", impl->mSubMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Inbox messages = %ld", impl->mInboxMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Control messages = %ld", impl->mControlMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Polls = %ld", impl->mPolls);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "No polls = %ld", impl->mNoPolls);

   free(impl);

   return status;
}


mama_status zmqBridgeMamaTransport_getNativeTransport(transportBridge transport, void** result)
{
   if (NULL == transport || NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqTransportBridge* impl = (zmqTransportBridge*)transport;

   *result = impl;

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
zmqTransportBridge* zmqBridgeMamaTransportImpl_getTransportBridge(mamaTransport transport)
{
   zmqTransportBridge*    impl;
   mama_status status = mamaTransport_getBridgeTransport(transport, (transportBridge*) &impl);
   if (status != MAMA_STATUS_OK || impl == NULL) {
      return NULL;
   }

   return impl;
}

///////////////////////////////////////////////////////////////////////////////
// startup/shutdown
// initalize the transport
mama_status zmqBridgeMamaTransportImpl_init(zmqTransportBridge* impl)
{
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR,"transport NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   // create context
   impl->mZmqContext = zmq_ctx_new();
   if (impl->mZmqContext == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to allocate zmq context - error %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   // create control sockets for inter-thread commands
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlSubscriber, ZMQ_SERVER, "controlSub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqControlSubscriber,  ZMQ_CONTROL_ENDPOINT, NULL));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlPublisher, ZMQ_CLIENT, "controlPub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqControlPublisher,  ZMQ_CONTROL_ENDPOINT));

   // create data sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataPublisher, ZMQ_PUB_TYPE, "dataPub", 1));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataSubscriber, ZMQ_SUB_TYPE, "dataSub", 1));
   // set socket options as per mama.properties etc.
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqDataPublisher));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqDataSubscriber));

   // subscribe to inbox subjects
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSubscriber.mSocket, impl->mInboxSubject));

   if (impl->mIsNaming) {
      // create naming sockets
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingPublisher, ZMQ_PUB_TYPE, "namingPub", 1));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingSubscriber, ZMQ_SUB_TYPE, "namingSub", 1));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqNamingSubscriber.mSocket, ZMQ_NAMING_PREFIX));
   }

   // start the monitor thread (before any connects/binds)
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_startMonitor(impl));

   if (impl->mIsNaming) {
      // disable reconnect on data sockets
      // (to avoid reconnecting to ephemeral port which may have been re-used by another process)
      //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqDataPublisher));
      //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqDataSubscriber));

      // bind pub/sub sockets
      char endpointAddress[ZMQ_MAX_ENDPOINT_LENGTH];
      sprintf(endpointAddress, "tcp://%s:*", impl->mPublishAddress);
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqDataPublisher,  endpointAddress, &impl->mPubEndpoint));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqDataSubscriber, endpointAddress, &impl->mSubEndpoint));
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Binding data sockets to publisher:%s subscriber:%s", impl->mSubEndpoint, impl->mPubEndpoint);

      // one or the other, but not both (or will get duplicate msgs)
      //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataPublisher, impl->mSubEndpoint));
      // prefer connecting sub to pub (see https://github.com/zeromq/libzmq/issues/2267)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataSubscriber, impl->mPubEndpoint));

      // connect to proxy for naming messages
      for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
         // must specify both
         if ((impl->mOutgoingNamingAddress[i] == NULL) || impl->mIncomingNamingAddress[i] == NULL) {
            continue;
         }

         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingPublisher, impl->mOutgoingNamingAddress[i]));
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingSubscriber, impl->mIncomingNamingAddress[i]));
         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting naming sockets to publisher:%s subscriber:%s", impl->mIncomingNamingAddress[i], impl->mOutgoingNamingAddress[i]);
      }
   }
   else {
      // non-naming style
      for (int i = 0; (i < ZMQ_MAX_OUTGOING_URIS) && (NULL != impl->mOutgoingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataPublisher,
                                                               impl->mOutgoingAddress[i], ZMQ_TPORT_DIRECTION_OUTGOING));
         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting data publisher socket to subscriber:%s", impl->mOutgoingNamingAddress[i]);
      }

      for (int i = 0; (i < ZMQ_MAX_INCOMING_URIS) && (NULL != impl->mIncomingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataSubscriber,
                                                               impl->mIncomingAddress[i], ZMQ_TPORT_DIRECTION_INCOMING));
         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting data subscriber socket to publisher:%s", impl->mIncomingNamingAddress[i]);
      }
   }

   return MAMA_STATUS_OK;
}

// starts the main dispatch thread
mama_status zmqBridgeMamaTransportImpl_start(zmqTransportBridge* impl)
{
   /* Initialize dispatch thread */
   int rc = wthread_create(&(impl->mOmzmqDispatchThread), NULL, zmqBridgeMamaTransportImpl_dispatchThread, impl);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "create of dispatch thread failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

// stops the main dispatch thread
mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl)
{
   // make sure that transport has started before we try to stop it
   // prevents a race condition on mIsDispatching
   wsem_wait(&impl->mIsReady);

   // send disconnect msg to peers
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'D'));

   zmqControlMsg msg;
   memset(&msg, '\0', sizeof(msg));
   msg.command = 'X';
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(impl, &msg, sizeof(msg)));

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Waiting on dispatch thread to terminate.");
   wthread_join(impl->mOmzmqDispatchThread, NULL);
   mama_status status = impl->mOmzmqDispatchStatus;
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Rejoined with status: %s.", mamaStatus_stringForStatus(status));

   // TODO: return status?
   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// dispatch functions

// main thread that reads directly off zmq sockets and calls one of the ...dispatch methods
void* zmqBridgeMamaTransportImpl_dispatchThread(void* closure)
{
   zmqTransportBridge* impl = (zmqTransportBridge*)closure;

   zmq_msg_t zmsg;
   zmq_msg_init(&zmsg);

   /* Set the transport bridge mIsDispatching to true. */
   wInterlocked_initialize(&impl->mIsDispatching);
   wInterlocked_set(1, &impl->mIsDispatching);

   // force _stop method to wait for this
   // prevents a race condition on mIsDispatching
   wsem_post(&impl->mIsReady);


   wlock_lock(impl->mZmqDataSubscriber.mLock);
   if (impl->mIsNaming) {
      wlock_lock(impl->mZmqNamingSubscriber.mLock);
   }

   // Check if we should be still dispatching.
   while (1 == wInterlocked_read(&impl->mIsDispatching)) {

      // TODO: revisit?
      //  try reading directly from data socket, and poll only if no msg ready
      int size = zmq_msg_recv(&zmsg, impl->mZmqDataSubscriber.mSocket, ZMQ_DONTWAIT);
      if (size != -1) {
         ++impl->mNoPolls;
         zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         continue;
      }

      // no normal msg, so poll
      zmq_pollitem_t items[] = {
         { impl->mZmqControlSubscriber.mSocket, 0, ZMQ_POLLIN , 0},
         { impl->mZmqDataSubscriber.mSocket,    0, ZMQ_POLLIN , 0},
         { impl->mZmqNamingSubscriber.mSocket,  0, ZMQ_POLLIN , 0}
      };
      int rc = zmq_poll(items, impl->mIsNaming ? 3 : 2, -1);
      if (rc < 0) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned %d - errorno %d(%s)", rc, zmq_errno(), zmq_strerror(zmq_errno()));
         continue;
      }
      ++impl->mPolls;

      // got command msg?
      if (items[0].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqControlSubscriber.mSocket, ZMQ_DONTWAIT);
         if (size == -1) {
            MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no command msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchControlMsg(impl, &zmsg);
         }
      }

      // got normal msg?
      if (items[1].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqDataSubscriber.mSocket, ZMQ_DONTWAIT);
         if (size == -1) {
            MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no data msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         }
      }

      // got naming msg?
      if (items[2].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqNamingSubscriber.mSocket, ZMQ_DONTWAIT);
         if (size == -1) {
            MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no naming msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchNamingMsg(impl, &zmsg);
         }
      }
   }

   wlock_unlock(impl->mZmqDataSubscriber.mLock);
   if (impl->mIsNaming) {
      wlock_unlock(impl->mZmqNamingSubscriber.mLock);
   }

   impl->mOmzmqDispatchStatus = MAMA_STATUS_OK;
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// The ...dispatch functions all run on the main dispatch thread, and thus can access the
// control, normal and naming (if applicable) sockets without restriction.

// control messages are processed immediately on the dispatch thread
mama_status zmqBridgeMamaTransportImpl_dispatchControlMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   impl->mControlMessages++;

   zmqControlMsg* pMsg = zmq_msg_data(zmsg);

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "command=%c arg1=%s", pMsg->command, pMsg->arg1);

   if (pMsg->command == 'S') {
      // subscribe
      return zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'U') {
      // unsubscribe
      return zmqBridgeMamaTransportImpl_unsubscribe(impl->mZmqDataSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'X') {
      // exit
      wInterlocked_set(0, &impl->mIsDispatching);
   }
   else if (pMsg->command == 'N') {
      // no-op
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unknown command=%c", pMsg->command);
   }

   return MAMA_STATUS_OK;
}


// naming messages are processed immediately on the dispatch thread
mama_status zmqBridgeMamaTransportImpl_dispatchNamingMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   impl->mNamingMessages++;

   zmqNamingMsg* pMsg = zmq_msg_data(zmsg);

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Received naming msg: type=%c prog=%s host=%s pid=%d topic=%s pub=%s sub=%s", pMsg->mType, pMsg->mProgName, pMsg->mHost, pMsg->mPid, pMsg->mTopic, pMsg->mPubEndpoint, pMsg->mSubEndpoint);

   if (pMsg->mType == 'C') {
      // connect

      // is this our msg?
      if ((strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) == 0) && (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) == 0)) {
         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Got own endpoint msg -- signaling event");
         wsem_post(&impl->mNamingConnected);
         // NOTE: dont connect to self (avoid duplicate msgs)
         return MAMA_STATUS_OK;
      }

      // NOTE: multiple connections to same endpoint are silently ignored (see https://github.com/zeromq/libzmq/issues/788)
      // NOTE: dont connect to self (avoid duplicate msgs)
      if (strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataPublisher, pMsg->mSubEndpoint));
      if (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataSubscriber, pMsg->mPubEndpoint));

      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting data sockets to publisher:%s subscriber:%s", pMsg->mSubEndpoint, pMsg->mPubEndpoint);
   }
   else if (pMsg->mType == 'D') {
      // disconnect

      // is this our msg?
      if ((strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) == 0) && (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) == 0)) {
         MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Got own endpoint msg -- ignoring");
         // NOTE: dont disconnect from self
         return MAMA_STATUS_OK;
      }

      // zmq will silently ignore multiple attempts to connect to the same endpoint (see https://github.com/zeromq/libzmq/issues/788)
      // so, we want to explicitly disconnect from sockets on normal shutdown so that zmq will know that the endpoint is
      // disconnected and will *not* ignore a subsequent request to connect to it
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disconnectSocket(&impl->mZmqDataPublisher, pMsg->mSubEndpoint));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disconnectSocket(&impl->mZmqDataSubscriber, pMsg->mPubEndpoint));
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Disconnecting data sockets from publisher:%s subscriber:%s", pMsg->mSubEndpoint, pMsg->mPubEndpoint);
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unknown naming msg type=%c", pMsg->mType);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}


// "normal" (data) messages are enqueued on the dispatch thread of the inbox or subscription
mama_status zmqBridgeMamaTransportImpl_dispatchNormalMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   const char* subject = (char*) zmq_msg_data(zmsg);
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Got msg with subject %s", subject);

   impl->mNormalMessages++;

   if (memcmp(subject, ZMQ_REPLYHANDLE_PREFIX, strlen(ZMQ_REPLYHANDLE_PREFIX)) == 0) {
      return zmqBridgeMamaTransportImpl_dispatchInboxMsg(impl, subject, zmsg);
   }
   else {
      return zmqBridgeMamaTransportImpl_dispatchSubMsg(impl, subject, zmsg);
   }
}


// enqueue msg to the (one and only) inbox
mama_status zmqBridgeMamaTransportImpl_dispatchInboxMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg)
{
   impl->mInboxMessages++;

   // index directly into subject to pick up inbox name (last part)
   const char* inboxName = &subject[ZMQ_REPLYHANDLE_INBOXNAME_INDEX];
   wlock_lock(impl->mInboxesLock);
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, inboxName);
   if (inbox == NULL) {
      wlock_unlock(impl->mInboxesLock);
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for subject %s", subject);
      return MAMA_STATUS_NOT_FOUND;
   }

   void* queue = inbox->mZmqQueue;
   // at this point, we dont care if the inbox is deleted (as long as the queue remains)
   wlock_unlock(impl->mInboxesLock);

   // TODO: can/should move following to zmqBridgeMamaTransportImpl_queueCallback?
   memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(impl, queue, zmsg);
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mEndpointIdentifier = strdup(inboxName);

   // callback (queued) will release the message
   zmqBridgeMamaQueue_enqueueEventEx(queue, zmqBridgeMamaTransportImpl_inboxCallback, node);

   return MAMA_STATUS_OK;
}


// enqueue msg to all matching subscribers
// (both regular and wildcard subscribers)
mama_status zmqBridgeMamaTransportImpl_dispatchSubMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg)
{
   impl->mSubMessages++;

   // process wildcard subscriptions
   zmqWildcardClosure wcClosure;
   wcClosure.subject = subject;
   wcClosure.zmsg = zmsg;
   wcClosure.found = 0;
   wlock_lock(impl->mWcsLock);
   list_for_each(impl->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_matchWildcards, &wcClosure);
   wlock_unlock(impl->mWcsLock);
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found %d wildcard matches for %s", wcClosure.found, subject);

   // process regular (non-wildcard) subscriptions
   endpoint_t* subs = NULL;
   size_t subCount = 0;
   wlock_lock(impl->mSubsLock);
   mama_status status = endpointPool_getRegistered(impl->mSubEndpoints, subject, &subs, &subCount);
   if (MAMA_STATUS_OK != status) {
      wlock_unlock(impl->mSubsLock);
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s) querying registration table for subject %s", status, mamaStatus_stringForStatus(status), subject);
      return MAMA_STATUS_SYSTEM_ERROR;
   }
   if (0 == subCount) {
      wlock_unlock(impl->mSubsLock);
      if (wcClosure.found == 0) {
         MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for subject %s", subject);
      }
      return MAMA_STATUS_NOT_FOUND;
   }
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found %d non-wildcard matches for %s", subCount, subject);

   // process each subscriber
   for (size_t subInc = 0; subInc < subCount; subInc++) {
      zmqSubscription*  subscription = (zmqSubscription*)subs[subInc];

      // TODO: what is the purpose of this?
      if (1 == subscription->mIsTportDisconnected) {
         subscription->mIsTportDisconnected = 0;
      }

      if (1 != subscription->mIsNotMuted) {
         MAMA_LOG(MAMA_LOG_LEVEL_WARN, "muted - not queueing update for symbol %s", subject);
      }
      else {
         memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(impl, subscription->mZmqQueue, zmsg);
         zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
         tmsg->mEndpointIdentifier = strdup(subscription->mEndpointIdentifier);

         // callback (queued) will release the message
         zmqBridgeMamaQueue_enqueueEventEx(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_subCallback, node);
      }
   }
   wlock_unlock(impl->mSubsLock);

   return MAMA_STATUS_OK;
}


// called from ..dispatchSubMsg for entry in list of wildcard subscriptions and enqueues the message
// to each matching subscriber
void zmqBridgeMamaTransportImpl_matchWildcards(wList dummy, zmqSubscription** pSubscription, zmqWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;

   // check topic up to size of subscribed topic
   if (memcmp(subscription->mSubjectKey, closure->subject, strlen(subscription->mSubjectKey)) != 0) {
      return;
   }

   // check regex
   if (regexec(subscription->mCompRegex, closure->subject, 0, NULL, 0) != 0) {
      return;
   }

   // it's a match
   closure->found++;
   memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(subscription->mTransport, subscription->mZmqQueue, closure->zmsg);
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mEndpointIdentifier = strdup(subscription->mEndpointIdentifier);

   // callback (queued) will release the message
   zmqBridgeMamaQueue_enqueueEventEx(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_wcCallback, node);
}


///////////////////////////////////////////////////////////////////////////////
// The ...Callback functions are dispatched from the queue/dispatcher associated with the subscription or inbox

///////////////////////////////////////////////////////////////////////////////
// Called when inbox reply message removed from queue by dispatch thread
// NOTE: Needs to check inbox, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the inbox is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_inboxCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   zmqTransportBridge* impl = (zmqTransportBridge*) tmsg->mTransport;

   // find the inbox
   wlock_lock(impl->mInboxesLock);
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, tmsg->mEndpointIdentifier);
   wlock_unlock(impl->mInboxesLock);
   if (inbox == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for inbox %s", tmsg->mEndpointIdentifier);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(inbox->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   zmqBridgeMamaInboxImpl_onMsg(NULL, tmpMsg, inbox, NULL);

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}


///////////////////////////////////////////////////////////////////////////////
// Called when regular subscription message removed from queue by dispatch thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the subscription is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_subCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;

   // find the subscription based on its identifier
   zmqSubscription* subscription = NULL;
   endpointPool_getEndpointByIdentifiers(tmsg->mTransport->mSubEndpoints, tmsg->mSubject,
                                         tmsg->mEndpointIdentifier, (endpoint_t*) &subscription);

   /* Can't do anything without a subscriber */
   if (NULL == subscription) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "No endpoint found for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
      goto exit;
   }

   /* Make sure that the subscription is processing messages */
   if (1 != subscription->mIsNotMuted) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Skipping update - subscription %p is muted.", subscription);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(subscription->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
   }
   else {
      /* Process the message as normal */
      status = mamaSubscription_processMsg(subscription->mMamaSubscription, tmpMsg);
      if (MAMA_STATUS_OK != status) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "mamaSubscription_processMsg() failed. [%s]", mamaStatus_stringForStatus(status));
      }
   }

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}


///////////////////////////////////////////////////////////////////////////////
// Called when wildcard subscription message removed from queue by dispatch thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the subscription is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_wcCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;

   // is this subscription still in the list?
   zmqFindWildcardClosure findClosure;
   findClosure.mEndpointIdentifier = tmsg->mEndpointIdentifier;
   findClosure.mSubscription = NULL;
   list_for_each(tmsg->mTransport->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_findWildcard, &findClosure);
   if (findClosure.mSubscription == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "No endpoint found for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
      goto exit;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found wildcard subscriber for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
   zmqSubscription* subscription = findClosure.mSubscription;

   /* Make sure that the subscription is processing messages */
   if (1 != subscription->mIsNotMuted) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Skipping update - subscription %p is muted.", subscription);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(subscription->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
   }
   else {
      status = mamaSubscription_processWildCardMsg(subscription->mMamaSubscription, tmpMsg, tmsg->mSubject, subscription->mClosure);
      if (MAMA_STATUS_OK != status) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "mamaSubscription_processMsg() failed. [%s]", mamaStatus_stringForStatus(status));
      }
   }

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}

///////////////////////////////////////////////////////////////////////////////
// wilcard helpers
void zmqBridgeMamaTransportImpl_removeWildcard(wList wcList, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;
   if (strcmp(subscription->mEndpointIdentifier, closure->mEndpointIdentifier) == 0) {
      list_remove_element(wcList, pSubscription);
      list_free_element(wcList, pSubscription);
   }
}

void zmqBridgeMamaTransportImpl_unregisterWildcard(zmqTransportBridge* impl, zmqSubscription* subscription)
{
   zmqFindWildcardClosure findClosure;
   findClosure.mEndpointIdentifier = subscription->mEndpointIdentifier;
   findClosure.mSubscription = NULL;
   list_for_each(impl->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_removeWildcard, &findClosure);
}

void zmqBridgeMamaTransportImpl_findWildcard(wList dummy, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;
   if (strcmp(subscription->mEndpointIdentifier, closure->mEndpointIdentifier) == 0) {
      closure->mSubscription = subscription;
   }
}


///////////////////////////////////////////////////////////////////////////////
// inbox helpers
mama_status zmqBridgeMamaTransportImpl_getInboxSubject(zmqTransportBridge* impl, const char** inboxSubject)
{
   if ((impl == NULL) || (inboxSubject == NULL)) {
      return MAMA_STATUS_NULL_ARG;
   }

   *inboxSubject = impl->mInboxSubject;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_registerInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   MAMA_LOG(MAMA_LOG_INBOX_MSGS, "mamaInbox=%p,replyAddr=%s", inbox->mParent, inbox->mReplyHandle);

   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_insert(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX], inbox) >= 0 ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   assert(status == MAMA_STATUS_OK);
   return status;
}


mama_status zmqBridgeMamaTransportImpl_unregisterInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   MAMA_LOG(MAMA_LOG_INBOX_MSGS, "mamaInbox=%p,replyAddr=%s", inbox->mParent, inbox->mReplyHandle);

   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_remove(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX]) == inbox ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   if (status != MAMA_STATUS_OK) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "failed to unregister inbox (%s)", inbox->mReplyHandle);
   }

   assert(status == MAMA_STATUS_OK);
   return status;
}


///////////////////////////////////////////////////////////////////////////////
// ...dispatch helpers
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_queueClosureCleanupCb(void* closure)
{
   memoryPool* pool = (memoryPool*) closure;
   if (NULL != pool) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Destroying memory pool for queue %p.", closure);
      memoryPool_destroy(pool, NULL);
   }
}

memoryNode* zmqBridgeMamaTransportImpl_allocTransportMsg(zmqTransportBridge* impl, void* queue, zmq_msg_t* zmsg)
{
   queueBridge queueImpl = (queueBridge) queue;
   memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure(queueImpl);
   if (NULL == pool) {
      pool = memoryPool_create(impl->mMemoryPoolSize, impl->mMemoryNodeSize);
      zmqBridgeMamaQueueImpl_setClosure(queueImpl, pool, zmqBridgeMamaTransportImpl_queueClosureCleanupCb);
   }

   // mNodeBuffer consists of zmqTransportMsg followed by a copy of the zmq data
   memoryNode* node = memoryPool_getNode(pool, sizeof(zmqTransportMsg) + zmq_msg_size(zmsg));
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mTransport    = impl;
   tmsg->mNodeBuffer   = (uint8_t*)(tmsg + 1);
   tmsg->mNodeSize     = zmq_msg_size(zmsg);
   tmsg->mEndpointIdentifier = NULL;
   memcpy(tmsg->mNodeBuffer, zmq_msg_data(zmsg), tmsg->mNodeSize);

   return node;
}


///////////////////////////////////////////////////////////////////////////////
// zmq socket functions
mama_status zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* pSocket, int type, const char* name, int monitor)
{
   void* temp = zmq_socket(zmqContext, type);
   if (temp == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket failed %d(%s)", zmq_errno(), zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   pSocket->mSocket = temp;
   pSocket->mLock = wlock_create();

   // we dont use router/dealer or req/rep, so we hijack the identity property to set a name to make debugging easier
   CALL_ZMQ_FUNC(zmq_setsockopt(pSocket->mSocket, ZMQ_IDENTITY, name, strlen(name) +1));

   #ifdef MONITOR_SOCKETS
   if ((monitor ==1) && (name != NULL)) {
      char endpoint[256];
      sprintf(endpoint, "inproc://%s", name);
      CALL_ZMQ_FUNC(zmq_socket_monitor(pSocket->mSocket, endpoint, ZMQ_EVENT_ALL));
   }
   #endif

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "(%p, %d) succeeded", pSocket->mSocket, type);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_connect(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect(%p, %s) failed: %d(%s)", socket->mSocket, uri, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "zmq_connect(%p, %s) succeeded", socket->mSocket, uri);
      status = zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket);
   }
   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_disconnectSocket(zmqSocket* socket, const char* uri)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_disconnect(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_disconnect(%p, %s) failed: %d(%s)", socket->mSocket, uri, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "zmq_disconnect(%p, %s) succeeded", socket->mSocket, uri);
      status = zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket);
   }
   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_bind(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_bind(%x, %s) failed %d(%s)", socket->mSocket, uri, errno, zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket);
   }

   if (endpointName != NULL) {
      char temp[ZMQ_MAX_ENDPOINT_LENGTH];
      size_t tempSize = sizeof(temp);
      int rc = zmq_getsockopt(socket->mSocket, ZMQ_LAST_ENDPOINT, temp, &tempSize);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_getsockopt(%x) failed trying to get last endpoint %d(%s)", socket->mSocket, errno, zmq_strerror(errno));
         status = MAMA_STATUS_PLATFORM;
      }
      else {
         *endpointName = strdup(temp);
      }
   }
   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_destroySocket(zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   int linger = 0;
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_LINGER, &linger, sizeof(linger));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%x) failed trying to set linger %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   rc = zmq_socket_monitor(socket->mSocket, NULL, ZMQ_EVENT_ALL);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket_monitor(%x) failed trying to disable monitoring %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   rc = zmq_close(socket->mSocket);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_close(%x) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   wlock_unlock(socket->mLock);
   wlock_destroy(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_disableReconnect(zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int reconnectInterval = -1;
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_RECONNECT_IVL, &reconnectInterval, sizeof(reconnectInterval));
   if (rc != 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%x) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   wlock_unlock(socket->mLock);

   return status;
}


// NOTE: direction is only relevant for ipc transports, for others it is
// inferred from endpoint string (wildcard => bind, non-wildcard => connect)
mama_status zmqBridgeMamaTransportImpl_bindOrConnect(void* socket, const char* uri, zmqTransportDirection direction)
{
   char tportTypeStr[16];
   char* firstColon = NULL;
   zmqTransportType tportType = ZMQ_TPORT_TYPE_UNKNOWN;
   /* If set to non zero, will bind rather than connect */
   int isBinding = 0;

   wmStrSizeCpy(tportTypeStr, uri, sizeof(tportTypeStr));
   firstColon = strchr(tportTypeStr, ':');
   if (NULL != firstColon) {
      *firstColon = '\0';
   }

   if (0 == strcmp(tportTypeStr, "tcp")) {
      tportType = ZMQ_TPORT_TYPE_TCP;
   }
   else if (0 == strcmp(tportTypeStr, "epgm")) {
      tportType = ZMQ_TPORT_TYPE_EPGM;
   }
   else if (0 == strcmp(tportTypeStr, "pgm")) {
      tportType = ZMQ_TPORT_TYPE_PGM;
   }
   else if (0 == strcmp(tportTypeStr, "ipc")) {
      tportType = ZMQ_TPORT_TYPE_IPC;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unknown ZeroMQ transport type found: %s.", tportTypeStr);
      return MAMA_STATUS_INVALID_ARG;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Found ZeroMQ transport type %s (%d)", tportTypeStr, tportType);

   /* Get the transport type from the uri */
   switch (direction) {
      case ZMQ_TPORT_DIRECTION_INCOMING:
         switch (tportType) {
            case ZMQ_TPORT_TYPE_TCP:
               if (strchr(uri, '*')) {
                  isBinding = 1;
               }
               break;
            case ZMQ_TPORT_TYPE_EPGM:
            case ZMQ_TPORT_TYPE_PGM:
            case ZMQ_TPORT_TYPE_IPC:
            default:
               break;
         }
         break;
      case ZMQ_TPORT_DIRECTION_OUTGOING:
         switch (tportType) {
            case ZMQ_TPORT_TYPE_TCP:
               if (strchr(uri, '*')) {
                  isBinding = 1;
               }
               break;
            case ZMQ_TPORT_TYPE_IPC:
               isBinding = 1;
               break;
            case ZMQ_TPORT_TYPE_EPGM:
            case ZMQ_TPORT_TYPE_PGM:
            default:
               break;
         }
         break;
   }

   /* If this is a binding transport */
   if (isBinding) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(socket, uri, NULL));
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully bound socket to: %s", uri);
   }
   else {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(socket, uri));
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully connected socket to: %s", uri);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_setSocketOptions(const char* name, zmqSocket* socket)
{
   wlock_lock(socket->mLock);
   int value = 0;
   CALL_ZMQ_FUNC(zmq_setsockopt(socket->mSocket, ZMQ_RCVHWM, &value, sizeof(value)));
   CALL_ZMQ_FUNC(zmq_setsockopt(socket->mSocket, ZMQ_SNDHWM, &value, sizeof(value)));
   wlock_unlock(socket->mLock);

   // let everything (else) default ...
   #if 0
   wlock_lock(socket->mLock);
   // options apply to all sockets (?!)
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDHWM,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVHWM,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDBUF,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVBUF,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RECONNECT_IVL,      atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RECONNECT_IVL_MAX,  atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          BACKLOG,            atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVTIMEO,           atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDTIMEO,           atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RATE,               atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  uint64_t,     AFFINITY,           atoll);
   #if ZMQ_VERSION_MINOR >= 2
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  const char*,  IDENTITY,                );
   #endif
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int64_t,      MAXMSGSIZE,         atoll);
   wlock_unlock(socket->mLock);
   #endif

   return MAMA_STATUS_OK;
}

// "Sometimes" it is necessary to trigger the processing of outstanding commands against a
// zmq socket. See https://github.com/zeromq/libzmq/issues/2267
// NOTE: caller needs to have acquired lock
mama_status zmqBridgeMamaTransportImpl_kickSocket(void* socket)
{
   // see https://github.com/zeromq/libzmq/issues/2267
   zmq_pollitem_t pollitems [] = { { socket, 0, ZMQ_POLLIN, 0 } };
   CALL_ZMQ_FUNC(zmq_poll(pollitems, 1, 1));
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "zmq_poll(%x) complete", socket);
   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// These subscribe/unsubscribe methods operate directly on the zmq socket, and as such must only
// be called from the dispatch thread.
// To subscribe/unsubscribe from any other thread, you need to use the zmqBridgeMamaSubscriptionImpl_subscribe
// function, which posts a message to the control socket.
// NOTE: The subscribe method is controlled by the USE_XSUB preprocessor symbol.
mama_status zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %x subscribing to %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\1', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_SUBSCRIBE, topic, strlen(topic)));
   #endif

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket));

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_unsubscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %x unsubscribing from %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\0', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, topic, strlen(topic)));
   #endif

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket));

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// control msgs
mama_status zmqBridgeMamaTransportImpl_sendCommand(zmqTransportBridge* impl, zmqControlMsg* msg, int msgSize)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "command=%c arg1=%s", msg->command, msg->arg1);

   int i = zmq_send(impl->mZmqControlPublisher.mSocket, msg, msgSize, 0);
   if (i <= 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_send failed  %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// naming msgs
mama_status zmqBridgeMamaTransportImpl_sendEndpointsMsg(zmqTransportBridge* impl, char command)
{
   // publish our endpoints
   zmqNamingMsg msg;
   memset(&msg, '\0', sizeof(msg));
   strcpy(msg.mTopic, ZMQ_NAMING_PREFIX);
   msg.mType = command;
   wmStrSizeCpy(msg.mProgName, program_invocation_short_name, sizeof(msg.mProgName));
   gethostname(msg.mHost, sizeof(msg.mHost));
   msg.mPid = getpid();
   strcpy(msg.mPubEndpoint, impl->mPubEndpoint);
   strcpy(msg.mSubEndpoint, impl->mSubEndpoint);
   wlock_lock(impl->mZmqNamingPublisher.mLock);
   int i = zmq_send(impl->mZmqNamingPublisher.mSocket, &msg, sizeof(msg), 0);
   wlock_unlock(impl->mZmqNamingPublisher.mLock);
   if (i != sizeof(msg)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to publish endpoints: prog=%s host=%s pid=%d pub=%s sub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mPubEndpoint, msg.mSubEndpoint);
      return MAMA_STATUS_PLATFORM;
   }

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(impl->mZmqNamingPublisher.mSocket));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Sent endpoint msg: prog=%s host=%s pid=%d pub=%s sub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mPubEndpoint, msg.mSubEndpoint);
   return MAMA_STATUS_OK;
}


// publishes endpoint message continuously every 100ms until we get it back
void* zmqBridgeMamaTransportImpl_publishEndpoints(void* closure)
{
   zmqTransportBridge* impl = (zmqTransportBridge*) closure;

   wsem_init(&impl->mNamingConnected, 0, 0);

   do {
      zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'C');
   } while ((wsem_timedwait(&impl->mNamingConnected, 100) != 0) && (errno == ETIMEDOUT));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Successfully published endpoints");
   //return MAMA_STATUS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// socket monitor

// TODO: hard-coded endpoint names
void* zmqBridgeMamaTransportImpl_monitorThread(void* closure)
{
   zmqTransportBridge* impl = (zmqTransportBridge*) closure;

   void* dataPubMonitor = zmq_socket(impl->mZmqContext, ZMQ_PAIR);
   zmq_connect(dataPubMonitor, "inproc://dataPub");
   void* dataSubMonitor = zmq_socket(impl->mZmqContext, ZMQ_PAIR);
   zmq_connect(dataSubMonitor, "inproc://dataSub");
   void* namingPubMonitor = zmq_socket(impl->mZmqContext, ZMQ_PAIR);
   zmq_connect(namingPubMonitor, "inproc://namingPub");
   void* namingSubMonitor = zmq_socket(impl->mZmqContext, ZMQ_PAIR);
   zmq_connect(namingSubMonitor, "inproc://namingSub");

   while (1 == wInterlocked_read(&impl->mIsMonitoring)) {
      zmq_pollitem_t items[] = {
         { dataPubMonitor,    0, ZMQ_POLLIN , 0},
         { dataSubMonitor,    0, ZMQ_POLLIN , 0},
         { namingPubMonitor,  0, ZMQ_POLLIN , 0},
         { namingSubMonitor, 0, ZMQ_POLLIN , 0},
      };
      int rc = zmq_poll(items, 4, -1);
      //assert(rc >= 0);

      if (items[0].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent(dataPubMonitor, "dataPub");
      }
      if (items[1].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent(dataSubMonitor, "dataSub");
      }
      if (items[2].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent(namingPubMonitor, "namingPub");
      }
      if (items[3].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent(namingSubMonitor, "namingSub");
      }
   }

   zmq_close(dataPubMonitor);
   zmq_close(dataSubMonitor);
   zmq_close(namingPubMonitor);
   zmq_close(namingSubMonitor);

   return NULL;
}


mama_status zmqBridgeMamaTransportImpl_startMonitor(zmqTransportBridge* impl)
{
   #ifdef MONITOR_SOCKETS
   /* Set the transport bridge mIsMonitoring to true. */
   wInterlocked_initialize(&impl->mIsMonitoring);
   wInterlocked_set(1, &impl->mIsMonitoring);

   /* Initialize monitor thread */
   int rc = wthread_create(&(impl->mOmzmqMonitorThread), NULL, zmqBridgeMamaTransportImpl_monitorThread, impl);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "create of monitor thread failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }
   #endif

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_stopMonitor(zmqTransportBridge* impl)
{
   #ifdef MONITOR_SOCKETS
   wInterlocked_set(0, &impl->mIsMonitoring);

   wthread_join(impl->mOmzmqMonitorThread, NULL);
   #endif

   return MAMA_STATUS_OK;
}

typedef struct __attribute__ ((packed)) zmq_monitor_frame1 {
   uint16_t    event;
   uint32_t    value;
} zmq_monitor_frame1;

int zmqBridgeMamaTransportImpl_monitorEvent(void *socket, const char* socketName)
{
   // First frame in message contains event number and value
   zmq_msg_t msg;
   zmq_msg_init (&msg);
   if (zmq_msg_recv (&msg, socket, 0) == -1)
      return -1; // Interrupted, presumably
   assert (zmq_msg_more (&msg));

   zmq_monitor_frame1* pFrame1 = (zmq_monitor_frame1*) zmq_msg_data (&msg);
   int event = pFrame1->event;
   int value = pFrame1->value;
   const char* eventName = get_zmqEventName(event);

   // Second frame in message contains event address
   zmq_msg_init (&msg);
   if (zmq_msg_recv (&msg, socket, 0) == -1)
      return -1; // Interrupted, presumably
   assert (!zmq_msg_more (&msg));

   uint8_t* data = (uint8_t *) zmq_msg_data (&msg);
   size_t size = zmq_msg_size(&msg);
   char endpoint[ZMQ_MAX_ENDPOINT_LENGTH+1];
   memset(endpoint, '\0', sizeof(endpoint));
   memcpy(endpoint, data, size);

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "socket:%x name:%s value:%d event:%d desc:%s endpoint:%s", socket, socketName, value, event, eventName, endpoint);

   return 0;
}

