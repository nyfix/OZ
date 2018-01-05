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

// required for definition of program_invocation_short_name, which is used for
// naming messages
#define _GNU_SOURCE

// system includes
#include <stdio.h>
#include <errno.h>
#include <assert.h>

// MAMA includes
#include <mama/mama.h>
#include <wombat/wInterlocked.h>
#include <wombat/mempool.h>
#include <wombat/memnode.h>
#include <wombat/queue.h>
#include <queueimpl.h>
#include <msgimpl.h>
#include <queueimpl.h>
#include <subscriptionimpl.h>
#include <transportimpl.h>
#include <timers.h>

// local includes
#include "transport.h"
#include "subscription.h"
#include "zmqdefs.h"
#include "msg.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
#include "util.h"
#include "inbox.h"

// required for definition of ZMQ_CLIENT, ZMQ_SERVER
#define ZMQ_BUILD_DRAFT_API
#include <zmq.h>


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

// misc
// Note that hash table size is actually 10x value specified in wtable_create
// (to reduce collisions), and that there is no limit to # of entries
// So a table of size 1024 will use 8MB (1024*10*sizeof(void*))
#define     INBOX_TABLE_SIZE                 1024


// TODO: should all sockets have same options?
#define ZMQ_SET_SOCKET_OPTIONS(name, socket, type, opt, map)                     \
   do                                                                            \
   {                                                                             \
      const char* valStr = zmqBridgeMamaTransportImpl_getParameter (             \
                           DEFAULT_ZMQ_ ## opt,                                  \
                           "%s.%s.%s",                                           \
                           TPORT_PARAM_PREFIX,                                   \
                           name,                                                 \
                           TPORT_PARAM_ZMQ_ ## opt);                             \
      type value = (type) map (valStr);                                          \
                                                                                 \
      MAMA_LOG (MAMA_LOG_LEVEL_FINER,                                             \
                "ZeroMQ socket option %s=%s for transport %s",                   \
                TPORT_PARAM_ZMQ_ ## opt,                                         \
                valStr,                                                          \
                name);                                                           \
                                                                                 \
      CALL_ZMQ_FUNC(zmq_setsockopt (socket, ZMQ_ ## opt, &value, sizeof(type))); \
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
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_destroySocket(zmqSocket* socket);
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

   // stop the dispatcher(s)
   status = zmqBridgeMamaTransportImpl_stop(impl);
   wsem_destroy(&impl->mIsReady);
   wsem_destroy(&impl->mNamingConnected);

   // shtudown zmq
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataPublisher);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqDataSubscriber);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlPublisher);
   zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqControlSubscriber);
   if (impl->mIsNaming) {
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingPublisher);
      zmqBridgeMamaTransportImpl_destroySocket(&impl->mZmqNamingSubscriber);
   }
   //zmq_ctx_term(impl->mZmqContext);
   zmq_ctx_shutdown(impl->mZmqContext);

   // free memory
   endpointPool_destroy(impl->mSubEndpoints);
   wlock_destroy(impl->mInboxesLock);
   wtable_destroy(impl->mInboxes);
   list_destroy(impl->mWcEndpoints, NULL, NULL);
   free((void*) impl->mInboxSubject);
   free((void*) impl->mPubEndpoint);
   free((void*) impl->mSubEndpoint);

   for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
      free((void*) impl->mOutgoingNamingAddress[i]);
      free((void*) impl->mIncomingNamingAddress[i]);
   }

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Naming messages = %ld", impl->mNamingMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Normal messages = %ld", impl->mNormalMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Subscription messages = %ld", impl->mSubMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Inbox messages = %ld", impl->mInboxMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Control messages = %ld", impl->mControlMessages);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Polls = %ld", impl->mPolls);
   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "No polls = %ld", impl->mNoPolls);

   free(impl);

   return status;
}

mama_status zmqBridgeMamaTransport_create(transportBridge* result, const char* name, mamaTransport parent)
{
   if (NULL == result || NULL == name || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }
   mama_status status = MAMA_STATUS_OK;

   zmqTransportBridge* impl = (zmqTransportBridge*) calloc(1, sizeof(zmqTransportBridge));

   /* Back reference the MAMA transport */
   impl->mTransport           = parent;

   /* Initialize the dispatch thread pointer */
   impl->mOmzmqDispatchThread  = 0;
   impl->mOmzmqDispatchStatus  = MAMA_STATUS_OK;
   impl->mName                 = name;

   wsem_init(&impl->mIsReady, 0, 0);

   // initialize counters
   impl->mNamingMessages       = 0;
   impl->mNormalMessages       = 0;
   impl->mSubMessages          = 0;
   impl->mInboxMessages        = 0;
   impl->mControlMessages      = 0;
   impl->mPolls                = 0;
   impl->mNoPolls              = 0;

   MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Initializing Transport %s", impl->mName);

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
   impl->mInboxes = wtable_create("inboxes", INBOX_TABLE_SIZE);
   if (impl->mInboxes == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create inbox endpoints");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }
   impl->mInboxesLock = wlock_create();

   // create sub endpoints
   status = endpointPool_create(&impl->mSubEndpoints, "mSubEndpoints");
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create subscribing endpoints");
      free(impl);
      return status;
   }

   // generate inbox subject
   const char* uuid = zmq_generate_uuid();
   char temp[strlen(ZMQ_REPLYHANDLE_PREFIX)+1+UUID_STRING_SIZE+1];
   snprintf(temp, sizeof(temp) - 1, "%s.%s", ZMQ_REPLYHANDLE_PREFIX, uuid);
   free((void*) uuid);
   impl->mInboxSubject = strdup(temp);

   // connect/bind/subscribe/etc. all sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_init(impl));

   // start the dispatch thread
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_start(impl));

   if (impl->mIsNaming) {
      // publish connection information
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_publishEndpoints(impl));
   }

   // TODO: suspect?  make no change unless successful?
   impl->mIsValid = 1;
   *result = (transportBridge) impl;

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
   mama_status status = mamaTransport_getBridgeTransport(transport, (transportBridge*) &impl);
   if (status != MAMA_STATUS_OK || impl == NULL) {
      return NULL;
   }

   return impl;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/

// NOTE: direction is only relevant for ipc transports, for others it is
// inferred from endpoint string (wildcard => bind, non-wildcard => connect)
mama_status zmqBridgeMamaTransportImpl_bindOrConnect(void* socket, const char* uri, zmqTransportDirection direction)
{
   int rc = 0;
   char tportTypeStr[16];
   char* firstColon = NULL;
   zmqTransportType tportType = ZMQ_TPORT_TYPE_UNKNOWN;
   /* If set to non zero, will bind rather than connect */
   int isBinding = 0;

   wmStrSizeCpy(tportTypeStr, uri, sizeof(tportTypeStr));
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

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Found ZeroMQ transport type %s (%d)", tportTypeStr, tportType);

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
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully bound socket to: %s", uri);
   }
   else {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(socket, uri));
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Successfully connected socket to: %s", uri);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_init(zmqTransportBridge* impl)
{
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR,"transport NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   // create context
   impl->mZmqContext = zmq_ctx_new();
   if (impl->mZmqContext == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unable to allocate zmq context - error %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   // create control sockets for inter-thread commands
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlSubscriber, ZMQ_SERVER));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqControlSubscriber,  ZMQ_CONTROL_ENDPOINT, NULL));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqControlPublisher, ZMQ_CLIENT));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqControlPublisher,  ZMQ_CONTROL_ENDPOINT));

   // create data sockets
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataPublisher, ZMQ_PUB_TYPE));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqDataSubscriber, ZMQ_SUB_TYPE));
   // set socket options as per mama.properties etc.
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqDataPublisher));
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_setSocketOptions(impl->mName, &impl->mZmqDataSubscriber));

   // subscribe to inbox subjects
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSubscriber.mSocket, impl->mInboxSubject));

   if (impl->mIsNaming) {
      // disable reconnect on data sockets
      // (to avoid reconnecting to ephemeral port which may have been re-used by another process)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqDataPublisher));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_disableReconnect(&impl->mZmqDataSubscriber));

      // bind pub/sub sockets
      char endpointAddress[ZMQ_MAX_ENDPOINT_LENGTH];
      sprintf(endpointAddress, "tcp://%s:*", impl->mPublishAddress);
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqDataPublisher,  endpointAddress, &impl->mPubEndpoint));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindSocket(&impl->mZmqDataSubscriber, endpointAddress, &impl->mSubEndpoint));
      // one or the other, but not both (or will get duplicate msgs)
      //CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataPublisher, impl->mSubEndpoint));
      // prefer connecting sub to pub (see https://github.com/zeromq/libzmq/issues/2267)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataSubscriber, impl->mPubEndpoint));

      // create naming sockets & connect to broker(s)
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingPublisher, ZMQ_PUB_TYPE));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_createSocket(impl->mZmqContext, &impl->mZmqNamingSubscriber, ZMQ_SUB_TYPE));
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_subscribe(impl->mZmqNamingSubscriber.mSocket, ZMQ_NAMING_PREFIX));

      // connect to proxy for naming messages
      for (int i = 0; (i < ZMQ_MAX_NAMING_URIS); ++i) {
         if (impl->mOutgoingNamingAddress[i]) {
            CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingPublisher, impl->mOutgoingNamingAddress[i]));
         }
         if (impl->mIncomingNamingAddress[i]) {
            CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqNamingSubscriber, impl->mIncomingNamingAddress[i]));
         }
      }
   }
   else {
      // non-naming style
      for (int i = 0; (i < ZMQ_MAX_OUTGOING_URIS) && (NULL != impl->mOutgoingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataPublisher,
                                                               impl->mOutgoingAddress[i], ZMQ_TPORT_DIRECTION_OUTGOING));
      }

      for (int i = 0; (i < ZMQ_MAX_INCOMING_URIS) && (NULL != impl->mIncomingAddress[i]); i++) {
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_bindOrConnect(&impl->mZmqDataSubscriber,
                                                               impl->mIncomingAddress[i], ZMQ_TPORT_DIRECTION_INCOMING));
      }
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_start(zmqTransportBridge* impl)
{
   /* Initialize dispatch thread */
   int rc = wthread_create(&(impl->mOmzmqDispatchThread), NULL, zmqBridgeMamaTransportImpl_dispatchThread, impl);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "wthread_create failed %d(%s)", rc, strerror(rc));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_stop(zmqTransportBridge* impl)
{
   // make sure that transport has started before we try to stop it
   // prevents a race condition on mIsDispatching
   wsem_wait(&impl->mIsReady);

   zmqControlMsg msg;
   memset(&msg, '\0', sizeof(msg));
   msg.command = 'X';
   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendCommand(impl, &msg, sizeof(msg)));

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Waiting on dispatch thread to terminate.");
   wthread_join(impl->mOmzmqDispatchThread, NULL);
   mama_status status = impl->mOmzmqDispatchStatus;
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
   zmqTransportBridge* impl = (zmqTransportBridge*) tmsg->mTransport;

   // find the inbox
   wlock_lock(impl->mInboxesLock);
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, tmsg->mEndpointIdentifier);
   wlock_unlock(impl->mInboxesLock);
   if (inbox == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for inbox %s", tmsg->mEndpointIdentifier);
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


// helper definitions/functions for wildcard subscriptions
typedef struct zmqFindWildcardClosure {
   const char*       mEndpointIdentifier;
   zmqSubscription*  mSubscription;
} zmqFindWildcardClosure;

void zmqBridgeMamaTransportImpl_removeWildcard(wList wcList, zmqSubscription** pSubscription, zmqFindWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;
   if (strcmp(subscription->mEndpointIdentifier, closure->mEndpointIdentifier) == 0) {
      list_remove_element(wcList, pSubscription);
      list_free_element(wcList, pSubscription);
   }
}

void zmqBridgeMamaTransportImpl_unregisterWildcard(zmqTransportBridge* impl, zmqSubscription* subscription)
{
   zmqFindWildcardClosure findClosure;
   findClosure.mEndpointIdentifier = subscription->mEndpointIdentifier;
   findClosure.mSubscription = NULL;
   list_for_each(impl->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_removeWildcard, &findClosure);
}


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
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "No endpoint found for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
      goto exit;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found wildcard subscriber for topic %s with id %s", tmsg->mSubject, tmsg->mEndpointIdentifier);
   zmqSubscription* subscription = findClosure.mSubscription;

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



#define PARAM_NAME_MAX_LENGTH 1024

const char* zmqBridgeMamaTransportImpl_getParameterWithVaList(char* defaultVal, char* paramName, const char* format, va_list arguments)
{
   const char* property = NULL;

   /* Create the complete transport property string */
   vsnprintf(paramName, PARAM_NAME_MAX_LENGTH, format, arguments);

   /* Get the property out for analysis */
   property = properties_Get(mamaInternal_getProperties(), paramName);

   /* Properties will return NULL if parameter is not specified in configs */
   if (property == NULL) {
      property = defaultVal;
   }

   return property;
}

const char* zmqBridgeMamaTransportImpl_getParameter(const char* defaultVal, const char* format, ...)
{
   char        paramName[PARAM_NAME_MAX_LENGTH];

   /* Create list for storing the parameters passed in */
   va_list     arguments;
   va_start(arguments, format);

   const char* returnVal = zmqBridgeMamaTransportImpl_getParameterWithVaList((char*)defaultVal, paramName, format, arguments);
   /* These will be equal if unchanged */
   if (returnVal == defaultVal) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "parameter [%s]: [%s] (Default)", paramName, returnVal);
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "parameter [%s]: [%s] (User Defined)", paramName, returnVal);
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

   /* Set the transport bridge mIsDispatching to true. */
   wInterlocked_initialize(&impl->mIsDispatching);
   wInterlocked_set(1, &impl->mIsDispatching);

   // force _stop method to wait for this
   // prevents a race condition on mIsDispatching
   wsem_post(&impl->mIsReady);


   wlock_lock(impl->mZmqDataSubscriber.mLock);
   if (impl->mIsNaming) {
      wlock_lock(impl->mZmqNamingSubscriber.mLock);
   }


   // Check if we should be still dispatching.
   while (1 == wInterlocked_read(&impl->mIsDispatching)) {

      // TODO: revisit?
      //  try reading directly from data socket, and poll only if no msg ready
      int size = zmq_msg_recv(&zmsg, impl->mZmqDataSubscriber.mSocket, ZMQ_DONTWAIT);
      if (size != -1) {
         ++impl->mNoPolls;
         zmqBridgeMamaTransportImpl_dispatchNormalMsg(impl, &zmsg);
         continue;
      }

      // no normal msg, so poll
      zmq_pollitem_t items[] = {
         { impl->mZmqControlSubscriber.mSocket, 0, ZMQ_POLLIN , 0},
         { impl->mZmqDataSubscriber.mSocket, 0, ZMQ_POLLIN , 0},
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
         int size = zmq_msg_recv(&zmsg, impl->mZmqControlSubscriber.mSocket, ZMQ_DONTWAIT);
         while (size != -1) {
            zmqBridgeMamaTransportImpl_dispatchControlMsg(impl, &zmsg);
            size = zmq_msg_recv(&zmsg, impl->mZmqControlSubscriber.mSocket, ZMQ_DONTWAIT);
         }
         continue;
      }

      // got normal msg?
      if (items[1].revents & ZMQ_POLLIN) {
         int size = zmq_msg_recv(&zmsg, impl->mZmqDataSubscriber.mSocket, 0);
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

   wlock_unlock(impl->mZmqDataSubscriber.mLock);
   if (impl->mIsNaming) {
      wlock_unlock(impl->mZmqNamingSubscriber.mLock);
   }

   impl->mOmzmqDispatchStatus = MAMA_STATUS_OK;
   return NULL;
}

void MAMACALLTYPE  zmqBridgeMamaTransportImpl_queueClosureCleanupCb(void* closure)
{
   memoryPool* pool = (memoryPool*) closure;
   if (NULL != pool) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Destroying memory pool for queue %p.", closure);
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
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Any message pools created will contain %lu nodes of %lu bytes.", impl->mMemoryPoolSize, impl->mMemoryNodeSize);
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


mama_status zmqBridgeMamaTransportImpl_createSocket(void* zmqContext, zmqSocket* pSocket, int type)
{
   void* temp = zmq_socket(zmqContext, type);
   if (temp == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_socket failed %d(%s)", zmq_errno(), zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   pSocket->mSocket = temp;
   pSocket->mLock = wlock_create();

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "(%p, %d) succeeded", pSocket->mSocket, type);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_connectSocket(zmqSocket* socket, const char* uri)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_connect(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_connect(%p, %s) failed: %d(%s)", socket->mSocket, uri, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINE, "zmq_connect(%p, %s) succeeded", socket->mSocket, uri);
      status = zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket);
   }
   wlock_unlock(socket->mLock);

   return status;
}

mama_status zmqBridgeMamaTransportImpl_bindSocket(zmqSocket* socket, const char* uri, const char** endpointName)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int rc = zmq_bind(socket->mSocket, uri);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_bind(%x, %s) failed %d(%s)", socket->mSocket, uri, errno, zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   else {
      zmqBridgeMamaTransportImpl_kickSocket(socket->mSocket);
   }

   if (endpointName != NULL) {
      char temp[ZMQ_MAX_ENDPOINT_LENGTH];
      size_t tempSize = sizeof(temp);
      int rc = zmq_getsockopt(socket->mSocket, ZMQ_LAST_ENDPOINT, temp, &tempSize);
      if (0 != rc) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_getsockopt(%x) failed trying to get last endpoint %d(%s)", socket->mSocket, errno, zmq_strerror(errno));
         status = MAMA_STATUS_PLATFORM;
      }
      else {
         *endpointName = strdup(temp);
      }
   }
   wlock_unlock(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_destroySocket(zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);

   int linger = 0;
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_LINGER, &linger, sizeof(linger));
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%x) failed trying to set linger %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   rc = zmq_close(socket->mSocket);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_close(%x) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }

   wlock_unlock(socket->mLock);

   wlock_destroy(socket->mLock);

   return status;
}


mama_status zmqBridgeMamaTransportImpl_disableReconnect(zmqSocket* socket)
{
   mama_status status = MAMA_STATUS_OK;

   wlock_lock(socket->mLock);
   int reconnectInterval = -1;
   int rc = zmq_setsockopt(socket->mSocket, ZMQ_RECONNECT_IVL, &reconnectInterval, sizeof(reconnectInterval));
   if (rc != 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_setsockopt(%x) failed %d(%s)", socket->mSocket, zmq_errno(), zmq_strerror(errno));
      status = MAMA_STATUS_PLATFORM;
   }
   wlock_unlock(socket->mLock);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_setSocketOptions(const char* name, zmqSocket* socket)
{
   wlock_lock(socket->mLock);
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
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int,          RATE,               atoi);
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  uint64_t,     AFFINITY,           atoll);
   #if ZMQ_VERSION_MINOR >= 2
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  const char*,  IDENTITY,                );
   #endif
   ZMQ_SET_SOCKET_OPTIONS(name, socket->mSocket,  int64_t,      MAXMSGSIZE,         atoll);
   wlock_unlock(socket->mLock);

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


// caller needs to have acquired lock
mama_status zmqBridgeMamaTransportImpl_kickSocket(void* socket)
{
   // see https://github.com/zeromq/libzmq/issues/2267
   zmq_pollitem_t pollitems [] = { { socket, 0, ZMQ_POLLIN, 0 } };
   CALL_ZMQ_FUNC(zmq_poll(pollitems, 1, 1));
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "zmq_poll(%x) complete", socket);
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_subscribe(void* socket, const char* topic)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %x subscribing to %s", socket, topic);

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
   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Socket %x unsubscribing from %s", socket, topic);

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

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "command=%c arg1=%s", pMsg->command, pMsg->arg1);

   // TODO: fix Q&D copy/paste
   if (pMsg->command == 'S') {
      return zmqBridgeMamaTransportImpl_subscribe(impl->mZmqDataSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'U') {
      return zmqBridgeMamaTransportImpl_unsubscribe(impl->mZmqDataSubscriber.mSocket, pMsg->arg1);
   }
   else if (pMsg->command == 'X') {
      wInterlocked_set(0, &impl->mIsDispatching);
   }
   else if (pMsg->command == 'N') {
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Unknown command=%c", pMsg->command);
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_dispatchNamingMsg(zmqTransportBridge* impl, zmq_msg_t* zmsg)
{
   impl->mNamingMessages++;

   zmqNamingMsg* pMsg = zmq_msg_data(zmsg);

   MAMA_LOG(MAMA_LOG_LEVEL_FINE, "Received naming msg: type=%c prog=%s host=%s pid=%d topic=%s pub=%s sub=%s", pMsg->mType, pMsg->mProgName, pMsg->mHost, pMsg->mPid, pMsg->mTopic, pMsg->mPubEndpoint, pMsg->mSubEndpoint);

   if (pMsg->mType == 'C') {
      // connect

      // is this our msg?
      if ((strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) == 0) && (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) == 0)) {
         MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "Got own endpoint msg -- signaling event");
         wsem_post(&impl->mNamingConnected);
         // NOTE: dont connect to self (avoid duplicate msgs)
         return MAMA_STATUS_OK;
      }

      // NOTE: multiple connections to same endpoint are silently ignored (see https://github.com/zeromq/libzmq/issues/788)
      // NOTE: dont connect to self (avoid duplicate msgs)
      if (strcmp(pMsg->mSubEndpoint, impl->mSubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataPublisher, pMsg->mSubEndpoint));
      if (strcmp(pMsg->mPubEndpoint, impl->mPubEndpoint) != 0)
         CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_connectSocket(&impl->mZmqDataSubscriber, pMsg->mPubEndpoint));
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
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Got msg with subject %s", subject);

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

   // index directly into subject to pick up inbox name (last part)
   const char* inboxName = &subject[ZMQ_REPLYHANDLE_INBOXNAME_INDEX];
   wlock_lock(impl->mInboxesLock);
   zmqInboxImpl* inbox = wtable_lookup(impl->mInboxes, inboxName);
   wlock_unlock(impl->mInboxesLock);
   if (inbox == NULL) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for subject %s", subject);
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


// wildcard support
typedef struct zmqWildcardClosure {
   const char* subject;
   zmq_msg_t*  zmsg;
   int         found;
} zmqWildcardClosure;

void zmqBridgeMamaTransportImpl_matchWildcards(wList dummy, zmqSubscription** pSubscription, zmqWildcardClosure* closure)
{
   zmqSubscription* subscription = *pSubscription;

   // check topic up to size of subscribed topic
   if (memcmp(subscription->mSubjectKey, closure->subject, strlen(subscription->mSubjectKey)) != 0) {
      return;
   }

   // check regex
   if (regexec(subscription->mRegexTopic, closure->subject, 0, NULL, 0) != 0) {
      return;
   }

   // it's a match
   closure->found++;
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
   wcClosure.found = 0;
   list_for_each(impl->mWcEndpoints, (wListCallback) zmqBridgeMamaTransportImpl_matchWildcards, &wcClosure);
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found %d wildcard matches for %s", wcClosure.found, subject);

   // get list of subscribers for this subject
   endpoint_t* subs = NULL;
   size_t subCount = 0;
   mama_status status = endpointPool_getRegistered(impl->mSubEndpoints, subject, &subs, &subCount);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s) querying registration table for subject %s", status, mamaStatus_stringForStatus(status), subject);
      return MAMA_STATUS_SYSTEM_ERROR;
   }
   if (0 == subCount) {
      if (wcClosure.found == 0) {
         MAMA_LOG(MAMA_LOG_LEVEL_FINER, "discarding uninteresting message for subject %s", subject);
      }
      return MAMA_STATUS_NOT_FOUND;
   }
   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Found %d non-wildcard matches for %s", subCount, subject);

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

   // mNodeBuffer consists of zmqTransportMsg followed by a copy of the zmq data
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
   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_insert(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX], inbox) >= 0 ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   assert(status == MAMA_STATUS_OK);
   return status;
}


mama_status zmqBridgeMamaTransportImpl_unregisterInbox(zmqTransportBridge* impl, zmqInboxImpl* inbox)
{
   wlock_lock(impl->mInboxesLock);
   mama_status status = wtable_remove(impl->mInboxes, &inbox->mReplyHandle[ZMQ_REPLYHANDLE_INBOXNAME_INDEX]) == inbox ? MAMA_STATUS_OK : MAMA_STATUS_NOT_FOUND;
   wlock_unlock(impl->mInboxesLock);
   assert(status == MAMA_STATUS_OK);
   return status;
}


mama_status zmqBridgeMamaTransportImpl_sendCommand(zmqTransportBridge* impl, zmqControlMsg* msg, int msgSize)
{
   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "command=%c arg1=%s", msg->command, msg->arg1);

   int i = zmq_send(impl->mZmqControlPublisher.mSocket, msg, msgSize, 0);
   if (i <= 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_send failed  %d(%s)", errno, zmq_strerror(errno));
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaTransportImpl_sendEndpointsMsg(zmqTransportBridge* impl)
{
   // publish our endpoints
   zmqNamingMsg msg;
   memset(&msg, '\0', sizeof(msg));
   strcpy(msg.mTopic, ZMQ_NAMING_PREFIX);
   msg.mType = 'C';       // connect
   wmStrSizeCpy(msg.mProgName, program_invocation_short_name, sizeof(msg.mProgName));
   gethostname(msg.mHost, sizeof(msg.mHost));
   msg.mPid = getpid();
   strcpy(msg.mPubEndpoint, impl->mPubEndpoint);
   strcpy(msg.mSubEndpoint, impl->mSubEndpoint);
   wlock_lock(impl->mZmqNamingPublisher.mLock);
   int i = zmq_send(impl->mZmqNamingPublisher.mSocket, &msg, sizeof(msg), 0);
   wlock_unlock(impl->mZmqNamingPublisher.mLock);
   if (i != sizeof(msg)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to publish endpoints: prog=%s host=%s pid=%d pub=%s sub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mPubEndpoint, msg.mSubEndpoint);
      return MAMA_STATUS_PLATFORM;
   }

   CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_kickSocket(impl->mZmqNamingPublisher.mSocket));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Sent endpoint msg: prog=%s host=%s pid=%d pub=%s sub=%s", msg.mProgName, msg.mHost, msg.mPid, msg.mPubEndpoint, msg.mSubEndpoint);
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_publishEndpoints(zmqTransportBridge* impl)
{
   // publish endpoint message continuously every 100ms until it is received on naming socket
   wsem_init(&impl->mNamingConnected, 0, 0);

   do {
      CALL_MAMA_FUNC(zmqBridgeMamaTransportImpl_sendEndpointsMsg(impl));
   } while ((wsem_timedwait(&impl->mNamingConnected, 100) != 0) && (errno == ETIMEDOUT));

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Successfully published endpoints");
   return MAMA_STATUS_OK;
}
