/*
 * The MIT License (MIT)
 *
 * Original work Copyright (c) 2015 Frank Quinn (http://fquinner.github.io)
 * Modified work Copyright (c) 2020 Bill Torpey (http://btorpey.github.io)
 * and assigned to NYFIX, a division of Itiviti Group AB
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

// MAMA includes
#include <mama/mama.h>
#include <mama/integration/queue.h>
#include <wombat/wInterlocked.h>

// local includes
#include "zmqbridgefunctions.h"
#include "zmqdefs.h"
#include "uqueue.h"

/**
 * This funcion is called to check the current queue size against configured
 * watermarks to determine whether or not it should call the watermark callback
 * functions. If it determines that it should, it invokes the relevant callback
 * itself.
 *
 * @param impl The zmq queue bridge implementation to check.
 */
static void zmqBridgeMamaQueueImpl_checkWatermarks(zmqQueueBridge* impl);


mama_status zmqBridgeMamaQueue_create(queueBridge* queue, mamaQueue parent)
{
   /* Null initialize the queue to be created */
   zmqQueueBridge*    impl                = NULL;
   wombatQueueStatus  underlyingStatus    = WOMBAT_QUEUE_OK;

   if (queue == NULL || parent == NULL) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Null initialize the queueBridge */
   *queue = NULL;

   /* Allocate memory for the zmq queue implementation */
   impl = (zmqQueueBridge*) calloc(1, sizeof(zmqQueueBridge));
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to allocate memory for queue.");
      return MAMA_STATUS_NOMEM;
   }

   /* Initialize the active flag */
   wInterlocked_initialize(&impl->mIsActive);
   wInterlocked_set(1, &impl->mIsActive);

   /* Initialize the dispatch lock */
   wthread_mutex_init(&impl->mDispatchLock, NULL);

   /* Back-reference the parent for future use in the implementation struct */
   impl->mParent = parent;

   /* Allocate and create the wombat queue */
   underlyingStatus = uQueue_allocate(&impl->mQueue);
   if (WOMBAT_QUEUE_OK != underlyingStatus) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to allocate memory for underlying queue.");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   underlyingStatus = uQueue_create(impl->mQueue, ZMQ_QUEUE_MAX_SIZE, ZMQ_QUEUE_INITIAL_SIZE, ZMQ_QUEUE_CHUNK_SIZE);
   if (WOMBAT_QUEUE_OK != underlyingStatus) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create underlying queue.");
      uQueue_deallocate(impl->mQueue);
      free(impl);
      return MAMA_STATUS_PLATFORM;
   }

   /* Populate the queueBridge pointer with the implementation for return */
   *queue = (queueBridge) impl;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_create_usingNative(queueBridge* queue,
  mamaQueue parent, void* nativeQueue)
{
   zmqQueueBridge* impl = NULL;
   if (NULL == queue || NULL == parent || NULL == nativeQueue) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Null initialize the queueBridge to be returned */
   *queue = NULL;

   /* Allocate memory for the zmq bridge implementation */
   impl = (zmqQueueBridge*) calloc(1, sizeof(zmqQueueBridge));
   if (NULL == impl) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to allocate memory for queue.");
      return MAMA_STATUS_NOMEM;
   }

   /* Back-reference the parent for future use in the implementation struct */
   impl->mParent = parent;

   /* Wombat queue has already been created, so simply reference it here */
   impl->mQueue = (uQueue) nativeQueue;

   /* Populate the queueBridge pointer with the implementation for return */
   *queue = (queueBridge) impl;

   return MAMA_STATUS_OK;     // cppcheck-suppress memleak
}

mama_status zmqBridgeMamaQueue_destroy(queueBridge queue)
{
   wombatQueueStatus  status  = WOMBAT_QUEUE_OK;
   zmqQueueBridge*    impl    = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Destroy the underlying wombatQueue - can be called from any thread*/
   wthread_mutex_lock(&impl->mDispatchLock);
   status = uQueue_destroy(impl->mQueue);
   wthread_mutex_unlock(&impl->mDispatchLock);

   /* Free the zmqQueueImpl container struct */
   free(impl);

   if (WOMBAT_QUEUE_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Failed to destroy wombat queue (%d).", status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_getEventCount(queueBridge queue, size_t* count)
{
   zmqQueueBridge* impl       = (zmqQueueBridge*) queue;
   int             countInt   = 0;

   if (NULL == count) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Initialize count to zero */
   *count = 0;

   /* Get the wombatQueue size */
   uQueue_getSize(impl->mQueue, &countInt);
   *count = (size_t)countInt;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_dispatch(queueBridge queue)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Lock for dispatching */
   wthread_mutex_lock(&impl->mDispatchLock);

   wInterlocked_initialize(&impl->mIsDispatching);
   wInterlocked_set(1, &impl->mIsDispatching);

   /*
    * Continually dispatch as long as the calling application wants dispatching
    * to be done and no errors are encountered
    */
   do {
      /* Check the watermarks to see if thresholds have been breached */
      zmqBridgeMamaQueueImpl_checkWatermarks(impl);

      /*
       * Perform a dispatch with a timeout to allow the dispatching process
       * to be interrupted by the calling application between iterations
       */
      status = uQueue_timedDispatch(impl->mQueue, ZMQ_QUEUE_DISPATCH_TIMEOUT);
   }
   while ((WOMBAT_QUEUE_OK == status || WOMBAT_QUEUE_TIMEOUT == status)
          && wInterlocked_read(&impl->mIsDispatching) == 1);

   /* Unlock the dispatch lock */
   wthread_mutex_unlock(&impl->mDispatchLock);

   /* Timeout is encountered after each dispatch and so is expected here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to dispatch Zmq Middleware queue (%d). ", status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_timedDispatch(queueBridge queue, uint64_t timeout)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl        = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Check the watermarks to see if thresholds have been breached */
   zmqBridgeMamaQueueImpl_checkWatermarks(impl);

   /* Attempt to dispatch the queue with a timeout once */
   status = uQueue_timedDispatch(impl->mQueue, timeout);

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to dispatch Middleware queue (%d).", status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;

}

mama_status zmqBridgeMamaQueue_dispatchEvent(queueBridge queue)
{
   wombatQueueStatus  status;
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Check the watermarks to see if thresholds have been breached */
   zmqBridgeMamaQueueImpl_checkWatermarks(impl);

   /* Attempt to dispatch the queue with a timeout once */
   status = uQueue_dispatch(impl->mQueue);

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to dispatch Middleware queue (%d).",  status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_enqueueEventEx(queueBridge queue,
  mamaQueueEventCB callback, void* closure)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   if (wInterlocked_read(&impl->mIsActive) == 1) {
       return zmqBridgeMamaQueue_enqueueEvent(queue, callback, closure);
   }

   // silently drop events if the queue is set to inactive
   MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Dropping event from inactive queue");
   return MAMA_STATUS_OK;
}

static mama_status zmqBridgeMamaQueue_enqueueEventInt(queueBridge queue,
  mamaQueueEventCB callback, void* closure, uint8_t isMsg)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl = (zmqQueueBridge*) queue;

   if (NULL == callback) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   // dont enqueue event if queue is not dispatching
   if (wInterlocked_read(&impl->mIsDispatching) != 1) {
      MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Attempt to enqueue event on non-dispatching queue");
      // TODO - resolve race condition (?!)
      // following causes programs to intermittently fail at startup -- there is a race condition
      // where mIsDispatching is 0 and causes subsequent session create to fail
      //return MAMA_STATUS_INVALID_ARG;
   }

   /* Call the underlying wombatQueue_enqueue method */
   status = uQueue_enqueue(impl->mQueue, (wombatQueueCb) callback, impl->mParent, closure, isMsg);

   /* Call the enqueue callback if provided */
   if (NULL != impl->mEnqueueCallback) {
      impl->mEnqueueCallback(impl->mParent, impl->mEnqueueClosure);
   }

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to enqueueEvent (%d). Callback: %p; Closure: %p", status, callback, closure);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_enqueueMsg(queueBridge queue, mamaQueueEnqueueCB callback, struct zmqTransportMsg_ *msg)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   if (wInterlocked_read(&impl->mIsActive) == 1) {
      return zmqBridgeMamaQueue_enqueueEventInt(queue, callback, msg, 1);
   }

   // silently drop events if the queue is set to inactive
   MAMA_LOG(MAMA_LOG_LEVEL_WARN, "Dropping event from inactive queue");
   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_enqueueEvent(queueBridge queue, mamaQueueEventCB callback, void* closure) {
   return zmqBridgeMamaQueue_enqueueEventInt(queue, callback, closure, 0);
}

mama_status zmqBridgeMamaQueue_stopDispatch(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Tell this implementation to stop dispatching */
   wInterlocked_set(0, &impl->mIsDispatching);

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_setEnqueueCallback(queueBridge queue,
  mamaQueueEnqueueCB callback, void* closure)
{
   zmqQueueBridge* impl   = (zmqQueueBridge*) queue;

   if (NULL == callback) {
      return MAMA_STATUS_NULL_ARG;
   }
   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Set the enqueue callback and closure */
   impl->mEnqueueCallback  = callback;
   impl->mEnqueueClosure   = closure;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_removeEnqueueCallback(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Set the enqueue callback to NULL */
   impl->mEnqueueCallback  = NULL;
   impl->mEnqueueClosure   = NULL;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_getNativeHandle(queueBridge queue, void** nativeHandle)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   if (NULL == nativeHandle) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Return the handle to the native queue */
   *nativeHandle = queue;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_setHighWatermark(queueBridge queue, size_t highWatermark)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   if (0 == highWatermark) {
      return MAMA_STATUS_NULL_ARG;
   }
   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Set the high water mark */
   impl->mHighWatermark = highWatermark;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_setLowWatermark(queueBridge queue, size_t lowWatermark)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   if (0 == lowWatermark) {
      return MAMA_STATUS_NULL_ARG;
   }
   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Set the low water mark */
   impl->mLowWatermark = lowWatermark;

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_activate(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   wInterlocked_set(1, &impl->mIsActive);

   return MAMA_STATUS_OK;
}

mama_status zmqBridgeMamaQueue_deactivate(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   wInterlocked_set(0, &impl->mIsActive);

   return MAMA_STATUS_OK;
}

void zmqBridgeMamaQueueImpl_checkWatermarks(zmqQueueBridge* impl)
{
   size_t              eventCount      =  0;

   /* Get the current size of the wombat impl */
   zmqBridgeMamaQueue_getEventCount((queueBridge) impl, &eventCount);

   /* If the high watermark had been fired but the event count is now down */
   if (0 != impl->mHighWaterFired && eventCount == impl->mLowWatermark) {
      impl->mHighWaterFired = 0;
      mamaQueueImpl_lowWatermarkExceeded(impl->mParent, eventCount);
   }
   /* If the high watermark is not currently fired and now above threshold */
   else if (0 == impl->mHighWaterFired && eventCount >= impl->mHighWatermark) {
      impl->mHighWaterFired = 1;
      mamaQueueImpl_highWatermarkExceeded(impl->mParent, eventCount);
   }
}


