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

// MAMA includes
#include <mama/mama.h>
#include <subscriptionimpl.h>
#include <transportimpl.h>
#include <msgimpl.h>
#include <queueimpl.h>
#include <wombat/queue.h>

// local includes
#include "transport.h"
#include "zmqdefs.h"
#include "subscription.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include "msg.h"
#include "util.h"

#include <zmq.h>


zmqSubscription* zmqBridgeMamaSubscriptionImpl_allocate(mamaTransport tport, mamaQueue queue,
   mamaMsgCallbacks callback, mamaSubscription subscription, void* closure);

mama_status zmqBridgeMamaSubscriptionImpl_createWildcard(zmqSubscription* impl, const char* source, const char*symbol);

mama_status zmqBridgeMamaSubscriptionImpl_create(zmqSubscription* impl, const char* source, const char* symbol);

mama_status zmqBridgeMamaSubscriptionImpl_regex(const char* wsTopic, const char** mamaTopic, int* isWildcard);

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
   if (NULL == subscriber || NULL == subscription || NULL == tport) {
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
   // make a copy of the whole mess
   char temp[MAX_SUBJECT_LENGTH];
   strcpy(temp, source);
   // replace "." with null
   char* dotPos = strchr(temp, '?');
   if (dotPos == NULL) {
      return MAMA_STATUS_INVALID_ARG;
   }
   *dotPos = '\0';

   // now we have original topic in theSource, regex in theRegex
   char* theSource = temp;
   char* theRegex = dotPos +1;

   char* origSource = strdup(theSource);
   char* origRegex = strdup(theRegex);

   // zmq only does prefix matching, so subscribe to everything up to the first wildcard
   // TODO: for now, only deal with embedded (not final) wildcards
   char* wcPos = strchr(theSource, '*');
   if (wcPos == NULL) {
      wcPos = strstr(theSource, "//.");
   }
   if (wcPos == NULL) {
      free(impl);
      return MAMA_STATUS_INVALID_ARG;
   }
   *wcPos = '\0';

   #if 1
   // trim anchors
   theRegex[strlen(theRegex)-1] = '\0';
   impl->mOrigRegex = strdup(theRegex+1);
   #else
   // dont trim anchors
   impl->mOrigRegex = strdup(theRegex);
   #endif

   #if 1
   // do our own conversion
   const char* regex2;
   int isWildcard;
   mama_status status = zmqBridgeMamaSubscriptionImpl_regex(origSource, &regex2, &isWildcard);
   #endif

   #if 1
   // use our own conversion
   impl->mOrigRegex = strdup(regex2);
   #endif

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "\t%s\t%s\t%s\t%s\t%s", origSource, origRegex, theSource, impl->mOrigRegex, regex2);

   // create regex to match against
   impl->mRegexTopic = calloc(1, sizeof(regex_t));
   int rc = regcomp(impl->mRegexTopic, impl->mOrigRegex, REG_NOSUB | REG_EXTENDED);
   if (rc != 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to compile regex: %s", theRegex);
      free(impl);
      return MAMA_STATUS_INVALID_ARG;
   }

   // TODO: depending on resolution of https://github.com/OpenMAMA/OpenMAMA/issues/324
   // may need/want to pass source?
   CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_createWildcard(impl, NULL, theSource));

   *subscriber = (subscriptionBridge) impl;

   free(origSource);
   free(origRegex);

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
         endpointPool_unregister(transportBridge->mSubEndpoints, impl->mSubjectKey, impl->mEndpointIdentifier);
      }
   }
   else {
      if (NULL != transportBridge && NULL != impl->mEndpointIdentifier) {
         zmqBridgeMamaTransportImpl_unregisterWildcard(transportBridge, impl);
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

   free(impl->mSubjectKey);
   free((void*)impl->mEndpointIdentifier);
   free((void*)impl->mOrigRegex);
   if (NULL != impl->mRegexTopic) {
      regfree(impl->mRegexTopic);
      free((void*)impl->mRegexTopic);
   }

   free(impl);

   return MAMA_STATUS_OK;
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
   impl->mRegexTopic          = NULL;

   return impl;
}


mama_status zmqBridgeMamaSubscriptionImpl_createWildcard(zmqSubscription* impl, const char* source, const char*symbol)
{
   /* Use a standard centralized method to determine a topic key */
   zmqBridgeMamaSubscriptionImpl_generateSubjectKey(NULL, source, symbol, &impl->mSubjectKey);

   impl->mEndpointIdentifier = zmq_generate_uuid();

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

   impl->mEndpointIdentifier = zmq_generate_uuid();
   endpointPool_registerWithIdentifier(impl->mTransport->mSubEndpoints,
      impl->mSubjectKey, impl->mEndpointIdentifier, impl);

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
   char        subject[MAX_SUBJECT_LENGTH];
   char*       subjectPos     = subject;
   size_t      bytesRemaining = MAX_SUBJECT_LENGTH;
   size_t      written        = 0;

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


// converts Transact subject (hierarchical topic, as per WS-Topic) to Mama topic (extended regular expression)
mama_status zmqBridgeMamaSubscriptionImpl_regex(const char* wsTopic, const char** mamaTopic, int* isWildcard)
{
   // copy input
   // TODO: check len
   char inTopic[MAX_SUBJECT_LENGTH*2];
   strcpy(inTopic, wsTopic);

   // TODO: check len
   char regex[1024] = "^";       // anchor at beginning

   *isWildcard = 0;

   char* p = inTopic;
   char* end = p+strlen(p);      // trailing null

   while (p < end) {
      char* q = strchr(p, '*');  // find wildcard char
      if (q == NULL) {
         break;                  // no (more) wildcards
      }

      *isWildcard = 1;
      *q = '\0';                 // overwrite wildcard w/null
      strcat(regex, p);          // append string up to wildcard
      strcat(regex, "[^/]+");    // append regex (any char other than "/")
      p = q+1;                   // advance past wildcard
   }

   strcat(regex, p);             // append whatever is left of topic

   // check for special trailing wildcard -- convert to "super" wildcard if found
   int l = strlen(regex);
   if (strcmp(&regex[l-3], "//.") == 0) {
      *isWildcard = 1;
      strcpy(&regex[l-3], "/.*");
   }

   strcat(regex, "$");           // anchor at end

   *mamaTopic = strdup(regex);
   return MAMA_STATUS_OK;
}
