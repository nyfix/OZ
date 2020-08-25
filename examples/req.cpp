// minimal requester example

#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

bool notDone = true;
void doneSigHandler(int sig)
{
   notDone = false;
}

class myReqEvents : public requestEvents
{
public:
   virtual void MAMACALLTYPE onReply(request* pRequest, mamaMsg msg) override
   {
      const char* msgStr = mamaMsg_toString(msg);
      fprintf(stderr, "topic=%s,msg=%s\n", pRequest->getTopic().c_str(), msgStr);

      pRequest->destroy();
   }
};



int main(int argc, char** argv)
{
   string topic = "prefix/suffix";
   if (argc > 1) {
      topic = argv[1];
   }

   auto conn = createConnection("zmq", "omnmmsg", "oz");
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   mamaMsg msg;
   TRY_MAMA_FUNC(mamaMsg_create(&msg));
   TRY_MAMA_FUNC(mamaMsg_updateString(msg, "name", 0, "value"));

   signal(SIGINT, doneSigHandler);

   myReqEvents reqEvents;
   int i = 0;
   while(notDone) {
      sleep(1);
      ++i;
      TRY_MAMA_FUNC(mamaMsg_updateU32(msg, "num", 0, i));
      request* pRequest = sess->createRequest(topic, &reqEvents).release();
      TRY_MAMA_FUNC(pRequest->send(msg));
   }

   TRY_MAMA_FUNC(mamaMsg_destroy(msg));

   return 0;
}
