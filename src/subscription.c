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
#include <subscriptionimpl.h>
#include <transportimpl.h>
#include <msgimpl.h>
#include <queueimpl.h>
#include <wombat/queue.h>
#include "transport.h"
#include "zmqdefs.h"
#include "subscription.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include "msg.h"
#include <zmq.h>
#include <errno.h>


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

mama_status
zmqBridgeMamaSubscription_create (subscriptionBridge* subscriber,
                                  const char*         source,
                                  const char*         symbol,
                                  mamaTransport       tport,
                                  mamaQueue           queue,
                                  mamaMsgCallbacks    callback,
                                  mamaSubscription    subscription,
                                  void*               closure)
{
    zmqSubscription*       impl        = NULL;
    zmqTransportBridge*    transport   = NULL;
    mama_status            status      = MAMA_STATUS_OK;

    if ( NULL == subscriber || NULL == subscription || NULL == tport )
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                "zmqBridgeMamaSubscription_create(): something NULL");
        return MAMA_STATUS_NULL_ARG;
    }

    status = mamaTransport_getBridgeTransport (tport,
                                               (transportBridge*) &transport);

    if (MAMA_STATUS_OK != status || NULL == transport)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                "zmqBridgeMamaSubscription_create(): something NULL");
        return MAMA_STATUS_NULL_ARG;
    }

    /* Allocate memory for zmq subscription implementation */
    impl = (zmqSubscription*) calloc (1, sizeof (zmqSubscription));
    if (NULL == impl)
    {
        return MAMA_STATUS_NOMEM;
    }

    mamaQueue_getNativeHandle(queue, &impl->mZmqQueue);
    impl->mMamaCallback        = callback;
    impl->mMamaSubscription    = subscription;
    impl->mMamaQueue           = queue;
    impl->mTransport           = transport;
    impl->mSymbol              = symbol;
    impl->mClosure             = closure;
    impl->mIsNotMuted          = 1;
    impl->mIsTportDisconnected = 1;
    impl->mSubjectKey          = NULL;

    /* Use a standard centralized method to determine a topic key */
    zmqBridgeMamaSubscriptionImpl_generateSubjectKey (NULL,
                                                      source,
                                                      symbol,
                                                      &impl->mSubjectKey);

    /* Register the endpoint */
    endpointPool_registerWithoutIdentifier (transport->mSubEndpoints,
                                            impl->mSubjectKey,
                                            &impl->mEndpointIdentifier,
                                            impl);

    /* Set the message meta data to reflect a subscription request */
    zmqBridgeMamaMsgImpl_setMsgType (transport->mMsg,
                                     ZMQ_MSG_SUB_REQUEST);

    /* subscribe to the topic */
    zmq_setsockopt (transport->mZmqSocketSubscriber,
                    ZMQ_SUBSCRIBE,
                    impl->mSubjectKey,
                    strlen (impl->mSubjectKey));

    mama_log (MAMA_LOG_LEVEL_FINEST,
              "zmqBridgeMamaSubscription_create(): "
              "created interest for %s.",
              impl->mSubjectKey);

    /* Mark this subscription as valid */
    impl->mIsValid = 1;

    *subscriber =  (subscriptionBridge) impl;

    return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaSubscription_createWildCard (subscriptionBridge*     subscriber,
                                          const char*             source,
                                          const char*             symbol,
                                          mamaTransport           transport,
                                          mamaQueue               queue,
                                          mamaMsgCallbacks        callback,
                                          mamaSubscription        subscription,
                                          void*                   closure)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaSubscription_mute (subscriptionBridge subscriber)
{
    zmqSubscription* impl = (zmqSubscription*) subscriber;

    if (NULL == impl)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    impl->mIsNotMuted = 0;

    return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaSubscription_destroy (subscriptionBridge subscriber)
{
    zmqSubscription*             impl            = NULL;
    zmqTransportBridge*          transportBridge = NULL;
    mamaSubscription             parent          = NULL;
    void*                        closure         = NULL;
    wombat_subscriptionDestroyCB destroyCb       = NULL;

    if (NULL == subscriber)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    impl            = (zmqSubscription*) subscriber;
    parent          = impl->mMamaSubscription;
    closure         = impl->mClosure;
    destroyCb       = impl->mMamaCallback.onDestroy;
    transportBridge = impl->mTransport;

    /* Remove the subscription from the transport's subscription pool. */
    if (NULL != transportBridge && NULL != transportBridge->mSubEndpoints
        && NULL != impl->mSubjectKey)
    {
        endpointPool_unregister (transportBridge->mSubEndpoints,
                                 impl->mSubjectKey,
                                 impl->mEndpointIdentifier);
    }

    if (NULL != impl->mSubjectKey)
    {
        free (impl->mSubjectKey);
    }

    if (NULL != impl->mEndpointIdentifier)
    {
        free ((void*)impl->mEndpointIdentifier);
    }

    free (impl);

    /*
     * Invoke the subscription callback to inform that the bridge has been
     * destroyed.
     */
    if (NULL != destroyCb)
        (*(wombat_subscriptionDestroyCB)destroyCb)(parent, closure);

    return MAMA_STATUS_OK;
}

int
zmqBridgeMamaSubscription_isValid (subscriptionBridge subscriber)
{
    zmqSubscription* impl = (zmqSubscription*) subscriber;

    if (NULL != impl)
    {
        return impl->mIsValid;
    }
    return 0;
}

int
zmqBridgeMamaSubscription_hasWildcards (subscriptionBridge subscriber)
{
    return 0;
}

mama_status
zmqBridgeMamaSubscription_getPlatformError (subscriptionBridge subscriber,
                                            void** error)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

int
zmqBridgeMamaSubscription_isTportDisconnected (subscriptionBridge subscriber)
{
	zmqSubscription* impl = (zmqSubscription*) subscriber;
	if (NULL == impl)
	{
		return MAMA_STATUS_NULL_ARG;
	}
    return impl->mIsTportDisconnected;
}

mama_status
zmqBridgeMamaSubscription_setTopicClosure (subscriptionBridge subscriber,
                                           void*              closure)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaSubscription_muteCurrentTopic (subscriptionBridge subscriber)
{
    /* As there is one topic per subscription, this can act as an alias */
    return zmqBridgeMamaSubscription_mute (subscriber);
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

/*
 * Internal function to ensure that the topic names are always calculated
 * in a particular way
 */
mama_status
zmqBridgeMamaSubscriptionImpl_generateSubjectKey (const char*  root,
                                                  const char*  source,
                                                  const char*  topic,
                                                  char**       keyTarget)
{
    char        subject[MAX_SUBJECT_LENGTH];
    char*       subjectPos     = subject;
    size_t      bytesRemaining = MAX_SUBJECT_LENGTH;
    size_t      written        = 0;

    if (NULL != root)
    {
        mama_log (MAMA_LOG_LEVEL_FINEST,
                  "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): R.");
        written         = snprintf (subjectPos, bytesRemaining, "%s", root);
        subjectPos     += written;
        bytesRemaining -= written;
    }

    if (NULL != source)
    {
        mama_log (MAMA_LOG_LEVEL_FINEST,
                  "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): S.");
        /* If these are not the first bytes, prepend with a period */
        if(subjectPos != subject)
        {
            written     = snprintf (subjectPos, bytesRemaining, ".%s", source);
        }
        else
        {
            written     = snprintf (subjectPos, bytesRemaining, "%s", source);
        }
        subjectPos     += written;
        bytesRemaining -= written;
    }

    if (NULL != topic)
    {
        mama_log (MAMA_LOG_LEVEL_FINEST,
                  "zmqBridgeMamaSubscriptionImpl_generateSubjectKey(): T.");
        /* If these are not the first bytes, prepend with a period */
        if (subjectPos != subject)
        {
            snprintf (subjectPos, bytesRemaining, ".%s", topic);
        }
        else
        {
            snprintf (subjectPos, bytesRemaining, "%s", topic);
        }
    }

    /*
     * Allocate the memory for copying the string. Caller is responsible for
     * destroying.
     */
    *keyTarget = strdup (subject);
    if (NULL == *keyTarget)
    {
        return MAMA_STATUS_NOMEM;
    }
    else
    {
        return MAMA_STATUS_OK;
    }
}


mama_status
zmqBridgeMamaSubscriptionImpl_deactivate (subscriptionBridge subscriber)
{
    if (NULL == subscriber)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    zmqSubscription* impl = (zmqSubscription*) subscriber;
    zmqTransportBridge* transportBridge = impl->mTransport;
    if (NULL == transportBridge || NULL == transportBridge->mSubEndpoints
        || NULL == impl->mSubjectKey)
    {
       return MAMA_STATUS_NULL_ARG;
    }

    // zmq sockets are not thread-safe
    #if 0
    // NOTE: zmq filters are reference-counted?
    /* un-subscribe to the topic */
    zmq_setsockopt (transportBridge->mZmqSocketSubscriber,
                    ZMQ_UNSUBSCRIBE,
                    impl->mSubjectKey,
                    strlen (impl->mSubjectKey));

    mama_log (MAMA_LOG_LEVEL_FINEST,
              "zmqBridgeMamaSubscriptionImpl_deactivate(): "
              "deleted interest for %s.",
              impl->mSubjectKey);
    #endif

    /* Remove the subscription from the transport's subscription pool. */
    endpointPool_unregister (transportBridge->mSubEndpoints,
                             impl->mSubjectKey,
                             impl->mEndpointIdentifier);

    /* Mark this subscription as invalid */
    impl->mIsValid = 0;
    impl->mIsNotMuted = 0;

    return MAMA_STATUS_OK;
}
