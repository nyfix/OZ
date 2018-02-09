//
// split out the parameter handling code from main transport
//

#include <mama/mama.h>
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
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "parameter [%s]: [%s] (Default)", paramName, returnVal);
   }
   else {
      MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "parameter [%s]: [%s] (User Defined)", paramName, returnVal);
   }

   /* Clean up the list */
   va_end(arguments);

   return returnVal;
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


