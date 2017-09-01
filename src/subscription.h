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

#ifndef MAMA_BRIDGE_ZMQ_SUBSCRIPTION_H__
#define MAMA_BRIDGE_ZMQ_SUBSCRIPTION_H__

#if defined(__cplusplus)
extern "C" {
#endif

/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

/**
 * This function will generate a string which is unique to the root, source
 * and topic provided. Centralization of this function means that it can be used
 * in both the publisher and the subscriber in order to generate a consistent
 * topic for use throughout the platform.
 *
 * @param root      Prefix to associate with the subject (e.g. _MDDD)
 * @param inbox     Source to base the subject key on (e.g. EXCHANGENAME).
 * @param topic     Topic to base the subject key on (e.g. ISIN.CURRENCY).
 * @param keyTarget Pointer to populate with the generated subject key.
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
mama_status
zmqBridgeMamaSubscriptionImpl_generateSubjectKey(const char*  root,
                                                 const char*  source,
                                                 const char*  topic,
                                                 char**       keyTarget);




mama_status zmqBridgeMamaSubscriptionImpl_subscribe(void* socket, char* topic);
mama_status zmqBridgeMamaSubscriptionImpl_unsubscribe(void* socket, char* topic);

mama_status zmqBridgeMamaSubscriptionImpl_destroyInbox(subscriptionBridge subscriber);

#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_SUBSCRIPTION_H__ */
