// minimal subscriber example

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

class mySubEvents : public subscriberEvents
{
public:
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, const char* topic, mamaMsg msg, void* itemClosure) override
   {
      static auto reply = pSubscriber->getSession()->getConnection()->createReply();

      if (mamaMsg_isFromInbox(msg)) {
         mamaMsg temp;
         TRY_MAMA_FUNC(mamaMsg_getTempCopy(msg, &temp));
         mama_u32_t i;
         TRY_MAMA_FUNC( mamaMsg_getU32(temp, "num", 0, &i));
         TRY_MAMA_FUNC(mamaMsg_updateU32(temp, "reply", 0, i));
         TRY_MAMA_FUNC(reply->send(temp));

         const char* msgStr = mamaMsg_toString(temp);
         fprintf(stderr, "REQUEST:topic=%s,msg=%s\n", topic, msgStr);
      }
      else {
         const char* msgStr = mamaMsg_toString(msg);
         fprintf(stderr, "MSG:topic=%s,msg=%s\n", pSubscriber->getTopic().c_str(), msgStr);
      }
   }
};


int main(int argc, char** argv)
{
   auto conn = createConnection("zmq", "omnmmsg", "oz");
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   mySubEvents subscriberEvents;
   auto sub = sess->createSubscriber("topic", &subscriberEvents);
   TRY_MAMA_FUNC(sub->start());

   hangout();

   return 0;
}
