#ifndef OPENMAMA_ZMQ_PARAMS_H
#define OPENMAMA_ZMQ_PARAMS_H

/* Transport configuration parameters */
#define     TPORT_PARAM_PREFIX                  "mama.zmq.transport"
#define     TPORT_PARAM_OUTGOING_URL            "outgoing_url"
#define     TPORT_PARAM_INCOMING_URL            "incoming_url"

/* Default values for corresponding configuration parameters */
#define     DEFAULT_SUB_OUTGOING_URL        "tcp://*:5557"
#define     DEFAULT_SUB_INCOMING_URL        "tcp://127.0.0.1:5556"
#define     DEFAULT_PUB_OUTGOING_URL        "tcp://*:5556"
#define     DEFAULT_PUB_INCOMING_URL        "tcp://127.0.0.1:5557"



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
mama_status MAMACALLTYPE zmqBridgeMamaTransportImpl_setCommonSocketOptions(const char* name, zmqSocket* socket);

#endif