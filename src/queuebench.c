#include <zmq.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wombat/port.h>
#include <wombat/queue.h>
#include <wombat/wInterlocked.h>

#define QB_SIZE 10000000
#define QB_MAX_CONS 32

#define QUEUE_WOMBAT 1
#define QUEUE_ZMQ    2

struct qbn {
   char mUri[256];
   wombatQueue mWombatQueue;
   int mNumSubs;
   int mNumPubs;
};

struct qbnEndpoint {
   void* mSocket;
   int mIdx;
   struct qbn* mQbn;
};

void* gCtx = NULL;
int gDebug = 0;
int gQueueChoice = 0;
wInterlockedInt gSentEvents;
wInterlockedInt gReceivedEvents;
wthread_mutex_t gLock; /* for multiple readers */

void wQueueCb(void* data, void* closure)
{
   wInterlocked_increment(&gReceivedEvents);
}

void* dealer(void* closure)
{
   struct qbnEndpoint* ep = (struct qbnEndpoint*) closure;
   struct qbn* newQbn = ep->mQbn;
   int hwm = 0;
   int currentSize = 0;
   int local = 0;

   while ((currentSize = wInterlocked_read(&gSentEvents)) <= QB_SIZE) {
      if (gQueueChoice == QUEUE_ZMQ) {
         char buf[6];
         buf[0] = 'A';
         buf[1] = '\0';

         memcpy(buf + 2, &currentSize, sizeof(int));
         wthread_mutex_lock(&gLock);
         zmq_send(ep->mSocket, buf, 6, 0);
         wthread_mutex_unlock(&gLock);
      }
      if (gQueueChoice == QUEUE_WOMBAT) {
         wombatQueue_enqueue(newQbn->mWombatQueue, wQueueCb, NULL, NULL);
      }
      local++;
      if (currentSize % 1000000 == 0) {
         printf("So far spammed %ld messages onto queue %d\n", currentSize, ep->mIdx);
      }
      wInterlocked_increment(&gSentEvents);
   }
   printf("Done spamming from producer %d (%d messages from here)\n", ep->mIdx, local);
}

void* worker(void* closure)
{
   struct qbnEndpoint* ep = (struct qbnEndpoint*) closure;
   struct qbn* newQbn = ep->mQbn;
   wombatQueueStatus status;
   int hwm = 0;
   int rcvto = 10;

   zmq_msg_t zmsg;
   zmq_msg_init(&zmsg);

   int lastSeqNo = 0;
   int recvSeqNo = 0;
   int recEv = 0;
   int local = 0;
   while ((recEv = wInterlocked_read(&gReceivedEvents)) < QB_SIZE) {
      if (gQueueChoice == QUEUE_ZMQ) {
         wthread_mutex_lock(&gLock);
         int size = zmq_msg_recv(&zmsg, ep->mSocket, ZMQ_DONTWAIT);
         wthread_mutex_unlock(&gLock);
         if (size == -1) {
            continue;
         }

         memcpy(&recvSeqNo, ((char*)zmq_msg_data(&zmsg)) + 2, sizeof(int));
         //            if (ep->mQbn->mNumSubs == 1 && recvSeqNo > 0 && recvSeqNo != lastSeqNo + 1)
         //                printf ("Gap detected - %d != %d\n", recvSeqNo, lastSeqNo + 1);

         wInterlocked_increment(&gReceivedEvents);
         lastSeqNo = recvSeqNo;
         local++;
      }
      if (gQueueChoice == QUEUE_WOMBAT) {
         status = wombatQueue_timedDispatch(newQbn->mWombatQueue, NULL, NULL, 1000);
      }
      if (recEv > 0 && recEv % 1000000 == 0) {
         printf("So far chewed %d messages from queue %d\n", recEv, ep->mIdx);
      }
   }
}

int main(int argc, char* argv[])
{
   int i = 0;
   int hwm = 0;
   int rcvto = 100;
   int pubs = 1;
   int subs = 1;

   /* Create the counter lock. */
   wInterlocked_initialize(&gSentEvents);
   wInterlocked_set(0, &gSentEvents);

   wInterlocked_initialize(&gReceivedEvents);
   wInterlocked_set(0, &gReceivedEvents);

   wthread_mutex_init(&gLock, NULL);

   if (argc == 1 || argc > 4) {
      printf("Usage: queuebench [wombat|zmq] [producers] [consumers]");
      exit(1);
   }

   if (0 == strcmp(argv[1], "wombat")) {
      gQueueChoice = QUEUE_WOMBAT;
   }
   else if (0 == strcmp(argv[1], "zmq")) {
      gQueueChoice = QUEUE_ZMQ;
   }
   else {
      printf("First arg must be wombat or zmq\n");
      exit(2);
   }

   pubs = atoi(argv[2]);
   subs = atoi(argv[3]);

   struct qbn* newQbn = calloc(1, sizeof(struct qbn));
   newQbn->mNumSubs = subs;
   newQbn->mNumPubs = pubs;
   wthread_t* subThreads = calloc(subs, sizeof(wthread_t));
   wthread_t* pubThreads = calloc(pubs, sizeof(wthread_t));
   struct qbnEndpoint* subEps = calloc(subs, sizeof(struct qbnEndpoint));
   struct qbnEndpoint* pubEps = calloc(pubs, sizeof(struct qbnEndpoint));

   if (gQueueChoice == QUEUE_ZMQ) {
      gCtx = zmq_ctx_new();
      snprintf(newQbn->mUri, sizeof(newQbn->mUri), "inproc://worker");
   }

   if (gQueueChoice == QUEUE_WOMBAT) {
      wombatQueue_allocate(&newQbn->mWombatQueue);
      wombatQueue_create(newQbn->mWombatQueue,
                         WOMBAT_QUEUE_MAX_SIZE,
                         WOMBAT_QUEUE_CHUNK_SIZE,
                         WOMBAT_QUEUE_CHUNK_SIZE);
   }

   void* dealerSocket = NULL;
   for (i = 0; i < pubs; i++) {
      struct qbnEndpoint* ep = &pubEps[i];
      ep->mIdx = i;
      ep->mQbn = newQbn;
      if (0 == i) {
         dealerSocket = zmq_socket(gCtx, ZMQ_PUB);
         zmq_setsockopt(dealerSocket, ZMQ_SNDHWM, &hwm, sizeof(int));
         zmq_bind(dealerSocket, newQbn->mUri);
      } // Else reuse the last one

      ep->mSocket = dealerSocket;
   }

   for (i = 0; i < subs; i++) {
      struct qbnEndpoint* ep = &subEps[i];
      ep->mIdx = i;
      ep->mQbn = newQbn;
      void* workerSocket = zmq_socket(gCtx, ZMQ_SUB);
      zmq_setsockopt(workerSocket, ZMQ_RCVHWM, &hwm, sizeof(int));
      zmq_setsockopt(workerSocket, ZMQ_RCVTIMEO, &rcvto, sizeof(int));
      zmq_setsockopt(workerSocket, ZMQ_SUBSCRIBE, "A", 1);
      zmq_connect(workerSocket, newQbn->mUri);
      ep->mSocket = workerSocket;

   }

   for (i = 0; i < subs; i++) {
      wthread_create(&subThreads[i], NULL, worker, &subEps[i]);
   }

   for (i = 0; i < pubs; i++) {
      wthread_create(&pubThreads[i], NULL, dealer, &pubEps[i]);
   }

   for (i = 0; i < subs; i++) {
      wthread_join(subThreads[i], NULL);
   }

   for (i = 0; i < pubs; i++) {
      wthread_join(pubThreads[i], NULL);
   }

   printf("Finished chewing through %ld messages across %d producers and %d consumers\n", QB_SIZE, pubs, subs);
}
