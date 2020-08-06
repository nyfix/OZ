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
public:
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, mamaMsg msg, void* itemClosure) override
   {
      static auto pReply = pSubscriber->getSession()->getConnection()->createReply();

      if (mamaMsg_isFromInbox(msg)) {
         mama_u32_t i;
         mama_status status = mamaMsg_getU32(msg, "num", 0, &i);
         status = mamaMsg_updateU32(msg, "reply", 0, i);
         status = pReply->send(msg);

         const char* msgStr = mamaMsg_toString(msg);
         printf("REQUEST:topic=%s,msg=%s\n", pSubscriber->getTopic().c_str(), msgStr);
      }
      else {
         const char* msgStr = mamaMsg_toString(msg);
         printf("MSG:topic=%s,msg=%s\n", pSubscriber->getTopic().c_str(), msgStr);
      }
   }
};


int main(int argc, char** argv)
{
   auto pConnection = makeconnection();
   mama_status status = pConnection->start("zmq", "omnmmsg", "oz");

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
