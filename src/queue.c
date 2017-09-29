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
#include <wombat/wInterlocked.h>
#include <wombat/queue.h>
#include <bridge.h>
#include "queueimpl.h"
#include "zmqbridgefunctions.h"
#include "zmqdefs.h"


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/


/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

#define     CHECK_QUEUE(IMPL)                                                  \
   do {                                                                       \
      if (IMPL == NULL)              return MAMA_STATUS_NULL_ARG;            \
      if (IMPL->mQueue == NULL)      return MAMA_STATUS_NULL_ARG;            \
   } while(0)

/* Timeout is in milliseconds */
#define     ZMQ_QUEUE_DISPATCH_TIMEOUT     500
#define     ZMQ_QUEUE_MAX_SIZE             WOMBAT_QUEUE_MAX_SIZE
#define     ZMQ_QUEUE_CHUNK_SIZE           WOMBAT_QUEUE_CHUNK_SIZE
#define     ZMQ_QUEUE_INITIAL_SIZE         WOMBAT_QUEUE_CHUNK_SIZE


/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

/**
 * This funcion is called to check the current queue size against configured
 * watermarks to determine whether or not it should call the watermark callback
 * functions. If it determines that it should, it invokes the relevant callback
 * itself.
 *
 * @param impl The zmq queue bridge implementation to check.
 */
static void
zmqBridgeMamaQueueImpl_checkWatermarks(zmqQueueBridge* impl);


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

mama_status
zmqBridgeMamaQueue_create(queueBridge* queue,
                          mamaQueue    parent)
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
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_create (): "
               "Failed to allocate memory for queue.");
      return MAMA_STATUS_NOMEM;
   }

   /* Initialize the dispatch lock */
   wthread_mutex_init(&impl->mDispatchLock, NULL);

   /* Back-reference the parent for future use in the implementation struct */
   impl->mParent = parent;

   /* Allocate and create the wombat queue */
   underlyingStatus = wombatQueue_allocate(&impl->mQueue);
   if (WOMBAT_QUEUE_OK != underlyingStatus) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_create (): "
               "Failed to allocate memory for underlying queue.");
      free(impl);
      return MAMA_STATUS_NOMEM;
   }

   underlyingStatus = wombatQueue_create(impl->mQueue,
                                         ZMQ_QUEUE_MAX_SIZE,
                                         ZMQ_QUEUE_INITIAL_SIZE,
                                         ZMQ_QUEUE_CHUNK_SIZE);
   if (WOMBAT_QUEUE_OK != underlyingStatus) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_create (): "
               "Failed to create underlying queue.");
      wombatQueue_deallocate(impl->mQueue);
      free(impl);
      return MAMA_STATUS_PLATFORM;
   }

   /* Populate the queueBridge pointer with the implementation for return */
   *queue = (queueBridge) impl;

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_create_usingNative(queueBridge* queue,
                                      mamaQueue    parent,
                                      void*        nativeQueue)
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
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_create_usingNative (): "
               "Failed to allocate memory for queue.");
      return MAMA_STATUS_NOMEM;
   }

   /* Back-reference the parent for future use in the implementation struct */
   impl->mParent = parent;

   /* Wombat queue has already been created, so simply reference it here */
   impl->mQueue = (wombatQueue) nativeQueue;

   /* Populate the queueBridge pointer with the implementation for return */
   *queue = (queueBridge) impl;

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_destroy(queueBridge queue)
{
   wombatQueueStatus  status  = WOMBAT_QUEUE_OK;
   zmqQueueBridge*    impl    = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Destroy the underlying wombatQueue - can be called from any thread*/
   wthread_mutex_lock(&impl->mDispatchLock);
   status = wombatQueue_destroy(impl->mQueue);
   wthread_mutex_unlock(&impl->mDispatchLock);

   if (NULL != impl->mClosureCleanupCb && NULL != impl->mClosure) {
      impl->mClosureCleanupCb(impl->mClosure);
   }

   /* Free the zmqQueueImpl container struct */
   free(impl);

   if (WOMBAT_QUEUE_OK != status) {
      mama_log(MAMA_LOG_LEVEL_WARN,
               "zmqBridgeMamaQueue_destroy (): "
               "Failed to destroy wombat queue (%d).",
               status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_getEventCount(queueBridge queue, size_t* count)
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
   wombatQueue_getSize(impl->mQueue, &countInt);
   *count = (size_t)countInt;

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_dispatch(queueBridge queue)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Lock for dispatching */
   wthread_mutex_lock(&impl->mDispatchLock);

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
      status = wombatQueue_timedDispatch(impl->mQueue,
                                         NULL,
                                         NULL,
                                         ZMQ_QUEUE_DISPATCH_TIMEOUT);
   }
   while ((WOMBAT_QUEUE_OK == status || WOMBAT_QUEUE_TIMEOUT == status)
          && wInterlocked_read(&impl->mIsDispatching) == 1);

   /* Unlock the dispatch lock */
   wthread_mutex_unlock(&impl->mDispatchLock);

   /* Timeout is encountered after each dispatch and so is expected here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_dispatch (): "
               "Failed to dispatch Zmq Middleware queue (%d). ",
               status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_timedDispatch(queueBridge queue, uint64_t timeout)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl        = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Check the watermarks to see if thresholds have been breached */
   zmqBridgeMamaQueueImpl_checkWatermarks(impl);

   /* Attempt to dispatch the queue with a timeout once */
   status = wombatQueue_timedDispatch(impl->mQueue,
                                      NULL,
                                      NULL,
                                      timeout);

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_timedDispatch (): "
               "Failed to dispatch Qpid Middleware queue (%d).",
               status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;

}

mama_status
zmqBridgeMamaQueue_dispatchEvent(queueBridge queue)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Check the watermarks to see if thresholds have been breached */
   zmqBridgeMamaQueueImpl_checkWatermarks(impl);

   /* Attempt to dispatch the queue with a timeout once */
   status = wombatQueue_dispatch(impl->mQueue, NULL, NULL);

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status && WOMBAT_QUEUE_TIMEOUT != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_dispatchEvent (): "
               "Failed to dispatch Middleware queue (%d).",
               status);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_enqueueEvent(queueBridge        queue,
                                mamaQueueEventCB   callback,
                                void*              closure)
{
   wombatQueueStatus  status;
   zmqQueueBridge*    impl = (zmqQueueBridge*) queue;

   if (NULL == callback) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Call the underlying wombatQueue_enqueue method */
   status = wombatQueue_enqueue(impl->mQueue,
                                (wombatQueueCb) callback,
                                impl->mParent,
                                closure);

   /* Call the enqueue callback if provided */
   if (NULL != impl->mEnqueueCallback) {
      impl->mEnqueueCallback(impl->mParent, impl->mEnqueueClosure);
   }

   /* If dispatch failed, report here */
   if (WOMBAT_QUEUE_OK != status) {
      mama_log(MAMA_LOG_LEVEL_ERROR,
               "zmqBridgeMamaQueue_enqueueEvent (): "
               "Failed to enqueueEvent (%d). Callback: %p; Closure: %p",
               status, callback, closure);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_stopDispatch(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Tell this implementation to stop dispatching */
   wInterlocked_set(0, &impl->mIsDispatching);

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_setEnqueueCallback(queueBridge        queue,
                                      mamaQueueEnqueueCB callback,
                                      void*              closure)
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

mama_status
zmqBridgeMamaQueue_removeEnqueueCallback(queueBridge queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   /* Perform null checks and return if null arguments provided */
   CHECK_QUEUE(impl);

   /* Set the enqueue callback to NULL */
   impl->mEnqueueCallback  = NULL;
   impl->mEnqueueClosure   = NULL;

   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaQueue_getNativeHandle(queueBridge queue,
                                   void**      nativeHandle)
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

mama_status
zmqBridgeMamaQueue_setHighWatermark(queueBridge queue,
                                    size_t      highWatermark)
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

mama_status
zmqBridgeMamaQueue_setLowWatermark(queueBridge    queue,
                                   size_t         lowWatermark)
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


/*=========================================================================
  =                  Public implementation functions                      =
  =========================================================================*/

void
zmqBridgeMamaQueueImpl_setClosure(queueBridge              queue,
                                  void*                    closure,
                                  zmqQueueClosureCleanup   callback)
{
   zmqQueueBridge* impl    = (zmqQueueBridge*) queue;
   impl->mClosure          = closure;
   impl->mClosureCleanupCb = callback;
}

void*
zmqBridgeMamaQueueImpl_getClosure(queueBridge              queue)
{
   zmqQueueBridge* impl = (zmqQueueBridge*) queue;

   return impl->mClosure;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/

void
zmqBridgeMamaQueueImpl_checkWatermarks(zmqQueueBridge* impl)
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


