#include <zmq.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wombat/port.h>

#define QB_SIZE 10000000

void* gCtx = NULL;

struct qbn {
    char mUri[256];
    void* mDealer;
    void* mWorker;
    int mIdx;
};

void* dealer (void* closure)
{
    struct qbn* newQbn = (struct qbn*) closure;
    long int i = QB_SIZE;

    for (i = 0; i < QB_SIZE; i++)
    {
        char buf[2] = {'A', '\0'};
        zmq_send (newQbn->mDealer, buf, sizeof(buf), 0);
    }
    printf ("Finished spamming %ld messages onto %s\n", i, newQbn->mUri);
}

void* worker (void* closure)
{
    struct qbn* newQbn = (struct qbn*) closure;
    long int i = 0;
    zmq_msg_t zmsg;
    zmq_msg_init (&zmsg);

    while (i < QB_SIZE)
    {
        int size = zmq_msg_recv (&zmsg, newQbn->mWorker, ZMQ_DONTWAIT);
        if (size == -1)
        {
            continue;
        }
        i++;
    }
    printf ("Finished chewing through %ld messages on %s\n", i, newQbn->mUri);

}

int main()
{
    int i = 0;
    int threads = 8;
    int hwm = 0;
    int rcvto = 10;
    wthread_t* subThreads = calloc (threads, sizeof(wthread_t));
    wthread_t* pubThreads = calloc (threads, sizeof(wthread_t));

    gCtx = zmq_ctx_new ();

    /* create subscribing threads */
    for (i = 0; i < threads; i++)
    {
        struct qbn* newQbn = calloc(1, sizeof(struct qbn));

        newQbn->mIdx = i;

        /* Create the dealer (note pub sub rather than rep as there will be no
         * replies necessary in a unidirectional queue such as this. */
        snprintf (newQbn->mUri, sizeof(newQbn->mUri), "inproc://worker%d", i);

        newQbn->mWorker = zmq_socket (gCtx, ZMQ_SUB);
        newQbn->mDealer = zmq_socket (gCtx, ZMQ_PUB);

        zmq_setsockopt (newQbn->mDealer, ZMQ_SNDHWM, &hwm, sizeof(int));
        zmq_setsockopt (newQbn->mWorker, ZMQ_RCVHWM, &hwm, sizeof(int));
        zmq_setsockopt (newQbn->mWorker, ZMQ_RCVTIMEO, &rcvto, sizeof(int));
        zmq_setsockopt (newQbn->mWorker, ZMQ_SUBSCRIBE, "A", 1);

        /* Create the dealer */
        zmq_bind (newQbn->mDealer, newQbn->mUri);

        /* Create the worker */
        zmq_connect (newQbn->mWorker, newQbn->mUri);

        /* Initialize dispatch thread */
        wthread_create (&pubThreads[i], NULL, dealer, newQbn);
        wthread_create (&subThreads[i], NULL, worker, newQbn);
    }

    /* create subscribing threads */
    for (i = 0; i < threads; i++)
    {
        wthread_join (subThreads[i], NULL);
        wthread_join (pubThreads[i], NULL);
    }
}
