//
// This file contains all the functions that exist solely for compatability with Mama API, but which
// don't have an implementation (i.e., return MAMA_STATUS_NOT_IMPLEMENTED).
//
// This keeps the main files cleaner.
//

// MAMA includes
#include <mama/mama.h>
#include <transportimpl.h>


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
   return MAMA_STATUS_NOT_IMPLEMENTED;
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

mama_status zmqBridgeMamaTransport_getNativeTransportNamingCtx(transportBridge transport,
                                                   void**          result)
{
   return MAMA_STATUS_NOT_IMPLEMENTED;
}

