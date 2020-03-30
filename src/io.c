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

/*=========================================================================
  =                             Includes                                  =
  =========================================================================*/

// MAMA includes
#include <mama/mama.h>
#include <mama/io.h>
#include <wombat/port.h>
#include <event.h>

// local includes
#include "zmqbridgefunctions.h"
#include "io.h"
#include "util.h"

/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

typedef struct zmqIoImpl {
   struct event_base*  mEventBase;
   wthread_t           mDispatchThread;
   uint8_t             mActive;
   uint8_t             mEventsRegistered;
   wsem_t              mResumeDispatching;
} zmqIoImpl;

typedef struct zmqIoEventImpl {
   uint32_t            mDescriptor;
   mamaIoCb            mAction;
   mamaIoType          mIoType;
   mamaIo              mParent;
   void*               mClosure;
   struct event        mEvent;
} zmqIoEventImpl;

/*
 * Global static container to hold instance-wide information not otherwise
 * available in this interface.
 */
static zmqIoImpl       gZmqIoContainer;


/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

void*
zmqBridgeMamaIoImpl_dispatchThread(void* closure);

void
zmqBridgeMamaIoImpl_libeventIoCallback(int fd, short type, void* closure);


/*=========================================================================
  =                   Public implementation functions                     =
  =========================================================================*/

mama_status
zmqBridgeMamaIo_create(ioBridge*   result,
                       void*       nativeQueueHandle,
                       uint32_t    descriptor,
                       mamaIoCb    action,
                       mamaIoType  ioType,
                       mamaIo      parent,
                       void*       closure)
{
   zmqIoEventImpl*    impl    = NULL;
   short               evtType = 0;

   if (NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }

   *result = 0;

   /* Check for supported types so we don't prematurely allocate */
   switch (ioType) {
      case MAMA_IO_READ:
         evtType = EV_READ;
         break;
      case MAMA_IO_WRITE:
         evtType = EV_WRITE;
         break;
      case MAMA_IO_ERROR:
         evtType = EV_READ | EV_WRITE;
         break;
      case MAMA_IO_CONNECT:
      case MAMA_IO_ACCEPT:
      case MAMA_IO_CLOSE:
      case MAMA_IO_EXCEPT:
      default:
         return MAMA_STATUS_UNSUPPORTED_IO_TYPE;
         break;
   }

   impl = (zmqIoEventImpl*) calloc(1, sizeof(zmqIoEventImpl));
   if (NULL == impl) {
      return MAMA_STATUS_NOMEM;
   }

   impl->mDescriptor           = descriptor;
   impl->mAction               = action;
   impl->mIoType               = ioType;
   impl->mParent               = parent;
   impl->mClosure              = closure;

   event_set(&impl->mEvent,
             impl->mDescriptor,
             evtType,
             zmqBridgeMamaIoImpl_libeventIoCallback,
             impl);

   event_add(&impl->mEvent, NULL);

   event_base_set(gZmqIoContainer.mEventBase, &impl->mEvent);

   /* If this is the first event since base was emptied or created */
   if (0 == gZmqIoContainer.mEventsRegistered) {
      wsem_post(&gZmqIoContainer.mResumeDispatching);
   }
   gZmqIoContainer.mEventsRegistered++;

   *result = (ioBridge)impl;

   return MAMA_STATUS_OK;
}

/* Not implemented in the zmq bridge */
mama_status
zmqBridgeMamaIo_destroy(ioBridge io)
{
   zmqIoEventImpl* impl = (zmqIoEventImpl*) io;
   if (NULL == io) {
      return MAMA_STATUS_NULL_ARG;
   }
   event_del(&impl->mEvent);

   free(impl);
   gZmqIoContainer.mEventsRegistered--;

   return MAMA_STATUS_OK;
}

/* Not implemented in the zmq bridge */
mama_status
zmqBridgeMamaIo_getDescriptor(ioBridge    io,
                              uint32_t*   result)
{
   zmqIoEventImpl* impl = (zmqIoEventImpl*) io;
   if (NULL == io || NULL == result) {
      return MAMA_STATUS_NULL_ARG;
   }

   *result = impl->mDescriptor;

   return MAMA_STATUS_OK;
}


/*=========================================================================
  =                  Public implementation prototypes                     =
  =========================================================================*/

mama_status
zmqBridgeMamaIoImpl_start()
{
   int threadResult                        = 0;
   gZmqIoContainer.mEventsRegistered      = 0;
   gZmqIoContainer.mActive                = 1;
   gZmqIoContainer.mEventBase             = event_init();

   wsem_init(&gZmqIoContainer.mResumeDispatching, 0, 0);
   threadResult = wthread_create(&gZmqIoContainer.mDispatchThread,
                                 NULL,
                                 zmqBridgeMamaIoImpl_dispatchThread,
                                 gZmqIoContainer.mEventBase);
   if (0 != threadResult) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "wthread_create returned %d", threadResult);
      return MAMA_STATUS_PLATFORM;
   }
   return MAMA_STATUS_OK;
}

mama_status
zmqBridgeMamaIoImpl_stop()
{
   gZmqIoContainer.mActive = 0;

   /* Alert the semaphore so the dispatch loop can exit */
   wsem_post(&gZmqIoContainer.mResumeDispatching);

   /* Tell the event loop to exit */
   event_base_loopexit(gZmqIoContainer.mEventBase, NULL);

   /* Join with the dispatch thread - it should exit shortly */
   wthread_join(gZmqIoContainer.mDispatchThread, NULL);
   wsem_destroy(&gZmqIoContainer.mResumeDispatching);

   /* Free the main event base */
   event_base_free(gZmqIoContainer.mEventBase);

   return MAMA_STATUS_OK;
}


/*=========================================================================
  =                  Private implementation prototypes                    =
  =========================================================================*/

void*
zmqBridgeMamaIoImpl_dispatchThread(void* closure)
{
   int             dispatchResult = 0;

   /* Wait on the first event to register before starting dispatching */
   wsem_wait(&gZmqIoContainer.mResumeDispatching);

   while (0 != gZmqIoContainer.mActive) {
      dispatchResult = event_base_loop(gZmqIoContainer.mEventBase,
                                       EVLOOP_NONBLOCK | EVLOOP_ONCE);

      /* If no events are currently registered */
      if (1 == dispatchResult) {
         /* Wait until they are */
         gZmqIoContainer.mEventsRegistered = 0;
         wsem_wait(&gZmqIoContainer.mResumeDispatching);
      }
   }
   return NULL;
}

void
zmqBridgeMamaIoImpl_libeventIoCallback(int fd, short type, void* closure)
{
   zmqIoEventImpl* impl = (zmqIoEventImpl*) closure;

   /* Timeout is the only error detectable with libevent */
   if (EV_TIMEOUT == type) {
      /* If this is an error IO type, fire the callback */
      if (impl->mIoType == MAMA_IO_ERROR && NULL != impl->mAction) {
         (impl->mAction)(impl->mParent, impl->mIoType, impl->mClosure);
      }
      /* If this is not an error IO type, do nothing */
      else {
         return;
      }
   }

   /* Call the action callback if defined */
   if (NULL != impl->mAction) {
      (impl->mAction)(impl->mParent, impl->mIoType, impl->mClosure);
   }

   /* Enqueue for the next time */
   event_add(&impl->mEvent, NULL);
}
