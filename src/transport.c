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

#define _GNU_SOURCE

#include <assert.h>

#include <mama/mama.h>
#include <wombat/wInterlocked.h>

#include <queueimpl.h>
#include <msgimpl.h>
#include <queueimpl.h>
#include <subscriptionimpl.h>
#include <transportimpl.h>
#include <timers.h>
#include <stdio.h>
#include <errno.h>
#include <wombat/queue.h>
#include "transport.h"
#include "subscription.h"
#include "zmqdefs.h"
#include "msg.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include "util.h"
#include "inbox.h"
#include <zmq.h>
#include <errno.h>
#include <wombat/mempool.h>
#include <wombat/memnode.h>


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Transport configuration parameters */
#define     TPORT_PARAM_PREFIX                  "mama.zmq.transport"
#define     TPORT_PARAM_OUTGOING_URL            "outgoing_url"
#define     TPORT_PARAM_INCOMING_URL            "incoming_url"
#define     TPORT_PARAM_MSG_POOL_SIZE           "msg_pool_size"
#define     TPORT_PARAM_MSG_NODE_SIZE           "msg_node_size"

// zmq params
#define     TPORT_PARAM_ZMQ_SNDHWM              "zmq_sndhwm"
#define     TPORT_PARAM_ZMQ_RCVHWM              "zmq_rcvhwm"
#define     TPORT_PARAM_ZMQ_AFFINITY            "zmq_affinity"
#define     TPORT_PARAM_ZMQ_IDENTITY            "zmq_identity"
#define     TPORT_PARAM_ZMQ_SNDBUF              "zmq_sndbuf"
#define     TPORT_PARAM_ZMQ_RCVBUF              "zmq_rcvbuf"
#define     TPORT_PARAM_ZMQ_RECONNECT_IVL       "zmq_reconnect_ivl"
#define     TPORT_PARAM_ZMQ_RECONNECT_IVL_MAX   "zmq_reconnect_ivl_max"
#define     TPORT_PARAM_ZMQ_BACKLOG             "zmq_backlog"
#define     TPORT_PARAM_ZMQ_MAXMSGSIZE          "zmq_maxmsgsize"
#define     TPORT_PARAM_ZMQ_RCVTIMEO            "zmq_rcvtimeo"
#define     TPORT_PARAM_ZMQ_SNDTIMEO            "zmq_sndtimeo"
#define     TPORT_PARAM_ZMQ_RATE                "zmq_rate"

// naming params
#define     TPORT_PARAM_ISNAMING                "is_naming"
#define     TPORT_PARAM_NAMING_ADDR             "naming.subscribe_address"
#define     TPORT_PARAM_NAMING_PORT             "naming.subscribe_port"
#define     TPORT_PARAM_PUBLISH_ADDRESS         "publish_address"

/* Default values for corresponding configuration parameters */
#define     DEFAULT_SUB_OUTGOING_URL        "tcp://*:5557"
#define     DEFAULT_SUB_INCOMING_URL        "tcp://127.0.0.1:5556"
#define     DEFAULT_PUB_OUTGOING_URL        "tcp://*:5556"
#define     DEFAULT_PUB_INCOMING_URL        "tcp://127.0.0.1:5557"
#define     DEFAULT_MEMPOOL_SIZE            "1024"
#define     DEFAULT_MEMNODE_SIZE            "4096"

// zmq params
#define     DEFAULT_ZMQ_SNDHWM              "0"       /* ZMQ Default = 1000 */
#define     DEFAULT_ZMQ_RCVHWM              "0"       /* ZMQ Default = 1000 */
#define     DEFAULT_ZMQ_AFFINITY            "0"       /* ZMQ Default        */
#define     DEFAULT_ZMQ_IDENTITY            NULL      /* ZMQ Default        */
#define     DEFAULT_ZMQ_SNDBUF              "0"       /* ZMQ Default        */
#define     DEFAULT_ZMQ_RCVBUF              "0"       /* ZMQ Default        */
#define     DEFAULT_ZMQ_RECONNECT_IVL       "100"     /* ZMQ Default        */
#define     DEFAULT_ZMQ_RECONNECT_IVL_MAX   "0"       /* ZMQ Default        */
#define     DEFAULT_ZMQ_BACKLOG             "100"     /* ZMQ Default        */
#define     DEFAULT_ZMQ_MAXMSGSIZE          "-1"      /* ZMQ Default        */
#define     DEFAULT_ZMQ_RCVTIMEO            "10"      /* ZMQ Default = -1   */
#define     DEFAULT_ZMQ_SNDTIMEO            "-1"      /* ZMQ Default        */
#define     DEFAULT_ZMQ_RATE                "1000000" /* ZMQ Default = 100  */

// naming params
#define     DEFAULT_ISNAMING                "0"
#define     DEFAULT_PUBLISH_ADDRESS         "lo"

/* Non configurable runtime defaults */
#define     PARAM_NAME_MAX_LENGTH           1024L

// TODO: sizeof(int) below?
#define ZMQ_SET_SOCKET_OPTIONS(name, socket,type,opt,map)                      \
   do                                                                          \
   {                                                                           \
      const char* valStr = zmqBridgeMamaTransportImpl_getParameter (           \
                           DEFAULT_ZMQ_ ## opt,                                \
                           "%s.%s.%s",                                         \
                           TPORT_PARAM_PREFIX,                                 \
                           name,                                               \
                           TPORT_PARAM_ZMQ_ ## opt);                           \
      type value = (type) map (valStr);                                        \
                                                                               \
      MAMA_LOG (MAMA_LOG_LEVEL_FINE,                                           \
                "ZeroMQ socket option %s=%s for transport %s",                 \
                TPORT_PARAM_ZMQ_ ## opt,                                       \
                valStr,                                                        \
                name);                                                         \
                                                                               \
      CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_ ## opt, &value, sizeof(int)));\
   } while (0);

/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

/**
 * This function is called in the create function and is responsible for
 * actually subscribing to any transport level data sources and forking off the
 * recv dispatch thread for proton.
 *
 * @param impl  Qpid transport bridge to start
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
static mama_status zmqBridgeMamaTransportImpl_init(zmqTransportBridge* impl);
static mama_status zmqBridgeMamaTransportImpl_start(zmqTransportBridge* impl);

/**
 * This function is called in the destroy function and is responsible for
 * stopping the proton messengers and joining back with the recv thread created
 * in zmqBridgeMamaTransportImpl_start.
 *
 * @param impl  Qpid transport bridge to start
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
static mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl);

/**
 * This function is a queue callback which is enqueued in the recv thread and
 * is then fired once it has reached the head of the queue.
 *
 * @param queue   MAMA queue from which this callback was fired
 * @param closure In this instance, the closure is the zmqMsgNode which was
 *                pulled from the pool in the recv callback and then sent
 *                down the MAMA queue.
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
static void MAMACALLTYPE  zmqBridgeMamaTransportImpl_subCallback(mamaQueue queue, void* closure);
static void MAMACALLTYPE  zmqBridgeMamaTransportImpl_inboxCallback(mamaQueue queue, void* closure);

/**
 * This is a local function for parsing string configuration parameters from the
 * MAMA properties object, and supports default values. This function should
 * be used where the configuration parameter itself can be variable.
 *
 * @param defaultVal This is the default value to use if the parameter does not
 *                   exist in the configuration file
 * @param paramName  The format and variable list combine to form the real
 *                   configuration parameter used. This configuration parameter
 *                   will be stored at this location so the calling function
 *                   can log this.
 * @param format     This is the format string which is used to build the
 *                   name of the configuration parameter which is to be parsed.
 * @param ...        This is the variable list of arguments to be used along
 *                   with the format string.
 *
 * @return const char* containing the parameter value or the default.
 */
static const char*
zmqBridgeMamaTransportImpl_getParameterWithVaList(char*       defaultVal,
                                                  char*       paramName,
                                                  const char* format,
                                                  va_list     arguments);

/**
 * This is a local function for parsing string configuration parameters from the
 * MAMA properties object, and supports default values. This function should
 * be used where the configuration parameter itself can be variable.
 *
 * @param defaultVal This is the default value to use if the parameter does not
 *                   exist in the configuration file
 * @param format     This is the format string which is used to build the
 *                   name of the configuration parameter which is to be parsed.
 * @param ...        This is the variable list of arguments to be used along
 *                   with the format string.
 *
 * @return const char* containing the parameter value or the default.
 */
static const char*
zmqBridgeMamaTransportImpl_getParameter(const char* defaultVal,
                                        const char* format,
                                        ...);

/**
 * This function is called on its own thread to run the main recv dispatch
 * for all messages coming off the mIncoming messenger. This function is
 * responsible for routing all incoming messages to their required destination
 * and parsing all administrative messages.
 *
 * @param closure    In this case, the closure refers to the zmqTransportBridge
 */
static void*
zmqBridgeMamaTransportImpl_dispatchThread(void* closure);

void MAMACALLTYPE  zmqBridgeMamaTransportImpl_queueClosureCleanupCb(void* closure);



// parameter parsing
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseCommonParams(zmqTransportBridge* impl);
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNamingParams(zmqTransportBridge* impl);
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNonNamingParams(zmqTransportBridge* impl);

// socket helpers
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* pSocket, int type);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_closeSocket(void* socket);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri);
// sets socket options as specified in Mama configuration file
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_setSocketOptions(const char* name, zmqSocket* socket);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_disableReconnect(zmqSocket* socket);

// message processing
// The dispatch... functions all run on main dispatch thread, and thus can access the control, normal and
// naming (if applicable) sockets without restriction.
mama_status MAMACALLTYPE  zmqBridgeMamaTransportImpl_dispatchNamingMsg(zmqTransportBridge* zmqTransport, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE  zmqBridgeMamaTransportImpl_dispatchNormalMsg(zmqTransportBridge* zmqTransport, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchControlMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchSubMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_dispatchInboxMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg);

memoryNode* MAMACALLTYPE zmqBridgeMamaTransportImpl_allocTransportMsg(zmqTransportBridge* impl, void* queue, zmq_msg_t* zmsg);

// These subscribe methods operate directly on the zmq socket, and as such should only be called
// from the dispatch thread.
// To subscribe from any other thread, you need to use the zmqBridgeMamaSubscriptionImpl_subscribe function, which
// posts a message to the control socket.
// NOTE: The subscribe method is controlled by the USE_XSUB preprocessor symbol.
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic);
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_unsubscribe(void* socket, const char* topic);

mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_publishEndpoints(zmqTransportBridge* impl);

// "Sometimes" it is necessary to trigger the processing of outstanding commands against a
// zmq socket.
// see https://github.com/zeromq/libzmq/issues/2267
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_kickSocket(void* socket);

/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

int
zmqBridgeMamaTransport_isValid(transportBridge transport)
{
   zmqTransportBridge*    impl   = (zmqTransportBridge*) transport;
   int                    status = 0;

   if (NULL != impl) {
      status = impl->mIsValid;
   }
   return status;
}

mama_status zmqBridgeMamaTransport_destroy(transportBridge transport)
{
   zmqTransportBridge*    impl    = NULL;
   mama_status            status  = MAMA_STATUS_OK;

   if (NULL == transport) {
      return MAMA_STATUS_NULL_ARG;
   }

   impl  = (zmqTransportBridge*) transport;
   status = zmqBridgeMamaTransportImpl_stop(impl);

   // close sockets
   zmqBridgeMamaTransportImpl_closeSocket(impl->mZmqSocketPublisher.mSocket);
   zmqBridgeMamaTransportImpl_closeSocket(impl->mZmqSocketSubscriber.mSocket);
   zmqBridgeMamaTransportImpl_closeSocket(impl->mZmqSocketControl.mSocket);
   if (impl->mIsNaming) {
      zmqBridgeMamaTransportImpl_closeSocket(impl->mZmqNamingPublisher.mSocket);
      zmqBridgeMamaTransportImpl_closeSocket(impl->mZmqNamingSubscriber.mSocket);
   }

   zmq_ctx_destroy(impl->mZmqContext);

   endpointPool_destroy(impl->mSubEndpoints);

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Naming messages = %ld", impl->mNamingMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Normal messages = %ld", impl->mNormalMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Subscription messages = %ld", impl->mSubMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Inbox messages = %ld", impl->mInboxMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Control messages = %ld", impl->mControlMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Polls = %ld", impl->mPolls);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "No polls = %ld", impl->mNoPolls);

   // TODO: lots of stuff in impl is allocated on heap and should be freeed
   free(impl);

   return status;
}

mama_status zmqBridgeMamaTransport_create(transportBridge* result, const char* name, mamaTransport parent)
{
   zmqTransportBridge*   impl            = NULL;
   mama_status           status          = MAMA_STATUS_OK;

   if (NULL == result || NULL == name || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }

   int major, minor, patch;
   zmq_version(&major, &minor, &patch);
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Using libzmq version %d.%d.%d", major, minor, patch);


   impl = (zmqTransportBridge*) calloc(1, sizeof(zmqTransportBridge));

   /* Back reference the MAMA transport */
   impl->mTransport           = parent;

   /* Initialize the dispatch thread pointer */
   impl->mOmzmqDispatchThread  = 0;
   impl->mOmzmqDispatchStatus  = MAMA_STATUS_OK;
   impl->mName                 = name;
   impl->mNamingMessages       = 0;
   impl->mNormalMessages       = 0;
   impl->mSubMessages          = 0;
   impl->mInboxMessages        = 0;
   impl->mControlMessages      = 0;
   impl->mPolls                = 0;
   impl->mNoPolls              = 0;

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Initializing Transport %s", impl->mName);

   // parse params
   zmqBridgeMamaTransportImpl_parseCommonParams(impl);
   if (impl->mIsNaming == 1) {
      zmqBridgeMamaTransportImpl_parseNamingParams(impl);
   }
   else {
      zmqBridgeMamaTransportImpl_parseNonNamingParams(impl);
   }

   // create wildcard endpoints
   impl->mWcEndpoints = list_create(sizeof(zmqSubscription*));
   if (impl->mWcEndpoints == INVALID_LIST) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create wildcard endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   // create inboxes
   impl->mInboxes = wtable_create("inboxes", 1024);
   if (impl->mInboxes == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create inbox endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   // create sub endpoints
   status = endpointPool_create(&impl->mSubEndpoints, "mSubEndpoints");
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create subscribing endpoints");
      free(impl);
      return status;
   }

   // suspect?  make no change unless successful?
   impl->mIsValid = 1;
   *result = (transportBridge) impl;

   // generate inbox subject
   const char* uuid = zmq_generate_uuid();
   char temp[strlen(ZMQ_REPLYHANDLE_PREFIX)+1+UUID_STRING_SIZE+1];
   snprintf(temp, sizeof(temp) - 1, "%s.%s", ZMQ_REPLYHANDLE_PREFIX, uuid);
   free((void*) uuid);
   impl->mInboxSubject = strdup(temp);

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_init(impl));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_start(impl));

   if (impl->mIsNaming) {
      // subscribe to naming msgs
      //CALL_MAMA_FUNC(zmqBridgeMamaSubscriptionImpl_subscribe(impl, &impl->mZmqNamingSubscriber, ZMQ_NAMING_PREFIX));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_publishEndpoints(impl));
   }

   //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_start(impl));

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransport_forceClientDisconnect(transportBridge*   transports,
                                             int                numTransports,
                                             const char*        ipAddress,
                                             uint16_t           port)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_findConnection(transportBridge*    transports,
                                      int                 numTransports,
                                      mamaConnection*     result,
                                      const char*         ipAddress,
                                      uint16_t            port)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getAllConnections(transportBridge*    transports,
                                         int                 numTransports,
                                         mamaConnection**    result,
                                         uint32_t*           len)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getAllConnectionsForTopic(
   transportBridge*    transports,
   int                 numTransports,
   const char*         topic,
   mamaConnection**    result,
   uint32_t*           len)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_requestConflation(transportBridge*     transports,
                                         int                  numTransports)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_requestEndConflation(transportBridge*  transports,
                                            int               numTransports)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getAllServerConnections(
   transportBridge*        transports,
   int                     numTransports,
   mamaServerConnection**  result,
   uint32_t*               len)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_freeAllServerConnections(
   transportBridge*        transports,
   int                     numTransports,
   mamaServerConnection*   result,
   uint32_t                len)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_freeAllConnections(transportBridge*    transports,
                                          int                 numTransports,
                                          mamaConnection*     result,
                                          uint32_t            len)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getNumLoadBalanceAttributes(
   const char*     name,
   int*            numLoadBalanceAttributes)
{
   if (NULL == numLoadBalanceAttributes || NULL == name) {
      return MAMA_STATUS_NULL_ARG;
   }

   *numLoadBalanceAttributes = 0;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransport_getLoadBalanceSharedObjectName(
   const char*     name,
   const char**    loadBalanceSharedObjectName)
{
   if (NULL == loadBalanceSharedObjectName) {
      return MAMA_STATUS_NULL_ARG;
   }

   *loadBalanceSharedObjectName = NULL;
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getLoadBalanceScheme(const char*       name,
                                            tportLbScheme*    scheme)
{
   if (NULL == scheme || NULL == name) {
      return MAMA_STATUS_NULL_ARG;
   }

   *scheme = TPORT_LB_SCHEME_STATIC;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransport_sendMsgToConnection(transportBridge    tport,
                                           mamaConnection     connection,
                                           mamaMsg            msg,
                                           const char*        topic)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_isConnectionIntercepted(mamaConnection connection,
                                               uint8_t*       result)
{
   if (NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }

   *result = 0;
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_installConnectConflateMgr(
   transportBridge         handle,
   mamaConflationManager   mgr,
   mamaConnection          connection,
   conflateProcessCb       processCb,
   conflateGetMsgCb        msgCb)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_uninstallConnectConflateMgr(
   transportBridge         handle,
   mamaConflationManager   mgr,
   mamaConnection          connection)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_startConnectionConflation(
   transportBridge         tport,
   mamaConflationManager   mgr,
   mamaConnection          connection)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status zmqBridgeMamaTransport_getNativeTransport(transportBridge     transport,
                                          void**              result)
{
   zmqTransportBridge* impl = (zmqTransportBridge*)transport;

   if (NULL == transport || NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }
   *result = impl;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransport_getNativeTransportNamingCtx(transportBridge transport,
                                                   void**          result)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

zmqTransportBridge* zmqBridgeMamaTransportImpl_getTransportBridge(mamaTransport transport)
{
   zmqTransportBridge*    impl;
   mama_status             status = MAMA_STATUS_OK;

   status = mamaTransport_getBridgeTransport(transport,
                                             (transportBridge*) &impl);

   if (status != MAMA_STATUS_OK || impl == NULL) {
      return NULL;
   }

   return impl;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/
// NOTE: direction is only relevant for ipc transports
mama_status zmqBridgeMamaTransportImpl_setupSocket(void* socket, const char* uri, zmqTransportDirection direction)
{
   int rc = 0;
   char tportTypeStr[16];
   char* firstColon = NULL;
   zmqTransportType tportType = ZMQ_TPORT_TYPE_UNKNOWN;
   /* If set to non zero, will bind rather than connect */
   int isBinding = 0;

   strncpy(tportTypeStr, uri, sizeof(tportTypeStr));
   tportTypeStr[sizeof(tportTypeStr) - 1] = '\0';
   firstColon = strchr(tportTypeStr, ':');
   if (NULL != firstColon) {
      *firstColon = '\0';
   }

   if (0 == strcmp(tportTypeStr, "tcp")) {
      tportType = ZMQ_TPORT_TYPE_TCP;
   }
   else if (0 == strcmp(tportTypeStr, "epgm")) {
      tportType = ZMQ_TPORT_TYPE_EPGM;
   }
   else if (0 == strcmp(tportTypeStr, "pgm")) {
      tportType = ZMQ_TPORT_TYPE_PGM;
   }
   else if (0 == strcmp(tportTypeStr, "ipc")) {
      tportType = ZMQ_TPORT_TYPE_IPC;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unknown ZeroMQ transport type found: %s.", tportTypeStr);
      return MAMA_STATUS_INVALID_ARG;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Found ZeroMQ transport type %s (%d)", tportTypeStr, tportType);

   /* Get the transport type from the uri */
   switch (direction) {
      case ZMQ_TPORT_DIRECTION_INCOMING:
         switch (tportType) {
            case ZMQ_TPORT_TYPE_TCP:
               if (strchr(uri, '*')) {
                  isBinding = 1;
               }
               break;
            case ZMQ_TPORT_TYPE_EPGM:
            case ZMQ_TPORT_TYPE_PGM:
            case ZMQ_TPORT_TYPE_IPC:
            default:
               break;
         }
         break;
      case ZMQ_TPORT_DIRECTION_OUTGOING:
         switch (tportType) {
            case ZMQ_TPORT_TYPE_TCP:
               if (strchr(uri, '*')) {
                  isBinding = 1;
               }
               break;
            case ZMQ_TPORT_TYPE_IPC:
               isBinding = 1;
               break;
            case ZMQ_TPORT_TYPE_EPGM:
            case ZMQ_TPORT_TYPE_PGM:
            default:
               break;
         }
         break;
   }

   /* If this is a binding transport */
   if (isBinding) {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(socket, uri, NULL));
   }
   else {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(socket, uri));
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_init(zmqTransportBridge* impl)
{
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR,"transport NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   impl->mZmqContext = zmq_ctx_new();
   if (impl->mZmqContext == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to allocate zmq context - error %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   // create command socket
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqSocketControl, ZMQ_CONTROL_LISTENER));
   #ifndef  ZMQ_CONTROL_USEPAIRS
   // if using pub/sub for control, need to subscribe to get any messages
   CALL_ZMQ_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqSocketControl.mSocket, ""));
   #endif
   //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqSocketControl));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqSocketControl,  ZMQ_CONTROL_ENDPOINT, NULL));

   // initialize pub/sub sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqSocketPublisher, ZMQ_PUB_TYPE));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqSocketPublisher));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqSocketSubscriber, ZMQ_SUB_TYPE));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqSocketSubscriber));
   //CALL_ZMQ_FUNC(zmq_setsockopt(impl->mZmqSocketSubscriber.mSocket, ZMQ_SUBSCRIBE, impl->mInboxSubject, strlen(impl->mInboxSubject)));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqSocketSubscriber.mSocket, impl->mInboxSubject));

   if (impl->mIsNaming) {
      // disable reconnect on non-naming sockets
      // (to avoid reconnecting to ephemeral port which may have been re-used by another process)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqSocketPublisher));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqSocketSubscriber));

      // bind pub/sub sockets
      char endpointAddress[1024];
      sprintf(endpointAddress, "tcp://%s:*", impl->mPublishAddress);
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqSocketPublisher,  endpointAddress, &impl->mPubEndpoint));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqSocketSubscriber, endpointAddress, &impl->mSubEndpoint));
      // one or the other, but not both (or will get duplicate msgs)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketPublisher, impl->mSubEndpoint, ZMQ_TPORT_DIRECTION_DONTCARE));
      //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketSubscriber, impl->mPubEndpoint, ZMQ_TPORT_DIRECTION_DONTCARE));

      // create naming sockets & connect to broker(s)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingPublisher, ZMQ_PUB_TYPE));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqNamingPublisher));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingSubscriber, ZMQ_SUB_TYPE));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqNamingSubscriber));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqNamingSubscriber.mSocket, ZMQ_NAMING_PREFIX));

      // connect to proxy for naming messages
      for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
         if (impl->mOutgoingNamingAddress[i]) {
            CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqNamingPublisher, impl->mOutgoingNamingAddress[i], ZMQ_TPORT_DIRECTION_DONTCARE));
         }
         if (impl->mIncomingNamingAddress[i]) {
            CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqNamingSubscriber, impl->mIncomingNamingAddress[i], ZMQ_TPORT_DIRECTION_DONTCARE));
         }
      }
   }
   else {
      // non-naming style
      for (int i = 0; (i < ZMQ_MAX_OUTGOING_URIS) && (NULL != impl->mOutgoingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketPublisher,
                                                               impl->mOutgoingAddress[i], ZMQ_TPORT_DIRECTION_OUTGOING));
         MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully set up outgoing ZeroMQ socket for URI: %s", impl->mOutgoingAddress[i]);
      }

      for (int i = 0; (i < ZMQ_MAX_INCOMING_URIS) && (NULL != impl->mIncomingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketSubscriber,
                                                               impl->mIncomingAddress[i], ZMQ_TPORT_DIRECTION_INCOMING));
         MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully set up incoming ZeroMQ socket for URI: %s", impl->mIncomingAddress[i]);
      }
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_start(zmqTransportBridge* impl)
{
   /* Set the transport bridge mIsDispatching to true. */
   wInterlocked_set(1, &impl->mIsDispatching);

   /* Initialize dispatch thread */
   int rc = wthread_create(&(impl->mOmzmqDispatchThread),
                       NULL, zmqBridgeMamaTransportImpl_dispatchThread, impl);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "wthread_create failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl)
{
   /* There are two mechanisms by which we can stop the transport
    * - Send a special message, which will be picked up by recv
    *   For the instance when there is very little data flowing.
    * - Set the mIsDispatching variable in the transportBridge object to
    *   false, for instances when there is a lot of data flowing.
    */
   mama_status     status = MAMA_STATUS_OK;

   /* Set the transportBridge mIsDispatching to false */
   wInterlocked_set(0, &impl->mIsDispatching);

   zmqControlMsg msg;
   memset(&msg, '\0', sizeof(msg));
   msg.command = 'X';
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(impl, &msg, sizeof(msg)));

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Waiting on dispatch thread to terminate.");

   wthread_join(impl->mOmzmqDispatchThread, NULL);
   status = impl->mOmzmqDispatchStatus;

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Rejoined with status: %s.", mamaStatus_stringForStatus(status));

   // TODO: return status?
   return MAMA_STATUS_OK;
}

// Called when message removed from queue by dispatch thread
// NOTE: Needs to check inbox, which may have been deleted after this event was queued but before it
// was dequeued.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_inboxCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;

   // find the inbox
   zmqInboxImpl* inbox = wtable_lookup(tmsg->mTransport->mInboxes, tmsg->mEndpointIdentifier);
   if (inbox == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "discarding uninteresting message for inbox %s", tmsg->mEndpointIdentifier);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(inbox->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   zmqBridgeMamaInboxImpl_onMsg(NULL, tmpMsg, inbox, NULL);

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}

// Called when message removed from queue by dispatch thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_subCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;

   // find the subscription based on its identifier
   zmqSubscription* subscription = NULL;
   endpointPool_getEndpointByIdentifiers(tmsg->mSubEndpoints, tmsg->mSubject,
                                         tmsg->mEndpointIdentifier, (endpoint_t*) &subscription);

   /* Can't do anything without a subscriber */
   if (NULL == subscription) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "No endpoint found for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
      goto exit;
   }

   // TODO: re-evaluate -- possible race condition?
   // It *appears* that the subscription is valid even after its mTransport member is set to NULL,
   // so do this test first to avoid a SEGV dereferencing impl->mSubEndpoints below
   /* Make sure that the subscription is processing messages */
   if (1 != subscription->mIsNotMuted) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Skipping update - subscription %p is muted.", subscription);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(subscription->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
   }
   else {
      /* Process the message as normal */
      status = mamaSubscription_processMsg(subscription->mMamaSubscription, tmpMsg);
      if (MAMA_STATUS_OK != status) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "mamaSubscription_processMsg() failed. [%s]", mamaStatus_stringForStatus(status));
      }
   }

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}

typedef struct zmqFindWildcardClosure {
   const char*       mEndpointIdentifier;
   zmqSubscription*  mSubscription;
} zmqFindWildcardClosure;

void zmqBridgeMamaTransportImpl_findWildcard(wList dummy, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;

   if (strcmp(subscription->mEndpointIdentifier, closure->mEndpointIdentifier) == 0) {
      closure->mSubscription = subscription;
   }
}

// Called when message removed from queue by dispatch thread
// NOTE: Needs to check subscription, which may have been deleted after this event was queued but before it
// was dequeued.
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_wcCallback(mamaQueue queue, void* closure)
{
   memoryNode* node = (memoryNode*) closure;
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;

   // is this subscription still in the list?
   zmqFindWildcardClosure findClosure;
   findClosure.mEndpointIdentifier = tmsg->mEndpointIdentifier;
   findClosure.mSubscription = NULL;
   list_for_each(tmsg->mTransport->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_findWildcard, &findClosure);
   if (findClosure.mSubscription == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "No endpoint found for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
      goto exit;
   }
   zmqSubscription* subscription = findClosure.mSubscription;

   // TODO: re-evaluate -- possible race condition?
   // It *appears* that the subscription is valid even after its mTransport member is set to NULL,
   // so do this test first to avoid a SEGV dereferencing impl->mSubEndpoints below
   /* Make sure that the subscription is processing messages */
   if (1 != subscription->mIsNotMuted) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Skipping update - subscription %p is muted.", subscription);
      goto exit;
   }

   /* This is the reuseable message stored on the associated MamaQueue */
   mamaMsg tmpMsg = mamaQueueImpl_getMsg(subscription->mMamaQueue);
   if (NULL == tmpMsg) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get cached mamaMsg from event queue.");
      goto exit;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(tmpMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from cached queue mamaMsg [%s]", mamaStatus_stringForStatus(status));
      goto exit;
   }

   /* Unpack this bridge message into a MAMA msg implementation */
   status = zmqBridgeMamaMsgImpl_deserialize(bridgeMsg, tmsg->mNodeBuffer, tmsg->mNodeSize, tmpMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaMsgImpl_deserialize() failed. [%s]", mamaStatus_stringForStatus(status));
   }
   else {
      status = mamaSubscription_processWildCardMsg(subscription->mMamaSubscription, tmpMsg, tmsg->mSubject, subscription->mClosure);
      if (MAMA_STATUS_OK != status) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "mamaSubscription_processMsg() failed. [%s]", mamaStatus_stringForStatus(status));
      }
   }

exit:
   free(tmsg->mEndpointIdentifier);

   // Free the memory node (allocated in zmqBridgeMamaTransportImpl_dispatchThread) to the pool
   zmqQueueBridge* queueImpl = NULL;
   mamaQueue_getNativeHandle(queue, (void**)&queueImpl);
   if (queueImpl) {
      memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure((queueBridge) queueImpl);
      if (pool) {
         memoryPool_returnNode(pool, node);
      }
   }

   return;
}

const char* zmqBridgeMamaTransportImpl_getParameterWithVaList(
   char*       defaultVal,
   char*       paramName,
   const char* format,
   va_list     arguments)
{
   const char* property = NULL;

   /* Create the complete transport property string */
   vsnprintf(paramName, PARAM_NAME_MAX_LENGTH,
             format, arguments);

   /* Get the property out for analysis */
   property = properties_Get(mamaInternal_getProperties(),
                             paramName);

   /* Properties will return NULL if parameter is not specified in configs */
   if (property == NULL) {
      property = defaultVal;
   }

   return property;
}

const char* zmqBridgeMamaTransportImpl_getParameter(
   const char* defaultVal,
   const char* format, ...)
{
   char        paramName[PARAM_NAME_MAX_LENGTH];
   const char* returnVal = NULL;
   /* Create list for storing the parameters passed in */
   va_list     arguments;

   /* Populate list with arguments passed in */
   va_start(arguments, format);

   returnVal = zmqBridgeMamaTransportImpl_getParameterWithVaList(
                  (char*)defaultVal,
                  paramName,
                  format,
                  arguments);

   /* These will be equal if unchanged */
   if (returnVal == defaultVal) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "parameter [%s]: [%s] (Default)", paramName, returnVal);
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "parameter [%s]: [%s] (User Defined)", paramName, returnVal);
   }

   /* Clean up the list */
   va_end(arguments);

   return returnVal;
}


void* zmqBridgeMamaTransportImpl_dispatchThread(void* closure)
{
   zmqTransportBridge* impl = (zmqTransportBridge*)closure;

   zmq_msg_t zmsg;
   zmq_msg_init(&zmsg);

   /*
    * Check if we should be still dispatching.
    * We shouldn't need to lock around this, as we're performing a simple value
    * read - if it changes in the middle of the read, we don't actually care.
    */
   while (1 == wInterlocked_read(&impl->mIsDispatching)) {

      // zmq_poll is really slow?!
      // so try reading directly from data socket, and poll only if no msg ready
      int size = zmq_msg_recv(&zmsg, impl->mZmqSocketSubscriber.mSocket, ZMQ_DONTWAIT);
      if (size != -1) {
         ++impl->mNoPolls;
         zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         continue;
      }

      // no normal msg, so poll
      zmq_pollitem_t items[] = {
         { impl->mZmqSocketControl.mSocket, 0, ZMQ_POLLIN , 0},
         { impl->mZmqSocketSubscriber.mSocket, 0, ZMQ_POLLIN , 0},
         { impl->mZmqNamingSubscriber.mSocket, 0, ZMQ_POLLIN , 0}
      };
      int rc = zmq_poll(items, impl->mIsNaming ? 3 : 2, -1);
      if (rc < 0) {
         break;
      }
      ++impl->mPolls;

      // got command msg?
      if (items[0].revents & ZMQ_POLLIN) {
         // TODO: zmq_poll is supposed to be level-triggered, not edge-triggered, so this should not be necessary?
         int size = zmq_msg_recv(&zmsg, impl->mZmqSocketControl.mSocket, ZMQ_DONTWAIT);
         while (size != -1) {
            zmqBridgeMamaTransportImpl_dispatchControlMsg(impl, &zmsg);
            size = zmq_msg_recv(&zmsg, impl->mZmqSocketControl.mSocket, ZMQ_DONTWAIT);
         }
         continue;
      }

      // got normal msg?
      if (items[1].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqSocketSubscriber.mSocket, 0);
         if (size != -1) {
            zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         }
         continue;
      }

      // got naming msg?
      if (items[2].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqNamingSubscriber.mSocket, 0);
         if (size != -1) {
            zmqBridgeMamaTransportImpl_dispatchNamingMsg(impl, &zmsg);
         }
         continue;
      }
   }

   impl->mOmzmqDispatchStatus = MAMA_STATUS_OK;
   return NULL;
}

void MAMACALLTYPE  zmqBridgeMamaTransportImpl_queueClosureCleanupCb(void* closure)
{
   memoryPool* pool = (memoryPool*) closure;
   if (NULL != pool) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Destroying memory pool for queue %p.", closure);
      memoryPool_destroy(pool, NULL);
   }
}


void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseCommonParams(zmqTransportBridge* impl)
{
   impl->mIsNaming = atoi(zmqBridgeMamaTransportImpl_getParameter(
                             DEFAULT_ISNAMING,
                             "%s.%s.%s",
                             TPORT_PARAM_PREFIX,
                             impl->mName,
                             TPORT_PARAM_ISNAMING));

   impl->mPublishAddress = zmqBridgeMamaTransportImpl_getParameter(
                              DEFAULT_PUBLISH_ADDRESS,
                              "%s.%s.%s",
                              TPORT_PARAM_PREFIX,
                              impl->mName,
                              TPORT_PARAM_PUBLISH_ADDRESS);

   impl->mMemoryPoolSize = atol(zmqBridgeMamaTransportImpl_getParameter(
                                   DEFAULT_MEMPOOL_SIZE,
                                   "%s.%s.%s",
                                   TPORT_PARAM_PREFIX,
                                   impl->mName,
                                   TPORT_PARAM_MSG_POOL_SIZE));

   impl->mMemoryNodeSize = atol(zmqBridgeMamaTransportImpl_getParameter(
                                   DEFAULT_MEMNODE_SIZE,
                                   "%s.%s.%s",
                                   TPORT_PARAM_PREFIX,
                                   impl->mName,
                                   TPORT_PARAM_MSG_NODE_SIZE));
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Any message pools created will contain %lu nodes of %lu bytes.", impl->mMemoryPoolSize, impl->mMemoryNodeSize);
}


// The naming server address can be specified in any of the following formats:
// 1. naming.subscribe_address[_n]/naming.subscribe_port[_n]
// 2. naming.nsd_addr[_n]
// 3. naming.outgoing_url[_n]/naming.incoming_url[_n]
// TODO: implement 2, 3
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNamingParams(zmqTransportBridge* impl)
{
   // nsd addr
   const char* address = zmqBridgeMamaTransportImpl_getParameter(NULL, "%s.%s.%s", TPORT_PARAM_PREFIX, impl->mName, TPORT_PARAM_NAMING_ADDR);
   if (address) {
      int port = atoi(zmqBridgeMamaTransportImpl_getParameter("0","%s.%s.%s", TPORT_PARAM_PREFIX, impl->mName, TPORT_PARAM_NAMING_PORT));
      char endpoint[1024];
      sprintf(endpoint, "tcp://%s:%d", address, port);
      impl->mOutgoingNamingAddress[0] = strdup(endpoint);
      // by convention, subscribe port = publish port +1
      sprintf(endpoint, "tcp://%s:%d", address, port+1);
      impl->mIncomingNamingAddress[0] = strdup(endpoint);
   }

   for (int i = 0; i < ZMQ_MAX_NAMING_URIS; ++i) {
      const char* address = zmqBridgeMamaTransportImpl_getParameter(NULL, "%s.%s.%s_%d", TPORT_PARAM_PREFIX, impl->mName, TPORT_PARAM_NAMING_ADDR, i);
      if (!address) {
         break;
      }
      int port = atoi(zmqBridgeMamaTransportImpl_getParameter("0","%s.%s.%s_%d", TPORT_PARAM_PREFIX, impl->mName, TPORT_PARAM_NAMING_PORT, i));
      char endpoint[1024];
      sprintf(endpoint, "tcp://%s:%d", address, port);
      impl->mOutgoingNamingAddress[i] = strdup(endpoint);
      // by convention, subscribe port = publish port +1
      sprintf(endpoint, "tcp://%s:%d", address, port+1);
      impl->mIncomingNamingAddress[i] = strdup(endpoint);
   }


}

void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNonNamingParams(zmqTransportBridge* impl)
{
   char*                 mDefIncoming    = NULL;
   char*                 mDefOutgoing    = NULL;
   const char*           uri             = NULL;
   int                   uri_index       = 0;

   if (0 == strcmp(impl->mName, "pub")) {
      mDefIncoming = DEFAULT_PUB_INCOMING_URL;
      mDefOutgoing = DEFAULT_PUB_OUTGOING_URL;
   }
   else {
      mDefIncoming = DEFAULT_SUB_INCOMING_URL;
      mDefOutgoing = DEFAULT_SUB_OUTGOING_URL;
   }

   /* Start with bare incoming address */
   impl->mIncomingAddress[0] = zmqBridgeMamaTransportImpl_getParameter(
                                  mDefIncoming,
                                  "%s.%s.%s",
                                  TPORT_PARAM_PREFIX,
                                  impl->mName,
                                  TPORT_PARAM_INCOMING_URL);

   /* Now parse any _0, _1 etc. */
   uri_index = 0;
   while (NULL != (uri = zmqBridgeMamaTransportImpl_getParameter(
                            NULL,
                            "%s.%s.%s_%d",
                            TPORT_PARAM_PREFIX,
                            impl->mName,
                            TPORT_PARAM_INCOMING_URL,
                            uri_index))) {
      impl->mIncomingAddress[uri_index] = uri;
      uri_index++;
   }

   /* Start with bare outgoing address */
   impl->mOutgoingAddress[0] = zmqBridgeMamaTransportImpl_getParameter(
                                  mDefOutgoing,
                                  "%s.%s.%s",
                                  TPORT_PARAM_PREFIX,
                                  impl->mName,
                                  TPORT_PARAM_OUTGOING_URL);

   /* Now parse any _0, _1 etc. */
   uri_index = 0;
   while (NULL != (uri = zmqBridgeMamaTransportImpl_getParameter(
                            NULL,
                            "%s.%s.%s_%d",
                            TPORT_PARAM_PREFIX,
                            impl->mName,
                            TPORT_PARAM_OUTGOING_URL,
                            uri_index))) {
      impl->mOutgoingAddress[uri_index] = uri;
      uri_index++;
   }
}


mama_status zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri)
{
   int rc = zmq_connect(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect failed trying to connect to '%s' %d(%s)", uri, errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect(%x) connected to %s", socket->mSocket, uri);

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket));
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_poll(%x) complete", socket->mSocket, uri);

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName)
{
   int rc = zmq_bind(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_bind failed trying to bind to '%s' %d(%s)", uri, errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   // TODO: superfuous on bind?
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket));

   if (endpointName != NULL) {
      char temp[1024];
      size_t tempSize = sizeof(temp);
      CALL_ZMQ_FUNC(zmq_getsockopt(socket->mSocket, ZMQ_LAST_ENDPOINT, temp, &tempSize));
      *endpointName = strdup(temp);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* pSocket, int type)
{
   void* temp = zmq_socket(zmqContext, type);
   if (temp == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   pSocket->mSocket = temp;
   pSocket->mLock = wlock_create();

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_closeSocket(void* socket)
{
   int linger = 0;
   int rc = zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger));
   assert(rc == 0);
   zmq_close(socket);
   assert(rc == 0);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_disableReconnect(zmqSocket* socket)
{
   int disableReconnect = -1;
   CALL_ZMQ_FUNC(zmq_setsockopt(socket->mSocket, ZMQ_RECONNECT_IVL, &disableReconnect, sizeof(disableReconnect)));
   return MAMA_STATUS_OK;
}

// TODO: commented lines fail with errno 22: invalid argument
mama_status zmqBridgeMamaTransportImpl_setSocketOptions(const char* name, zmqSocket* socket)
{
   // options apply to all sockets (?!)
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDHWM,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVHWM,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDBUF,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVBUF,             atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RECONNECT_IVL,      atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RECONNECT_IVL_MAX,  atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          BACKLOG,            atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RCVTIMEO,           atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          SNDTIMEO,           atoi);
   //ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RATE,               atoi);
   //ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  uint64_t,     AFFINITY,           atoll);
   #if ZMQ_VERSION_MINOR >= 2
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  const char*,  IDENTITY,                );
   #endif
   //ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int64_t,      MAXMSGSIZE,         atoll);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_getInboxSubject(zmqTransportBridge* impl, const char** inboxSubject)
{
   if ((impl == NULL) || (inboxSubject == NULL)) {
      return MAMA_STATUS_NULL_ARG;
   }

   *inboxSubject = impl->mInboxSubject;
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_kickSocket(void* socket)
{
   // see https://github.com/zeromq/libzmq/issues/2267
   zmq_pollitem_t pollitems [] = { { socket, 0, ZMQ_POLLIN, 0 } };
   CALL_ZMQ_FUNC(zmq_poll(pollitems, 1, 1));
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Socket %x subscribing to %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\1', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_SUBSCRIBE, topic, strlen(topic)));
   #endif

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket));

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_unsubscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Socket %x unsubscribing from %s", socket, topic);

   #ifdef USE_XSUB
   char buf[MAX_SUBJECT_LENGTH + 1];
   memset(buf, '\0', sizeof(buf));
   memcpy(&buf[1], topic, strlen(topic));
   CALL_ZMQ_FUNC(zmq_send(socket, buf, strlen(topic) + 1, 0));
   #else
   CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_UNSUBSCRIBE, topic, strlen(topic)));
   #endif

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(socket));

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_dispatchControlMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   impl->mControlMessages++;

   zmqControlMsg* pMsg = zmq_msg_data(zmsg);

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "command=%c arg1=%s", pMsg->command, pMsg->arg1);

   // TODO: fix Q&D copy/paste
   if (pMsg->command == 'S') {
      return zmqBridgeMamaTransportImpl_subscribe(impl->mZmqSocketSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'U') {
      return zmqBridgeMamaTransportImpl_unsubscribe(impl->mZmqSocketSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'X') {
      wInterlocked_set(0, &impl->mIsDispatching);
   }
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_dispatchNamingMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   impl->mNamingMessages++;

   zmqNamingMsg* pMsg = zmq_msg_data(zmsg);

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "type=%c prog=%s host=%s pid=%d topic=%s pub=%s sub=%s", pMsg->mType, pMsg->mProgName, pMsg->mHost, pMsg->mPid, pMsg->mTopic, pMsg->mPubEndpoint, pMsg->mSubEndpoint);

   if (pMsg->mType == 'C') {
      // connect
      // NOTE: multiple connections to same endpoint are silently ignored (see https://github.com/zeromq/libzmq/issues/788)
      // dont connect to self (avoid duplicate msgs)
      if (strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketPublisher, pMsg->mSubEndpoint, ZMQ_TPORT_DIRECTION_DONTCARE));
      if (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setupSocket(&impl->mZmqSocketSubscriber, pMsg->mPubEndpoint, ZMQ_TPORT_DIRECTION_DONTCARE));
   }
   else {
      // TODO: implement disconnect
      // needed in case another process binds to same port
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_dispatchNormalMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   const char* subject = (char*) zmq_msg_data(zmsg);
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Got msg with subject %s", subject);

   impl->mNormalMessages++;

   if (memcmp(subject, ZMQ_REPLYHANDLE_PREFIX, strlen(ZMQ_REPLYHANDLE_PREFIX)) == 0) {
      return zmqBridgeMamaTransportImpl_dispatchInboxMsg(impl, subject, zmsg);
   }
   else {
      return zmqBridgeMamaTransportImpl_dispatchSubMsg(impl, subject, zmsg);
   }
}

mama_status zmqBridgeMamaTransportImpl_dispatchInboxMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg)
{
   impl->mInboxMessages++;

   const char* inboxName = &subject[ZMQ_REPLYHANDLE_INBOXNAME_INDEX];
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, inboxName);
   if (inbox == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "discarding uninteresting message for subject %s", subject);
      return MAMA_STATUS_NOT_FOUND;
   }

   // TODO: can/should move following to zmqBridgeMamaTransportImpl_queueCallback?
   memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(impl, inbox->mZmqQueue, zmsg);
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mEndpointIdentifier = strdup(inboxName);

   // callback (queued) will release the message
   zmqBridgeMamaQueue_enqueueEvent(inbox->mZmqQueue, zmqBridgeMamaTransportImpl_inboxCallback, node);

   return MAMA_STATUS_OK;
}

typedef struct zmqWildcardClosure {
   const char* subject;
   zmq_msg_t*  zmsg;
} zmqWildcardClosure;

void zmqBridgeMamaTransportImpl_matchWildcards(wList dummy, zmqSubscription** pSubscription, zmqWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;

   // check subject up to size of subscribed topic
   if (strncmp(subscription->mSubjectKey, closure->subject, strlen(subscription->mSubjectKey)) != 0) {
      return;
   }

   // topics match -- check regex
   if (regexec(subscription->mRegexTopic, closure->subject, 0, NULL, 0) != 0) {
      return;
   }

   // it's a match
   memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(subscription->mTransport, subscription->mZmqQueue, closure->zmsg);
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mSubEndpoints = NULL;
   tmsg->mEndpointIdentifier = strdup(subscription->mEndpointIdentifier);

   // callback (queued) will release the message
   zmqBridgeMamaQueue_enqueueEvent(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_wcCallback, node);
}

mama_status zmqBridgeMamaTransportImpl_dispatchSubMsg(zmqTransportBridge* impl, const char* subject, zmq_msg_t* zmsg)
{
   impl->mSubMessages++;

   // process wilcard subscriptions
   zmqWildcardClosure wcClosure;
   wcClosure.subject = subject;
   wcClosure.zmsg = zmsg;
   list_for_each(impl->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_matchWildcards, &wcClosure);

   // get list of subscribers for this subject
   endpoint_t* subs = NULL;
   size_t subCount = 0;
   mama_status status = endpointPool_getRegistered(impl->mSubEndpoints, subject, &subs, &subCount);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s) querying registration table for subject %s", status, mamaStatus_stringForStatus(status), subject);
      return MAMA_STATUS_SYSTEM_ERROR;
   }
   if (0 == subCount) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "discarding uninteresting message for subject %s", subject);
      return MAMA_STATUS_NOT_FOUND;
   }

   // process each subscriber
   for (size_t subInc = 0; subInc < subCount; subInc++) {
      zmqSubscription*  subscription = (zmqSubscription*)subs[subInc];

      // TODO: what is the purpose of this?
      if (1 == subscription->mIsTportDisconnected) {
         subscription->mIsTportDisconnected = 0;
      }

      if (1 != subscription->mIsNotMuted) {
         MAMA_LOG(MAMA_LOG_LEVEL_WARN, "muted - not queueing update for symbol %s", subject);
         return MAMA_STATUS_NOT_FOUND;
      }

      memoryNode* node = zmqBridgeMamaTransportImpl_allocTransportMsg(impl, subscription->mZmqQueue, zmsg);
      zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
      tmsg->mSubEndpoints = impl->mSubEndpoints;
      tmsg->mEndpointIdentifier = strdup(subscription->mEndpointIdentifier);

      // callback (queued) will release the message
      zmqBridgeMamaQueue_enqueueEvent(subscription->mZmqQueue, zmqBridgeMamaTransportImpl_subCallback, node);
   }

   return MAMA_STATUS_OK;
}


memoryNode* zmqBridgeMamaTransportImpl_allocTransportMsg(zmqTransportBridge* impl, void* queue, zmq_msg_t* zmsg)
{
   queueBridge queueImpl = (queueBridge) queue;
   memoryPool* pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure(queueImpl);
   if (NULL == pool) {
      pool = memoryPool_create(impl->mMemoryPoolSize, impl->mMemoryNodeSize);
      zmqBridgeMamaQueueImpl_setClosure(queueImpl, pool, zmqBridgeMamaTransportImpl_queueClosureCleanupCb);
   }

   memoryNode* node = memoryPool_getNode(pool, sizeof(zmqTransportMsg) + zmq_msg_size(zmsg));
   zmqTransportMsg* tmsg = (zmqTransportMsg*) node->mNodeBuffer;
   tmsg->mTransport    = impl;
   tmsg->mNodeBuffer   = (uint8_t*)(tmsg + 1);
   tmsg->mNodeSize     = zmq_msg_size(zmsg);
   tmsg->mSubEndpoints = NULL;
   tmsg->mEndpointIdentifier = NULL;
   memcpy(tmsg->mNodeBuffer, zmq_msg_data(zmsg), tmsg->mNodeSize);

   return node;
}

mama_status zmqBridgeMamaTransportImpl_registerInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   return wtable_insert(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX], inbox) >= 0 ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
}

mama_status zmqBridgeMamaTransportImpl_unregisterInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   return wtable_remove(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX]) == inbox ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
}



mama_status zmqBridgeMamaTransportImpl_sendCommand(zmqTransportBridge* impl, zmqControlMsg* msg, int msgSize)
{
   mama_status status = MAMA_STATUS_OK;
   void* temp = zmq_socket(impl->mZmqContext, ZMQ_CONTROL_SENDER);
   if (temp == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket failed %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   int rc = zmq_connect(temp, ZMQ_CONTROL_ENDPOINT);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect failed trying to connect to '%s' %d(%s)", ZMQ_CONTROL_ENDPOINT, errno, zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
      goto close;
   }

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(temp));

   int i = zmq_send(temp, msg, msgSize, 0);
   if (i <= 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_send failed  %d(%s)", errno, zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   // this hangs ... zmq_poll deadlocked in this thread and dispatch thread
   //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(temp));

   //zmq_disconnect(temp, ZMQ_CONTROL_ENDPOINT);

close:
   zmq_close(temp);

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "command=%c arg1=%s", msg->command, msg->arg1);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_publishEndpoints(zmqTransportBridge* impl)
{
   // publish our endpoints
   zmqNamingMsg msg;
   memset(&msg, '\0', sizeof(msg));
   strcpy(msg.mTopic, ZMQ_NAMING_PREFIX);
   msg.mType = 'C';       // connect
   strcpy(msg.mProgName, program_invocation_short_name);
   gethostname(msg.mHost, sizeof(msg.mHost));
   msg.mPid = getpid();
   strncpy(msg.mPubEndpoint, impl->mPubEndpoint, sizeof(msg.mPubEndpoint) - 1);
   strncpy(msg.mSubEndpoint, impl->mSubEndpoint, sizeof(msg.mSubEndpoint) - 1);
   //WLOCK_LOCK(impl->mZmqNamingPublisher.mLock);
   CALL_ZMQ_FUNC(zmq_send(impl->mZmqNamingPublisher.mSocket, &msg, sizeof(msg), 0));
   //WLOCK_UNLOCK(impl->mZmqNamingPublisher.mLock);

   MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "prog=%s host=%s pid=%d pub=%s sub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mPubEndpoint, msg.mSubEndpoint);

   return MAMA_STATUS_OK;
}


