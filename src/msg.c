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

#include <stdlib.h>
#include <string.h>
#include <mama/mama.h>
#include <msgimpl.h>
#include "transport.h"
#include "msg.h"

#include <wombat/memnode.h>

#include "zmqbridgefunctions.h"


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

typedef struct zmqBridgeMsgImpl {
   mamaMsg                     mParent;
   zmqMsgType                  mMsgType;
   uint8_t                     mIsValid;
   const char*                 mReplyHandle;
   char                        mSendSubject[MAX_SUBJECT_LENGTH];
   void*                       mSerializedBuffer;
   size_t                      mSerializedBufferSize;
   size_t                      mPayloadSize;
} zmqBridgeMsgImpl;


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

   mama_status status = zmqBridgeMamaMsgImpl_createMsgOnly(msg);
   if (MAMA_STATUS_OK != status) {
      return status;
   }

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

   if ((impl->mReplyHandle != NULL) && (impl->mReplyHandle[0] != '\0')) {
      return 1;
   }

   return 0;
}

mama_status zmqBridgeMamaMsg_destroy(msgBridge msg, int destroyMsg)
{
   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   free((void*) impl->mSerializedBuffer);
   free((void*) impl->mReplyHandle);
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
   if (NULL == msg || NULL == symbol || (NULL == symbol && NULL == subject)) {
      return MAMA_STATUS_NULL_ARG;
   }

   zmqBridgeMsgImpl* impl     = (zmqBridgeMsgImpl*) msg;
   mama_status        status   = MAMA_STATUS_OK;


   if (wmStrSizeCpy(impl->mSendSubject, symbol, sizeof(impl->mSendSubject)) != strlen(symbol)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Could not set send subject: %s", symbol);
      return MAMA_STATUS_PLATFORM;
   }

   /* Update the MAMA message with the send subject if it has a parent */
   if (NULL != impl->mParent) {
      status = mamaMsg_updateString(impl->mParent,
                                    MamaFieldSubscSymbol.mName,
                                    MamaFieldSubscSymbol.mFid,
                                    symbol);
   }
   return status;
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

   *handle = strdup(replyHandle);
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsg_copyReplyHandle(void* src, void** dest)
{
   if (NULL == src || NULL == dest) {
      return MAMA_STATUS_NULL_ARG;
   }

   *dest = strdup((const char*) src);
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsg_destroyReplyHandle(void* result)
{
   // TODO: What do we do here if the replyHandle is attached to a message?
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
   free((void*) impl->mReplyHandle);
   impl->mReplyHandle = strdup((const char*) handle);
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_setReplyHandleAndIncrement(msgBridge msg, void* handle)
{
   return zmqBridgeMamaMsgImpl_setReplyHandle(msg, handle);
}


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

mama_status zmqBridgeMamaMsgImpl_isValid(msgBridge    msg,
                                         uint8_t*     result)
{
   zmqBridgeMsgImpl* impl   = (zmqBridgeMsgImpl*) msg;
   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
      *result = 0;
   }

   *result = impl->mIsValid;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_setMsgType(msgBridge     msg,
                                            zmqMsgType   type)
{
   zmqBridgeMsgImpl*  impl        = (zmqBridgeMsgImpl*) msg;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }
   impl->mMsgType = type;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_getMsgType(msgBridge     msg,
                                            zmqMsgType*  type)
{
   zmqBridgeMsgImpl*  impl        = (zmqBridgeMsgImpl*) msg;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }
   *type = impl->mMsgType;
   return MAMA_STATUS_OK;
}


mama_status zmqBridgeMamaMsgImpl_getPayloadSize(msgBridge   msg,
                                                size_t*     size)
{
   zmqBridgeMsgImpl*  impl        = (zmqBridgeMsgImpl*) msg;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }
   *size = impl->mPayloadSize;
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_getSendSubject(msgBridge     msg,
                                                char**        value)
{
   zmqBridgeMsgImpl*  impl        = (zmqBridgeMsgImpl*) msg;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }
   *value = impl->mSendSubject;
   return MAMA_STATUS_OK;
}

/* Non-interface version of create which permits null parent */
mama_status zmqBridgeMamaMsgImpl_createMsgOnly(msgBridge* msg)
{
   zmqBridgeMsgImpl* impl = NULL;

   if (NULL == msg) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Null initialize the msgBridge pointer */
   *msg = NULL;

   /* Allocate memory for the implementation struct */
   impl = (zmqBridgeMsgImpl*) calloc(1, sizeof(zmqBridgeMsgImpl));
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to allocate memory for bridge message.");
      return MAMA_STATUS_NOMEM;
   }

   /* Back reference the parent message */
   impl->mIsValid      = 1;

   /* Populate the msgBridge pointer with the implementation */
   *msg = (msgBridge) impl;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_serialize(msgBridge msg, mamaMsg source, void** target, size_t* size)
{
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;
   mama_size_t msgSize = 0;
   const void* msgBuffer = NULL;
   size_t msgSubjectByteCount = 0;
   size_t msgInboxByteCount = 0;
   size_t msgReplyToByteCount = 0;
   size_t msgTargetSubjectByteCount = 0;
   size_t serializedSize = 0;

   // Serialize payload
   CALL_MAMA_FUNC(mamaMsg_getByteBuffer(source, &msgBuffer, &msgSize));

   // TODO: no naked constants
   serializedSize = strlen(impl->mSendSubject) + 10 + msgSize;

   switch (impl->mMsgType) {
      case ZMQ_MSG_INBOX_REQUEST:
      case ZMQ_MSG_INBOX_RESPONSE:
         serializedSize += strlen(impl->mReplyHandle) + 1;
         break;
      case ZMQ_MSG_SUB_REQUEST:
      case ZMQ_MSG_PUB_SUB:
      default:
         break;
   }

   allocateBufferMemory(&impl->mSerializedBuffer, &impl->mSerializedBufferSize, serializedSize);

   // Ok great - we have a buffer now of appropriate size, let's populate it
   uint8_t* bufferPos = (uint8_t*)impl->mSerializedBuffer;

   // Copy across the subject
   msgSubjectByteCount = strlen(impl->mSendSubject) + 1;
   memcpy(bufferPos, impl->mSendSubject, msgSubjectByteCount);
   bufferPos += msgSubjectByteCount;

   // this is just silly?!
   #if 0
   // Leave 8 bytes empty - receive side will be thankful for them
   memset((void*)bufferPos, 0, 8);
   bufferPos += 8;
   #endif

   // Copy across the message type
   *bufferPos = (uint8_t) impl->mMsgType;
   bufferPos++;

   switch (impl->mMsgType) {
      case ZMQ_MSG_INBOX_REQUEST:
      case ZMQ_MSG_INBOX_RESPONSE:
         // Copy across reply handle
         msgInboxByteCount = strlen(impl->mReplyHandle) + 1;
         memcpy(bufferPos, impl->mReplyHandle, msgInboxByteCount);
         bufferPos += msgInboxByteCount;
         break;
      case ZMQ_MSG_SUB_REQUEST:
      case ZMQ_MSG_PUB_SUB:
      default:
         break;
   }

   // Copy across the payload
   memcpy((void*)bufferPos, msgBuffer, msgSize);
   impl->mPayloadSize = msgSize;

   // Populate return pointers
   *target = impl->mSerializedBuffer;
   *size = serializedSize;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaMsgImpl_deserialize(msgBridge msg, const void* source, mama_size_t size, mamaMsg target)
{
   zmqBridgeMsgImpl* impl = (zmqBridgeMsgImpl*) msg;

   uint8_t* bufferPos = (uint8_t*)source;

   // Skip past the subject - don't care about that here
   bufferPos += strlen((char*)source) + 1;

   // this is just silly?!
   #if 0
   // Leave 8 bytes empty - receive side will be thankful for them
   memset((void*)bufferPos, 0, 8);
   bufferPos += 8;
   #endif

   // Set the message type
   impl->mMsgType = (zmqMsgType) * bufferPos;
   bufferPos++;

   if (impl->mReplyHandle) {
      free((void*) impl->mReplyHandle);
      impl->mReplyHandle = NULL;
   }

   switch (impl->mMsgType) {
      case ZMQ_MSG_INBOX_REQUEST:
      case ZMQ_MSG_INBOX_RESPONSE:
         impl->mReplyHandle = strdup((const char*)bufferPos);
         bufferPos += strlen(impl->mReplyHandle) + 1;
         break;
      case ZMQ_MSG_SUB_REQUEST:
      case ZMQ_MSG_PUB_SUB:
      default:
         break;
   }

   // Parse the payload into a MAMA Message
   size_t payloadSize = size - (bufferPos - (uint8_t*)source);

   MAMA_LOG(MAMA_LOG_LEVEL_FINER, "Received %lu bytes [payload=%lu; type=%d]", size, payloadSize, impl->mMsgType);

   return mamaMsgImpl_setMsgBuffer(target, (void*) bufferPos, payloadSize, *bufferPos);
}

