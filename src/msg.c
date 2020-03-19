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
// system includes
#include <stdlib.h>
#include <string.h>

// Mama includes
#include <mama/mama.h>
#include <wombat/memnode.h>
#include <wombat/strutils.h>

// local includes
#include "transport.h"
#include "msg.h"
#include "zmqbridgefunctions.h"
#include "zmqdefs.h"


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/


/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/



/*=========================================================================
  =              Public interface implementation functions                =
  =========================================================================*/

/* Bridge specific implementations below here */
mama_status zmqBridgeMamaMsg_create(msgBridge* msg, mamaMsg parent)
{
   if (NULL == msg || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }

   CALL_MAMA_FUNC(zmqBridgeMamaMsgImpl_createMsgOnly(msg));

   /* Cast back to implementation to set parent */
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) *msg;
   impl->mParent       = parent;

   return MAMA_STATUS_OK;
}


int zmqBridgeMamaMsg_isFromInbox(msgBridge msg)
{
   if (NULL == msg) {
      return 0;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   if (ZMQ_MSG_INBOX_REQUEST == (impl->mMsgType)) {
      return 1;
   }

   if (impl->mReplyHandle[0] != '\0') {
      return 1;
   }

   return 0;
}


mama_status zmqBridgeMamaMsg_destroy(msgBridge msg, int destroyMsg)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   free(msg);
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsg_destroyMiddlewareMsg(msgBridge msg)
{
   return zmqBridgeMamaMsg_destroy(msg, 1);
}


mama_status zmqBridgeMamaMsg_detach(msgBridge msg)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl*  impl = (zmqBridgeMsgImpl*) msg;

   return mamaMsgImpl_setMessageOwner(impl->mParent, 1);
}


mama_status zmqBridgeMamaMsg_getPlatformError(msgBridge msg, void** error)
{
   /* Null initialize the error return */
   if (NULL != error) {
      *error  = NULL;
   }

   return MAMA_STATUS_NOT_IMPLEMENTED;
}


mama_status zmqBridgeMamaMsg_setSendSubject(msgBridge msg, const char* symbol, const char* subject)
{
   if ((NULL == symbol && NULL == subject) || NULL == msg || NULL == symbol ) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl     = (zmqBridgeMsgImpl*) msg;

   if (wmStrSizeCpy(impl->mSendSubject, symbol, sizeof(impl->mSendSubject)) != strlen(symbol)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not set send subject: %s", symbol);
      return MAMA_STATUS_PLATFORM;
   }

   /* Update the MAMA message with the send subject if it has a parent */
   if (NULL != impl->mParent) {
      CALL_MAMA_FUNC(mamaMsg_updateString(impl->mParent, MamaFieldSubscSymbol.mName, MamaFieldSubscSymbol.mFid, symbol));
   }

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsg_getNativeHandle(msgBridge msg, void** result)
{
   if (NULL == msg || NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   *result = impl;
   return MAMA_STATUS_OK;
}


const char* zmqBridgeMamaMsg_getReplyHandle(msgBridge msg)
{
   if (NULL == msg) {
      return NULL;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   return impl->mReplyHandle;
}


mama_status zmqBridgeMamaMsg_duplicateReplyHandle(msgBridge msg, void** handle)
{
   if (NULL == msg || NULL == handle) {
      return MAMA_STATUS_NULL_ARG;
   }

   const char* replyHandle = zmqBridgeMamaMsg_getReplyHandle(msg);
   if (replyHandle == NULL) {
      return MAMA_STATUS_INVALID_ARG;
   }

   return zmqBridgeMamaMsg_copyReplyHandle(replyHandle, handle);
}


mama_status zmqBridgeMamaMsg_copyReplyHandle(const void* src, void** dest)
{
   if (NULL == src || NULL == dest) {
      return MAMA_STATUS_NULL_ARG;
   }

   *dest = strdup((const char*) src);
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsg_destroyReplyHandle(void* result)
{
   if (NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }

   free(result);
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsgImpl_setReplyHandle(msgBridge msg, void* handle)
{
   if (NULL == msg || NULL == handle) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;
   strcpy(impl->mReplyHandle, (const char*) handle);
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsgImpl_setReplyHandleAndIncrement(msgBridge msg, void* handle)
{
   return zmqBridgeMamaMsgImpl_setReplyHandle(msg, handle);
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

mama_status zmqBridgeMamaMsgImpl_setMsgType(msgBridge msg, zmqMsgType type)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl*  impl = (zmqBridgeMsgImpl*) msg;

   impl->mMsgType = type;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_getSendSubject(msgBridge msg, char** value)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl*  impl = (zmqBridgeMsgImpl*) msg;

   *value = impl->mSendSubject;
   return MAMA_STATUS_OK;
}


/* Non-interface version of create which permits null parent */
mama_status zmqBridgeMamaMsgImpl_createMsgOnly(msgBridge* msg)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl = NULL;

   /* Null initialize the msgBridge pointer */
   *msg = NULL;

   /* Allocate memory for the implementation struct */
   impl = (zmqBridgeMsgImpl*) calloc(1, sizeof(zmqBridgeMsgImpl));
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to allocate memory for bridge message.");
      return MAMA_STATUS_NOMEM;
   }

   /* Populate the msgBridge pointer with the implementation */
   *msg = (msgBridge) impl;

   return zmqBridgeMamaMsgImpl_init(impl);
}



mama_status zmqBridgeMamaMsgImpl_serialize(msgBridge msg, mamaMsg source, zmq_msg_t *zmsg)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   // Serialize payload
   const void* payloadBuffer;
   mama_size_t payloadSize;
   CALL_MAMA_FUNC(mamaMsg_getByteBuffer(source, &payloadBuffer, &payloadSize));

   // get size of buffer needed
   size_t serializedSize = (strlen(impl->mSendSubject) + 1) + sizeof(impl->mMsgType) + payloadSize;
   if (impl->mMsgType== ZMQ_MSG_INBOX_REQUEST) {
      serializedSize += strlen(impl->mReplyHandle);
   }
   serializedSize++;    // trailing null for reply handle (even if not present)

   int rc =zmq_msg_init_size(zmsg, serializedSize);
   if (0 != rc) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "zmq_msg_init_size failed %d(%s)", zmq_errno (), zmq_strerror (errno));
      return MAMA_STATUS_PLATFORM;
   }

   // Ok great - we have a buffer now of appropriate size, let's populate it
   uint8_t* bufferPos = (uint8_t*)zmq_msg_data(zmsg);

   // Copy across the subject
   size_t msgSubjectByteCount = strlen(impl->mSendSubject) + 1;
   memcpy(bufferPos, impl->mSendSubject, msgSubjectByteCount);
   bufferPos += msgSubjectByteCount;

   // Copy across the message type
   memcpy(bufferPos, &impl->mMsgType, sizeof(impl->mMsgType));
   bufferPos+=sizeof(impl->mMsgType);

   // copy reply address (only for request)
   if (impl->mMsgType == ZMQ_MSG_INBOX_REQUEST) {
      size_t msgInboxByteCount = strlen(impl->mReplyHandle);
      memcpy(bufferPos, impl->mReplyHandle, msgInboxByteCount);
      bufferPos += msgInboxByteCount;
   }
   *bufferPos = '\0';   // trailing null for reply handle (even if not present)
   bufferPos++;

   // Copy across the payload
   memcpy((void*)bufferPos, payloadBuffer, payloadSize);

   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsgImpl_deserialize(msgBridge msg, zmq_msg_t *zmsg, mamaMsg target)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   void *source = zmq_msg_data(zmsg);
   mama_size_t size = zmq_msg_size(zmsg);
   uint8_t* bufferPos = (uint8_t*)source;

   // Skip past the subject - don't care about that here
   bufferPos += strlen((char*)source) + 1;

   // Set the message type
   memcpy(&impl->mMsgType, bufferPos, sizeof(impl->mMsgType));
   bufferPos+=sizeof(impl->mMsgType);

   // set reply handle
   if (impl->mMsgType == ZMQ_MSG_INBOX_REQUEST) {
      // for requests, reply address is embedded in msg
      strcpy(impl->mReplyHandle, (const char*)bufferPos);
      bufferPos += strlen((char*) bufferPos);
   }
   else if (impl->mMsgType == ZMQ_MSG_INBOX_RESPONSE) {
      // for responses, reply address is the subject
      strcpy(impl->mReplyHandle, (const char*) source);
   }
   bufferPos++;                     // trailing null for reply handle (even if not present)

   // Parse the payload into a MAMA Message
   int payloadSize = size - (bufferPos - (uint8_t*)source);
   if (payloadSize < 0) {
      MAMA_LOG(MAMA_LOG_LEVEL_SEVERE, "Payload size < 0 for message: %zu bytes [payload=%d; type=%d]", size, payloadSize, impl->mMsgType);
      return MAMA_STATUS_SYSTEM_ERROR;
   }

   MAMA_LOG(MAMA_LOG_LEVEL_FINEST, "Received %zu bytes [payload=%d; type=%d]", size, payloadSize, impl->mMsgType);
   return mamaMsgImpl_setMsgBuffer(target, (void*) bufferPos, payloadSize, *bufferPos);
}


mama_status zmqBridgeMamaMsgImpl_init(zmqBridgeMsgImpl* msg)
{
   msg->mParent = NULL;
   msg->mMsgType = ZMQ_MSG_PUB_SUB;
   strcpy(msg->mReplyHandle, "");
   strcpy(msg->mSendSubject, "");

   return MAMA_STATUS_OK;
}


msgBridge zmqBridgeMamaMsgImpl_getBridgeMsg(mamaMsg mamaMsg)
{
   if (mamaMsg == NULL) {
      return NULL;
   }

   /* Get the bridge message from the mamaMsg */
   msgBridge bridgeMsg;
   mama_status status = mamaMsgImpl_getBridgeMsg(mamaMsg, &bridgeMsg);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not get bridge message from mamaMsg [%s]", mamaStatus_stringForStatus(status));
      return NULL;
   }

   return bridgeMsg;
}