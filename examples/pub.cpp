//
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
   connection* conn = new connection("zmq", "omnmmsg", "oz");
   mama_status status = conn->start();

   publisher* pub = new publisher(conn, "topic");

   mamaMsg msg;
   status = mamaMsg_create(&msg);
   status = mamaMsg_updateString(msg, "name", 0, "value");

   signal(SIGINT, doneSigHandler);

   int i = 0;
   while(notDone) {
      sleep(1);
      ++i;
      status = mamaMsg_updateU32(msg, "num", 0, i);
      status = pub->publish(msg);
   }

   status = conn->stop();

   return 0;
}
