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

#ifndef MAMA_BRIDGE_ZMQ_TRANSPORT_H__
#define MAMA_BRIDGE_ZMQ_TRANSPORT_H__


/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

#include <stdlib.h>
#include <string.h>

#include <mama/mama.h>
#include <bridge.h>

// required for definition of ZMQ_CLIENT, ZMQ_SERVER
#define ZMQ_BUILD_DRAFT_API
#include <zmq.h>

#include "zmqdefs.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * This is a simple convenience function to return a zmqTransportBridge
 * pointer based on the provided mamaTransport.
 *
 * @param transport  The mamaTransport to extract the bridge transport from.
 *
 * @return zmqTransportBridge* associated with the mamaTransport.
 */
zmqTransportBridge* zmqBridgeMamaTransportImpl_getTransportBridge(mamaTransport transport);

///////////////////////////////////////////////////////////////////////////////
static mama_status zmqBridgeMamaTransportImpl_init(zmqTransportBridge* impl);
static mama_status zmqBridgeMamaTransportImpl_start(zmqTransportBridge* impl);
static mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl);

///////////////////////////////////////////////////////////////////////////////
// socket helpers
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* pSocket, int type, const char* name, int monitor);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_destroySocket(zmqSocket* socket);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName,
   int reconnect, double reconnect_timeout);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_unbindSocket(zmqSocket* socket, const char* uri);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri,
   int reconnect, double reconnect_timeout);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_disconnectSocket(zmqSocket* socket, const char* uri);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_disableReconnect(zmqSocket* socket);
mama_status zmqBridgeMamaTransportImpl_bindOrConnect(void* socket, const char* uri, zmqTransportDirection direction,
   int reconnect, double reconnect_timeout);


///////////////////////////////////////////////////////////////////////////////
// message processing
//
static void* zmqBridgeMamaTransportImpl_dispatchThread(void* closure);
//
mama_status MAMACALLTYPE  zmqBridgeMamaTransportImpl_dispatchNamingMsg(zmqTransportBridge* zmqTransport, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE  zmqBridgeMamaTransportImpl_dispatchNormalMsg(zmqTransportBridge* zmqTransport, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchControlMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchSubMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchInboxMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg);
//
static void MAMACALLTYPE  zmqBridgeMamaTransportImpl_subCallback(mamaQueue queue, void* closure);
static void MAMACALLTYPE  zmqBridgeMamaTransportImpl_inboxCallback(mamaQueue queue, void* closure);
static void MAMACALLTYPE  zmqBridgeMamaTransportImpl_wcCallback(mamaQueue queue, void* closure);
memoryNode* MAMACALLTYPE zmqBridgeMamaTransportImpl_allocTransportMsg(zmqTransportBridge* impl, void* queue, zmq_msg_t* zmsg);


mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_unsubscribe(void* socket, const char* topic);

// naming-style transports publish their endpoints so peers can connect
void* MAMACALLTYPE zmqBridgeMamaTransportImpl_publishEndpoints(void* closure);
mama_status zmqBridgeMamaTransportImpl_sendEndpointsMsg(zmqTransportBridge* impl, char command);

mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_kickSocket(void* socket);

// wildcard support
typedef struct zmqWildcardClosure {
   const char* subject;
   zmq_msg_t*  zmsg;
   int         found;
} zmqWildcardClosure;
void zmqBridgeMamaTransportImpl_matchWildcards(wList dummy, zmqSubscription** pSubscription, zmqWildcardClosure* closure);

typedef struct zmqFindWildcardClosure {
   const char*       mEndpointIdentifier;
   zmqSubscription*  mSubscription;
} zmqFindWildcardClosure;
void zmqBridgeMamaTransportImpl_findWildcard(wList dummy, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure);
void zmqBridgeMamaTransportImpl_removeWildcard(wList wcList, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure);
void zmqBridgeMamaTransportImpl_unregisterWildcard(zmqTransportBridge* impl, zmqSubscription* subscription);

// inbox support
mama_status zmqBridgeMamaTransportImpl_getInboxSubject(zmqTransportBridge* impl, const char** inboxSubject);
mama_status zmqBridgeMamaTransportImpl_registerInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox);
mama_status zmqBridgeMamaTransportImpl_unregisterInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox);

// control socket
mama_status zmqBridgeMamaTransportImpl_sendCommand(zmqTransportBridge* impl, zmqControlMsg* msg, int msgSize);

// socket monitor
void* zmqBridgeMamaTransportImpl_monitorThread(void* closure);
mama_status zmqBridgeMamaTransportImpl_startMonitor(zmqTransportBridge* impl);
mama_status zmqBridgeMamaTransportImpl_stopMonitor(zmqTransportBridge* impl);
int zmqBridgeMamaTransportImpl_monitorEvent(void *socket, const char* socketName);


#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_TRANSPORT_H__*/
