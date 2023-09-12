// just to verify that asan/lsan are working properly

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

class myTimerEvents : public timerEvents
{
   virtual void MAMACALLTYPE onTimer() override
   {
      static int i = 0;
      fprintf(stderr, "timer=%d\n", ++i);
   }
};


int main(int argc, char** argv)
{
   cmdLine cli(argc, argv);

   auto conn = createConnection(cli.getMw(), cli.getPayload(), cli.getTportSub());
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   myTimerEvents timerEvents;
   auto pTimer = sess->createTimer(0.5, &timerEvents).release();
   TRY_MAMA_FUNC(pTimer->start());

   sleep(5);

   pTimer->destroy();

   void* leak = malloc(512);

   return 0;
}
