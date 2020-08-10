// minimal subscriber example

#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

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
   auto pConnection = makeconnection("zmq", "omnmmsg", "oz");
   mama_status status = pConnection->start();

   auto pSession = pConnection->createSession();
   status = pSession->start();

   myTimerEvents timerEvents;
   auto pTimer = pSession->createTimer(0.5, &timerEvents);
   status = pTimer->start();

   sleep(5);

   status = pSession->stop();
   status = pConnection->stop();

   return 0;
}
