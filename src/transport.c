/*
 * The MIT License (MIT)
 *
 * Original work Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
 * Modified work Copyright (c) 2020 Bill Torpey (http://btorpey.github.io)
 * and assigned to NYFIX, a division of Itiviti Group AB
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

// required for definition of progname/program_invocation_short_name,
// which is used for naming messages
#if defined __APPLE__
#include <stdlib.h>
#else
#define _GNU_SOURCE
#endif

// system includes
#include <stdio.h>
#include <errno.h>

// MAMA includes
#include <mama/mama.h>
#include <mama/integration/subscription.h>
#include <mama/integration/transport.h>
#include <mama/integration/msg.h>
#include <mama/integration/queue.h>
#include <timers.h>
#include <mama/integration/endpointpool.h>
#include <wombat/wInterlocked.h>
#include <wombat/mempool.h>
#include <wombat/memnode.h>
#include <wombat/queue.h>
#include <wombat/strutils.h>

// local includes
#include "subscription.h"
#include "msg.h"
#include "zmqbridgefunctions.h"
#include "util.h"
#include "inbox.h"
#include "params.h"

#include "transport.h"

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
   if (NULL == impl) return MAMA_STATUS_NOMEM;

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

   {
   // init logging
   char* temp = getenv("MAMA_STDERR_LOGGING");
   if (temp) {
      int logLevel = atoi(temp);
      mama_enableLogging(stderr, (MamaLogLevel) logLevel);
   }
   }

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Initializing transport with name %s", impl->mName);

   // parse params
   wInterlocked_initialize(&impl->mBeaconInterval);
   zmqBridgeMamaTransportImpl_parseCommonParams(impl);
   if (impl->mIsNaming == 1) {
      zmqBridgeMamaTransportImpl_parseNamingParams(impl);
   }
   else {
      zmqBridgeMamaTransportImpl_parseNonNamingParams(impl);
   }

   mamaTransport_disableRefresh(impl->mTransport, (uint8_t) impl->mDisableRefresh);

   // create wildcard endpoints
   impl->mWcEndpoints = list_create(sizeof(zmqSubscription*));
   if (impl->mWcEndpoints == INVALID_LIST) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create wildcard endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }
   impl->mWcsLock = wlock_create();
   __sync_and_and_fetch(&impl->mWcsUid, 0);


   // create peers table
   impl->mPeers = wtable_create("peers", PEER_TABLE_SIZE);
   if (impl->mPeers == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create peers table");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   // create inboxes
   impl->mInboxes = wtable_create("inboxes", INBOX_TABLE_SIZE);
   if (impl->mInboxes == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create inbox endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }
   impl->mInboxesLock = wlock_create();
   __sync_and_and_fetch(&impl->mInboxUid, 0);

   // create sub endpoints
   status = endpointPool_create(&impl->mSubEndpoints, "mSubEndpoints");
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create subscribing endpoints");
      free(impl);
      return status;
   }
   impl->mSubsLock = wlock_create();
   __sync_and_and_fetch(&impl->mSubUid, 0);

   // generate inbox subject
   impl->mUuid = zmqBridge_generateUuid();
   if (impl->mUuid == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "Failed to generate safe wUuid");
      free(impl);
      return MAMA_STATUS_SYSTEM_ERROR;
   }
   char temp[ZMQ_INBOX_SUBJECT_SIZE +1];
   sprintf(temp, "%s.%s", ZMQ_REPLYHANDLE_PREFIX, impl->mUuid);
   impl->mInboxSubject = strdup(temp);

   wInterlocked_initialize(&impl->mNamingConnected);

   // connect/bind/subscribe/etc. all sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_init(impl));

   // start the dispatch thread
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_start(impl));

   *result = (transportBridge) impl;
   impl->mIsValid = 1;

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

   // TODO: is this good enough?
   // make sure we don't delete the transport out from under publishEndpoints, if it's running
   if ((impl->mIsNaming == 1) && (wInterlocked_read(&impl->mNamingConnected) == 0)) {
      // this can only be true if publishEndpoints is running in a separate thread(s)
      // if so, give the thread a chance to terminate
      usleep(impl->mNamingConnectInterval * 2);
   }

   wInterlocked_destroy(&impl->mNamingConnected);

   // close sockets
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataSub);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataPub);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlSub);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlPub);
   if (impl->mIsNaming == 1) {
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingSub);
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingPub);
   }

   // stop the monitor thread
   if (impl->mSocketMonitor != 0) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_stopMonitor(impl));
   }

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

   free((void*) impl->mUuid);
   free((void*) impl->mInboxSubject);
   free((void*) impl->mPubEndpoint);

   for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
      free((void*) impl->mNamingAddress[i]);
   }

   uint32_t peers = wtable_get_count(impl->mPeers);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Peers = %lu", peers);
   wtable_free_all(impl->mPeers);
   wtable_destroy(impl->mPeers);

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Naming messages = %ld", impl->mNamingMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Normal messages = %ld", impl->mNormalMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Subscription messages = %ld", impl->mSubMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Inbox messages = %ld", impl->mInboxMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Control messages = %ld", impl->mControlMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Polls = %ld", impl->mPolls);

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
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlSub, ZMQ_PULL, "controlSub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqControlSub,  ZMQ_CONTROL_ENDPOINT, NULL));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlPub, ZMQ_PUSH, "controlPub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqControlPub,  ZMQ_CONTROL_ENDPOINT, -1, 0));

   // create data sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataPub, ZMQ_PUB_TYPE, "dataPub", impl->mSocketMonitor));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataSub, ZMQ_SUB_TYPE, "dataSub", impl->mSocketMonitor));
   // set socket options as per mama.properties etc.
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setCommonSocketOptions(impl->mName, &impl->mZmqDataPub));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setCommonSocketOptions(impl->mName, &impl->mZmqDataSub));

   // subscribe to inbox subjects
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSub.mSocket, impl->mInboxSubject));

   if (impl->mIsNaming == 1) {
      // when using naming protocol, we want to stop data socket reconnecting on certain errors
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_stopReconnectOnError(&impl->mZmqDataSub, impl->mReconnectOptions));
      // create naming sockets
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingPub, ZMQ_PUB_TYPE, "namingPub", impl->mSocketMonitor));
      // namingPub wants to stop reconnecting on error
      // (if nsd crashes, namingPub will reconnect to address in welcome msg)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_stopReconnectOnError(&impl->mZmqNamingPub, impl->mReconnectOptions));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingSub, ZMQ_SUB_TYPE, "namingSub", impl->mSocketMonitor));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqNamingSub.mSocket, ZMQ_NAMING_PREFIX));
   }

   // start the monitor thread (before any connects/binds)
   if (impl->mSocketMonitor != 0) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_startMonitor(impl));
   }

   if (impl->mIsNaming == 1) {
      // bind data pub socket & get endpoint
      char endpointAddress[ZMQ_MAX_ENDPOINT_LENGTH +1];
      sprintf(endpointAddress, "tcp://%s:*", impl->mPublishAddress);
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqDataPub,  endpointAddress, &impl->mPubEndpoint));
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Bound publish socket to:%s ", impl->mPubEndpoint);

      // connect sub socket to proxy
      for (int i = 0; (i < ZMQ_MAX_NAMING_URIS) && (impl->mNamingAddress[i] != NULL); ++i) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingSub, impl->mNamingAddress[i],
            impl->mReconnectInterval, impl->mHeartbeatInterval));
         MAMA_LOG(log_level_naming, "Connecting naming subscriber to: %s", impl->mNamingAddress[i]);
      }
   }
   else {
      // non-naming style
      for (int i = 0; (i < ZMQ_MAX_OUTGOING_URIS) && (NULL != impl->mOutgoingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataPub,
         impl->mOutgoingAddress[i], ZMQ_TPORT_DIRECTION_OUTGOING,
         impl->mReconnectInterval, impl->mHeartbeatInterval));

         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting data publisher socket to subscriber:%s", impl->mOutgoingAddress[i]);
      }

      for (int i = 0; (i < ZMQ_MAX_INCOMING_URIS) && (NULL != impl->mIncomingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataSub,
            impl->mIncomingAddress[i], ZMQ_TPORT_DIRECTION_INCOMING,
            impl->mReconnectInterval, impl->mHeartbeatInterval));

         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Connecting data subscriber socket to publisher:%s", impl->mIncomingAddress[i]);
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

   // dont proceed until we are connected to proxy?
   if ( (impl->mIsNaming == 1) && (impl->mNamingWaitForConnect == 1) ) {
      // wait for welcome msg from proxy to trigger publishEndpoints, which in turn
      // causes mNamingConnected to be set on receipt of our naming msg
      int retries = impl->mNamingConnectRetries;
      while ((--retries > 0) && (wInterlocked_read(&impl->mNamingConnected) != 1)) {
         usleep(impl->mNamingConnectInterval);
      }
      if (wInterlocked_read(&impl->mNamingConnected) != 1) {
         MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "Failed connecting to naming service after %d retries", impl->mNamingConnectRetries);
         return MAMA_STATUS_TIMEOUT;
      }
   }

   return MAMA_STATUS_OK;
}

// stops the main dispatch thread
mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl)
{
   // disable beaconing if applicable
   wInterlocked_set(0, &impl->mBeaconInterval);

   // make sure that transport has started before we try to stop it
   // (prevents a race condition on mIsDispatching)
   wsem_wait(&impl->mIsReady);

   // send disconnect msg to peers
   if (impl->mIsNaming == 1) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'D'));
   }

   zmqControlMsg msg;
   memset(&msg, '\0', sizeof(msg));
   msg.command = 'X';
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(impl, &msg, sizeof(msg)));

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Waiting on dispatch thread to terminate.");
   int rc = wthread_join(impl->mOmzmqDispatchThread, NULL);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "join of dispatch thread failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }

   mama_status status = impl->mOmzmqDispatchStatus;
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Rejoined with status: %s.", mamaStatus_stringForStatus(status));

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// dispatch functions

// main thread that reads directly off zmq sockets and calls one of the dispatchXxxMsg methods
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

   // lock (non-thread-safe) sockets
   wlock_lock(impl->mZmqDataSub.mLock);
   if (impl->mIsNaming == 1) {
      wlock_lock(impl->mZmqNamingSub.mLock);
   }

   // set next beacon time
   uint64_t lastBeacon = 0;
   uint64_t nextBeacon = -1;
   if (wInterlocked_read(&impl->mBeaconInterval) > 0) {
      nextBeacon = getMillis() + wInterlocked_read(&impl->mBeaconInterval);
   }

   // The naming socket is defined last so it can be excluded from the list if we're not running a
   // "naming" transport.
   #define CONTROL_SOCKET  0
   #define NAMING_SOCKET   2
   #define DATA_SOCKET     1
   zmq_pollitem_t items[] = {
      { impl->mZmqControlSub.mSocket, 0, ZMQ_POLLIN , 0},
      { impl->mZmqDataSub.mSocket,    0, ZMQ_POLLIN , 0},
      { impl->mZmqNamingSub.mSocket,  0, ZMQ_POLLIN , 0}
   };

   // Following is the transport's main dispatch loop -- it runs "forever"
   // i.e., until mIsDispatching is set to zero, in dispatchControlMsg, on receipt of an exit ("X") command.
   while (1 == wInterlocked_read(&impl->mIsDispatching)) {

      // If we're beaconing, break out of the zmq_poll when it's time to send a beacon.
      long timeout = -1;
      if (wInterlocked_read(&impl->mBeaconInterval) > 0) {
         timeout = nextBeacon - lastBeacon;
      }
      int rc = zmq_poll(items, (impl->mIsNaming == 1) ? 3 : 2, timeout);
      if ((rc < 0) && (errno != EINTR)) {
         MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "zmq_poll failed  %d(%s)", errno, zmq_strerror(errno));
         continue;
      }
      ++impl->mPolls;

      // TODO: is this the best place?
      // Is it time to send a beacon? Note that doing this here means that once there is activity
      // on *any* socket, we won't send another beacon until *all* sockets have been drained.
      if (nextBeacon > 0) {
         // if we're shutting down, beaconInterval will be 0, so dont send it
         uint32_t beaconInterval = wInterlocked_read(&impl->mBeaconInterval);
         if (beaconInterval > 0) {
            uint64_t now = getMillis();
            if (now >= nextBeacon) {
               zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'c');
               lastBeacon = now;
               nextBeacon = now + beaconInterval;
            }
         }
      }

      // This implementation drains each of the sockets (control, naming and data) in turn before reading from
      // the next -- that is, it is not "fair", and it is theoretically possible for an earlier socket to starve
      // later socket(s).  In practice this should not be a problem, as there should be little traffic on the
      // control and naming sockets, and esp. for the control socket, its messages are more "important", as
      // they affect the state of the transport.

      // drain command msgs
      while (items[CONTROL_SOCKET].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqControlSub.mSocket, ZMQ_DONTWAIT);
         if (size <= 0) {
            items[CONTROL_SOCKET].revents = 0;
            if (errno != EAGAIN) {
               MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no command msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
            }
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchControlMsg(impl, &zmsg);
         }
      }

      // drain naming msgs
      while (items[NAMING_SOCKET].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqNamingSub.mSocket, ZMQ_DONTWAIT);
         if (size <= 0) {
            items[NAMING_SOCKET].revents = 0;
            if (errno != EAGAIN) {
               MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no naming msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
            }
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchNamingMsg(impl, &zmsg);
         }
      }

      // drain normal (data) msgs
      while (items[DATA_SOCKET].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqDataSub.mSocket, ZMQ_DONTWAIT);
         if (size <= 0) {
            items[DATA_SOCKET].revents = 0;
            if (errno != EAGAIN) {
               MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll returned w/ZMQ_POLLIN, but no normal msg - errorno %d(%s)", zmq_errno(), zmq_strerror(zmq_errno()));
            }
         }
         else {
            zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         }
      }
   }

   zmq_msg_close(&zmsg);

   // unlock sockets
   wlock_unlock(impl->mZmqDataSub.mLock);
   if (impl->mIsNaming == 1) {
      wlock_unlock(impl->mZmqNamingSub.mLock);
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
      return zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSub.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'U') {
      // unsubscribe
      return zmqBridgeMamaTransportImpl_unsubscribe(impl->mZmqDataSub.mSocket, pMsg->arg1);
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

   MAMA_LOG(getNamingLogLevel(pMsg->mType), "Received endpoint msg: type=%c prog=%s host=%s uuid=%s pid=%ld topic=%s pub=%s", pMsg->mType, pMsg->mProgName, pMsg->mHost, pMsg->mUuid, pMsg->mPid, pMsg->mTopic, pMsg->mEndPointAddr);

   if ((pMsg->mType == 'C') || (pMsg->mType == 'c')) {
      // connect

      zmqNamingMsg* pOrigMsg = wtable_lookup(impl->mPeers, pMsg->mUuid);
      if (pOrigMsg == NULL) {
         if (pMsg->mType == 'c') {
            // found peer via beacon message
            MAMA_LOG(log_level_beacon, "Received endpoint msg: type=%c prog=%s host=%s uuid=%s pid=%ld topic=%s pub=%s", pMsg->mType, pMsg->mProgName, pMsg->mHost, pMsg->mUuid, pMsg->mPid, pMsg->mTopic, pMsg->mEndPointAddr);
         }

         // we've never seen this peer before, so connect (sub => pub)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataSub, pMsg->mEndPointAddr, impl->mReconnectInterval, impl->mHeartbeatInterval));

         // send a discovery msg whenever we see a peer we haven't seen before
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'C'));

         // save peer in table
         pOrigMsg = malloc(sizeof(zmqNamingMsg));
         if (NULL == pOrigMsg) return MAMA_STATUS_NOMEM;
         memcpy(pOrigMsg, pMsg, sizeof(zmqNamingMsg));
         wtable_insert(impl->mPeers, pOrigMsg->mUuid, pOrigMsg);

         MAMA_LOG(log_level_naming, "Connecting to publisher at endpoint:%s", pMsg->mEndPointAddr);
      }

      // is this our msg? if so, we know we're connected to proxy
      if ((wInterlocked_read(&impl->mNamingConnected) != 1) && (strcmp(pMsg->mUuid, impl->mUuid) == 0)) {
         MAMA_LOG(log_level_naming, "Got own endpoint msg -- signaling");
         wInterlocked_set(1, &impl->mNamingConnected);
      }
   }
   else if (pMsg->mType == 'D') {
      // disconnect

      #undef USEAFTERFREE_REPRO
      #ifdef USEAFTERFREE_REPRO
      // TODO: do we want to wait until we receive our own disconnect msg before we shut down?
      // is this our msg?
      if (strcmp(pMsg->mUuid, impl->mUuid) == 0) {
         MAMA_LOG(log_level_naming, "Got own endpoint msg -- ignoring");
         // NOTE: dont disconnect from self
         return MAMA_STATUS_OK;
      }
      #endif

      // remove endpoint from the table
      zmqNamingMsg* pOrigMsg = wtable_remove(impl->mPeers, pMsg->mUuid);
      if (pOrigMsg != NULL) {
         free(pOrigMsg);
      }

      // zmq will silently ignore multiple attempts to connect to the same endpoint (see https://github.com/zeromq/libzmq/issues/788)
      // so, we want to explicitly disconnect from sockets on normal shutdown so that zmq will know that the endpoint is
      // disconnected and will *not* ignore a subsequent request to connect to it
      // Note that we ignore the return value -- any errors are reported in disconnectSocket
      // (which will happen if peer has already exited, for example)
      zmqBridgeMamaTransportImpl_disconnectSocket(&impl->mZmqDataSub, pMsg->mEndPointAddr);

      // TODO: do we even need this?  only matters for transports that *never* publish data
      #define KICK_DATAPUB
      #ifdef KICK_DATAPUB
      // In cases where a process doesn't send messages via dataPub socket, the socket must have an opportunity to
      // clean up resources (e.g., disconnected endpoints), and this is as good a place as any.
      // For more info see https://github.com/zeromq/libzmq/issues/3186
      wlock_lock(impl->mZmqDataPub.mLock);
      size_t fd_size = sizeof(uint32_t);
      uint32_t fd;
      zmq_getsockopt (impl->mZmqDataPub.mSocket, ZMQ_EVENTS, &fd, &fd_size);
      wlock_unlock(impl->mZmqDataPub.mLock);
      #endif

      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Disconnecting data sockets from publisher:%s", pMsg->mEndPointAddr);
   }
   else if (pMsg->mType == 'W') {
      // welcome msg - naming subscriber is connected

      // connect to proxy
      mama_status status = zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingPub, pMsg->mEndPointAddr,
         impl->mReconnectInterval, impl->mHeartbeatInterval);
      if (status != MAMA_STATUS_OK) {
         MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "connect to naming endpoint(%s) failed %d(%s)", pMsg->mEndPointAddr, status, mamaStatus_stringForStatus(status));
         return MAMA_STATUS_PLATFORM;
      }

      // start thread to publish naming msg
      wthread_t publishThread;
      int rc = wthread_create(&publishThread, NULL, zmqBridgeMamaTransportImpl_publishEndpoints, impl);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "create of endpoint publish thread failed %d(%s)", rc, strerror(rc));
         return MAMA_STATUS_PLATFORM;
      }
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
      MAMA_LOG(log_level_inbox, "discarding uninteresting message for subject %s", subject);
      return MAMA_STATUS_NOT_FOUND;
   }

   void* queue = inbox->mZmqQueue;
   // at this point, we dont care if the inbox is deleted (as long as the queue remains)
   wlock_unlock(impl->mInboxesLock);

   // queue up message, callback will free
   zmqTransportMsg tmsg;
   tmsg.mTransport = impl;
   strcpy(tmsg.mEndpointIdentifier, inboxName);
   zmq_msg_init(&tmsg.mZmsg);
   zmq_msg_copy(&tmsg.mZmsg, zmsg);
   zmqBridgeMamaQueue_enqueueMsg(queue, zmqBridgeMamaTransportImpl_inboxCallback, &tmsg);

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
         // queue up message, callback will free
         zmqTransportMsg tmsg;
         tmsg.mTransport = impl;
         strcpy(tmsg.mEndpointIdentifier, subscription->mEndpointIdentifier);
         zmq_msg_init(&tmsg.mZmsg);
         zmq_msg_copy(&tmsg.mZmsg, zmsg);
         zmqBridgeMamaQueue_enqueueMsg(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_subCallback, &tmsg);
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

   // queue up message, callback will free
   zmqTransportMsg tmsg;
   tmsg.mTransport = subscription->mTransport;
   strcpy(tmsg.mEndpointIdentifier, subscription->mEndpointIdentifier);
   zmq_msg_init(&tmsg.mZmsg);
   zmq_msg_copy(&tmsg.mZmsg, closure->zmsg);
   zmqBridgeMamaQueue_enqueueMsg(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_wcCallback, &tmsg);
}


///////////////////////////////////////////////////////////////////////////////
// The ...Callback functions are dispatched from the queue/dispatcher associated with the subscription or inbox

///////////////////////////////////////////////////////////////////////////////
// Called when inbox reply message removed from queue by callback thread
// NOTE: Needs to check inbox, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the inbox is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_inboxCallback(mamaQueue queue, void* closure)
{
   zmqTransportMsg* tmsg = (zmqTransportMsg*) closure;
   zmqTransportBridge* impl = (zmqTransportBridge*) tmsg->mTransport;

   // find the inbox
   wlock_lock(impl->mInboxesLock);
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, tmsg->mEndpointIdentifier);
   wlock_unlock(impl->mInboxesLock);
   if (inbox == NULL) {
      MAMA_LOG(log_level_inbox, "discarding uninteresting message for inbox %s", tmsg->mEndpointIdentifier);
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
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, &tmsg->mZmsg, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   zmqBridgeMamaInboxImpl_onMsg(NULL, tmpMsg, inbox, NULL);

exit:
   zmq_msg_close(&tmsg->mZmsg);

   return;
}


///////////////////////////////////////////////////////////////////////////////
// Called when regular subscription message removed from queue by callback thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the subscription is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_subCallback(mamaQueue queue, void* closure)
{
   zmqTransportMsg* tmsg = (zmqTransportMsg*) closure;
   const char *subject = (const char*) zmq_msg_data(&tmsg->mZmsg);

   // find the subscription based on its identifier
   zmqSubscription* subscription = NULL;
   endpointPool_getEndpointByIdentifiers(tmsg->mTransport->mSubEndpoints, subject,
                                         tmsg->mEndpointIdentifier, (endpoint_t*) &subscription);

   /* Can't do anything without a subscriber */
   if (NULL == subscription) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "No endpoint found for topic %s with id %s", subject, tmsg->mEndpointIdentifier);
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
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, &tmsg->mZmsg, tmpMsg);
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
   zmq_msg_close(&tmsg->mZmsg);

   return;
}


///////////////////////////////////////////////////////////////////////////////
// Called when wildcard subscription message removed from queue by callback thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
// Note also that if the subscription is found, it is guaranteed to exist for the duration of the function,
// so long as all deletes are done from this thread (the callback thread), which is guaranteed by MME.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_wcCallback(mamaQueue queue, void* closure)
{
   zmqTransportMsg* tmsg = (zmqTransportMsg*) closure;
   const char *subject = (const char*) zmq_msg_data(&tmsg->mZmsg);

   // is this subscription still in the list?
   zmqFindWildcardClosure findClosure;
   findClosure.mEndpointIdentifier = tmsg->mEndpointIdentifier;
   findClosure.mSubscription = NULL;
   list_for_each(tmsg->mTransport->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_findWildcard, &findClosure);
   if (findClosure.mSubscription == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "No endpoint found for topic %s with id %s", subject, tmsg->mEndpointIdentifier);
      goto exit;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found wildcard subscriber for topic %s with id %s", subject, tmsg->mEndpointIdentifier);
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
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, &tmsg->mZmsg, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
   }
   else {
      status = mamaSubscription_processWildCardMsg(subscription->mMamaSubscription, tmpMsg, subject, subscription->mClosure);
      if (MAMA_STATUS_OK != status) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "mamaSubscription_processMsg() failed. [%s]", mamaStatus_stringForStatus(status));
      }
   }

exit:
   zmq_msg_close(&tmsg->mZmsg);

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
   MAMA_LOG(log_level_inbox, "mamaInbox=%p,replyAddr=%s", inbox->mParent, inbox->mReplyHandle);

   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_insert(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX], inbox) >= 0 ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   if (status != MAMA_STATUS_OK) {
      MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "failed to register inbox (%s)", inbox->mReplyHandle);
   }

   return status;
}


mama_status zmqBridgeMamaTransportImpl_unregisterInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   MAMA_LOG(log_level_inbox, "mamaInbox=%p,replyAddr=%s", inbox->mParent, inbox->mReplyHandle);

   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_remove(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX]) == inbox ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   if (status != MAMA_STATUS_OK) {
      MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "failed to unregister inbox (%s)", inbox->mReplyHandle);
   }

   return status;
}


///////////////////////////////////////////////////////////////////////////////
// zmq socket functions
mama_status zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* socket, int type, const char* name, int monitor)
{
   void* temp = zmq_socket(zmqContext, type);
   if (temp == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket failed %d(%s)", zmq_errno(), zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   socket->mSocket = temp;
   socket->mLock = wlock_create();
   socket->mMonitor = monitor;

   // we dont use router/dealer or req/rep, so we hijack the identity property to set a name to make debugging easier
   if (NULL != name) {
      CALL_ZMQ_FUNC(zmq_setsockopt(socket->mSocket, ZMQ_IDENTITY, name, strlen(name) +1));
   }

   if ((socket->mMonitor != 0) && (name != NULL)) {
      char endpoint[ZMQ_MAX_ENDPOINT_LENGTH +1];
      sprintf(endpoint, "inproc://%s", name);
      CALL_ZMQ_FUNC(zmq_socket_monitor_versioned(socket->mSocket, endpoint, get_zmqEventMask(gMamaLogLevel), 2, ZMQ_PAIR));
   }

   // do this here, rather than when closing the socket
   // (see https://github.com/zeromq/libzmq/issues/3252)
   int linger = 0;
   if (name && (strcmp(name, "namingPub") == 0)) {
      // need to linger briefly to publish disconnect msg.
      linger = 100;
   }
   CALL_ZMQ_FUNC(zmq_setsockopt(socket->mSocket, ZMQ_LINGER, &linger, sizeof(linger)));

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "(%p, %d) succeeded", socket->mSocket, type);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri, int reconnectInterval, int heartbeatInterval)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   // set heartbeat interval
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_HEARTBEAT_IVL, &heartbeatInterval, sizeof(heartbeatInterval));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p, ZMQ_HEARTBEAT_IVL, %d) failed: %d(%s)", socket->mSocket, heartbeatInterval, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
      goto cleanup;
   }

   // set reconnect interval
   rc = zmq_setsockopt(socket->mSocket, ZMQ_RECONNECT_IVL, &reconnectInterval, sizeof(reconnectInterval));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p, ZMQ_RECONNECT_IVL, %d) failed: %d(%s)", socket->mSocket, reconnectInterval, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
      goto cleanup;
   }

   // connect socket
   rc = zmq_connect(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect(%p, %s) failed: %d(%s)", socket->mSocket, uri, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

cleanup:
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

   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   // bind socket
   int rc = zmq_bind(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_bind(%p, %s) failed %d(%s)", socket->mSocket, uri, errno, zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
      goto cleanup;
   }

   // get endpoint name
   if (endpointName != NULL) {
      char temp[ZMQ_MAX_ENDPOINT_LENGTH +1];
      size_t tempSize = sizeof(temp);
      rc = zmq_getsockopt(socket->mSocket, ZMQ_LAST_ENDPOINT, temp, &tempSize);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_getsockopt(%p) failed trying to get last endpoint %d(%s)", socket->mSocket, errno, zmq_strerror(errno));
         status = MAMA_STATUS_PLATFORM;
      }
      else {
         *endpointName = strdup(temp);
      }
   }

cleanup:
   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_unbindSocket(zmqSocket* socket, const char* uri)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   int rc = zmq_unbind(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_unbind(%p, %s) failed: %d(%s)", socket->mSocket, uri, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_destroySocket(zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;
   int rc;

   wlock_lock(socket->mLock);

   if (socket->mMonitor != 0) {
      rc = zmq_socket_monitor_versioned(socket->mSocket, NULL, ZMQ_EVENT_ALL_V2, 2, ZMQ_PAIR);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket_monitor(%p) failed trying to disable monitoring %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
         status = MAMA_STATUS_PLATFORM;
      }
   }

   rc = zmq_close(socket->mSocket);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_close(%p) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   wlock_unlock(socket->mLock);
   wlock_destroy(socket->mLock);

   return status;
}

// sets the socket to stop trying to reconnect if it gets an error condition on
// reconnect (e.g., ECONNREFUSED)
mama_status zmqBridgeMamaTransportImpl_stopReconnectOnError(zmqSocket* socket, int reconnectOptions)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_RECONNECT_STOP, &reconnectOptions, sizeof(reconnectOptions));
   if (rc != 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   wlock_unlock(socket->mLock);

   return status;
}


// NOTE: direction is only relevant for ipc transports, for others it is
// inferred from endpoint string (wildcard => bind, non-wildcard => connect)
mama_status zmqBridgeMamaTransportImpl_bindOrConnect(void* socket, const char* uri, zmqTransportDirection direction, int reconnectInterval, int heartbeatInterval)
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
      default:
         return MAMA_STATUS_INVALID_ARG;
   }

   /* If this is a binding transport */
   if (isBinding) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(socket, uri, NULL));
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully bound socket to: %s", uri);
   }
   else {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(socket, uri, reconnectInterval, heartbeatInterval));
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully connected socket to: %s", uri);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_setCommonSocketOptions(const char* name, zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   int value = 0;
   int rc;
   rc = zmq_setsockopt(socket->mSocket, ZMQ_RCVHWM, &value, sizeof(value));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p, ZMQ_RCVHWM, %d) failed: %d(%s)", socket->mSocket, value, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   rc = zmq_setsockopt(socket->mSocket, ZMQ_SNDHWM, &value, sizeof(value));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p, ZMQ_SNDHWM, %d) failed: %d(%s)", socket->mSocket, value, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   value = 200;
   rc = zmq_setsockopt(socket->mSocket, ZMQ_BACKLOG, &value, sizeof(value));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%p, ZMQ_BACKLOG, %d) failed: %d(%s)", socket->mSocket, value, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   wlock_unlock(socket->mLock);

   return status;
}


///////////////////////////////////////////////////////////////////////////////
// These subscribe/unsubscribe methods operate directly on the zmq socket, and as such must only
// be called from the dispatch thread.
// To subscribe/unsubscribe from any other thread, you need to use the zmqBridgeMamaSubscriptionImpl_subscribe
// function, which posts a message to the control socket.
// NOTE: The subscribe method is controlled by the USE_XSUB preprocessor symbol.
mama_status zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %p subscribing to %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\1', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_SUBSCRIBE, topic, strlen(topic)));
   #endif

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_unsubscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %p unsubscribing from %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\0', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, topic, strlen(topic)));
   #endif

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// control msgs
mama_status zmqBridgeMamaTransportImpl_sendCommand(zmqTransportBridge* impl, zmqControlMsg* msg, int msgSize)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "command=%c arg1=%s", msg->command, msg->arg1);

   wlock_lock(impl->mZmqControlPub.mLock);
   int i = zmq_send(impl->mZmqControlPub.mSocket, msg, msgSize, 0);
   wlock_unlock(impl->mZmqControlPub.mLock);

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
   mama_status status = MAMA_STATUS_OK;

   // publish our endpoint
   zmqNamingMsg msg;
   memset(&msg, '\0', sizeof(msg));
   strcpy(msg.mTopic, ZMQ_NAMING_PREFIX);
   msg.mType = command;
   #if defined __APPLE__
   wmStrSizeCpy(msg.mProgName, getprogname(), sizeof(msg.mProgName));
   #else
   wmStrSizeCpy(msg.mProgName, program_invocation_short_name, sizeof(msg.mProgName));
   #endif
   gethostname(msg.mHost, sizeof(msg.mHost));
   msg.mPid = getpid();
   strcpy(msg.mUuid, impl->mUuid);
   strcpy(msg.mEndPointAddr, impl->mPubEndpoint);

   wlock_lock(impl->mZmqNamingPub.mLock);
   int i = zmq_send(impl->mZmqNamingPub.mSocket, &msg, sizeof(msg), 0);
   if (i != sizeof(msg)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to publish endpoints: prog=%s host=%s pid=%ld pub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mEndPointAddr);
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      MAMA_LOG(getNamingLogLevel(msg.mType), "Published endpoint msg: type=%c prog=%s host=%s uuid=%s pid=%ld topic=%s pub=%s", msg.mType, msg.mProgName, msg.mHost, msg.mUuid, msg.mPid, msg.mTopic, msg.mEndPointAddr);
   }
   wlock_unlock(impl->mZmqNamingPub.mLock);

   return status;
}


// publishes endpoint message continuously until we get it back
void* zmqBridgeMamaTransportImpl_publishEndpoints(void* closure)
{
   zmqTransportBridge* impl = (zmqTransportBridge*) closure;

   wInterlocked_set(0, &impl->mNamingConnected);
   int retries = impl->mNamingConnectRetries;
   while ( (--retries > 0) && (1 == wInterlocked_read(&impl->mIsDispatching)) ) {
      zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl, 'C');
      if (wInterlocked_read(&impl->mNamingConnected) == 1) {
         MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Successfully connected to proxy");
         return NULL;
      }

      usleep(impl->mNamingConnectInterval);
   }

   return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// socket monitor
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
         { dataPubMonitor,                   0, ZMQ_POLLIN , 0},
         { dataSubMonitor,                   0, ZMQ_POLLIN , 0},
         { namingPubMonitor,                 0, ZMQ_POLLIN , 0},
         { namingSubMonitor,                 0, ZMQ_POLLIN , 0},
         { impl->mZmqMonitorSub.mSocket,     0, ZMQ_POLLIN , 0},
      };
      int rc = zmq_poll(items, 5, -1);
      if ((rc < 0) && (errno != EINTR)) {
         MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "zmq_poll failed  %d(%s)", errno, zmq_strerror(errno));
         continue;
      }

      if (items[0].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent_v2(dataPubMonitor, "dataPub");
      }
      if (items[1].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent_v2(dataSubMonitor, "dataSub");
      }
      if (items[2].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent_v2(namingPubMonitor, "namingPub");
      }
      if (items[3].revents & ZMQ_POLLIN) {
         zmqBridgeMamaTransportImpl_monitorEvent_v2(namingSubMonitor, "namingSub");
      }
      if (items[4].revents & ZMQ_POLLIN) {
         // nothing to do -- just loop around and check mIsMonitoring flag
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

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqMonitorSub, ZMQ_SERVER, "monitorSub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqMonitorSub,  ZMQ_MONITOR_ENDPOINT, NULL));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqMonitorPub, ZMQ_CLIENT, "monitorPub", 0));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqMonitorPub,  ZMQ_MONITOR_ENDPOINT, 0, 0));

   /* Set the transport bridge mIsMonitoring to true. */
   wInterlocked_initialize(&impl->mIsMonitoring);
   wInterlocked_set(1, &impl->mIsMonitoring);

   /* Initialize monitor thread */
   int rc = wthread_create(&(impl->mOmzmqMonitorThread), NULL, zmqBridgeMamaTransportImpl_monitorThread, impl);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "create of monitor thread failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_stopMonitor(zmqTransportBridge* impl)
{
   wInterlocked_set(0, &impl->mIsMonitoring);

   // send command to force zmq_poll call to return and eval mIsMonitoring
   zmqControlMsg msg;
   memset(&msg, '\0', sizeof(msg));
   msg.command = 'X';
   int i = zmq_send(impl->mZmqMonitorPub.mSocket, &msg, sizeof(msg), 0);
   if (i <= 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_send failed  %d(%s)", errno, zmq_strerror(errno));
   }

   int rc = wthread_join(impl->mOmzmqMonitorThread, NULL);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "join of monitor thread failed %d(%s)", rc, strerror(rc));
   }

   // TODO: resolve https://github.com/zeromq/libzmq/issues/3152
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disconnectSocket(&impl->mZmqMonitorPub, ZMQ_MONITOR_ENDPOINT));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqMonitorPub));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_unbindSocket(&impl->mZmqMonitorSub, ZMQ_MONITOR_ENDPOINT));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqMonitorSub));

   return MAMA_STATUS_OK;
}

uint64_t zmqBridgeMamaTransportImpl_monitorEvent_v2(void *socket, const char* socketName)
{
    //  First frame in message contains event number
    zmq_msg_t msg;
    zmq_msg_init (&msg);
    if (zmq_msg_recv (&msg, socket, 0) == -1)
        return -1;              //  Interrupted, presumably
    //assert (zmq_msg_more (&msg));

    uint64_t event;
    memcpy (&event, zmq_msg_data (&msg), sizeof (event));
    zmq_msg_close (&msg);

    //  Second frame in message contains the number of values
    zmq_msg_init (&msg);
    if (zmq_msg_recv (&msg, socket, 0) == -1)
        return -1;              //  Interrupted, presumably
    //assert (zmq_msg_more (&msg));

    uint64_t value_count;
    memcpy (&value_count, zmq_msg_data (&msg), sizeof (value_count));
    zmq_msg_close (&msg);

    uint64_t value = 0;
    for (uint64_t i = 0; i < value_count; ++i) {
        //  Subsequent frames in message contain event values
        zmq_msg_init (&msg);
        if (zmq_msg_recv (&msg, socket, 0) == -1)
            return -1;              //  Interrupted, presumably
        //assert (zmq_msg_more (&msg));

        memcpy (&value, zmq_msg_data (&msg), sizeof (value));
        zmq_msg_close (&msg);
    }

    //  Second-to-last frame in message contains local address
    zmq_msg_init (&msg);
    if (zmq_msg_recv (&msg, socket, 0) == -1)
        return -1;              //  Interrupted, presumably
    //assert (zmq_msg_more (&msg));
    char local_address[ZMQ_MAX_ENDPOINT_LENGTH +1];
    memset(local_address, '\0', sizeof(local_address));
    uint8_t *data1 = (uint8_t *) zmq_msg_data (&msg);
    size_t size1 = zmq_msg_size (&msg);
    memcpy (local_address, data1, size1);
    zmq_msg_close (&msg);

    //  Last frame in message contains remote address
    zmq_msg_init (&msg);
    if (zmq_msg_recv (&msg, socket, 0) == -1)
        return -1;              //  Interrupted, presumably
    //assert (!zmq_msg_more (&msg));
    char remote_address[ZMQ_MAX_ENDPOINT_LENGTH +1];
    memset(remote_address, '\0', sizeof(remote_address));
    uint8_t *data2 = (uint8_t *) zmq_msg_data (&msg);
    size_t size2 = zmq_msg_size (&msg);
    memcpy (remote_address, data2, size2);
    zmq_msg_close (&msg);

   // how should this be logged?
   const char* eventName = get_zmqEventName(event);
   int logLevel = get_zmqEventLogLevel(event);
   MAMA_LOG(logLevel, "name:%s event:%s value:%llu local:%s remote:%s", socketName, eventName, value, local_address, remote_address);

   return 0;
}
