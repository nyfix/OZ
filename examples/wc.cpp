// minimal subscriber example using regex wildcards

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

class mySubEvents : public subscriberEvents
{
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, const char* topic, mamaMsg msg, void* itemClosure) override
   {
      const char* msgStr = mamaMsg_toString(msg);
      fprintf(stderr, "topic=%s,msg=%s\n", topic, msgStr);
   }
};



int main(int argc, char** argv)
{
   cmdLine cli(argc, argv);
   string topic = cli.getTopic("^prefix/[^/]+$");

   auto conn = createConnection(cli.getMw(), cli.getPayload(), cli.getTportSub());
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   mySubEvents subEvents;
   auto sub = sess->createSubscriber(topic, &subEvents);
   TRY_MAMA_FUNC(sub->start());

   hangout();

   return 0;
}
