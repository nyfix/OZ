// minimal subscriber example

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
   auto conn = createConnection("zmq", "omnmmsg", "oz");
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   myTimerEvents timerEvents;
   auto pTimer = sess->createTimer(0.5, &timerEvents).release();
   TRY_MAMA_FUNC(pTimer->start());

   sleep(5);

   pTimer->destroy();

   return 0;
}
