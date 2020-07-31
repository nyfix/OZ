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
   mySubscriberEvents(subscriber* pSubscriber) : pSubscriber_
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure)
   {
      if (mamaMsg_isFromInbox(msg)) {
         auto pReply = makeReply(pSession_->connection());
         //reply* pReply = reply::create(pSession_->connection());
         mama_u32_t i;
         mama_status status = mamaMsg_getU32(msg, "num", 0, &i);
         status = mamaMsg_updateU32(msg, "reply", 0, i);
         status = pReply->send(msg);

         const char* msgStr = mamaMsg_toString(msg);
         printf("REQUEST:topic=%s,msg=%s\n", topic_.c_str(), msgStr);
      }
      else {
         const char* msgStr = mamaMsg_toString(msg);
         printf("MSG:topic=%s,msg=%s\n", topic_.c_str(), msgStr);
      }
   }
};


int main(int argc, char** argv)
{
   auto pConnection = makeconnection();
   mama_status status = pConnection->start("zmq", "omnmmsg", "oz");

   auto pSession = pConnection->createSession();
   status = pSession->start();

   auto pSubscriber = new mySubscriber(pSession.get(), "topic");
   status = pSubscriber->subscribe();

   hangout();

   status = pSubscriber->destroy();

   status = pSession->stop();
   status = pSession->destroy();

   status = pConnection->stop();
   status = pConnection->destroy();

   return 0;
}
