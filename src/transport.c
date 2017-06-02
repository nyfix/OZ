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
#include "zmqdefs.h"
#include "msg.h"
#include "endpointpool.h"
#include "zmqbridgefunctions.h"
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

/* Default values for corresponding configuration parameters */
#define     DEFAULT_SUB_OUTGOING_URL        "tcp://*:5557"
#define     DEFAULT_SUB_INCOMING_URL        "tcp://127.0.0.1:5556"
#define     DEFAULT_PUB_OUTGOING_URL        "tcp://*:5556"
#define     DEFAULT_PUB_INCOMING_URL        "tcp://127.0.0.1:5557"
#define     DEFAULT_MEMPOOL_SIZE            "1024"
#define     DEFAULT_MEMNODE_SIZE            "4096"
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

/* Non configurable runtime defaults */
#define     PARAM_NAME_MAX_LENGTH           1024L

#define ZMQ_SET_SOCKET_OPTIONS(type,impl,opt,map)                              \
do                                                                             \
{                                                                              \
    const char* valStr = zmqBridgeMamaTransportImpl_getParameter (             \
                          DEFAULT_ZMQ_ ## opt,                                 \
                          "%s.%s.%s",                                          \
                          TPORT_PARAM_PREFIX,                                  \
                          impl->mName,                                         \
                          TPORT_PARAM_ZMQ_ ## opt);                            \
    type value = (type) map (valStr);                                          \
                                                                               \
    mama_log (MAMA_LOG_LEVEL_FINE,                                             \
              "zmqBridgeMamaTransportImpl_setSocketOptionInt(): Setting "      \
              "ZeroMQ socket option %s=%s for transport %s",                   \
              TPORT_PARAM_ZMQ_ ## opt,                                         \
              valStr,                                                          \
              impl->mName);                                                    \
                                                                               \
    zmq_setsockopt (impl->mZmqSocketPublisher, ZMQ_ ## opt, &value, sizeof(int));   \
    zmq_setsockopt (impl->mZmqSocketSubscriber, ZMQ_ ## opt, &value, sizeof(int));  \
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
static mama_status
zmqBridgeMamaTransportImpl_start (zmqTransportBridge* impl);

/**
 * This function is called in the destroy function and is responsible for
 * stopping the proton messengers and joining back with the recv thread created
 * in zmqBridgeMamaTransportImpl_start.
 *
 * @param impl  Qpid transport bridge to start
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
static mama_status
zmqBridgeMamaTransportImpl_stop (zmqTransportBridge* impl);

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
static void MAMACALLTYPE
zmqBridgeMamaTransportImpl_queueCallback (mamaQueue queue, void* closure);

/**
 * This is a local function for parsing long configuration parameters from the
 * MAMA properties object, and supports minimum and maximum limits as well
 * as default values to reduce the amount of code duplicated throughout.
 *
 * @param defaultVal This is the default value to use if the parameter does not
 *                   exist in the configuration file
 * @param minimum    If the parameter obtained from the configuration file is
 *                   less than this value, this value will be used.
 * @param maximum    If the parameter obtained from the configuration file is
 *                   greater than this value, this value will be used.
 * @param format     This is the format string which is used to build the
 *                   name of the configuration parameter which is to be parsed.
 * @param ...        This is the variable list of arguments to be used along
 *                   with the format string.
 *
 * @return long int containing the paramter value, default, minimum or maximum.
 */
/*
static long int
zmqBridgeMamaTransportImpl_getParameterAsLong (long        defaultVal,
                                               long        minimum,
                                               long        maximum,
                                               const char* format,
                                               ...);
*/
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
zmqBridgeMamaTransportImpl_getParameterWithVaList (char*       defaultVal,
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
zmqBridgeMamaTransportImpl_getParameter (const char* defaultVal,
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
zmqBridgeMamaTransportImpl_dispatchThread (void* closure);

void MAMACALLTYPE
zmqBridgeMamaTransportImpl_queueClosureCleanupCb (void* closure);

/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

int
zmqBridgeMamaTransport_isValid (transportBridge transport)
{
    zmqTransportBridge*    impl   = (zmqTransportBridge*) transport;
    int                    status = 0;

    if (NULL != impl)
    {
        status = impl->mIsValid;
    }
    return status;
}

mama_status
zmqBridgeMamaTransport_destroy (transportBridge transport)
{
    zmqTransportBridge*    impl    = NULL;
    mama_status            status  = MAMA_STATUS_OK;

    if (NULL == transport)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    impl  = (zmqTransportBridge*) transport;

    status = zmqBridgeMamaTransportImpl_stop (impl);

    //zmq_close (impl->mZmqSocketDispatcher);
    zmq_close (impl->mZmqSocketPublisher);
    zmq_close (impl->mZmqSocketSubscriber);

    zmq_ctx_destroy(impl->mZmqContext);


    endpointPool_destroy (impl->mSubEndpoints);
    endpointPool_destroy (impl->mPubEndpoints);

    free (impl);

    return status;
}

mama_status
zmqBridgeMamaTransport_create (transportBridge*    result,
                               const char*         name,
                               mamaTransport       parent)
{
    zmqTransportBridge*   impl            = NULL;
    mama_status           status          = MAMA_STATUS_OK;
    char*                 mDefIncoming    = NULL;
    char*                 mDefOutgoing    = NULL;
    const char*           uri             = NULL;
    int                   uri_index       = 0;

    if (NULL == result || NULL == name || NULL == parent)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    impl = (zmqTransportBridge*) calloc (1, sizeof (zmqTransportBridge));

    /* Back reference the MAMA transport */
    impl->mTransport           = parent;

    /* Initialize the dispatch thread pointer */
    impl->mOmzmqDispatchThread  = 0;
    impl->mOmzmqDispatchStatus  = MAMA_STATUS_OK;
    impl->mName                 = name;

    mama_log (MAMA_LOG_LEVEL_FINE,
              "zmqBridgeMamaTransport_create(): Initializing Transport %s",
              name);

    impl->mMemoryPoolSize = atol(zmqBridgeMamaTransportImpl_getParameter (
            DEFAULT_MEMPOOL_SIZE,
            "%s.%s.%s",
            TPORT_PARAM_PREFIX,
            name,
            TPORT_PARAM_MSG_POOL_SIZE));

    impl->mMemoryNodeSize = atol(zmqBridgeMamaTransportImpl_getParameter (
            DEFAULT_MEMNODE_SIZE,
            "%s.%s.%s",
            TPORT_PARAM_PREFIX,
            name,
            TPORT_PARAM_MSG_NODE_SIZE));

    mama_log (MAMA_LOG_LEVEL_FINE,
              "zmqBridgeMamaTransport_create(): Any message pools created will "
              "contain %lu nodes of %lu bytes.",
              name,
              impl->mMemoryPoolSize,
              impl->mMemoryNodeSize);

    if (0 == strcmp(impl->mName, "pub"))
    {
        mDefIncoming = DEFAULT_PUB_INCOMING_URL;
        mDefOutgoing = DEFAULT_PUB_OUTGOING_URL;
    }
    else
    {
        mDefIncoming = DEFAULT_SUB_INCOMING_URL;
        mDefOutgoing = DEFAULT_SUB_OUTGOING_URL;
    }

    /* Start with bare incoming address */
    impl->mIncomingAddress[0] = zmqBridgeMamaTransportImpl_getParameter (
                mDefIncoming,
                "%s.%s.%s",
                TPORT_PARAM_PREFIX,
                name,
                TPORT_PARAM_INCOMING_URL);

    /* Now parse any _0, _1 etc. */
    uri_index = 0;
    while (NULL != (uri = zmqBridgeMamaTransportImpl_getParameter (
            NULL,
            "%s.%s.%s_%d",
            TPORT_PARAM_PREFIX,
            name,
            TPORT_PARAM_INCOMING_URL,
            uri_index)))
    {
        impl->mIncomingAddress[uri_index] = uri;
        uri_index++;
    }

    /* Start with bare outgoing address */
    impl->mOutgoingAddress[0] = zmqBridgeMamaTransportImpl_getParameter (
                mDefOutgoing,
                "%s.%s.%s",
                TPORT_PARAM_PREFIX,
                name,
                TPORT_PARAM_OUTGOING_URL);

    /* Now parse any _0, _1 etc. */
    uri_index = 0;
    while (NULL != (uri = zmqBridgeMamaTransportImpl_getParameter (
            NULL,
            "%s.%s.%s_%d",
            TPORT_PARAM_PREFIX,
            name,
            TPORT_PARAM_OUTGOING_URL,
            uri_index)))
    {
        impl->mOutgoingAddress[uri_index] = uri;
        uri_index++;
    }

    status = endpointPool_create (&impl->mSubEndpoints, "mSubEndpoints");
    if (MAMA_STATUS_OK != status)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransport_create(): "
                  "Failed to create subscribing endpoints");
        free (impl);
        return MAMA_STATUS_PLATFORM;
    }

    status = endpointPool_create (&impl->mPubEndpoints, "mPubEndpoints");
    if (MAMA_STATUS_OK != status)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransport_create(): "
                  "Failed to create publishing endpoints");
        free (impl);
        return MAMA_STATUS_PLATFORM;
    }

    impl->mIsValid = 1;

    *result = (transportBridge) impl;

    return zmqBridgeMamaTransportImpl_start (impl);
}

mama_status
zmqBridgeMamaTransport_forceClientDisconnect (transportBridge*   transports,
                                              int                numTransports,
                                              const char*        ipAddress,
                                              uint16_t           port)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_findConnection (transportBridge*    transports,
                                       int                 numTransports,
                                       mamaConnection*     result,
                                       const char*         ipAddress,
                                       uint16_t            port)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getAllConnections (transportBridge*    transports,
                                          int                 numTransports,
                                          mamaConnection**    result,
                                          uint32_t*           len)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getAllConnectionsForTopic (
                    transportBridge*    transports,
                    int                 numTransports,
                    const char*         topic,
                    mamaConnection**    result,
                    uint32_t*           len)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_requestConflation (transportBridge*     transports,
                                          int                  numTransports)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_requestEndConflation (transportBridge*  transports,
                                             int               numTransports)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getAllServerConnections (
        transportBridge*        transports,
        int                     numTransports,
        mamaServerConnection**  result,
        uint32_t*               len)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_freeAllServerConnections (
        transportBridge*        transports,
        int                     numTransports,
        mamaServerConnection*   result,
        uint32_t                len)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_freeAllConnections (transportBridge*    transports,
                                           int                 numTransports,
                                           mamaConnection*     result,
                                           uint32_t            len)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getNumLoadBalanceAttributes (
        const char*     name,
        int*            numLoadBalanceAttributes)
{
    if (NULL == numLoadBalanceAttributes || NULL == name)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    *numLoadBalanceAttributes = 0;
    return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaTransport_getLoadBalanceSharedObjectName (
        const char*     name,
        const char**    loadBalanceSharedObjectName)
{
    if (NULL == loadBalanceSharedObjectName)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    *loadBalanceSharedObjectName = NULL;
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getLoadBalanceScheme (const char*       name,
                                             tportLbScheme*    scheme)
{
    if (NULL == scheme || NULL == name)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    *scheme = TPORT_LB_SCHEME_STATIC;
    return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaTransport_sendMsgToConnection (transportBridge    tport,
                                            mamaConnection     connection,
                                            mamaMsg            msg,
                                            const char*        topic)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_isConnectionIntercepted (mamaConnection connection,
                                                uint8_t*       result)
{
    if (NULL == result)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    *result = 0;
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_installConnectConflateMgr (
        transportBridge         handle,
        mamaConflationManager   mgr,
        mamaConnection          connection,
        conflateProcessCb       processCb,
        conflateGetMsgCb        msgCb)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_uninstallConnectConflateMgr (
        transportBridge         handle,
        mamaConflationManager   mgr,
        mamaConnection          connection)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_startConnectionConflation (
        transportBridge         tport,
        mamaConflationManager   mgr,
        mamaConnection          connection)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}

mama_status
zmqBridgeMamaTransport_getNativeTransport (transportBridge     transport,
                                           void**              result)
{
    zmqTransportBridge* impl = (zmqTransportBridge*)transport;

    if (NULL == transport || NULL == result)
    {
        return MAMA_STATUS_NULL_ARG;
    }
    *result = impl;

    return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaTransport_getNativeTransportNamingCtx (transportBridge transport,
                                                    void**          result)
{
    return MAMA_STATUS_NOT_IMPLEMENTED;
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

zmqTransportBridge*
zmqBridgeMamaTransportImpl_getTransportBridge (mamaTransport transport)
{
    zmqTransportBridge*    impl;
    mama_status             status = MAMA_STATUS_OK;

    status = mamaTransport_getBridgeTransport (transport,
                                               (transportBridge*) &impl);

    if (status != MAMA_STATUS_OK || impl == NULL)
    {
        return NULL;
    }

    return impl;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/
int
zmqBridgeMamaTransportImpl_setupSocket (void* socket, const char* uri, zmqTransportDirection direction)
{
    int rc = 0;
    char tportTypeStr[16];
    char* firstColon = NULL;
    zmqTransportType tportType = ZMQ_TPORT_TYPE_UNKNOWN;
    /* If set to non zero, will bind rather than connect */
    int isBinding = 0;

    strncpy (tportTypeStr, uri, sizeof(tportTypeStr));
    tportTypeStr[sizeof(tportTypeStr)-1] = '\0';
    firstColon = strchr(tportTypeStr, ':');
    if (NULL != firstColon)
    {
        *firstColon = '\0';
    }

    if (0 == strcmp(tportTypeStr, "tcp"))
    {
        tportType = ZMQ_TPORT_TYPE_TCP;
    }
    else if (0 == strcmp(tportTypeStr, "epgm"))
    {
        tportType = ZMQ_TPORT_TYPE_EPGM;
    }
    else if (0 == strcmp(tportTypeStr, "pgm"))
    {
        tportType = ZMQ_TPORT_TYPE_PGM;
    }
    else if (0 == strcmp(tportTypeStr, "ipc"))
    {
        tportType = ZMQ_TPORT_TYPE_IPC;
    }
    else
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "Unknown ZeroMQ transport type found: %s. Trying anyway.",
                  tportTypeStr);
    }

    mama_log (MAMA_LOG_LEVEL_FINE, "Found ZeroMQ transport type %s (%d)",
              tportTypeStr, tportType);

    /* Get the transport type from the uri */
    switch (direction)
    {
    case ZMQ_TPORT_DIRECTION_INCOMING:
        switch (tportType)
        {
        case ZMQ_TPORT_TYPE_TCP:
            if (strchr(uri, '*'))
            {
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
        switch (tportType)
        {
        case ZMQ_TPORT_TYPE_TCP:
            if (strchr(uri, '*'))
            {
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
    default:
        mama_log (MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaTransportImpl_setupSocket(): "
                  "Didn't recognize transport direction %d",
                  direction);
        break;
    }

    /* If this is a binding transport */
    if (isBinding)
    {
        rc = zmq_bind (socket, uri);
        if (0 != rc)
        {
            mama_log (MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaTransportImpl_setupSocket(): "
                      "zmq_bind returned %d trying to bind to '%s' (%s)",
                      rc,
                      uri,
                      strerror(errno));
        }
        return rc;
    }
    else
    {
        rc = zmq_connect (socket, uri);
        if (0 != rc)
        {
            mama_log (MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaTransportImpl_start(): "
                      "zmq_connect returned %d trying to connect to '%s' (%s)",
                      rc,
                      uri,
                      strerror(errno));
            return rc;
        }
    }
}

mama_status
zmqBridgeMamaTransportImpl_start (zmqTransportBridge* impl)
{
    int rc = 0;
    int i  = 0;

    if (NULL == impl)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                "zmqBridgeMamaTransportImpl_start(): transport NULL");
        return MAMA_STATUS_NULL_ARG;
    }


    impl->mZmqContext = zmq_ctx_new ();
    impl->mZmqSocketSubscriber = zmq_socket (impl->mZmqContext, ZMQ_SUB);
    impl->mZmqSocketPublisher  = zmq_socket (impl->mZmqContext, ZMQ_PUB);

    ZMQ_SET_SOCKET_OPTIONS (int,impl,SNDHWM,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RCVHWM,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,SNDBUF,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RCVBUF,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RECONNECT_IVL,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RECONNECT_IVL_MAX,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,BACKLOG,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RCVTIMEO,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,SNDTIMEO,atoi);
    ZMQ_SET_SOCKET_OPTIONS (int,impl,RATE,atoi);
    ZMQ_SET_SOCKET_OPTIONS (uint64_t,impl,AFFINITY,atoll);
    ZMQ_SET_SOCKET_OPTIONS (const char*,impl,IDENTITY,);
    ZMQ_SET_SOCKET_OPTIONS (int64_t,impl,MAXMSGSIZE,atoll);

    for (i = 0; i < ZMQ_MAX_INCOMING_URIS; i++)
    {
        if (NULL == impl->mOutgoingAddress[i])
        {
            break;
        }
        if (0 != zmqBridgeMamaTransportImpl_setupSocket (impl->mZmqSocketPublisher,
                                                         impl->mOutgoingAddress[i],
                                                         ZMQ_TPORT_DIRECTION_OUTGOING))
        {
            return MAMA_STATUS_PLATFORM;
        }
        else
        {
            mama_log (MAMA_LOG_LEVEL_FINE,
                      "zmqBridgeMamaTransportImpl_start(): Successfully set up "
                      "outgoing ZeroMQ socket for URI: %s",
                      impl->mOutgoingAddress[i]);
        }
    }

    for (i = 0; i < ZMQ_MAX_OUTGOING_URIS; i++)
    {
        if (NULL == impl->mIncomingAddress[i])
        {
            break;
        }
        if (0 != zmqBridgeMamaTransportImpl_setupSocket (impl->mZmqSocketSubscriber,
                                                         impl->mIncomingAddress[i],
                                                         ZMQ_TPORT_DIRECTION_INCOMING))
        {
            return MAMA_STATUS_PLATFORM;
        }
        else
        {
            mama_log (MAMA_LOG_LEVEL_FINE,
                      "zmqBridgeMamaTransportImpl_start(): Successfully set up "
                      "incoming ZeroMQ socket for URI: %s",
                      impl->mIncomingAddress[i]);
        }
    }

    /* Set the transport bridge mIsDispatching to true. */
    impl->mIsDispatching = 1;

    /* Initialize dispatch thread */
    rc = wthread_create (&(impl->mOmzmqDispatchThread),
                         NULL,
                         zmqBridgeMamaTransportImpl_dispatchThread,
                         impl);
    if (0 != rc)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR, "zmqBridgeMamaTransportImpl_start(): "
                  "wthread_create returned %d", rc);
        return MAMA_STATUS_PLATFORM;
    }
    return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaTransportImpl_stop (zmqTransportBridge* impl)
{
    /* There are two mechanisms by which we can stop the transport
     * - Send a special message, which will be picked up by recv
     *   For the instance when there is very little data flowing.
     * - Set the mIsDispatching variable in the transportBridge object to
     *   false, for instances when there is a lot of data flowing.
     */
    mama_status     status = MAMA_STATUS_OK;

    /* Set the transportBridge mIsDispatching to false */
    impl->mIsDispatching = 0;

    mama_log (MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaTransportImpl_stop(): "
                  "Waiting on dispatch thread to terminate.");

    wthread_join (impl->mOmzmqDispatchThread, NULL);
    status = impl->mOmzmqDispatchStatus;

    mama_log (MAMA_LOG_LEVEL_FINEST, "zmqBridgeMamaTransportImpl_stop(): "
                      "Rejoined with status: %s.",
                      mamaStatus_stringForStatus(status));

    return MAMA_STATUS_OK;
}

/**
 * Called when message removed from queue by dispatch thread
 *
 * @param data The AMQP payload
 * @param closure The subscriber
 */
void MAMACALLTYPE
zmqBridgeMamaTransportImpl_queueCallback (mamaQueue queue, void* closure)
{

    mama_status           status          = MAMA_STATUS_OK;
    mamaMsg               tmpMsg          = NULL;
    msgBridge             bridgeMsg       = NULL;
    memoryPool*           pool            = NULL;
    memoryNode*           node            = (memoryNode*) closure;
    zmqTransportMsg*      tmsg            = (zmqTransportMsg*) node->mNodeBuffer;
    uint32_t              bufferSize      = tmsg->mNodeSize;
    const void*           buffer          = tmsg->mNodeBuffer;
    const char*           subject         = (char*)buffer;
    zmqSubscription*      subscription    = (zmqSubscription*) tmsg->mSubscription;
    zmqTransportBridge*   impl            = subscription->mTransport;
    zmqQueueBridge*       queueImpl       = NULL;

    /* Can't do anything without a subscriber */
    if (NULL == subscription)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "Called with NULL subscriber (destroyed?)");
        return;
    }

    if (0 == endpointPool_isRegistedByContent (impl->mSubEndpoints,
                                               subject,
                                               subscription))
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "Subscriber has been unregistered since msg was enqueued.");
        return;
    }

    /* Make sure that the subscription is processing messages */
    if (1 != subscription->mIsNotMuted)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "Skipping update - subscription %p is muted.", subscription);
        return;
    }

    /* This is the reuseable message stored on the associated MamaQueue */
    tmpMsg = mamaQueueImpl_getMsg (subscription->mMamaQueue);
    if (NULL == tmpMsg)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "Could not get cached mamaMsg from event queue.");
        return;
    }

    /* Get the bridge message from the mamaMsg */
    status = mamaMsgImpl_getBridgeMsg (tmpMsg, &bridgeMsg);
    if (MAMA_STATUS_OK != status)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "Could not get bridge message from cached "
                  "queue mamaMsg [%s]", mamaStatus_stringForStatus (status));
        return;
    }

    /* Unpack this bridge message into a MAMA msg implementation */
    status = zmqBridgeMamaMsgImpl_deserialize (bridgeMsg, buffer, bufferSize, tmpMsg);
    if (MAMA_STATUS_OK != status)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridgeMamaTransportImpl_queueCallback(): "
                  "zmqBridgeMamaMsgImpl_unpack() failed. [%s]",
                  mamaStatus_stringForStatus (status));
    }
    else
    {
        /* Process the message as normal */
        status = mamaSubscription_processMsg (subscription->mMamaSubscription,
                                              tmpMsg);
        if (MAMA_STATUS_OK != status)
        {
            mama_log (MAMA_LOG_LEVEL_ERROR,
                      "zmqBridgeMamaTransportImpl_queueCallback(): "
                      "mamaSubscription_processMsg() failed. [%d]", status);
        }
    }

    mamaQueue_getNativeHandle (queue, (void**)&queueImpl);

    // Return the memory node to the pool
    pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure ((queueBridge) queueImpl);
    memoryPool_returnNode (pool, node);

    return;
}

const char* zmqBridgeMamaTransportImpl_getParameterWithVaList (
                                            char*       defaultVal,
                                            char*       paramName,
                                            const char* format,
                                            va_list     arguments)
{
    const char* property = NULL;

    /* Create the complete transport property string */
    vsnprintf (paramName, PARAM_NAME_MAX_LENGTH,
               format, arguments);

    /* Get the property out for analysis */
    property = properties_Get (mamaInternal_getProperties (),
                               paramName);

    /* Properties will return NULL if parameter is not specified in configs */
    if (property == NULL)
    {
        property = defaultVal;
    }

    return property;
}

const char* zmqBridgeMamaTransportImpl_getParameter (
                                            const char* defaultVal,
                                            const char* format, ...)
{
    char        paramName[PARAM_NAME_MAX_LENGTH];
    const char* returnVal = NULL;
    /* Create list for storing the parameters passed in */
    va_list     arguments;

    /* Populate list with arguments passed in */
    va_start (arguments, format);

    returnVal = zmqBridgeMamaTransportImpl_getParameterWithVaList (
                        (char*)defaultVal,
                        paramName,
                        format,
                        arguments);

    /* These will be equal if unchanged */
    if (returnVal == defaultVal)
    {
        mama_log (MAMA_LOG_LEVEL_FINER,
                  "zmqBridgeMamaTransportImpl_getParameter: "
                  "parameter [%s]: [%s] (Default)",
                  paramName,
                  returnVal);
    }
    else
    {
        mama_log (MAMA_LOG_LEVEL_FINER,
                  "zmqBridgeMamaTransportImpl_getParameter: "
                  "parameter [%s]: [%s] (User Defined)",
                  paramName,
                  returnVal);
    }

    /* Clean up the list */
    va_end(arguments);

    return returnVal;
}

void* zmqBridgeMamaTransportImpl_dispatchThread (void* closure)
{
    zmqTransportBridge*     impl          = (zmqTransportBridge*)closure;
    const char*             subject       = NULL;
    endpoint_t*             subs          = NULL;
    size_t                  subCount      = 0;
    size_t                  subInc        = 0;
    mama_status             status        = MAMA_STATUS_OK;
    zmqSubscription*        subscription  = NULL;

    /*
     * Check if we should be still dispatching.
     * We shouldn't need to lock around this, as we're performing a simple value
     * read - if it changes in the middle of the read, we don't actually care.
     */
    zmq_msg_t zmsg;
    zmq_msg_init (&zmsg);
    while (1 == impl->mIsDispatching)
    {
        int size = -1;
        size = zmq_msg_recv(&zmsg, impl->mZmqSocketSubscriber, 0);
        if (size == -1)
        {
            continue;
        }

        // We just received a message if we got this far
        subject = (char*) zmq_msg_data (&zmsg);

        status = endpointPool_getRegistered (impl->mSubEndpoints,
                                             subject,
                                             &subs,
                                             &subCount);

        if (MAMA_STATUS_OK != status)
        {
            mama_log (MAMA_LOG_LEVEL_ERROR,
                      "zmqBridgeMamaTransportImpl_dispatchThread(): "
                      "Could not query registration table "
                      "for symbol %s (%s)",
                      subject,
                      mamaStatus_stringForStatus (status));

            continue;
        }

        if (0 == subCount)
        {
            mama_log (MAMA_LOG_LEVEL_FINEST,
                      "zmqBridgeMamaTransportImpl_dispatchThread(): "
                      "discarding uninteresting message "
                      "for symbol %s", subject);

            continue;
        }

        /* Within this loop, queue callbacks release the pool messages */
        for (subInc = 0; subInc < subCount; subInc++)
        {
            subscription = (zmqSubscription*)subs[subInc];

            if (1 == subscription->mIsTportDisconnected)
            {
                subscription->mIsTportDisconnected = 0;
            }

            if (1 != subscription->mIsNotMuted)
            {
                mama_log (MAMA_LOG_LEVEL_FINEST,
                          "zmqBridgeMamaTransportImpl_dispatchThread(): "
                          "muted - not queueing update for symbol %s",
                          subject);
                continue;
            }
            /* If this isn't the last one in the list */
            else if (subInc != (subCount - 1))
            {
                // TODO: Handle multiple subscriptions per app like this
                mama_log (MAMA_LOG_LEVEL_WARN,
                          "zmqBridgeMamaTransportImpl_dispatchThread(): "
                          "Cannot currently handle multiple subscribers in "
                          "same application",
                          subject);
                //zmqBridgeMamaQueue_enqueueEvent (
                //        (queueBridge) subscription->mZmqQueue,
                //        zmqBridgeMamaTransportImpl_queueCallback,
                //        NULL);
            }
            /*
             * If this is the last (or only) element and all copies are
             * done, make use of the original message rather than copy.
             */
            else
            {
                queueBridge      queueImpl    = NULL;
                memoryPool*      pool         = NULL;
                memoryNode*      node         = NULL;
                zmqTransportMsg* tmsg         = NULL;
                size_t           memLength    = sizeof(zmqTransportMsg) +
                                                     zmq_msg_size(&zmsg);

                queueImpl = (queueBridge) subscription->mZmqQueue;

                /* Get the memory pool from the queue, creating if necessary */
                pool = (memoryPool*) zmqBridgeMamaQueueImpl_getClosure (queueImpl);
                if (NULL == zmqBridgeMamaQueueImpl_getClosure (queueImpl))
                {
                    pool = memoryPool_create (impl->mMemoryPoolSize,
                                              impl->mMemoryNodeSize);
                    zmqBridgeMamaQueueImpl_setClosure (
                            queueImpl,
                            pool,
                            zmqBridgeMamaTransportImpl_queueClosureCleanupCb);
                }

                node = memoryPool_getNode (pool, memLength);

                tmsg = (zmqTransportMsg*) node->mNodeBuffer;

                tmsg->mNodeBuffer   = (uint8_t*)(tmsg + 1);
                tmsg->mNodeSize     = zmq_msg_size(&zmsg);
                tmsg->mSubscription = subscription;

                memcpy (tmsg->mNodeBuffer,
                        zmq_msg_data(&zmsg),
                        tmsg->mNodeSize);

                zmqBridgeMamaQueue_enqueueEvent (
                        (queueBridge) queueImpl,
                        zmqBridgeMamaTransportImpl_queueCallback,
                        (void*) node);
            }
        }
    }

    impl->mOmzmqDispatchStatus = MAMA_STATUS_OK;
    return NULL;
}

void MAMACALLTYPE
zmqBridgeMamaTransportImpl_queueClosureCleanupCb (void* closure)
{
    memoryPool* pool = (memoryPool*) closure;
    if (NULL != pool)
    {
        mama_log (MAMA_LOG_LEVEL_FINE,
                  "Destroying memory pool for queue %p.", closure);
        memoryPool_destroy (pool, NULL);
    }
}
