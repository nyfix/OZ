// minimal publisher example

#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "ozimpl.h"
using namespace oz;

int main(int argc, char** argv)
{
   cmdLine cli(argc, argv);
   string topic = cli.getTopic("prefix/suffix");

   auto conn = createConnection(cli.getMw(), cli.getPayload(), cli.getTportPub());
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   return 0;
}
