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

#include <mama/mama.h>
#include <msgimpl.h>
#include <string.h>
#include <wombat/wUuid.h>
#include <wombat/port.h>
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

#define                 UUID_STRING_BUF_SIZE                37


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

typedef struct zmqInboxImpl {
   mamaSubscription                mSubscription;
   void*                           mClosure;
   mamaInboxMsgCallback            mMsgCB;
   mamaInboxErrorCallback          mErrCB;
   mamaInboxDestroyCallback        mOnInboxDestroyed;
   mamaInbox                       mParent;
   zmqBridgeMsgReplyHandle         mReplyHandle;
} zmqInboxImpl;

/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

/**
 * This is the onMsg callback to call when a message is received for this inbox.
 * This will in turn relay the message to the mamaInboxMsgCallback callback
 * provided on inbox creation.
 *
 * @param subscription The MAMA subscription originating this callback.
 * @param msg          The message received.
 * @param closure      The closure passed to the mamaSubscription_create
 *                     function (in this case, the inbox impl).
 * @param itemClosure  The item closure for the subscription can be set with
 *                     mamaSubscription_setItemClosure (not used in this case).
 */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onMsg(mamaSubscription    subscription,
                             mamaMsg             msg,
                             void*               closure,
                             void*               itemClosure);

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

mama_status
zmqBridgeMamaInbox_create(inboxBridge*             bridge,
                          mamaTransport            transport,
                          mamaQueue                queue,
                          mamaInboxMsgCallback     msgCB,
                          mamaInboxErrorCallback   errorCB,
                          mamaInboxDestroyCallback onInboxDestroyed,
                          void*                    closure,
                          mamaInbox                parent)
{
   return zmqBridgeMamaInbox_createByIndex(bridge,
                                           transport,
                                           0,
                                           queue,
                                           msgCB,
                                           errorCB,
                                           onInboxDestroyed,
                                           closure,
                                           parent);
}

mama_status
zmqBridgeMamaInbox_createByIndex(inboxBridge*             bridge,
                                 mamaTransport            transport,
                                 int                      tportIndex,
                                 mamaQueue                queue,
                                 mamaInboxMsgCallback     msgCB,
                                 mamaInboxErrorCallback   errorCB,
                                 mamaInboxDestroyCallback onInboxDestroyed,
                                 void*                    closure,
                                 mamaInbox                parent)
{
   zmqInboxImpl*       impl        = NULL;
   mama_status         status      = MAMA_STATUS_OK;
   mamaMsgCallbacks    cb;
   wUuid               tempUuid;
   char                uuidStringBuffer[UUID_STRING_BUF_SIZE];

   if (NULL == bridge || NULL == transport || NULL == queue || NULL == msgCB) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Allocate memory for the zmq inbox implementation */
   impl = (zmqInboxImpl*) calloc(1, sizeof(zmqInboxImpl));
   if (NULL == impl) {
      return MAMA_STATUS_NOMEM;
   }

   status = mamaSubscription_allocate(&impl->mSubscription);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaInbox_createByIndex(): "
               "Failed to allocate subscription ");
      mamaSubscription_deallocate(impl->mSubscription);
      free(impl);
      return status;
   }

   /* Create the unique name allocated to this inbox */
   // NB: uuid_generate is very expensive, so we use cheaper uuid_generate_time
   wUuid_generate_time(tempUuid);
   wUuid_unparse(tempUuid, uuidStringBuffer);
   snprintf(impl->mReplyHandle.mInboxName, sizeof(impl->mReplyHandle.mInboxName) - 1,
            "_INBOX.%s", uuidStringBuffer);

   /* Set the mandatory callbacks for basic subscriptions */
   cb.onCreate             = &zmqBridgeMamaInboxImpl_onCreate;
   cb.onError              = &zmqBridgeMamaInboxImpl_onError;
   cb.onMsg                = &zmqBridgeMamaInboxImpl_onMsg;
   cb.onDestroy            = &zmqBridgeMamaInboxImpl_onDestroy;

   /* These callbacks are not used by basic subscriptions */
   cb.onQuality            = NULL;
   cb.onGap                = NULL;
   cb.onRecapRequest       = NULL;

   /* Initialize the remaining members for the zmq inbox implementation */
   impl->mClosure          = closure;
   impl->mMsgCB            = msgCB;
   impl->mErrCB            = errorCB;
   impl->mParent           = parent;
   impl->mOnInboxDestroyed = onInboxDestroyed;

   // get reply address from transport
   const char* inboxSubject = NULL;
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_getInboxSubject(transport, &inboxSubject));
   strcpy(impl->mReplyHandle.mReplyTo, inboxSubject);

   /* Subscribe to the inbox topic name */
   status = mamaSubscription_createBasic(impl->mSubscription,
                                         transport,
                                         queue,
                                         &cb,
                                         impl->mReplyHandle.mReplyTo,
                                         impl);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaInbox_createByIndex(): "
               "Failed to create subscription ");
      mamaSubscription_deallocate(impl->mSubscription);
      free(impl);
      return status;
   }

   /* Populate the bridge with the newly created implementation */
   *bridge = (inboxBridge) impl;

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaInbox_destroy(inboxBridge inbox)
{
   zmqInboxImpl* impl = (zmqInboxImpl*) inbox;
   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }

   mama_status status;

   // immediately stop the subscription
   subscriptionBridge subBridge = mamaSubscription_getSubscriptionBridge(impl->mSubscription);
   status = zmqBridgeMamaSubscriptionImpl_destroyInbox(subBridge);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaSubscriptionImpl_destroyInbox(): "
               "Failed to destroy inbox subscription ");
      return status;
   }

   // queue up the destroy
   status = mamaSubscription_destroyEx(impl->mSubscription);
   if (MAMA_STATUS_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "mamaSubscription_destroyEx(): "
               "Failed ");
      return status;
   }

   return MAMA_STATUS_OK;
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

zmqBridgeMsgReplyHandle*
zmqBridgeMamaInboxImpl_getReplyHandle(inboxBridge inbox)
{
   zmqInboxImpl* impl = (zmqInboxImpl*) inbox;
   if (NULL == impl) {
      return NULL;
   }
   return &impl->mReplyHandle;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/

/* Inbox bridge callbacks */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onMsg(mamaSubscription    subscription,
                             mamaMsg             msg,
                             void*               closure,
                             void*               itemClosure)
{
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;
   if (NULL == impl) {
      return;
   }

   // is this our reply?
   msgBridge tmp;
   mamaMsgImpl_getBridgeMsg(msg, &tmp);
   char* inboxName;
   zmqBridgeMamaMsgImpl_getInboxName(tmp, &inboxName);
   if (strcmp(inboxName, impl->mReplyHandle.mInboxName) != 0) {
      mama_log(MAMA_LOG_LEVEL_FINE,
               "zmqBridgeMamaInboxImpl_onMsg(): "
               "Discarding msg w/%s which does not match %s.", inboxName, impl->mReplyHandle.mInboxName);
      return;
   }

   /* If a message callback is defined, call it */
   if (NULL != impl->mMsgCB) {
      (impl->mMsgCB)(msg, impl->mClosure);
   }
}

/* No additional processing is required on inbox creation */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onCreate(mamaSubscription    subscription,
                                void*               closure)
{
}

/*
   Calls the implementation's destroy callback on execution
   Also deallocates the subscription and frees the inbox impl
*/
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onDestroy(mamaSubscription    subscription,
                                 void*               closure)
{
   /* The closure provided is the zmq inbox implementation */
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;
   if (NULL != impl) {
      /* Call the zmq inbox destroy callback if defined */
      if (NULL != impl->mOnInboxDestroyed) {
         (impl->mOnInboxDestroyed)(impl->mParent, impl->mClosure);
      }

      free(impl);
   }

   mamaSubscription_deallocate(subscription);
}

/* Calls the implementation's error callback on execution */
static void MAMACALLTYPE
zmqBridgeMamaInboxImpl_onError(mamaSubscription    subscription,
                               mama_status         status,
                               void*               platformError,
                               const char*         subject,
                               void*               closure)
{
   /* The closure provided is the zmq inbox implementation */
   zmqInboxImpl* impl = (zmqInboxImpl*) closure;
   if (NULL == impl) {
      return;
   }

   /* Call the zmq inbox error callback if defined */
   if (NULL != impl->mErrCB) {
      (impl->mErrCB)(status, impl->mClosure);
   }
}
