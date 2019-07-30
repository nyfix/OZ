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

// system includes
#include <string.h>
#include <errno.h>
#include <assert.h>

// MAMA includes
#include <mama/mama.h>
#include <mama/inbox.h>
#include <mama/publisher.h>
#include <bridge.h>
#include <inboximpl.h>
#include <msgimpl.h>

// local includes
#include "transport.h"
#include "zmqdefs.h"
#include "msg.h"
#include "inbox.h"
#include "subscription.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"

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

mama_status zmqBridgeMamaPublisher_sendSubject(publisherBridge publisher, mamaMsg msg, const char* subject);

mama_status zmqBridgeMamaPublisherImpl_sendSubject(publisherBridge publisher, mamaMsg mamaMsg, msgBridge bridgeMsg, const char* subject);

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

   impl->mTopic = topic;
   impl->mSource = source;
   impl->mRoot = root;

   /* Generate a topic name based on the publisher details */
   mama_status status = zmqBridgeMamaPublisherImpl_buildSendSubject(impl);

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

   free(impl);

   if (NULL != callbacks.onDestroy) {
      (*callbacks.onDestroy)(parent, closure);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaPublisher_send(publisherBridge publisher, mamaMsg msg)
{
   return zmqBridgeMamaPublisherImpl_sendSubject(publisher, msg, NULL, NULL);
}


mama_status zmqBridgeMamaPublisher_sendSubject(publisherBridge publisher, mamaMsg msg, const char* subject)
{

   return zmqBridgeMamaPublisherImpl_sendSubject(publisher, msg, NULL, subject);
}


// Send reply to inbox from original request
mama_status zmqBridgeMamaPublisher_sendReplyToInbox(publisherBridge publisher, void* request, mamaMsg reply)
{
   if (NULL == publisher || NULL == request || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Get the incoming bridge message from the mamaMsg */
   msgBridge bridgeMsg = zmqBridgeMamaMsgImpl_getBridgeMsg((mamaMsg) request);
   if (bridgeMsg == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get reply handle");
      assert(0);
      return MAMA_STATUS_NULL_ARG;
   }

   /* Get reply address from the bridge message */
   const char* replyHandle = zmqBridgeMamaMsg_getReplyHandle(bridgeMsg);
   if (replyHandle == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get reply handle");
      assert(0);
      return MAMA_STATUS_NULL_ARG;
   }

   return zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisher, (void*) replyHandle, reply);
}


// Send reply to inbox
mama_status zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisherBridge publisher, void* replyHandle, mamaMsg reply)
{
   if (NULL == publisher || NULL == replyHandle || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }

   // allocate bridge msg on stack
   zmqBridgeMsgImpl bridgeMsg;
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_init(&bridgeMsg));

   // Set message type
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setMsgType((msgBridge) &bridgeMsg, ZMQ_MSG_INBOX_RESPONSE));
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setReplyHandle((msgBridge) &bridgeMsg, replyHandle));

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Sent inbox reply to %s", (const char*) replyHandle);

   return zmqBridgeMamaPublisherImpl_sendSubject(publisher, reply, (msgBridge) &bridgeMsg, (const char*) replyHandle);
}


mama_status zmqBridgeMamaPublisher_sendFromInboxByIndex(publisherBridge publisher, int tportIndex, mamaInbox inbox, mamaMsg msg)
{
   if (NULL == publisher || NULL == inbox || NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   // allocate bridge msg on stack
   zmqBridgeMsgImpl bridgeMsg;
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_init(&bridgeMsg));

   // Mark this as being a request from an inbox
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setMsgType((msgBridge) &bridgeMsg, ZMQ_MSG_INBOX_REQUEST));

   // Set the reply address
   inboxBridge inboxBridge = mamaInboxImpl_getInboxBridge(inbox);
   if (inboxBridge == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get inbox bridge");
      assert(0);
      return MAMA_STATUS_NULL_ARG;
   }
   const char* replyHandle = zmqBridgeMamaInboxImpl_getReplyHandle(inboxBridge);
   if (replyHandle == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get reply handle");
      assert(0);
      return MAMA_STATUS_NULL_ARG;
   }
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_setReplyHandle((msgBridge) &bridgeMsg, (void*) replyHandle));

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaPublisher_sendFromInboxByIndex: Send from inbox %s", replyHandle);

   return zmqBridgeMamaPublisherImpl_sendSubject(publisher, msg, (msgBridge) &bridgeMsg, NULL);
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

// TODO: wtf is this all about?
mama_status zmqBridgeMamaPublisherImpl_buildSendSubject(zmqPublisherBridge* impl)
{
   char* keyTarget = NULL;

   /* If this is a special _MD publisher, lose the topic unless dictionary */
   if (impl->mRoot != NULL) {
      /*
       * May use strlen here to increase speed but would need to test to
       * verify this is the only circumstance in which we want to consider the
       * topic when a root is specified.
       */
      if (strcmp(impl->mRoot, "_MDDD") == 0) {
         zmqBridgeMamaSubscriptionImpl_generateSubjectKey(impl->mRoot, impl->mSource, impl->mTopic, &keyTarget);
      }
      else {
         zmqBridgeMamaSubscriptionImpl_generateSubjectKey(impl->mRoot, impl->mSource, NULL, &keyTarget);
      }
   }
   /* If this isn't a special _MD publisher */
   else {
      zmqBridgeMamaSubscriptionImpl_generateSubjectKey(NULL, impl->mSource, impl->mTopic, &keyTarget);
   }

   /* Set the subject for publishing here */
   impl->mSubject = keyTarget;

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaPublisherImpl_sendSubject(publisherBridge publisher, mamaMsg mamaMsg, msgBridge bridgeMsg, const char* subject)
{
   if (NULL == publisher || NULL == mamaMsg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqPublisherBridge* impl = (zmqPublisherBridge*) publisher;

   // if no bridge msg passed in, allocate one on the stack
   zmqBridgeMsgImpl tempMsg;
   if (bridgeMsg == NULL) {
      CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_init(&tempMsg));
      bridgeMsg = (msgBridge) &tempMsg;
   }

   // use subject passed in, or publisher's subject?
   if (subject != NULL) {
      zmqBridgeMamaMsg_setSendSubject(bridgeMsg, subject, NULL);
   }
   else {
      zmqBridgeMamaMsg_setSendSubject(bridgeMsg, impl->mSubject, impl->mSource);
   }

   // serialize the msg
   zmq_msg_t zmq_msg;
   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_serialize(bridgeMsg, mamaMsg, &zmq_msg));

   // send it
   mama_status status = MAMA_STATUS_OK;
   wlock_lock(impl->mTransport->mZmqDataPub.mLock);
   // ZMQ_DONTWAIT is superfluous w/PUB sockets, but...
   int i = zmq_msg_send(&zmq_msg, impl->mTransport->mZmqDataPub.mSocket, ZMQ_DONTWAIT);
   wlock_unlock(impl->mTransport->mZmqDataPub.mLock);
   if (i < 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_msg_send failed %d(%s)", zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Sent msg w/subject:%s, size=%ld", zmq_msg_data(&zmq_msg), zmq_msg_size(&zmq_msg));
   }
   zmq_msg_close (&zmq_msg);

   return status;
}
