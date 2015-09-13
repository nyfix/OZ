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
#include <timers.h>
#include "io.h"
#include "zmqbridgefunctions.h"


/*=========================================================================
  =                Typedefs, structs, enums and globals                   =
  =========================================================================*/

/* Global timer heap */
timerHeap           gOmzmqTimerHeap;

//#define USE_OMNMSG 1

/* Default payload names and IDs to be loaded when this bridge is loaded */
#ifdef USE_OMNMSG
static char*        PAYLOAD_NAMES[]         =   { "omnmmsg", NULL };
static char         PAYLOAD_IDS[]           =   { MAMA_PAYLOAD_OMNM, '\0' };
#endif
#ifndef USE_OMNMSG
static char*        PAYLOAD_NAMES[]         =   { "qpidmsg", NULL };
static char         PAYLOAD_IDS[]           =   { MAMA_PAYLOAD_QPID, '\0' };
#endif

/*=========================================================================
  =                              Macros                                   =
  =========================================================================*/

/* Version identifiers */
#define             ZMQ_BRIDGE_NAME            "zmq"
#define             ZMQ_BRIDGE_VERSION         "0.1"

/* Name to be given to the default queue. Should be bridge-specific. */
#define             ZMQ_DEFAULT_QUEUE_NAME     "ZMQ_DEFAULT_MAMA_QUEUE"

/* Timeout for dispatching queues on shutdown in milliseconds */
#define             ZMQ_SHUTDOWN_TIMEOUT       5000


/*=========================================================================
  =               Public interface implementation functions               =
  =========================================================================*/

void zmqBridge_createImpl (mamaBridge* result)
{
    mamaBridgeImpl* bridge = NULL;

    if (NULL == result)
    {
        return;
    }

    /* Create the wrapping MAMA bridge */
    bridge = (mamaBridgeImpl*) calloc (1, sizeof (mamaBridgeImpl));
    if (NULL == bridge)
    {
        mama_log (MAMA_LOG_LEVEL_SEVERE, "zmqBridge_createImpl(): "
                  "Could not allocate memory for MAMA bridge implementation.");
        *result = NULL;
        return;
    }

    /* Populate the bridge impl structure with the function pointers */
    INITIALIZE_BRIDGE (bridge, zmq);

    /* Return the newly created bridge */
    *result = (mamaBridge) bridge;

    mamaBridgeImpl_setReadOnlyProperty (
            (mamaBridge) bridge,
            "mama.zmq.entitlements.deferred", "false");
}

mama_status
zmqBridge_open (mamaBridge bridgeImpl)
{
    mama_status         status  = MAMA_STATUS_OK;
    mamaBridgeImpl*     bridge  = (mamaBridgeImpl*) bridgeImpl;

    wsocketstartup();

    if (NULL == bridgeImpl)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    /* Create the default event queue */
    status = mamaQueue_create (&bridge->mDefaultEventQueue, bridgeImpl);
    if (MAMA_STATUS_OK != status)
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridge_open(): Failed to create zmq queue (%s).",
                  mamaStatus_stringForStatus (status));
        return status;
    }

    /* Set the queue name (used to identify this queue in MAMA stats) */
    mamaQueue_setQueueName (bridge->mDefaultEventQueue,
                            ZMQ_DEFAULT_QUEUE_NAME);

    /* Create the timer heap */
    if (0 != createTimerHeap (&gOmzmqTimerHeap))
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridge_open(): Failed to initialize timers.");
        return MAMA_STATUS_PLATFORM;
    }

    /* Start the dispatch timer heap which will create a new thread */
    if (0 != startDispatchTimerHeap (gOmzmqTimerHeap))
    {
        mama_log (MAMA_LOG_LEVEL_ERROR,
                  "zmqBridge_open(): Failed to start timer thread.");
        return MAMA_STATUS_PLATFORM;
    }

    /* Start the io thread */
    zmqBridgeMamaIoImpl_start ();

    return MAMA_STATUS_OK;
}

mama_status
zmqBridge_close (mamaBridge bridgeImpl)
{
    mama_status      status      = MAMA_STATUS_OK;
    mamaBridgeImpl*  bridge      = (mamaBridgeImpl*) bridgeImpl;
    wthread_t        timerThread;

    if (NULL ==  bridgeImpl)
    {
        return MAMA_STATUS_NULL_ARG;
    }

    /* Remove the timer heap */
    if (NULL != gOmzmqTimerHeap)
    {
        /* The timer heap allows us to access it's thread ID for joining */
        timerThread = timerHeapGetTid (gOmzmqTimerHeap);
        if (0 != destroyHeap (gOmzmqTimerHeap))
        {
            mama_log (MAMA_LOG_LEVEL_ERROR,
                      "zmqBridge_close(): Failed to destroy zmq timer heap.");
            status = MAMA_STATUS_PLATFORM;
        }
        /* The timer thread expects us to be responsible for terminating it */
        wthread_join    (timerThread, NULL);
    }
    gOmzmqTimerHeap = NULL;

    /* Destroy once queue has been emptied */
    mamaQueue_destroyTimedWait (bridge->mDefaultEventQueue,
                                ZMQ_SHUTDOWN_TIMEOUT);

    /* Stop and destroy the io thread */
    zmqBridgeMamaIoImpl_stop ();

    /* Wait for zmqBridge_start to finish before destroying implementation */
    if (NULL != bridgeImpl)
    {
        free (bridgeImpl);
    }

    return status;
}

mama_status
zmqBridge_start (mamaQueue defaultEventQueue)
{
    if (NULL == defaultEventQueue)
    {
      mama_log (MAMA_LOG_LEVEL_FINER,
                "zmqBridge_start(): defaultEventQueue is NULL");
      return MAMA_STATUS_NULL_ARG;
    }

    /* Start the default event queue */
    return mamaQueue_dispatch (defaultEventQueue);;
}

mama_status
zmqBridge_stop (mamaQueue defaultEventQueue)
{
    if (NULL == defaultEventQueue)
    {
      mama_log (MAMA_LOG_LEVEL_FINER,
                "zmqBridge_start(): defaultEventQueue is NULL");
      return MAMA_STATUS_NULL_ARG;
    }

    return mamaQueue_stopDispatch (defaultEventQueue);;
}

const char*
zmqBridge_getVersion (void)
{
    return ZMQ_BRIDGE_VERSION;
}

const char*
zmqBridge_getName (void)
{
    return ZMQ_BRIDGE_NAME;
}

mama_status
zmqBridge_getDefaultPayloadId (char ***name, char **id)
{
    if (NULL == name || NULL == id)
    {
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
