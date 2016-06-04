#include <zmq.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wombat/port.h>
#include <wombat/queue.h>

#define QB_SIZE 10000000

#define QUEUE_WOMBAT 1
#define QUEUE_ZMQ    2

struct qbn {
    char mUri[256];
    wombatQueue mWombatQueue;
    int mReceived;
};

struct qbnEndpoint {
    void* mSocket;
    int mIdx;
    struct qbn* mQbn;
};

void* gCtx = NULL;
int gDebug = 0;
int gQueueChoice = 0;

void wQueueCb (void* data, void* closure)
{
    struct qbn* newQbn = (struct qbn*) data;
    newQbn->mReceived++;
}

void* dealer (void* closure)
{
    struct qbnEndpoint* ep = (struct qbnEndpoint*) closure;
    struct qbn* newQbn = ep->mQbn;
    long int i = 0;
    int hwm = 0;

    for (i = 0; i < QB_SIZE*10; i++)
    {
        if (gQueueChoice == QUEUE_ZMQ) {
            char buf[2] = {'A', '\0'};
            int rc = zmq_send (ep->mSocket, strdup(buf), sizeof(buf), 0);
            //printf ("SENT %d [%d]\n", rc, errno);
        }
        if (gQueueChoice == QUEUE_WOMBAT) {
            wombatQueue_enqueue (newQbn->mWombatQueue, wQueueCb, newQbn, NULL);
        }
        //if (i % 100000 == 0)
        //    printf ("So far spammed %ld messages onto queue %d\n", i, ep->mIdx);
    }
    printf ("Finished spamming %ld messages onto queue\n");
}

void* worker (void* closure)
{
    struct qbnEndpoint* ep = (struct qbnEndpoint*) closure;
    struct qbn* newQbn = ep->mQbn;
    wombatQueueStatus status;
    zmq_msg_t zmsg;
    zmq_msg_init (&zmsg);

    while (newQbn->mReceived < QB_SIZE)
    {
        if (gQueueChoice == QUEUE_ZMQ) {
            int size = zmq_msg_recv (&zmsg, ep->mSocket, ZMQ_DONTWAIT);
            if (size == -1)
            {
                continue;
            }
            newQbn->mReceived++;
        }
        if (gQueueChoice == QUEUE_WOMBAT) {
            status = wombatQueue_timedDispatch (newQbn->mWombatQueue, NULL, NULL, 1);
            if (WOMBAT_QUEUE_TIMEOUT == status)
                continue;
        }
        //if (newQbn->mReceived % 100000 == 0)
        //    printf ("So far chewed %ld messages from queue %d\n", newQbn->mReceived, ep->mIdx);

    }

}

int main (int argc, char *argv[])
{
    int i = 0;
    int hwm = 0;
    int rcvto = 10;
    int pubs = 1;
    int subs = 1;

    if (argc == 1 || argc > 4) {
        printf ("Usage: queuebench [wombat|zmq] [producers] [consumers]");
        exit(1);
    }

    if (0 == strcmp(argv[1], "wombat")) {
        gQueueChoice = QUEUE_WOMBAT;
    } else if (0 == strcmp(argv[1], "zmq")) {
        gQueueChoice = QUEUE_ZMQ;
    } else {
        printf ("First arg must be wombat or zmq\n");
        exit(2);
    }

    pubs = atoi(argv[2]);
    subs = atoi(argv[3]);

    struct qbn* newQbn = calloc(1, sizeof(struct qbn));
    wthread_t* subThreads = calloc (subs, sizeof(wthread_t));
    wthread_t* pubThreads = calloc (pubs, sizeof(wthread_t));

    if (gQueueChoice == QUEUE_ZMQ) {
        gCtx = zmq_ctx_new ();
        snprintf (newQbn->mUri, sizeof(newQbn->mUri), "inproc://worker");
    }

    if (gQueueChoice == QUEUE_WOMBAT) {
        wombatQueue_allocate (&newQbn->mWombatQueue);
        wombatQueue_create (newQbn->mWombatQueue,
                            WOMBAT_QUEUE_MAX_SIZE,
                            WOMBAT_QUEUE_CHUNK_SIZE,
                            WOMBAT_QUEUE_CHUNK_SIZE);
    }

    for (i = 0; i < pubs; i++)
    {
        struct qbnEndpoint* ep = calloc(1, sizeof(struct qbnEndpoint));
        void* dealerSocket = zmq_socket (gCtx, ZMQ_PUB);
        zmq_setsockopt (dealerSocket, ZMQ_SNDHWM, &hwm, sizeof(int));
        zmq_bind (dealerSocket, newQbn->mUri);
        ep->mIdx = i;
        ep->mSocket = dealerSocket;
        ep->mQbn = newQbn;
        wthread_create (&pubThreads[i], NULL, dealer, ep);
    }

    for (i = 0; i < subs; i++)
    {
        struct qbnEndpoint* ep = calloc(1, sizeof(struct qbnEndpoint));
        void* workerSocket = zmq_socket (gCtx, ZMQ_SUB);
        zmq_setsockopt (workerSocket, ZMQ_RCVHWM, &hwm, sizeof(int));
        zmq_setsockopt (workerSocket, ZMQ_RCVTIMEO, &rcvto, sizeof(int));
        zmq_setsockopt (workerSocket, ZMQ_SUBSCRIBE, "A", 1);
        zmq_connect (workerSocket, newQbn->mUri);
        ep->mIdx = i;
        ep->mSocket = workerSocket;
        ep->mQbn = newQbn;
        wthread_create (&subThreads[i], NULL, worker, ep);
    }

    for (i = 0; i < subs; i++)
        wthread_join (subThreads[i], NULL);

    printf ("Finished chewing through %ld messages across %d producers and %d consumers\n", QB_SIZE, pubs, subs);
}
