#ifndef OPENMAMA_ZMQ_PARAMS_H
#define OPENMAMA_ZMQ_PARAMS_H

/* Transport configuration parameters */
#define     TPORT_PARAM_PREFIX                  "mama.zmq.transport"
#define     TPORT_PARAM_OUTGOING_URL            "outgoing_url"
#define     TPORT_PARAM_INCOMING_URL            "incoming_url"
#define     TPORT_PARAM_MSG_POOL_SIZE           "msg_pool_size"
#define     TPORT_PARAM_MSG_NODE_SIZE           "msg_node_size"

// zmq params
#define     TPORT_PARAM_ZMQ_SNDHWM              "zmq_sndhwm"
#define     DEFAULT_ZMQ_SNDHWM                  "0"       /* ZMQ Default = 1000 */

#define     TPORT_PARAM_ZMQ_RCVHWM              "zmq_rcvhwm"
#define     DEFAULT_ZMQ_RCVHWM                  "0"       /* ZMQ Default = 1000 */

#define     TPORT_PARAM_ZMQ_AFFINITY            "zmq_affinity"
#define     DEFAULT_ZMQ_AFFINITY                "0"       /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_IDENTITY            "zmq_identity"
#define     DEFAULT_ZMQ_IDENTITY                NULL      /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_SNDBUF              "zmq_sndbuf"
#define     DEFAULT_ZMQ_SNDBUF                  "0"       /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_RCVBUF              "zmq_rcvbuf"
#define     DEFAULT_ZMQ_RCVBUF                  "0"       /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_RECONNECT_IVL       "zmq_reconnect_ivl"
#define     DEFAULT_ZMQ_RECONNECT_IVL           "100"     /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_RECONNECT_IVL_MAX   "zmq_reconnect_ivl_max"
#define     DEFAULT_ZMQ_RECONNECT_IVL_MAX       "0"       /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_BACKLOG             "zmq_backlog"
#define     DEFAULT_ZMQ_BACKLOG                 "100"     /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_MAXMSGSIZE          "zmq_maxmsgsize"
#define     DEFAULT_ZMQ_MAXMSGSIZE              "-1"      /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_RCVTIMEO            "zmq_rcvtimeo"
#define     DEFAULT_ZMQ_RCVTIMEO                "10"      /* ZMQ Default = -1   */

#define     TPORT_PARAM_ZMQ_SNDTIMEO            "zmq_sndtimeo"
#define     DEFAULT_ZMQ_SNDTIMEO                "-1"      /* ZMQ Default        */

#define     TPORT_PARAM_ZMQ_RATE                "zmq_rate"
#define     DEFAULT_ZMQ_RATE                    "1000000" /* ZMQ Default = 100  */

// non-naming params (data sockets)
#define     TPORT_PARAM_RECONNECT               "retry_connects"
#define     DEFAULT_RECONNECT                   "0"

#define     TPORT_PARAM_CONNECT_TIMEOUT         "connect_timeout"
#define     DEFAULT_CONNECT_TIMEOUT             ".1"

// naming params
#define     TPORT_PARAM_ISNAMING                "is_naming"
#define     DEFAULT_ISNAMING                    "0"

#define     TPORT_PARAM_NAMING_RECONNECT        "naming.retry_connects"
#define     DEFAULT_NAMING_RECONNECT            "1"

#define     TPORT_PARAM_NAMING_WAIT_FOR_CONNECT "naming.wait_for_connect"
#define     DEFAULT_NAMING_WAIT_FOR_CONNECT     "1"

#define     TPORT_PARAM_NAMING_CONNECT_INTERVAL "naming.connect_interval"
#define     DEFAULT_NAMING_CONNECT_INTERVAL     ".1"

#define     TPORT_PARAM_NAMING_CONNECT_RETRIES  "naming.connect_retries"
#define     DEFAULT_NAMING_CONNECT_RETRIES      "100"

#define     TPORT_PARAM_NAMING_CONNECT_TIMEOUT  "naming.connect_timeout"
#define     DEFAULT_NAMING_CONNECT_TIMEOUT      ".1"

#define     TPORT_PARAM_NAMING_ADDR             "naming.subscribe_address"
#define     DEFAULT_NAMING_ADDR                 "127.0.0.1"

#define     TPORT_PARAM_NAMING_PORT             "naming.subscribe_port"
#define     DEFAULT_NAMING_PORT                 "5756"

#define     TPORT_PARAM_PUBLISH_ADDRESS         "publish_address"
#define     DEFAULT_PUBLISH_ADDRESS             "lo"

#define     TPORT_PARAM_SOCKET_MONITOR          "socket_monitor"
#define     DEFAULT_SOCKET_MONITOR              "0"

#define     TPORT_PARAM_BEACON_INTERVAL         "beacon_interval"
#define     DEFAULT_BEACON_INTERVAL             "1000000"

/* Default values for corresponding configuration parameters */
#define     DEFAULT_SUB_OUTGOING_URL        "tcp://*:5557"
#define     DEFAULT_SUB_INCOMING_URL        "tcp://127.0.0.1:5556"
#define     DEFAULT_PUB_OUTGOING_URL        "tcp://*:5556"
#define     DEFAULT_PUB_INCOMING_URL        "tcp://127.0.0.1:5557"
#define     DEFAULT_MEMPOOL_SIZE            "1024"
#define     DEFAULT_MEMNODE_SIZE            "4096"


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



// parameter parsing
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseCommonParams(zmqTransportBridge* impl);
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNamingParams(zmqTransportBridge* impl);
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNonNamingParams(zmqTransportBridge* impl);

// sets socket options as specified in Mama configuration file
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_setSocketOptions(const char* name, zmqSocket* socket);

#endif