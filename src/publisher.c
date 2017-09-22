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

#include <string.h>

#include <mama/mama.h>
#include <mama/inbox.h>
#include <mama/publisher.h>
#include <bridge.h>
#include <inboximpl.h>
#include <msgimpl.h>
#include "transport.h"
#include "zmqdefs.h"
#include "msg.h"
#include "inbox.h"
#include "subscription.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include <errno.h>
#include <zmq.h>

/*=========================================================================
 =                Typedefs, structs, enums and globals                   =
 =========================================================================*/

typedef struct zmqPublisherBridge {
   zmqTransportBridge*     mTransport;
   const char*             mTopic;
   const char*             mSource;
   const char*             mRoot;
   const char*             mSubject;
   msgBridge               mMamaBridgeMsg;
   mamaPublisher           mParent;
   mamaPublisherCallbacks  mCallbacks;
   void*                   mCallbackClosure;
} zmqPublisherBridge;

/*=========================================================================
 =                  Private implementation prototypes                    =
 =========================================================================*/

/**
 * When a zmq publisher is created, it calls this function to generate a
 * standard subject key using zmqBridgeMamaSubscriptionImpl_generateSubjectKey
 * with different parameters depending on if it's a market data publisher,
 * basic publisher or a data dictionary publisher.
 *
 * @param msg   The MAMA message to enqueue for sending.
 * @param url   The URL to eneueue the message for sending to.
 * @param impl  The related zmq publisher bridge.
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
static mama_status zmqBridgeMamaPublisherImpl_buildSendSubject(zmqPublisherBridge* impl);


/*=========================================================================
 =               Public interface implementation functions               =
 =========================================================================*/

mama_status zmqBridgeMamaPublisher_createByIndex(publisherBridge* result, mamaTransport tport, int tportIndex,
   const char* topic, const char* source, const char* root, mamaPublisher parent)
{
   if (NULL == result || NULL == tport || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqTransportBridge* transport = zmqBridgeMamaTransportImpl_getTransportBridge(tport);
   if (NULL == transport) {
      MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "Could not find transport.");
      return MAMA_STATUS_NULL_ARG;
   }

   zmqPublisherBridge* impl = (zmqPublisherBridge*) calloc(1, sizeof(zmqPublisherBridge));
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not allocate mem publisher.");
      return MAMA_STATUS_NOMEM;
   }

   /* Initialize the publisher members */
   impl->mTransport = transport;
   impl->mParent    = parent;

   /* Create an underlying bridge message with no parent to be used in sends */
   mama_status status = zmqBridgeMamaMsgImpl_createMsgOnly(&impl->mMamaBridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not create zmq bridge message for publisher: %s.", mamaStatus_stringForStatus(status));
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   impl->mTopic = topic;
   impl->mSource = source;
   impl->mRoot = root;

   /* Generate a topic name based on the publisher details */
   status = zmqBridgeMamaPublisherImpl_buildSendSubject(impl);

   /* Populate the publisherBridge pointer with the publisher implementation */
   *result = (publisherBridge) impl;

   return status;
}

mama_status zmqBridgeMamaPublisher_destroy(publisherBridge publisher)
{
   if (NULL == publisher) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   /* Take a copy of the callbacks - we'll need those */
   mamaPublisherCallbacks callbacks = impl->mCallbacks;
   mamaPublisher parent = impl->mParent;
   void* closure = impl->mCallbackClosure;

   if (NULL != impl->mSubject) {
      free((void*) impl->mSubject);
   }
   if (NULL != impl->mMamaBridgeMsg) {
      zmqBridgeMamaMsg_destroy(impl->mMamaBridgeMsg, 0);
   }

   free(impl);

   if (NULL != callbacks.onDestroy) {
      (*callbacks.onDestroy)(parent, closure);
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaPublisher_send(publisherBridge publisher, mamaMsg msg)
{
   if (NULL == publisher) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "No publisher.");
      return MAMA_STATUS_NULL_ARG;
   }
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;
   zmqBridgeMamaMsg_setSendSubject(impl->mMamaBridgeMsg, impl->mSubject, impl->mSource);

   // serialize the msg
   void* buf = NULL;
   size_t bufSize = 0;
   size_t payloadSize = 0;
   /* Pack the provided MAMA message into a zmq message */
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_serialize(impl->mMamaBridgeMsg, msg, &buf, &bufSize));
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_getPayloadSize(impl->mMamaBridgeMsg, &payloadSize));

   // send it
   WLOCK_LOCK(impl->mTransport->mZmqSocketPublisher.mLock);
   int i = zmq_send(impl->mTransport->mZmqSocketPublisher.mSocket, buf, bufSize, 0);
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Sent %d bytes [msgSize=%lu,payload=%lu]",i, bufSize, payloadSize);
   WLOCK_UNLOCK(impl->mTransport->mZmqSocketPublisher.mLock);

   // TODO: ????
   /* Reset the message type for the next publish */
   zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg, ZMQ_MSG_PUB_SUB);

   return MAMA_STATUS_OK;
}

/* Send reply to inbox. */
// TODO: fix these fucking void*s!
mama_status zmqBridgeMamaPublisher_sendReplyToInbox(publisherBridge publisher, void* request, mamaMsg reply)
{
   if (NULL == publisher || NULL == request || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   /* Get the incoming bridge message from the mamaMsg */
   msgBridge bridgeMsg = NULL;
   mama_status status = mamaMsgImpl_getBridgeMsg((mamaMsg) request, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      return status;
   }

   /* Get properties from the incoming bridge message */
   const char* replyHandle = NULL;
   status = zmqBridgeMamaMsg_duplicateReplyHandle(bridgeMsg, (void**) &replyHandle);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get reply handle[%s]", mamaStatus_stringForStatus(status));
      return status;
   }

   return zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisher, (void*) replyHandle, reply);
}

mama_status zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisherBridge publisher, void* replyHandle, mamaMsg reply)
{
   if (NULL == publisher || NULL == replyHandle || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   // Set properties for the outgoing bridge message
   zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg, ZMQ_MSG_INBOX_RESPONSE);
   // Set replyHandle so it will be serialized along with the rest of the message
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setReplyHandle(impl->mMamaBridgeMsg, replyHandle));
   // Set the send subject to publish onto the replyAddress
   CALL_MAMA_FUNC(zmqBridgeMamaMsg_setSendSubject(impl->mMamaBridgeMsg, (const char*) replyHandle, impl->mSource));

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Sent inbox reply to %s", (const char*) replyHandle);

   /* Fire out the message to the inbox */
   return zmqBridgeMamaPublisher_send(publisher, reply);
}

/* Send a message from the specified inbox using the throttle. */
mama_status zmqBridgeMamaPublisher_sendFromInboxByIndex(publisherBridge publisher, int tportIndex, mamaInbox inbox, mamaMsg msg)
{
   if (NULL == publisher || NULL == inbox || NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   // Mark this as being a request from an inbox
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg, ZMQ_MSG_INBOX_REQUEST));
   // Set the reply address
   const char* replyHandle = zmqBridgeMamaInboxImpl_getReplyHandle(mamaInboxImpl_getInboxBridge(inbox));
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setReplyHandle(impl->mMamaBridgeMsg, (void*) replyHandle));

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaPublisher_sendFromInboxByIndex: Send from inbox %s with reply %s", replyHandle);

   return zmqBridgeMamaPublisher_send(publisher, msg);
}

mama_status zmqBridgeMamaPublisher_sendFromInbox(publisherBridge publisher, mamaInbox inbox, mamaMsg msg)
{
   return zmqBridgeMamaPublisher_sendFromInboxByIndex(publisher, 0, inbox, msg);
}

mama_status zmqBridgeMamaPublisher_setUserCallbacks(publisherBridge publisher, mamaQueue queue, mamaPublisherCallbacks* cb, void* closure)
{
   if (NULL == publisher || NULL == cb) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   /* Take a copy of the callbacks */
   impl->mCallbacks = *cb;
   impl->mCallbackClosure = closure;

   return MAMA_STATUS_OK;
}

/*=========================================================================
 =                  Private implementation functions                     =
 =========================================================================*/

mama_status zmqBridgeMamaPublisherImpl_buildSendSubject(zmqPublisherBridge* impl)
{
   char* keyTarget = NULL;

   /* If this is a special _MD publisher, lose the topic unless dictionary */
   if (impl->mRoot != NULL) {
      // TODO: wtf is this all about?
      /*
       * May use strlen here to increase speed but would need to test to
       * verify this is the only circumstance in which we want to consider the
       * topic when a root is specified.
       */
      if (strcmp(impl->mRoot, "_MDDD") == 0) {
         zmqBridgeMamaSubscriptionImpl_generateSubjectKey(impl->mRoot,
                                                          impl->mSource,
                                                          impl->mTopic,
                                                          &keyTarget);
      }
      else {
         zmqBridgeMamaSubscriptionImpl_generateSubjectKey(impl->mRoot,
                                                          impl->mSource,
                                                          NULL,
                                                          &keyTarget);
      }
   }
   /* If this isn't a special _MD publisher */
   else {
      zmqBridgeMamaSubscriptionImpl_generateSubjectKey(NULL,
                                                       impl->mSource,
                                                       impl->mTopic,
                                                       &keyTarget);
   }

   /* Set the subject for publishing here */
   impl->mSubject = keyTarget;

   return MAMA_STATUS_OK;
}
