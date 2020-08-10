// minimal subscriber example

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"
using namespace oz;

class mySubscriberEvents : public subscriberEvents
{
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, mamaMsg msg, void* itemClosure) override
   {
      const char* msgStr = mamaMsg_toString(msg);
      fprintf(stderr, "topic=%s,msg=%s\n", pSubscriber->getTopic().c_str(), msgStr);
   }
};


int main(int argc, char** argv)
{
   auto pConnection = makeconnection("zmq", "omnmmsg", "oz");
   mama_status status = pConnection->start();

   auto pSession = pConnection->createSession();
   status = pSession->start();

   mySubscriberEvents subscriberEvents;
   auto pSubscriber = pSession->createSubscriber("topic", &subscriberEvents);
   status = pSubscriber->subscribe();

   hangout();

   status = pSession->stop();
   status = pConnection->stop();
   return 0;
}
