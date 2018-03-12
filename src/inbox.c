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
#include <assert.h>

// Mama includes
#include <mama/mama.h>
#include <msgimpl.h>
#include <wombat/wUuid.h>
#include <wombat/port.h>

// local includes
#include "zmqdefs.h"
#include "inbox.h"
#include "transport.h"
#include "msg.h"
#include "subscription.h"
#include "zmqbridgefunctions.h"

extern subscriptionBridge
mamaSubscription_getSubscriptionBridge(
   mamaSubscription subscription);

/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/


/**
 * This is the onCreate callback to call when the inbox subscription is created.
 * This currently does nothing but needs to be specified for the subscription
 * callbacks.
 *
 * @param subscription The MAMA subscription originating this callback.
 * @param closure      The closure passed to the mamaSubscription_create
 *                     function (in this case, the inbox impl).
 */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onCreate(mamaSubscription    subscription,
                                void*               closure);

/**
 * This is the onDestroy callback to call when the inbox subscription is
 * destroyed. This will relay this destroy request to the mamaInboxDestroy
 * callback provided on inbox creation when hit.
 *
 * @param subscription The MAMA subscription originating this callback.
 * @param closure      The closure passed to the mamaSubscription_create
 *                     function (in this case, the inbox impl).
 */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onDestroy(mamaSubscription    subscription,
                                 void*               closure);

/**
 * This is the onError callback to call when the inbox subscription receives
 * an error. This will relay this error to the mamaInboxErrorCallback callback
 * provided on inbox creation when hit.
 *
 * @param subscription  The MAMA subscription originating this callback.
 * @param status        The error code encountered.
 * @param platformError Third party, platform specific messaging error.
 * @param subject       The subject if NOT_ENTITLED encountered.
 * @param closure       The closure passed to the mamaSubscription_create
 *                      function (in this case, the inbox impl).
 */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onError(mamaSubscription    subscription,
                               mama_status         status,
                               void*               platformError,
                               const char*         subject,
                               void*               closure);


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

mama_status zmqBridgeMamaInbox_create(inboxBridge* bridge, mamaTransport transport, mamaQueue queue,
   mamaInboxMsgCallback msgCB, mamaInboxErrorCallback errorCB, mamaInboxDestroyCallback onInboxDestroyed,
    void* closure, mamaInbox parent)
{
   return zmqBridgeMamaInbox_createByIndex(bridge, transport, 0, queue, msgCB, errorCB, onInboxDestroyed, closure, parent);
}

mama_status zmqBridgeMamaInbox_createByIndex(inboxBridge* bridge, mamaTransport transport, int tportIndex, mamaQueue queue,
   mamaInboxMsgCallback msgCB, mamaInboxErrorCallback errorCB, mamaInboxDestroyCallback onInboxDestroyed,
    void* closure, mamaInbox parent)
{
   if (NULL == bridge || NULL == transport || NULL == queue || NULL == msgCB) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Allocate memory for the zmq inbox implementation */
   zmqInboxImpl* impl = (zmqInboxImpl*) calloc(1, sizeof(zmqInboxImpl));
   if (NULL == impl) {
      return MAMA_STATUS_NOMEM;
   }

   impl->mTransport = zmqBridgeMamaTransportImpl_getTransportBridge(transport);
   impl->mMamaQueue = queue;
   mamaQueue_getNativeHandle(queue, &impl->mZmqQueue);

   // generate reply address
   const char* inboxSubject;
   zmqBridgeMamaTransportImpl_getInboxSubject(impl->mTransport, &inboxSubject);
   char replyHandle[ZMQ_REPLYHANDLE_SIZE];
   const char* uid = zmqBridge_generateUid(&impl->mTransport->mInboxUid);
   sprintf(replyHandle, "%s.%s", inboxSubject, uid);
   free((void*) uid);
   impl->mReplyHandle = strdup(replyHandle);

   /* Initialize the remaining members for the zmq inbox implementation */
   impl->mClosure          = closure;
   impl->mMsgCB            = msgCB;
   impl->mErrCB            = errorCB;
   impl->mOnInboxDestroyed = onInboxDestroyed;
   impl->mParent           = parent;

   // register the inbox with the transport
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_registerInbox(impl->mTransport, impl));

   /* Populate the bridge with the newly created implementation */
   *bridge = (inboxBridge) impl;

   MAMA_LOG(MAMA_LOG_INBOX_MSGS, "mamaInbox=%p,replyAddr=%s", impl->mParent, impl->mReplyHandle);

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaInbox_destroy(inboxBridge inbox)
{
   if (NULL == inbox) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqInboxImpl* impl = (zmqInboxImpl*) inbox;

   MAMA_LOG(MAMA_LOG_INBOX_MSGS, "mamaInbox=%p,replyAddr=%s", impl->mParent, impl->mReplyHandle);

   // unregister the inbox with the transport
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_unregisterInbox(impl->mTransport, impl));

   if (NULL != impl->mOnInboxDestroyed) {
     // calls mamaInbox_onInboxDestroyed, which decrements the queue's object count
     // to enable graceful shutdown
     (*impl->mOnInboxDestroyed)(impl->mParent, impl->mClosure);
   }

   free((void*) impl->mReplyHandle);
   free(impl);

   return MAMA_STATUS_OK;
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

const char* zmqBridgeMamaInboxImpl_getReplyHandle(inboxBridge inbox)
{
   if (NULL == inbox) {
      return NULL;
   }
   zmqInboxImpl* impl = (zmqInboxImpl*) inbox;

   return impl->mReplyHandle;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/

/* Inbox bridge callbacks */
void MAMACALLTYPE zmqBridgeMamaInboxImpl_onMsg(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure)
{
   if (NULL == closure) {
      return;
   }
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;

   // TODO: following should not be necessary? what about error checking?
   msgBridge tmp;
   mamaMsgImpl_getBridgeMsg(msg, &tmp);
   const char* msgReplyHandle = zmqBridgeMamaMsg_getReplyHandle(tmp);
   if ((msgReplyHandle == NULL) || (strlen(msgReplyHandle) == 0)) {
      // TODO: this should never happen?!
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Got inbox msg w/no reply handle for (%s)", impl->mReplyHandle);
      assert(0);
      return;
   }
   else {
      if (strcmp(impl->mReplyHandle, msgReplyHandle) != 0) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Discarding msg w/replyHandle(%s) which does not match our replyHandle(%s)", msgReplyHandle, impl->mReplyHandle);
         assert(0);
         return;
      }
   }

   /* If a message callback is defined, call it */
   if (NULL != impl->mMsgCB) {
      (impl->mMsgCB)(msg, impl->mClosure);
   }
}

/* No additional processing is required on inbox creation */
static void MAMACALLTYPE zmqBridgeMamaInboxImpl_onCreate(mamaSubscription subscription, void* closure)
{
}

/*
   Calls the implementation's destroy callback on execution
   Also deallocates the subscription and frees the inbox impl
*/
static void MAMACALLTYPE zmqBridgeMamaInboxImpl_onDestroy(mamaSubscription subscription, void* closure)
{
   /* The closure provided is the zmq inbox implementation */
   if (NULL == closure) {
      return;
   }
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;

   /* Call the zmq inbox destroy callback if defined */
   if (NULL != impl->mOnInboxDestroyed) {
      (impl->mOnInboxDestroyed)(impl->mParent, impl->mClosure);
   }
}

/* Calls the implementation's error callback on execution */
static void MAMACALLTYPE zmqBridgeMamaInboxImpl_onError(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure)
{
   /* The closure provided is the zmq inbox implementation */
   if (NULL == closure) {
      return;
   }
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;

   /* Call the zmq inbox error callback if defined */
   if (NULL != impl->mErrCB) {
      (impl->mErrCB)(status, impl->mClosure);
   }
}
