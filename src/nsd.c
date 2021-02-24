//
// This code was cribbed from "The ZeroMQ Guide - for C Developers" -- Example 2.7 Weather Update proxy
//

// required for definition of progname/program_invocation_short_name,
// which is used for naming messages
#if defined __APPLE__
#include <stdlib.h>
#else
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <wombat/strutils.h>

#include <zmq.h>
#include "zmqdefs.h"

void sighandler(int unused)
{
}

int main (int argc, char** argv)
{
   // setup logging
   int logLevel = MAMA_LOG_LEVEL_NORMAL;
   char* temp = getenv("MAMA_STDERR_LOGGING");
   if (temp) {
      logLevel = atoi(temp);
   }
   mama_enableLogging(stderr, (MamaLogLevel) logLevel);

   // get/check params
   const char* interface = NULL;
   int port = 0;
   for (int i = 1; i < argc; i++) {
      if (strcasecmp("-i", argv[i]) == 0) {
         interface = argv[++i];
      }
      if (strcasecmp("-p", argv[i]) == 0) {
         port = atoi(argv[++i]);
      }
   }
   if ((interface == NULL) || (port == 0)) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Must specify both -i and -p");
      exit(1);
   }

   void *context = zmq_ctx_new ();
   if (context == NULL) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to create context: %d(%s)", errno, zmq_strerror(errno));
      exit(2);
   }

   // public endpoint for publishers to connect to
   char subEndpoint[ZMQ_MAX_ENDPOINT_LENGTH];
   sprintf(subEndpoint, "tcp://%s:*", interface);
   void *frontend = zmq_socket (context, ZMQ_XSUB);
   if (frontend == NULL) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to create frontend: %d(%s)", errno, zmq_strerror(errno));
      exit(3);
   }
   int rc = zmq_bind (frontend, subEndpoint);
   if (rc != 0) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to bind frontend: %d(%s)", errno, zmq_strerror(errno));
      exit(4);
   }

   // get endpoint name
   size_t nameSize = sizeof(subEndpoint);
   rc = zmq_getsockopt(frontend, ZMQ_LAST_ENDPOINT, subEndpoint, &nameSize);
   if (0 != rc) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to retrieve bind endpoint: %d(%s)", errno, zmq_strerror(errno));
      exit(5);
   }

   void *backend = zmq_socket (context, ZMQ_XPUB);
   if (backend == NULL) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to create backend: %d(%s)", errno, zmq_strerror(errno));
      exit(6);
   }

   // public endpoint for subscribers to connect to
   char pubEndpoint[1024];
   sprintf(pubEndpoint, "tcp://%s:%d", interface, port);

   const char* programName;
   #if defined __APPLE__
   programName =  getprogname();
   #else
   programName = program_invocation_short_name;
   #endif


   // set welcome msg to be delivered when sub connects
   zmqNamingMsg welcomeMsg;
   memset(&welcomeMsg, '\0', sizeof(welcomeMsg));
   strcpy(welcomeMsg.mTopic, ZMQ_NAMING_PREFIX);
   welcomeMsg.mType = 'W';
   wmStrSizeCpy(welcomeMsg.mProgName, programName, sizeof(welcomeMsg.mProgName));
   wmStrSizeCpy(welcomeMsg.mEndPointAddr, subEndpoint, sizeof(welcomeMsg.mEndPointAddr));
   gethostname(welcomeMsg.mHost, sizeof(welcomeMsg.mHost));
   welcomeMsg.mPid = getpid();
   rc = zmq_setsockopt(backend, ZMQ_XPUB_WELCOME_MSG, &welcomeMsg, sizeof(welcomeMsg));
   if (rc != 0) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to set welcome message: %d(%s)", errno, zmq_strerror(errno));
      exit(7);
   }

   // bind the backend socket to pub endpoint
   rc = zmq_bind (backend, pubEndpoint);
   if (rc != 0) {
      mama_log(MAMA_LOG_LEVEL_SEVERE, "Unable to bind backend: %d(%s)", errno, zmq_strerror(errno));
      exit(8);
   }

   //  Run the proxy until the user interrupts us
   signal(SIGINT, &sighandler);
   mama_log(MAMA_LOG_LEVEL_NORMAL, "%s running at %s", programName, pubEndpoint);
   zmq_proxy (frontend, backend, NULL);
   mama_log(MAMA_LOG_LEVEL_NORMAL, "%s shutting down at %s", programName, pubEndpoint);

   zmq_close (frontend);
   zmq_close (backend);
   zmq_ctx_destroy (context);

   return 0;
}
