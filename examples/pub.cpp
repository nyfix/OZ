// minimal publisher example

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


int main(int argc, char** argv)
{
   auto pConnection = makeconnection();
   mama_status status = pConnection->start("zmq", "omnmmsg", "oz");

   auto pPublisher = pConnection->getPublisher("topic");

   mamaMsg msg;
   status = mamaMsg_create(&msg);
   status = mamaMsg_updateString(msg, "name", 0, "value");

   signal(SIGINT, doneSigHandler);

   int i = 0;
   while(notDone) {
      sleep(1);
      ++i;
      status = mamaMsg_updateU32(msg, "num", 0, i);

      const char* msgStr = mamaMsg_toString(msg);
      printf("msg=%s\n", msgStr);

      status = pPublisher->publish(msg);
   }

   status = pPublisher->destroy();
   status = pConnection->stop();

   return 0;
}
