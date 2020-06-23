//
// split out the parameter handling code from main transport
//

#include <mama/mama.h>
#include <mama/integration/mama.h>
#include <property.h>
#include <wombat/wInterlocked.h>

#include "zmqdefs.h"
#include "params.h"

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
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "parameter [%s]: [%s] (Default)", paramName, returnVal);
   }
   else if ( (returnVal != NULL) && (defaultVal != NULL) && (strcmp(returnVal, defaultVal) == 0) ) {
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "parameter [%s]: [%s] (Default)", paramName, returnVal);
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_NORMAL, "parameter [%s]: [%s] (User Defined)", paramName, returnVal);
   }

   /* Clean up the list */
   va_end(arguments);

   return returnVal;
}


//////////////////////////////////////////
// helper routines for above ...
int getInt(const char* name, const char* property, int defaultValue, int minValue)
{
   char valStr[256];
   sprintf(valStr, "%d", defaultValue);
   const char* result = zmqBridgeMamaTransportImpl_getParameter(valStr, "%s.%s.%s", TPORT_PARAM_PREFIX, name, property);
   int temp = atoi(result);
   if ((minVal >= 0) && (temp < 0)) {
      temp = 0;
   }
   if ((temp > 0) && (temp < minValue)) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "%s cannot be less than %d", property, minValue);
      temp = minValue;
   }

   return temp;
}

long long getLong(const char* name, const char* property, long long defaultValue, long long minValue)
{
   char valStr[256];
   sprintf(valStr, "%lld", defaultValue);
   const char* result = zmqBridgeMamaTransportImpl_getParameter(valStr, "%s.%s.%s", TPORT_PARAM_PREFIX, name, property);
   long long temp = atoll(result);
   if ((minVal >= 0) && (temp < 0)) {
      temp = 0;
   }
   if (temp < minValue) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "%s cannot be less than %lld", property, minValue);
      temp = minValue;
   }

   return temp;
}

double getFloat(const char* name, const char* property, double defaultValue, double minValue)
{
   char valStr[256];
   sprintf(valStr, "%f", defaultValue);
   const char* result = zmqBridgeMamaTransportImpl_getParameter(valStr, "%s.%s.%s", TPORT_PARAM_PREFIX, name, property);
   double temp = atof(result);
   if ((minVal >= 0) && (temp < 0)) {
      temp = 0;
   }
   if (temp < minValue) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "%s cannot be less than %f", property, minValue);
      temp = minValue;
   }

   return temp;

}

const char* getStr(const char* name, const char* property, const char* defaultValue)
{
   const char* result = zmqBridgeMamaTransportImpl_getParameter(defaultValue, "%s.%s.%s", TPORT_PARAM_PREFIX, name, property);
   return result;
}


// These parameters apply to both naming and non-naming transports
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseCommonParams(zmqTransportBridge* impl)
{
   // the name of the transport
   const char* name = impl->mName;

   impl->mReconnectInterval = getFloat(name, "reconnect_interval", 10, -1) * 1000.0;    // millis
   impl->mHeartbeatInterval = getFloat(name, "heartbeat_interval", 10, 0) * 1000.0;    // millis
   impl->mSocketMonitor = getInt(name, "socket_monitor", 1, 0);
   impl->mIsNaming = getInt(name, "is_naming", 1, 0);
   impl->mPublishAddress = getStr(name, "publish_address", "lo");
}


// These parameters apply only to naming transports
void MAMACALLTYPE  zmqBridgeMamaTransportImpl_parseNamingParams(zmqTransportBridge* impl)
{
   // the name of the transport
   const char* name = impl->mName;

   impl->mNamingWaitForConnect = getInt(name, "naming.wait_for_connect", 1, 0);
   impl->mNamingConnectInterval = getFloat(name, "naming.connect_interval", .1, .1) * 1000000.0;    // micros
   impl->mNamingConnectRetries = getInt(name, "naming.connect_retries", 100, 10);
   impl->mBeaconInterval = getFloat(name, "naming.beacon_interval", 1, .1) * 1000.0;    // millis;

   // The naming server address can be specified in any of the following formats:
   // 1. naming.subscribe_address[_n]/naming.subscribe_port[_n]
   // 2. naming.nsd_addr[_n]
   // 3. naming.outgoing_url[_n]/naming.incoming_url[_n]
   // TODO: implement 2, 3

   // nsd addr/port
   // Note that we DO provide default values for the first/only nsd
   // This is necessary to allow the OpenMAMA unit tests to run w/o a special mama.properties file
   // It also simplifies development
   char endpoint[ZMQ_MAX_ENDPOINT_LENGTH +1];
   const char* address;
   int port;
   address =  getStr(name, "naming.subscribe_address", NULL);
   if (address == NULL) {
      address =  getStr(name, "naming.subscribe_address_0", "127.0.0.1");
   }
   port = getInt(name, "naming.subscribe_port", 0, 0);
   if (port == 0) {
      port = getInt(name, "naming.subscribe_port_0", 5756, 0);
   }
   sprintf(endpoint, "tcp://%s:%d", address, port);
   impl->mNamingAddress[0] = strdup(endpoint);

   // No default values for _1, _2
   address =  getStr(name, "naming.subscribe_address_1", NULL );
   if (address != NULL) {
      port = getInt(name, "naming.subscribe_port_1", 0, 0);
      if (port > 0) {
       sprintf(endpoint, "tcp://%s:%d", address, port);
       impl->mNamingAddress[1] = strdup(endpoint);
      }
   }
   address =  getStr(name, "naming.subscribe_address_2", NULL );
   if (address != NULL) {
      port = getInt(name, "naming.subscribe_port_2", 0, 0);
      if (port > 0) {
       sprintf(endpoint, "tcp://%s:%d", address, port);
       impl->mNamingAddress[2] = strdup(endpoint);
      }
   }
}


// These parameters apply only to non-naming transports
// TODO: anything having to do with non-naming transports needs to be re-examined as it may be stale
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


