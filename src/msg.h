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

#ifndef MAMA_BRIDGE_ZMQ_MSG_H__
#define MAMA_BRIDGE_ZMQ_MSG_H__


/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

#include "zmqdefs.h"


#if defined(__cplusplus)
extern "C" {
#endif


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

/**
 * This will set the bridge's internal message type according to the value
 * provided.
 *
 * @param msg    The bridge message to update.
 * @param type   The new bridge message type (e.g. ZMQ_MSG_PUB_SUB).
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
mama_status
zmqBridgeMamaMsgImpl_setMsgType(msgBridge    msg,
                                zmqMsgType  type);


/**
 * This will get the bridge's send subject which is currently the topic name.
 * Note the setter for this is an interface bridge function.
 *
 * @param msg    The bridge message to examine.
 * @param type   Pointer to populate with the value of the send subject.
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
mama_status
zmqBridgeMamaMsgImpl_getSendSubject(msgBridge    msg,
                                    char**       value);

/**
 * This will create a bridge message but will not create a parent with it.
 *
 * @param msg    The bridge message to create.
 *
 * @return mama_status indicating whether the method succeeded or failed.
 */
mama_status zmqBridgeMamaMsgImpl_createMsgOnly(msgBridge*  msg);


mama_status zmqBridgeMamaMsgImpl_serialize(msgBridge msg, mamaMsg source, zmq_msg_t *zmsg);
mama_status zmqBridgeMamaMsgImpl_deserialize(msgBridge msg, zmq_msg_t *zmsg, mamaMsg target);
const char* zmqBridgeMamaMsg_getReplyHandle(msgBridge msg);
msgBridge zmqBridgeMamaMsgImpl_getBridgeMsg(mamaMsg mamaMsg);
mama_status zmqBridgeMamaMsgImpl_init(zmqBridgeMsgImpl* msg);


#if defined(__cplusplus)
}
#endif

#endif /* MAMA_BRIDGE_ZMQ_MSG_H__ */
