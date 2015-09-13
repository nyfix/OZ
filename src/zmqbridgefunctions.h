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

#ifndef ZMQ_BRIDGE_FUNCTIONS__
#define ZMQ_BRIDGE_FUNCTIONS__

#include <mama/mama.h>
#include <bridge.h>

#if defined(__cplusplus)
extern "C" {
#endif

MAMAExpDLL
extern void
zmqBridge_createImpl (mamaBridge* result);

extern mama_status
zmqBridge_open (mamaBridge bridgeImpl);

extern mama_status
zmqBridge_close (mamaBridge bridgeImpl);

extern mama_status
zmqBridge_start (mamaQueue defaultEventQueue);

extern mama_status
zmqBridge_stop (mamaQueue defaultEventQueue);

extern const char*
zmqBridge_getVersion (void);

extern const char*
zmqBridge_getName (void);

mama_status
zmqBridge_getDefaultPayloadId (char*** name, char** id);

extern mama_status
zmqBridgeMamaQueue_create (queueBridge *queue, mamaQueue parent);

extern mama_status
zmqBridgeMamaQueue_create_usingNative (queueBridge *queue, mamaQueue parent,
                                       void* nativeQueue);

extern mama_status
zmqBridgeMamaQueue_destroy (queueBridge queue);

extern mama_status
zmqBridgeMamaQueue_getEventCount (queueBridge queue, size_t* count);

extern mama_status
zmqBridgeMamaQueue_dispatch (queueBridge queue);

extern mama_status
zmqBridgeMamaQueue_timedDispatch (queueBridge queue, uint64_t timeout);

extern mama_status
zmqBridgeMamaQueue_dispatchEvent (queueBridge queue);

extern mama_status
zmqBridgeMamaQueue_enqueueEvent (queueBridge        queue,
                                 mamaQueueEnqueueCB callback,
                                 void*              closure);

extern mama_status
zmqBridgeMamaQueue_stopDispatch (queueBridge queue);

extern mama_status
zmqBridgeMamaQueue_setEnqueueCallback (queueBridge        queue,
                                       mamaQueueEnqueueCB callback,
                                       void*              closure);

extern mama_status
zmqBridgeMamaQueue_removeEnqueueCallback (queueBridge queue);

extern mama_status
zmqBridgeMamaQueue_getNativeHandle (queueBridge queue,
                                    void**      nativeHandle);

extern mama_status
zmqBridgeMamaQueue_setHighWatermark (queueBridge queue,
                                     size_t      highWatermark);

extern mama_status
zmqBridgeMamaQueue_setLowWatermark (queueBridge queue,
                                    size_t      lowWatermark);

extern int
zmqBridgeMamaTransport_isValid (transportBridge transport);

extern mama_status
zmqBridgeMamaTransport_destroy (transportBridge transport);

extern mama_status
zmqBridgeMamaTransport_create (transportBridge* result,
                               const char*      name,
                               mamaTransport    parent);

extern mama_status
zmqBridgeMamaTransport_forceClientDisconnect (
                               transportBridge* transports,
                               int              numTransports,
                               const char*      ipAddress,
                               uint16_t         port);

extern mama_status
zmqBridgeMamaTransport_findConnection (transportBridge* transports,
                                       int              numTransports,
                                       mamaConnection*  result,
                                       const char*      ipAddress,
                                       uint16_t         port);

extern mama_status
zmqBridgeMamaTransport_getAllConnections (transportBridge* transports,
                                          int              numTransports,
                                          mamaConnection** result,
                                          uint32_t*        len);

extern mama_status
zmqBridgeMamaTransport_getAllConnectionsForTopic (transportBridge* transports,
                                                  int              numTransports,
                                                  const char*      topic,
                                                  mamaConnection** result,
                                                  uint32_t*        len);

extern mama_status
zmqBridgeMamaTransport_requestConflation (transportBridge* transports,
                                          int              numTransports);

extern mama_status
zmqBridgeMamaTransport_requestEndConflation (transportBridge* transports,
                                             int              numTransports);

extern mama_status
zmqBridgeMamaTransport_getAllServerConnections (
                               transportBridge*       transports,
                               int                    numTransports,
                               mamaServerConnection** result,
                               uint32_t*              len);

extern mama_status
zmqBridgeMamaTransport_freeAllServerConnections (
                               transportBridge*        transports,
                               int                     numTransports,
                               mamaServerConnection*   connections,
                               uint32_t                len);

extern mama_status
zmqBridgeMamaTransport_freeAllConnections (transportBridge* transports,
                                           int              numTransports,
                                           mamaConnection*  connections,
                                           uint32_t         len);

extern mama_status
zmqBridgeMamaTransport_getNumLoadBalanceAttributes (
                               const char* name,
                               int*        numLoadBalanceAttributes);

extern mama_status
zmqBridgeMamaTransport_getLoadBalanceSharedObjectName (
                               const char*  name,
                               const char** loadBalanceSharedObjectName);

extern mama_status
zmqBridgeMamaTransport_getLoadBalanceScheme (
                               const char*    name,
                               tportLbScheme* scheme);

extern mama_status
zmqBridgeMamaTransport_sendMsgToConnection (
                               transportBridge transport,
                               mamaConnection  connection,
                               mamaMsg         msg,
                               const char*     topic);

extern mama_status
zmqBridgeMamaTransport_isConnectionIntercepted (
                               mamaConnection connection,
                               uint8_t* result);

extern mama_status
zmqBridgeMamaTransport_installConnectConflateMgr (
                               transportBridge       transport,
                               mamaConflationManager mgr,
                               mamaConnection        connection,
                               conflateProcessCb     processCb,
                               conflateGetMsgCb      msgCb);

extern mama_status
zmqBridgeMamaTransport_uninstallConnectConflateMgr (
                               transportBridge       transport,
                               mamaConflationManager mgr,
                               mamaConnection        connection);

extern mama_status
zmqBridgeMamaTransport_startConnectionConflation (
                               transportBridge        transport,
                               mamaConflationManager  mgr,
                               mamaConnection         connection);

extern mama_status
zmqBridgeMamaTransport_getNativeTransport (transportBridge transport,
                                           void**          result);

extern mama_status
zmqBridgeMamaTransport_getNativeTransportNamingCtx (transportBridge transport,
                                                    void**          result);

extern mama_status zmqBridgeMamaSubscription_create
                              (subscriptionBridge* subscriber,
                               const char*         source,
                               const char*         symbol,
                               mamaTransport       transport,
                               mamaQueue           queue,
                               mamaMsgCallbacks    callback,
                               mamaSubscription    subscription,
                               void*               closure );

extern mama_status
zmqBridgeMamaSubscription_createWildCard (
                               subscriptionBridge* subsc_,
                               const char*         source,
                               const char*         symbol,
                               mamaTransport       transport,
                               mamaQueue           queue,
                               mamaMsgCallbacks    callback,
                               mamaSubscription    subscription,
                               void*               closure );

extern mama_status
zmqBridgeMamaSubscription_mute (subscriptionBridge subscriber);

extern  mama_status
zmqBridgeMamaSubscription_destroy (subscriptionBridge subscriber);

extern int
zmqBridgeMamaSubscription_isValid (subscriptionBridge bridge);

extern int
zmqBridgeMamaSubscription_hasWildcards (subscriptionBridge subscriber);

extern mama_status
zmqBridgeMamaSubscription_getPlatformError (subscriptionBridge subsc,
                                            void** error);

extern int
zmqBridgeMamaSubscription_isTportDisconnected (subscriptionBridge subsc);

extern mama_status
zmqBridgeMamaSubscription_setTopicClosure (subscriptionBridge subsc,
                                           void* closure);

extern mama_status
zmqBridgeMamaSubscription_muteCurrentTopic (subscriptionBridge subsc);

extern mama_status
zmqBridgeMamaTimer_create (timerBridge* timer,
                           void*        nativeQueueHandle,
                           mamaTimerCb  action,
                           mamaTimerCb  onTimerDestroyed,
                           mama_f64_t   interval,
                           mamaTimer    parent,
                           void*        closure);

extern mama_status
zmqBridgeMamaTimer_destroy (timerBridge timer);

extern mama_status
zmqBridgeMamaTimer_reset (timerBridge timer);

extern mama_status
zmqBridgeMamaTimer_setInterval (timerBridge timer, mama_f64_t interval);

extern mama_status
zmqBridgeMamaTimer_getInterval (timerBridge timer, mama_f64_t* interval);

extern mama_status
zmqBridgeMamaIo_create (ioBridge*       result,
                        void*           nativeQueueHandle,
                        uint32_t        descriptor,
                        mamaIoCb        action,
                        mamaIoType      ioType,
                        mamaIo          parent,
                        void*           closure);

extern mama_status
zmqBridgeMamaIo_getDescriptor (ioBridge io, uint32_t* result);

extern mama_status
zmqBridgeMamaIo_destroy (ioBridge io);

extern mama_status
zmqBridgeMamaPublisher_createByIndex (
                              publisherBridge*  result,
                              mamaTransport     tport,
                              int               tportIndex,
                              const char*       topic,
                              const char*       source,
                              const char*       root,
                              void*             nativeQueueHandle,
                              mamaPublisher     parent);

extern mama_status
zmqBridgeMamaPublisher_create (publisherBridge*  result,
                               mamaTransport     tport,
                               const char*       topic,
                               const char*       source,
                               const char*       root,
                               void*             nativeQueueHandle,
                               mamaPublisher     parent);

extern mama_status
zmqBridgeMamaPublisher_destroy (publisherBridge publisher);

extern mama_status
zmqBridgeMamaPublisher_send (publisherBridge publisher, mamaMsg msg);

extern mama_status
zmqBridgeMamaPublisher_sendReplyToInbox (publisherBridge publisher,
                                         void*           request,
                                         mamaMsg         reply);

extern mama_status
zmqBridgeMamaPublisher_sendReplyToInboxHandle (publisherBridge publisher,
                                               void*           wmwReply,
                                               mamaMsg         reply);

extern mama_status
zmqBridgeMamaPublisher_sendFromInboxByIndex (publisherBridge   publisher,
                                             int               tportIndex,
                                             mamaInbox         inbox,
                                             mamaMsg           msg);

extern mama_status
zmqBridgeMamaPublisher_sendFromInbox (publisherBridge publisher,
                                      mamaInbox       inbox,
                                      mamaMsg         msg);

extern mama_status
zmqBridgeMamaInbox_create (
            inboxBridge*                bridge,
            mamaTransport               tport,
            mamaQueue                   queue,
            mamaInboxMsgCallback        msgCB,
            mamaInboxErrorCallback      errorCB,
            mamaInboxDestroyCallback    onInboxDestroyed,
            void*                       closure,
            mamaInbox                   parent);

extern mama_status
zmqBridgeMamaInbox_createByIndex (
            inboxBridge*                bridge,
            mamaTransport               tport,
            int                         tportIndex,
            mamaQueue                   queue,
            mamaInboxMsgCallback        msgCB,
            mamaInboxErrorCallback      errorCB,
            mamaInboxDestroyCallback    onInboxDestroyed,
            void*                       closure,
            mamaInbox                   parent);

extern mama_status
zmqBridgeMamaInbox_destroy (inboxBridge inbox);

extern mama_status
zmqBridgeMamaMsg_create (msgBridge* msg, mamaMsg parent);

extern int
zmqBridgeMamaMsg_isFromInbox (msgBridge msg);

extern mama_status
zmqBridgeMamaMsg_destroy (msgBridge msg, int destroyMsg);

extern mama_status
zmqBridgeMamaMsg_destroyMiddlewareMsg (msgBridge msg);

extern mama_status
zmqBridgeMamaMsg_detach (msgBridge msg);

extern mama_status
zmqBridgeMamaMsg_getPlatformError (msgBridge msg, void** error);

extern mama_status
zmqBridgeMamaMsg_setSendSubject (msgBridge   msg,
                                 const char* symbol,
                                 const char* subject);

extern mama_status
zmqBridgeMamaMsg_getNativeHandle (msgBridge msg, void** result);

extern mama_status
zmqBridgeMamaMsg_duplicateReplyHandle (msgBridge msg, void** result);

extern mama_status
zmqBridgeMamaMsg_copyReplyHandle (void* src, void** dest);

extern mama_status
zmqBridgeMamaMsgImpl_setReplyHandle (msgBridge msg, void* handle);

extern mama_status
zmqBridgeMamaMsgImpl_setReplyHandleAndIncrement (msgBridge msg, void* handle);

extern mama_status
zmqBridgeMamaMsg_destroyReplyHandle (void* handle);

#if defined(__cplusplus)
}
#endif

#endif /*ZMQ_BRIDGE_FUNCTIONS__*/
