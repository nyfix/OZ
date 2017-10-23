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

// MAMA includes
#include <mama/mama.h>
#include <mama/timer.h>
#include <timers.h>
#include <wombat/queue.h>

// local includes
#include "zmqbridgefunctions.h"
#include "util.h"


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

extern timerHeap gOmzmqTimerHeap;

typedef struct zmqTimerImpl_ {
   timerElement    mTimerElement;
   double          mInterval;
   void*           mClosure;
   mamaTimer       mParent;
   void*           mQueue;
   uint8_t         mDestroying;
   /* This callback will be invoked whenever the timer has been destroyed. */
   mamaTimerCb     mOnTimerDestroyed;
   /* This callback will be invoked on each timer firing */
   mamaTimerCb     mAction;
} zmqTimerImpl;


/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

/**
 * Due to the fact that timed events may still be on the event queue, the
 * timer's destroy function does not destroy the implementation immediately.
 * Instead, it sets an implementation specific flag to stop further callbacks
 * from being enqueued from this timer, and then enqueues this function as a
 * callback on the queue to perform the actual destruction. This function also
 * calls the application developer's destroy callback function.
 *
 * @param queue   MAMA queue from which this callback was fired.
 * @param closure In this instance, the closure will contain the zmq timer
 *                implementation.
 */
static void MAMACALLTYPE
zmqBridgeMamaTimerImpl_destroyCallback(mamaQueue queue, void* closure);

/**
 * When a timer fires, it enqueues this callback for execution. This is where
 * the action callback provided in the timer's create function gets fired.
 *
 * @param queue   MAMA queue from which this callback was fired.
 * @param closure In this instance, the closure will contain the zmq timer
 *                implementation.
 */
static void MAMACALLTYPE
zmqBridgeMamaTimerImpl_queueCallback(mamaQueue queue, void* closure);

/**
 * Every time the timer fires, it calls this timer callback which adds
 * zmqBridgeMamaTimerImpl_queueCallback to the queue as long as the timer's
 * mDestroying flag is not currently set.
 *
 * @param timer   The underlying timer element which has just fired (not used).
 * @param closure In this instance, the closure will contain the zmq timer
 *                implementation.
 */
static void
zmqBridgeMamaTimerImpl_timerCallback(timerElement timer, void* closure);


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

mama_status
zmqBridgeMamaTimer_create(timerBridge*  result,
                          void*         nativeQueueHandle,
                          mamaTimerCb   action,
                          mamaTimerCb   onTimerDestroyed,
                          double        interval,
                          mamaTimer     parent,
                          void*         closure)
{

   zmqTimerImpl*              impl            = NULL;
   int                        timerResult     = 0;
   struct timeval             timeout;

   if (NULL == result || NULL == nativeQueueHandle
       || NULL == action
       || NULL == parent) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Null initialize the timer bridge supplied */
   *result = NULL;

   /* Allocate the timer implementation and set up */
   impl = (zmqTimerImpl*) calloc(1, sizeof(zmqTimerImpl));
   if (NULL == impl) {
      return MAMA_STATUS_NOMEM;
   }

   *result                     = (timerBridge) impl;
   impl->mQueue                = nativeQueueHandle;
   impl->mParent               = parent;
   impl->mAction               = action;
   impl->mClosure              = closure;
   impl->mInterval             = interval;
   impl->mOnTimerDestroyed     = onTimerDestroyed;
   impl->mDestroying           = 0;

   /* Determine when the next timer should fire */
   timeout.tv_sec  = (time_t) interval;
   timeout.tv_usec = ((interval - timeout.tv_sec) * 1000000.0);

   /* Create the first single fire timer */
   timerResult = createTimer(&impl->mTimerElement,
                             gOmzmqTimerHeap,
                             zmqBridgeMamaTimerImpl_timerCallback,
                             &timeout,
                             impl);
   if (0 != timerResult) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create underlying timer [%d].", timerResult);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

/* This call should always come from MAMA queue thread */
mama_status
zmqBridgeMamaTimer_destroy(timerBridge timer)
{
   zmqTimerImpl*  impl            = NULL;
   mama_status    returnStatus    = MAMA_STATUS_OK;
   int            timerResult     = 0;

   if (NULL == timer) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Nullify the callback and set destroy flag */
   impl                            = (zmqTimerImpl*) timer;
   impl->mDestroying               = 1;
   impl->mAction                   = NULL;

   /* Destroy the timer element */
   timerResult = destroyTimer(gOmzmqTimerHeap, impl->mTimerElement);
   if (0 != timerResult) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to destroy underlying timer [%d].", timerResult);
      returnStatus = MAMA_STATUS_PLATFORM;
   }

   /*
    * Put the impl free at the back of the queue to be executed when all
    * pending timer events have been completed
    */
   zmqBridgeMamaQueue_enqueueEvent((queueBridge) impl->mQueue,
                                   zmqBridgeMamaTimerImpl_destroyCallback,
                                   (void*) impl);

   return returnStatus;
}

mama_status
zmqBridgeMamaTimer_reset(timerBridge timer)
{
   zmqTimerImpl*      impl            = (zmqTimerImpl*) timer;
   int                timerResult     = 0;
   struct timeval     timeout;

   if (NULL == impl) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Destroy the existing timer element */
   destroyTimer(gOmzmqTimerHeap, impl->mTimerElement);

   /* Calculate next time interval */
   timeout.tv_sec  = (time_t) impl->mInterval;
   timeout.tv_usec = ((impl->mInterval - timeout.tv_sec) * 1000000.0);

   /* Create the timer for the next firing */
   timerResult = createTimer(&impl->mTimerElement,
                             gOmzmqTimerHeap,
                             zmqBridgeMamaTimerImpl_timerCallback,
                             &timeout,
                             impl);
   if (0 != timerResult) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to reset underlying timer [%d].", timerResult);
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;

}

mama_status
zmqBridgeMamaTimer_setInterval(timerBridge  timer,
                               mama_f64_t   interval)
{
   zmqTimerImpl* impl  = (zmqTimerImpl*) timer;
   if (NULL == timer) {
      return MAMA_STATUS_NULL_ARG;
   }

   impl->mInterval = interval;

   return  zmqBridgeMamaTimer_reset(timer);
}

mama_status
zmqBridgeMamaTimer_getInterval(timerBridge    timer,
                               mama_f64_t*    interval)
{
   zmqTimerImpl* impl  = NULL;
   if (NULL == timer || NULL == interval) {
      return MAMA_STATUS_NULL_ARG;
   }

   impl = (zmqTimerImpl*) timer;
   *interval = impl->mInterval;

   return MAMA_STATUS_OK;
}


/*=========================================================================
  =                  Private implementation functions                     =
  =========================================================================*/

/* This callback is invoked by the zmq bridge's destroy event */
static void MAMACALLTYPE
zmqBridgeMamaTimerImpl_destroyCallback(mamaQueue queue, void* closure)
{
   zmqTimerImpl* impl = (zmqTimerImpl*) closure;
   (*impl->mOnTimerDestroyed)(impl->mParent, impl->mClosure);

   /* Free the implementation memory here */
   free(impl);
}

/* This callback is invoked by the zmq bridge's timer event */
static void MAMACALLTYPE
zmqBridgeMamaTimerImpl_queueCallback(mamaQueue queue, void* closure)
{
   zmqTimerImpl* impl = (zmqTimerImpl*) closure;
   if (impl->mAction) {
      impl->mAction(impl->mParent, impl->mClosure);
   }
}

/* This callback is invoked by the common timer's dispatch thread */
static void
zmqBridgeMamaTimerImpl_timerCallback(timerElement  timer,
                                     void*         closure)
{
   zmqTimerImpl* impl = (zmqTimerImpl*) closure;

   if (NULL == impl) {
      return;
   }

   /*
    * Only enqueue further timer callbacks the timer is not currently getting
    * destroyed
    */
   if (0 == impl->mDestroying) {
      /* Set the timer for the next firing */
      zmqBridgeMamaTimer_reset((timerBridge) closure);

      /* Enqueue the callback for handling */
      zmqBridgeMamaQueue_enqueueEvent((queueBridge) impl->mQueue,
                                      zmqBridgeMamaTimerImpl_queueCallback,
                                      closure);
   }
}
