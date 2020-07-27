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

class myRequest : public request
{
public:
   myRequest(session* pSession, std::string topic)
      : request(pSession, topic)
   {}

   virtual void MAMACALLTYPE onReply(mamaMsg msg) override
   {
      const char* msgStr = mamaMsg_toString(msg);
      printf("topic=%s,msg=%s\n", topic_.c_str(), msgStr);

      delete this;
   }
};



int main(int argc, char** argv)
{
   connection* pConnection = connection::create("zmq", "omnmmsg", "oz");
   mama_status status = pConnection->start();

   session* pSession = session::create(pConnection);
   status = pSession->start();

   mamaMsg msg;
   status = mamaMsg_create(&msg);
   status = mamaMsg_updateString(msg, "name", 0, "value");

   signal(SIGINT, doneSigHandler);

   int i = 0;
   while(notDone && (status == MAMA_STATUS_OK)) {
      sleep(1);
      ++i;
      status = mamaMsg_updateU32(msg, "num", 0, i);
      myRequest* pRequest = new myRequest(pSession, "topic");
      status = pRequest->send(msg);
   }

   status = pConnection->stop();
   status = pConnection->destroy();

   return 0;
}
