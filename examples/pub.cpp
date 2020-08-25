// minimal publisher example

#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

bool notDone = true;
void doneSigHandler(int sig)
{
   notDone = false;
}



int main(int argc, char** argv)
{
   string topic = "prefix/suffix";
   if (argc > 1) {
      topic = argv[1];
   }

   auto conn = createConnection("zmq", "omnmmsg", "oz");
   TRY_MAMA_FUNC(conn->start());

   auto pub = conn->getPublisher(topic);

   mamaMsg msg;
   TRY_MAMA_FUNC(mamaMsg_create(&msg));
   TRY_MAMA_FUNC(mamaMsg_updateString(msg, "name", 0, "value"));

   signal(SIGINT, doneSigHandler);

   int i = 0;
   while(notDone) {
      sleep(1);
      ++i;
      TRY_MAMA_FUNC(mamaMsg_updateU32(msg, "num", 0, i));

      const char* msgStr = mamaMsg_toString(msg);
      printf("topic=%s,msg=%s\n", pub->getTopic().c_str(), msgStr);

      TRY_MAMA_FUNC(pub->publish(msg));
   }

   TRY_MAMA_FUNC(mamaMsg_destroy(msg));

   return 0;
}
