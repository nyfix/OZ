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
#include <timers.h>

// local includes
#include "zmqbridgefunctions.h"
#include <mama/integration/mama.h>
#include "util.h"

#include <zmq.h>


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

/* Global timer heap */
timerHeap           gOmzmqTimerHeap;

/* Default payload names and IDs to be loaded when this bridge is loaded */
static char*        PAYLOAD_NAMES[]         =   { "omnmmsg", NULL };
static char         PAYLOAD_IDS[]           =   { 'O', '\0' };

/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Version identifiers */
#define             ZMQ_BRIDGE_NAME            "zmq"
#define             ZMQ_BRIDGE_VERSION         "2.0"

/* Name to be given to the default queue. Should be bridge-specific. */
#define             ZMQ_DEFAULT_QUEUE_NAME     "ZMQ_DEFAULT_MAMA_QUEUE"

/* Timeout for dispatching queues on shutdown in milliseconds */
#define             ZMQ_SHUTDOWN_TIMEOUT       5000


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

mama_status zmqBridge_init(mamaBridge bridgeImpl)
{
   MAMA_SET_BRIDGE_COMPILE_TIME_VERSION(ZMQ_BRIDGE_NAME);

   mamaBridgeImpl_setReadOnlyProperty (bridgeImpl, MAMA_PROP_EXTENDS_BASE_BRIDGE, "true");

   return MAMA_STATUS_OK;
}

mama_status
zmqBridge_open(mamaBridge bridgeImpl)
{
   mama_status status = MAMA_STATUS_OK;
   mamaQueue defaultEventQueue = NULL;

   wsocketstartup();

   if (NULL == bridgeImpl) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Create the default event queue */
   status = mamaQueue_create(&defaultEventQueue, bridgeImpl);
   if (MAMA_STATUS_OK != status) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to create zmq queue (%s).", mamaStatus_stringForStatus(status));
      return status;
   }

   mamaImpl_setDefaultEventQueue(bridgeImpl, defaultEventQueue);

   /* Set the queue name (used to identify this queue in MAMA stats) */
   mamaQueue_setQueueName(defaultEventQueue, ZMQ_DEFAULT_QUEUE_NAME);

   /* Create the timer heap */
   if (0 != createTimerHeap(&gOmzmqTimerHeap)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to initialize timers.");
      return MAMA_STATUS_PLATFORM;
   }

   /* Start the dispatch timer heap which will create a new thread */
   if (0 != startDispatchTimerHeap(gOmzmqTimerHeap)) {
      MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to start timer thread.");
      return MAMA_STATUS_PLATFORM;
   }

   return MAMA_STATUS_OK;
}

mama_status
zmqBridge_close(mamaBridge bridgeImpl)
{
   mama_status      status      = MAMA_STATUS_OK;
   mamaQueue defaultEventQueue = NULL;

   if (NULL ==  bridgeImpl) {
      return MAMA_STATUS_NULL_ARG;
   }

   /* Remove the timer heap */
   if (NULL != gOmzmqTimerHeap) {
      /* The timer heap allows us to access it's thread ID for joining */
      wthread_t timerThread = timerHeapGetTid(gOmzmqTimerHeap);
      if (0 != destroyHeap(gOmzmqTimerHeap)) {
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Failed to destroy zmq timer heap.");
         status = MAMA_STATUS_PLATFORM;
      }
      /* The timer thread expects us to be responsible for terminating it */
      wthread_join(timerThread, NULL);
   }
   gOmzmqTimerHeap = NULL;

   /* Destroy once queue has been emptied */
   mama_getDefaultEventQueue(bridgeImpl, &defaultEventQueue);
   mamaQueue_destroyTimedWait(defaultEventQueue, ZMQ_SHUTDOWN_TIMEOUT);

   return status;
}

mama_status
zmqBridge_start(mamaQueue defaultEventQueue)
{
   if (NULL == defaultEventQueue) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "defaultEventQueue is NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   /* Start the default event queue */
   return mamaQueue_dispatch(defaultEventQueue);;
}

mama_status
zmqBridge_stop(mamaQueue defaultEventQueue)
{
   if (NULL == defaultEventQueue) {
      MAMA_LOG(MAMA_LOG_LEVEL_FINER, "defaultEventQueue is NULL");
      return MAMA_STATUS_NULL_ARG;
   }

   return mamaQueue_stopDispatch(defaultEventQueue);;
}

const char*
zmqBridge_getVersion(void)
{
   static char versionStr[1024];

   int major, minor, patch;
   zmq_version(&major, &minor, &patch);

   sprintf(versionStr, "%s version %s using libzmq version %d.%d.%d", zmqBridge_getName(), ZMQ_BRIDGE_VERSION, major, minor, patch);

   return versionStr;
}

const char*
zmqBridge_getName(void)
{
   return ZMQ_BRIDGE_NAME;
}

mama_status
zmqBridge_getDefaultPayloadId(char** *name, char** id)
{
   if (NULL == name || NULL == id) {
      return MAMA_STATUS_NULL_ARG;
   }
   /*
    * Populate name with the value of all supported payload names, the first
    * being the default
    */
   *name   = PAYLOAD_NAMES;

   /*
    * Populate id with the char keys for all supported payload names, the first
    * being the default
    */
   *id     = PAYLOAD_IDS;

   return MAMA_STATUS_OK;
}
