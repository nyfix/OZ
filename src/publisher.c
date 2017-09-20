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
static mama_status
zmqBridgeMamaPublisherImpl_buildSendSubject(zmqPublisherBridge* impl);


/*=========================================================================
 =               Public interface implementation functions               =
 =========================================================================*/

mama_status
zmqBridgeMamaPublisher_createByIndex(publisherBridge*     result,
                                     mamaTransport        tport,
                                     int                  tportIndex,
                                     const char*          topic,
                                     const char*          source,
                                     const char*          root,
                                     mamaPublisher        parent)
{
   zmqPublisherBridge*    impl        = NULL;
   zmqTransportBridge*    transport   = NULL;
   mama_status             status      = MAMA_STATUS_OK;

   if (NULL == result
       || NULL == tport
       || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }

   transport = zmqBridgeMamaTransportImpl_getTransportBridge(tport);
   if (NULL == transport) {
      mama_log(MAMA_LOG_LEVEL_SEVERE,
               "zmqBridgeMamaPublisher_createByIndex(): "
               "Could not find transport.");
      return MAMA_STATUS_NULL_ARG;
   }

   impl = (zmqPublisherBridge*) calloc(1, sizeof(zmqPublisherBridge));
   if (NULL == impl) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_createByIndex(): "
               "Could not allocate mem publisher.");
      return MAMA_STATUS_NOMEM;
   }

   /* Initialize the publisher members */
   impl->mTransport = transport;
   impl->mParent    = parent;

   /* Create an underlying bridge message with no parent to be used in sends */
   status = zmqBridgeMamaMsgImpl_createMsgOnly(&impl->mMamaBridgeMsg);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_createByIndex(): "
               "Could not create zmq bridge message for publisher: %s.",
               mamaStatus_stringForStatus(status));
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   if (NULL != topic) {
      impl->mTopic = topic;
   }

   if (NULL != source) {
      impl->mSource = source;
   }

   if (NULL != root) {
      impl->mRoot = root;
   }

   /* Generate a topic name based on the publisher details */
   status = zmqBridgeMamaPublisherImpl_buildSendSubject(impl);

   /* Populate the publisherBridge pointer with the publisher implementation */
   *result = (publisherBridge) impl;

   return status;
}

mama_status
zmqBridgeMamaPublisher_destroy(publisherBridge publisher)
{
   zmqPublisherBridge*    impl    = (zmqPublisherBridge*) publisher;

   /* Take a copy of the callbacks - we'll need those */
   mamaPublisherCallbacks callbacks;
   mamaPublisher          parent  = NULL;
   void*                  closure = NULL;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Take a copy of the callbacks - we'll need those */
   callbacks = impl->mCallbacks;
   parent    = impl->mParent;
   closure   = impl->mCallbackClosure;

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

mama_status
zmqBridgeMamaPublisher_send(publisherBridge publisher, mamaMsg msg)
{
   mama_status           status        = MAMA_STATUS_OK;
   zmqPublisherBridge*   impl          = (zmqPublisherBridge*) publisher;
   char*                 url           = NULL;
   zmqMsgType            type          = ZMQ_MSG_PUB_SUB;

   if (NULL == impl) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_send(): No publisher.");
      return MAMA_STATUS_NULL_ARG;
   }
   else if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqBridgeMamaMsg_setSendSubject(impl->mMamaBridgeMsg, impl->mSubject, impl->mSource);

   void* buf = NULL;
   size_t bufSize = 0;
   size_t payloadSize = 0;
   /* Pack the provided MAMA message into a proton message */
   zmqBridgeMamaMsgImpl_serialize(impl->mMamaBridgeMsg, msg, &buf, &bufSize);
   zmqBridgeMamaMsgImpl_getPayloadSize(impl->mMamaBridgeMsg, &payloadSize);

   wlock_lock(impl->mTransport->mZmqSocketPublisher.mLock);
   int i = zmq_send(impl->mTransport->mZmqSocketPublisher.mSocket, buf, bufSize, 0);
   mama_log(MAMA_LOG_LEVEL_FINE,
            "zmqBridgeMamaPublisher_send(): "
            "Sending %lu bytes [payload=%lu; type=%d]",
            bufSize,
            payloadSize,
            type,
            i);
   wlock_unlock(impl->mTransport->mZmqSocketPublisher.mLock);

   /* Reset the message type for the next publish */
   zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg, ZMQ_MSG_PUB_SUB);

   return status;
}

/* Send reply to inbox. */
mama_status
zmqBridgeMamaPublisher_sendReplyToInbox(publisherBridge   publisher,
                                        void*             request,
                                        mamaMsg           reply)
{
   zmqPublisherBridge*    impl            = (zmqPublisherBridge*) publisher;
   mamaMsg                requestMsg      = (mamaMsg) request;
   msgBridge              bridgeMsg       = NULL;
   mama_status            status          = MAMA_STATUS_OK;

   if (NULL == publisher || NULL == request || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Get the incoming bridge message from the mamaMsg */
   status = mamaMsgImpl_getBridgeMsg(requestMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInbox(): "
               "Could not get bridge message from cached"
               " queue mamaMsg [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }

   /* Get properties from the incoming bridge message */
   zmqBridgeMsgReplyHandle* replyHandle = NULL;
   status = zmqBridgeMamaMsg_duplicateReplyHandle(bridgeMsg, (void**) &replyHandle);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInbox(): "
               "Could not get reply handle[%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }

   return zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisher, replyHandle, reply);
}

mama_status
zmqBridgeMamaPublisher_sendReplyToInboxHandle(publisherBridge     publisher,
                                              void*               inbox,
                                              mamaMsg             reply)
{
   zmqPublisherBridge*       impl           = (zmqPublisherBridge*) publisher;
   const char*               inboxName      = NULL;
   const char*               replyTo        = NULL;
   mama_status               status         = MAMA_STATUS_OK;

   if (NULL == publisher || NULL == inbox || NULL == reply) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Set properties for the outgoing bridge message */
   zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg,
                                   ZMQ_MSG_INBOX_RESPONSE);

   /* Get properties from the incoming bridge message */
   status = zmqBridgeMamaMsgReplyHandleImpl_getInboxName(
               inbox,
               (char**) &inboxName);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInbox(): "
               "Could not get inbox name [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }

   status = zmqBridgeMamaMsgReplyHandleImpl_getReplyTo(
               inbox,
               (char**) &replyTo);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInbox(): "
               "Could not get reply to [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }


   if (NULL == inboxName || NULL == replyTo) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInboxHandle(): "
               "No reply address specified - cannot respond to inbox %s.",
               inboxName);
      return status;
   }

   /* Set the send subject to publish onto the inbox subject */
   status = zmqBridgeMamaMsg_setSendSubject(impl->mMamaBridgeMsg,
                                            replyTo,
                                            impl->mSource);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInboxHandle(): "
               "Could not set send subject '%s' [%s]",
               replyTo,
               mamaStatus_stringForStatus(status));
      return status;
   }

   /* Set inboxName and replyTo so they will be serialized along with the rest of the message */
   status = zmqBridgeMamaMsgImpl_setReplyTo(impl->mMamaBridgeMsg,
                                            replyTo);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInboxHandle(): "
               "Could not set send replyTo '%s' [%s]",
               replyTo,
               mamaStatus_stringForStatus(status));
      return status;
   }

   status = zmqBridgeMamaMsgImpl_setInboxName(impl->mMamaBridgeMsg,
                                              inboxName);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendReplyToInboxHandle(): "
               "Could not set send inbox name '%s' [%s]",
               inboxName,
               mamaStatus_stringForStatus(status));
      return status;
   }

   mama_log(MAMA_LOG_LEVEL_FINE, "zmqBridgeMamaPublisher_sendReplyToInboxHandle: Sent reply to %s for inbox", replyTo, inboxName);

   /* Fire out the message to the inbox */
   return zmqBridgeMamaPublisher_send(publisher, reply);
}

/* Send a message from the specified inbox using the throttle. */
mama_status
zmqBridgeMamaPublisher_sendFromInboxByIndex(publisherBridge   publisher,
                                            int               tportIndex,
                                            mamaInbox         inbox,
                                            mamaMsg           msg)
{
   zmqPublisherBridge*      impl        = (zmqPublisherBridge*) publisher;
   zmqBridgeMsgReplyHandle* replyHandle = NULL;
   inboxBridge              inboxImpl   = NULL;
   mama_status              status      = MAMA_STATUS_OK;

   if (NULL == impl || NULL == inbox || NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Get the inbox which you want the publisher to respond to */
   inboxImpl = mamaInboxImpl_getInboxBridge(inbox);
   replyHandle = zmqBridgeMamaInboxImpl_getReplyHandle(inboxImpl);

   /* Mark this as being a request from an inbox */
   status = zmqBridgeMamaMsgImpl_setMsgType(impl->mMamaBridgeMsg,
                                            ZMQ_MSG_INBOX_REQUEST);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendFromInboxByIndex(): "
               "Failed to set message type [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }

   /* Update meta data in outgoing message to reflect the inbox name */
   status = zmqBridgeMamaMsgImpl_setInboxName(impl->mMamaBridgeMsg,
                                              replyHandle->mInboxName);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendFromInboxByIndex(): "
               "Failed to set inbox name [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }
   status = zmqBridgeMamaMsgImpl_setReplyTo(impl->mMamaBridgeMsg,
                                            replyHandle->mReplyTo);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaPublisher_sendFromInboxByIndex(): "
               "Failed to set reply to  [%s]",
               mamaStatus_stringForStatus(status));
      return status;
   }

   mama_log(MAMA_LOG_LEVEL_FINE, "zmqBridgeMamaPublisher_sendFromInboxByIndex: Send from inbox %s with reply %s", replyHandle->mInboxName, replyHandle->mReplyTo);

   return zmqBridgeMamaPublisher_send(publisher, msg);;
}

mama_status
zmqBridgeMamaPublisher_sendFromInbox(publisherBridge  publisher,
                                     mamaInbox        inbox,
                                     mamaMsg          msg)
{
   return zmqBridgeMamaPublisher_sendFromInboxByIndex(publisher,
                                                      0,
                                                      inbox,
                                                      msg);
}

mama_status
zmqBridgeMamaPublisher_setUserCallbacks(publisherBridge         publisher,
                                        mamaQueue               queue,
                                        mamaPublisherCallbacks* cb,
                                        void*                   closure)
{
   zmqPublisherBridge*    impl        = (zmqPublisherBridge*) publisher;

   if (NULL == impl || NULL == cb) {
      return MAMA_STATUS_NULL_ARG;
   }
   /* Take a copy of the callbacks */
   impl->mCallbacks = *cb;
   impl->mCallbackClosure = closure;

   return MAMA_STATUS_OK;
}

/*=========================================================================
 =                  Private implementation functions                     =
 =========================================================================*/

mama_status
zmqBridgeMamaPublisherImpl_buildSendSubject(zmqPublisherBridge* impl)
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
