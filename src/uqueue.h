#ifndef MAMA_BRIDGE_ZMQ_UQUEUE_H__
#define MAMA_BRIDGE_ZMQ_UQUEUE_H__

typedef void* uQueue;

wombatQueueStatus uQueue_allocate (uQueue *result);
wombatQueueStatus uQueue_create (uQueue queue, uint32_t maxSize, uint32_t initialSize, uint32_t growBySize);
wombatQueueStatus uQueue_destroy (uQueue queue);
wombatQueueStatus uQueue_deallocate(uQueue queue);
wombatQueueStatus uQueue_getSize (uQueue queue, int* size);
wombatQueueStatus uQueue_enqueue (uQueue queue, wombatQueueCb cb, void* data, void* closure, uint8_t isMsg);
wombatQueueStatus uQueue_dispatch (uQueue queue);
wombatQueueStatus uQueue_timedDispatch (uQueue queue, uint64_t timeout);


#endif /* MAMA_BRIDGE_ZMQ_UQUEUE_H__ */