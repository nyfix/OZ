#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mama/mama.h>
#include <wombat/queue.h>
#include <wombat/wSemaphore.h>
#include <wombat/wInterlocked.h>
#include "uqueue.h"
#include "zmqdefs.h"

#define UQ_REMOVE(impl, ele)                  \
    (ele)->mPrev->mNext = (ele)->mNext;       \
    (ele)->mNext->mPrev = (ele)->mPrev;       \
    (ele)->mNext = impl->mFirstFree.mNext;    \
    (impl)->mFirstFree.mNext = (ele);         \
    --(impl)->mCurrSize;

/*
 * Items that get queued
 */
typedef struct uQueueItem_
{
    wombatQueueCb         mCb;
    void*                 mData;
    uint8_t               mIsMsg;
    union {
        void              *mClosure;
        zmqTransportMsg   mMsg;
    };
    struct uQueueItem_*   mNext;
    struct uQueueItem_*   mPrev;
    struct uQueueItem_*   mChunkNext;
} uQueueItem;


typedef struct
{
    wsem_t               mSem;
    wthread_mutex_t      mLock; /* for multiple readers */

    uint32_t             mMaxSize;
    uint32_t             mChunkSize;
    int32_t              mCurrSize;

    /* Dummy nodes for free, head and tail */
    uQueueItem   mHead;
    uQueueItem   mTail;
    uQueueItem   mFirstFree;
    uQueueItem*  mChunks;
} uQueueImpl;

static void
uQueueImpl_allocChunk ( uQueueImpl* impl, unsigned int items);

wombatQueueStatus
uQueue_allocate (uQueue *result)
{
   uQueueImpl *impl = NULL;
   *result = NULL;

   impl = (uQueueImpl*)calloc (1, sizeof(uQueueImpl));
   if (impl == NULL)
   {
      return WOMBAT_QUEUE_NOMEM;
   }
   *result = impl;

   /* Set defaults */
   impl->mMaxSize      = WOMBAT_QUEUE_MAX_SIZE;
   impl->mChunkSize    = WOMBAT_QUEUE_CHUNK_SIZE;

   memset (&impl->mHead, 0, sizeof (impl->mHead));
   memset (&impl->mTail, 0, sizeof (impl->mTail));

   impl->mHead.mNext = &impl->mTail;
   impl->mHead.mPrev = &impl->mHead; /* for iteration */
   impl->mTail.mPrev = &impl->mHead;
   impl->mTail.mNext = &impl->mTail; /* for iteration */

   return WOMBAT_QUEUE_OK;
}

wombatQueueStatus
uQueue_create (uQueue queue, uint32_t maxSize, uint32_t initialSize,
                    uint32_t growBySize)
{
   uQueueImpl *impl = (uQueueImpl*)queue;

   if (maxSize)
      impl->mMaxSize      = maxSize;

   if (growBySize)
      impl->mChunkSize    = growBySize;

   if (wsem_init (&impl->mSem, 0, 0) != 0)
   {
      return WOMBAT_QUEUE_SEM_ERR;
   }

   wthread_mutex_init( &impl->mLock, NULL);

   initialSize = initialSize == 0 ? WOMBAT_QUEUE_CHUNK_SIZE : initialSize;
   uQueueImpl_allocChunk (impl, initialSize);
   if (impl->mFirstFree.mNext == NULL) /* alloc failed */
   {
      return WOMBAT_QUEUE_NOMEM;
   }

   impl->mCurrSize = 0;

   return WOMBAT_QUEUE_OK;
}

wombatQueueStatus
uQueue_destroy (uQueue queue)
{
   uQueueImpl *impl      = (uQueueImpl*)queue;
   wombatQueueStatus result   = WOMBAT_QUEUE_OK;
   uQueueItem *curItem;

   wthread_mutex_lock (&impl->mLock);
   /* Free the datas */
   curItem = impl->mChunks;
   while (curItem)
   {
      uQueueItem* tmp = curItem->mChunkNext;
      free (curItem);
      curItem = tmp;
   }

   wthread_mutex_unlock (&impl->mLock);

   /* Thee wsem_destroy and wthread_mutex_destroy methods simply makes
    * sure that no threads are waiting since the synchronization objects can
    * not be destroyed if threads are waiting. We could devise a mechanism
    * to ensure that dispatchers are not waiting or dispatching. We can
    * certainly stop enqueueing events. Dequing we can only control if we
    * create our own dispatchers.
    *
    * Clients should make sure that all dispatchers are stopped before the
    * destroying a queue.
    *
    */
   if (wsem_destroy (&impl->mSem) != 0)
   {
      result = WOMBAT_QUEUE_SEM_ERR;
   }

   wthread_mutex_destroy( &impl->mLock);

   if (WOMBAT_QUEUE_OK == result)
   {
      return uQueue_deallocate(queue);
   }
   else
   {
      return result;
   }
}

wombatQueueStatus
uQueue_deallocate (uQueue queue)
{
   uQueueImpl *impl      = (uQueueImpl*)queue;
   free (impl);
   return WOMBAT_QUEUE_OK;
}

wombatQueueStatus
uQueue_enqueue (uQueue queue,
                     wombatQueueCb cb,
                     void* data,
                     void* closure,
                     uint8_t isMsg)
{
   uQueueImpl* impl = (uQueueImpl*)queue;
   uQueueItem* item = NULL;

   wthread_mutex_lock (&impl->mLock);

   /* If there are no items in the free list, allocate some. It will set the
    * next free node to NULL if the queue is too big or there is no memory.
    */
   if (impl->mFirstFree.mNext == NULL)
      uQueueImpl_allocChunk (impl, impl->mChunkSize);
   item = impl->mFirstFree.mNext;

   if (item == NULL)
   {
      wthread_mutex_unlock(&impl->mLock);
      return WOMBAT_QUEUE_FULL;
   }

   impl->mFirstFree.mNext = item->mNext;
   /* Initialize the item. */
   item->mCb      = cb;
   item->mData    = data;
   item->mIsMsg   = isMsg;

   if (isMsg)
   {
      zmqTransportMsg *msg = (zmqTransportMsg*) closure;
      item->mMsg = *msg;
   }
   else
   {
      item->mClosure = closure;
   }

   /* Put on queue (insert before dummy tail node */
   item->mNext              = &impl->mTail;
   item->mPrev              = impl->mTail.mPrev;
   item->mPrev->mNext       = item;
   impl->mTail.mPrev        = item;
   ++impl->mCurrSize;

   /* Notify next available thread that an item is ready */
   wsem_post (&impl->mSem);
   wthread_mutex_unlock (&impl->mLock);

   return WOMBAT_QUEUE_OK;
}

wombatQueueStatus
uQueue_getSize (uQueue queue, int* size)
{
   uQueueImpl* impl    = (uQueueImpl*)queue;
   wsem_getvalue (&impl->mSem, size);

   return WOMBAT_QUEUE_OK;

}

static wombatQueueStatus
uQueue_dispatchInt (uQueue queue, uint8_t isTimed, uint64_t timout)
{
   uQueueImpl* impl     = (uQueueImpl*)queue;
   uQueueItem* head     = NULL;
   wombatQueueCb    cb       = NULL;
   uint8_t          isMsg    = 0;
   zmqTransportMsg  msg;
   void*            closure = NULL;
   void*            data    = NULL;

   if (isTimed)
   {
      if (wsem_timedwait (&impl->mSem, (unsigned int)timout) !=0)
         return WOMBAT_QUEUE_TIMEOUT;
   }
   else
   {
      while (-1 == wsem_wait (&impl->mSem))
      {
         if (errno != EINTR)
            return WOMBAT_QUEUE_SEM_ERR;
      }
   }

   wthread_mutex_lock (&impl->mLock); /* May be multiple readers */

   /* remove the item */
   head = impl->mHead.mNext;
   if (head == &impl->mTail)
   {
      wthread_mutex_unlock (&impl->mLock);
      return WOMBAT_QUEUE_OK;
   }

   UQ_REMOVE (impl, head);

   /* so we can unlock (allows cb to dequeue) */
   cb = head->mCb;
   data = head->mData;
   isMsg   = head->mIsMsg;
   if (isMsg)
   {
      msg = head->mMsg;
   }
   else
   {
      closure = head->mClosure;
   }

   wthread_mutex_unlock (&impl->mLock);

   if (cb)
   {
      cb (data, isMsg == 1 ? &msg : closure);
   }

   return WOMBAT_QUEUE_OK;
}

wombatQueueStatus
uQueue_dispatch (uQueue queue)
{
   return uQueue_dispatchInt (queue, 0, 0);
}

wombatQueueStatus
uQueue_timedDispatch (uQueue queue, uint64_t timeout)
{
   return uQueue_dispatchInt (queue, 1, timeout);
}
/* Static/Private functions */
static void
uQueueImpl_allocChunk ( uQueueImpl* impl, unsigned int items)
{
   size_t sizeToAlloc =  items * sizeof(uQueueItem);
   uQueueItem* result;
   int size;

   wsem_getvalue (&impl->mSem, &size);
   if (size + items > impl->mMaxSize)
   {
      /* impl->mFirstFree.mNext is already NULL */
      return;
   }

   result = (uQueueItem*)calloc( 1, sizeToAlloc);
   result->mChunkNext = impl->mChunks;
   impl->mChunks   = result;
   impl->mFirstFree.mNext = result;

   /* Create the links */
   {
      unsigned int i;
      for (i = 0; i < items - 1; i++) /* -1 so last is NULL */
      {
         result[i].mNext   = &result[i+1];
         result[i+1].mPrev = &result[i];
      }
   }
}