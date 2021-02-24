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

// system includes
#include <string.h>
#include <errno.h>

// MAMA includes
#include <mama/mama.h>
#include <mama/integration/subscription.h>
#include <mama/integration/transport.h>
#include <mama/integration/msg.h>
#include <mama/integration/queue.h>
#include <mama/integration/endpointpool.h>
#include <wombat/queue.h>
#include <wombat/strutils.h>

// local includes
#include "transport.h"
#include "zmqdefs.h"
#include "subscription.h"
#include "zmqbridgefunctions.h"
#include "msg.h"
#include "util.h"

#include <zmq.h>


zmqSubscription* zmqBridgeMamaSubscriptionImpl_allocate(mamaTransport tport, mamaQueue queue,
   mamaMsgCallbacks callback, mamaSubscription subscription, void* closure);

mama_status zmqBridgeMamaSubscriptionImpl_createWildcard(zmqSubscription* impl, const char* source, const char*symbol);

mama_status zmqBridgeMamaSubscriptionImpl_create(zmqSubscription* impl, const char* source, const char* symbol);

/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/
mama_status zmqBridgeMamaSubscription_create(subscriptionBridge* subscriber,
     const char* source, const char* symbol,
     mamaTransport tport, mamaQueue queue,
     mamaMsgCallbacks callback, mamaSubscription    subscription, void* closure)
{
   if (NULL == subscriber || NULL == subscription || NULL == tport) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "something NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   zmqSubscription* impl = zmqBridgeMamaSubscriptionImpl_allocate(tport, queue, callback, subscription, closure);
   if (impl == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to create subscription for %s:%s", source, symbol);
      return MAMA_STATUS_NULL_ARG;
   }

   CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_create(impl, source, symbol));
   *subscriber = (subscriptionBridge) impl;
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaSubscription_createWildCard(subscriptionBridge* subscriber,
   const char* source, const char* symbol,
   mamaTransport tport, mamaQueue queue,
   mamaMsgCallbacks callback, mamaSubscription subscription,
   void* closure)
{
   if (NULL == subscriber || NULL == subscription || NULL == tport || NULL == source) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "something NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   zmqSubscription* impl = zmqBridgeMamaSubscriptionImpl_allocate(tport, queue, callback, subscription, closure);
   if (impl == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to create subscription for %s:%s", source, symbol);
      return MAMA_STATUS_NULL_ARG;
   }
   impl->mIsWildcard          = 1;
   // stupid api ... symbol is NULL, source contains source.symbol
   impl->mOrigRegex = strdup(source);
   if (NULL == impl->mOrigRegex) return MAMA_STATUS_NULL_ARG;

   // generate zmq prefix from regex
   char* prefix = (char*) alloca(strlen(impl->mOrigRegex)+1);
   if (NULL == prefix) return MAMA_STATUS_NOMEM;
   strcpy(prefix, impl->mOrigRegex);
   if (prefix[0] == '^') {
      ++prefix;                                  // skip beginning anchor
   }
   char* regexPos = strpbrk(prefix, "[^/]+.*");  // find wildcard regex?
   if (regexPos != NULL) {
      *regexPos = '\0';                          // overwrite with null
   }
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "source=%s,prefix=%s", source, prefix);

   // create regex to match against
   impl->mCompRegex = calloc(1, sizeof(regex_t));
   if (NULL == impl->mCompRegex) return MAMA_STATUS_NOMEM;
   int rc = regcomp(impl->mCompRegex, impl->mOrigRegex, REG_NOSUB | REG_EXTENDED);
   if (rc != 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to compile regex: %s", impl->mOrigRegex);
      free(impl);
      return MAMA_STATUS_INVALID_ARG;
   }

   // TODO: depending on resolution of https://github.com/OpenMAMA/OpenMAMA/issues/324
   // may need/want to pass source?
   CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_createWildcard(impl, NULL, prefix));

   *subscriber = (subscriptionBridge) impl;

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaSubscription_mute(subscriptionBridge subscriber)
{
   if (NULL == subscriber) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqSubscription* impl = (zmqSubscription*) subscriber;

   impl->mIsNotMuted = 0;

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaSubscription_destroy(subscriptionBridge subscriber)
{
   if (NULL == subscriber) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqSubscription* impl = (zmqSubscription*) subscriber;
   zmqTransportBridge* transportBridge = impl->mTransport;

   if (impl->mIsWildcard == 0) {
      /* Remove the subscription from the transport's subscription pool. */
      if (NULL != transportBridge && NULL != transportBridge->mSubEndpoints && NULL != impl->mSubjectKey) {
         wlock_lock(transportBridge->mSubsLock);
         endpointPool_unregister(transportBridge->mSubEndpoints, impl->mSubjectKey, impl->mEndpointIdentifier);
         wlock_unlock(transportBridge->mSubsLock);
      }
   }
   else {
      if (NULL != transportBridge && NULL != impl->mEndpointIdentifier) {
         wlock_lock(transportBridge->mWcsLock);
         zmqBridgeMamaTransportImpl_unregisterWildcard(transportBridge, impl);
         wlock_unlock(transportBridge->mWcsLock);
      }
   }

   /*
    * Invoke the subscription callback to inform that the bridge has been
    * destroyed.
    */
   wombat_subscriptionDestroyCB destroyCb = impl->mMamaCallback.onDestroy;
   if (NULL != destroyCb) {
      mamaSubscription parent = impl->mMamaSubscription;
      void* closure = impl->mClosure;
      (*(wombat_subscriptionDestroyCB)destroyCb)(parent, closure);
   }

   // unsubscribe at the zmq layer
   // note that zmq subscriptions are reference-counted, such that the socket will continue to
   // receive subscribed topics until *all* subscribers have unsubscribed
   // see http://api.zeromq.org/4-2:zmq-setsockopt under ZMQ_UNSUBSCRIBE
   mama_status status = zmqBridgeMamaSubscriptionImpl_unsubscribe(transportBridge, impl->mSubjectKey);

   free((void*)impl->mSubjectKey);
   free((void*)impl->mEndpointIdentifier);
   if (impl->mIsWildcard == 1) {
      free((void*)impl->mOrigRegex);
      if (NULL != impl->mCompRegex) {
         regfree(impl->mCompRegex);
         free((void*)impl->mCompRegex);
      }
   }

   free(impl);

   return status;
}


int zmqBridgeMamaSubscription_isValid(subscriptionBridge subscriber)
{
   if (NULL == subscriber) {
      return 0;
   }
   zmqSubscription* impl = (zmqSubscription*) subscriber;

   return impl->mIsValid;
}


int zmqBridgeMamaSubscription_hasWildcards(subscriptionBridge subscriber)
{
   if (NULL == subscriber) {
      return 0;
   }
   zmqSubscription* impl = (zmqSubscription*) subscriber;

   return impl->mIsWildcard;
}


mama_status zmqBridgeMamaSubscription_getPlatformError(subscriptionBridge subscriber, void** error)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}


int zmqBridgeMamaSubscription_isTportDisconnected(subscriptionBridge subscriber)
{
   if (NULL == subscriber) {
      return 1;
   }
   zmqSubscription* impl = (zmqSubscription*) subscriber;

   return impl->mIsTportDisconnected;
}


mama_status zmqBridgeMamaSubscription_setTopicClosure(subscriptionBridge subscriber, void* closure)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}


mama_status zmqBridgeMamaSubscription_muteCurrentTopic(subscriptionBridge subscriber)
{
   /* As there is one topic per subscription, this can act as an alias */
   return zmqBridgeMamaSubscription_mute(subscriber);
}


/*=========================================================================
  =                  Private implementation functions                      =
  =========================================================================*/

zmqSubscription* zmqBridgeMamaSubscriptionImpl_allocate(mamaTransport tport, mamaQueue queue,
   mamaMsgCallbacks callback, mamaSubscription subscription, void* closure)
{
   /* Allocate memory for zmq subscription implementation */
   zmqSubscription* impl = (zmqSubscription*) calloc(1, sizeof(zmqSubscription));
   if (NULL == impl) {
      return NULL;
   }

   mamaTransport_getBridgeTransport(tport, (transportBridge*) &impl->mTransport);
   mamaQueue_getNativeHandle(queue, &impl->mZmqQueue);
   impl->mMamaQueue           = queue;
   impl->mMamaCallback        = callback;
   impl->mMamaSubscription    = subscription;
   impl->mClosure             = closure;

   impl->mIsNotMuted          = 1;
   impl->mIsTportDisconnected = 1;
   impl->mSubjectKey          = NULL;
   impl->mIsWildcard          = 0;
   impl->mOrigRegex           = NULL;
   impl->mCompRegex           = NULL;

   return impl;
}


mama_status zmqBridgeMamaSubscriptionImpl_createWildcard(zmqSubscription* impl, const char* source, const char*symbol)
{
   /* Use a standard centralized method to determine a topic key */
   zmqBridgeMamaSubscriptionImpl_generateSubjectKey(NULL, source, symbol, &impl->mSubjectKey);

   impl->mEndpointIdentifier = zmqBridge_generateSerial(&impl->mTransport->mWcsUid);

   // add this to list of wildcards
   zmqSubscription** pSub = (zmqSubscription**) list_allocate_element(impl->mTransport->mWcEndpoints);
   *pSub  = impl;
   list_push_back(impl->mTransport->mWcEndpoints, pSub);

   /* subscribe to the topic */
   CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_subscribe(impl->mTransport, impl->mSubjectKey));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "created interest for %s.", impl->mSubjectKey);

   /* Mark this subscription as valid */
   impl->mIsValid = 1;

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaSubscriptionImpl_create(zmqSubscription* impl, const char* source, const char*symbol)
{
   /* Use a standard centralized method to determine a topic key */
   zmqBridgeMamaSubscriptionImpl_generateSubjectKey(NULL, source, symbol, &impl->mSubjectKey);

   impl->mEndpointIdentifier = zmqBridge_generateSerial(&impl->mTransport->mSubUid);
   endpointPool_registerWithIdentifier(impl->mTransport->mSubEndpoints, impl->mSubjectKey, impl->mEndpointIdentifier, impl);

   /* subscribe to the topic */
   CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_subscribe(impl->mTransport, impl->mSubjectKey));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "created interest for %s.", impl->mSubjectKey);

   /* Mark this subscription as valid */
   impl->mIsValid = 1;

   return MAMA_STATUS_OK;
}


/*
 * Internal function to ensure that the topic names are always calculated
 * in a particular way
 */
mama_status zmqBridgeMamaSubscriptionImpl_generateSubjectKey(const char*  root,
   const char*  source, const char*  topic, char**       keyTarget)
{
   char        subject[MAX_SUBJECT_LENGTH +1];
   char*       subjectPos     = subject;
   size_t      bytesRemaining = MAX_SUBJECT_LENGTH;
   size_t      written;

   if (NULL != root) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): R.");
      written         = snprintf(subjectPos, bytesRemaining, "%s", root);
      subjectPos     += written;
      bytesRemaining -= written;
   }

   if (NULL != source) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): S.");
      /* If these are not the first bytes, prepend with a period */
      if (subjectPos != subject) {
         written     = snprintf(subjectPos, bytesRemaining, ".%s", source);
      }
      else {
         written     = snprintf(subjectPos, bytesRemaining, "%s", source);
      }
      subjectPos     += written;
      bytesRemaining -= written;
   }

   if (NULL != topic) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): T.");
      /* If these are not the first bytes, prepend with a period */
      if (subjectPos != subject) {
         snprintf(subjectPos, bytesRemaining, ".%s", topic);
      }
      else {
         snprintf(subjectPos, bytesRemaining, "%s", topic);
      }
   }

   /*
    * Allocate the memory for copying the string. Caller is responsible for
    * destroying.
    */
   *keyTarget = strdup(subject);
   if (NULL == *keyTarget) {
      return MAMA_STATUS_NOMEM;
   }
   else {
      return MAMA_STATUS_OK;
   }
}


// This subscribe call actually sends a control msg to the transport's control socket.
// The purpose is to allow applications to subscribe and unsubscribe in a thread-safe manner.
// Any subscriptions created this way will be issued against the transport's default sub socket.
mama_status zmqBridgeMamaSubscriptionImpl_subscribe(zmqTransportBridge* transport, const char* topic)
{
   zmqControlMsg msg;
   msg.command = 'S';
   wmStrSizeCpy(msg.arg1, topic, sizeof(msg.arg1));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(transport, &msg, sizeof(msg)));
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaSubscriptionImpl_unsubscribe(zmqTransportBridge* transport, const char* topic)
{
   zmqControlMsg msg;
   msg.command = 'U';
   wmStrSizeCpy(msg.arg1, topic, sizeof(msg.arg1));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(transport, &msg, sizeof(msg)));
   return MAMA_STATUS_OK;
}
