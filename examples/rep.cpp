// minimal subscriber example

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"
using namespace oz;

class mySubscriber : public subscriber, public subscriberEvents
{
public:
   mySubscriber(session* pSession, std::string topic)
      : subscriber(pSession, topic)
   {}

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
   connection* pConnection = connection::create("zmq", "omnmmsg", "oz");
   mama_status status = pConnection->start();

   session* pSession = session::create(pConnection);
   status = pSession->start();

   mySubscriber* pSubscriber = new mySubscriber(pSession, "topic");
   status = pSubscriber->subscribe();

   hangout();

   status = pSubscriber->destroy();

   status = pSession->stop();
   status = pSession->destroy();

   status = pConnection->stop();
   status = pConnection->destroy();

   return 0;
}
