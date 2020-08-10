// minimal requester example

#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"
using namespace oz;

bool notDone = true;
void doneSigHandler(int sig)
{
   notDone = false;
}

class myRequestEvents : public requestEvents
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
   auto pConnection = makeconnection("zmq", "omnmmsg", "oz");
   mama_status status = pConnection->start();

   auto pSession = pConnection->createSession();
   status = pSession->start();

   mamaMsg msg;
   status = mamaMsg_create(&msg);
   status = mamaMsg_updateString(msg, "name", 0, "value");

   signal(SIGINT, doneSigHandler);

   myRequestEvents requestEvents;
   int i = 0;
   while(notDone && (status == MAMA_STATUS_OK)) {
      sleep(1);
      ++i;
      status = mamaMsg_updateU32(msg, "num", 0, i);
      request* pRequest = pSession->createRequest("topic", &requestEvents).release();
      status = pRequest->send(msg);
   }

   status = mamaMsg_destroy(msg);

   status = pSession->stop();
   status = pConnection->stop();

   return 0;
}
