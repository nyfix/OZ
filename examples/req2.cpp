// minimal requester example

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

class myRequest : public request, requestEvents
{
public:
   myRequest(session* pSession, string topic)
      : request(pSession, topic)
   {
      pSink_ = this;
   }

   mamaMsg getReply()         { return reply_; }

private:
   virtual void MAMACALLTYPE onReply(request* pRequest, mamaMsg msg) override
   {
      TRY_MAMA_FUNC(mamaMsg_copy(msg, &reply_));
   }

   mamaMsg     reply_;
};



int main(int argc, char** argv)
{
   cmdLine cli(argc, argv);
   string topic = cli.getTopic("prefix/suffix");

   auto conn = createConnection(cli.getMw(), cli.getPayload(), cli.getTportPub());
   TRY_MAMA_FUNC(conn->start());

   auto sess = conn->createSession();
   TRY_MAMA_FUNC(sess->start());

   mamaMsg msg;
   TRY_MAMA_FUNC(mamaMsg_create(&msg));
   TRY_MAMA_FUNC(mamaMsg_updateString(msg, "name", 0, "value"));

   signal(SIGINT, doneSigHandler);

   int i = 1;
   TRY_MAMA_FUNC(mamaMsg_updateU32(msg, "num", 0, i));
   auto req = new myRequest(sess.get(), topic);
   TRY_MAMA_FUNC(req->send(msg));

   mama_status status = req->waitReply(5);
   if (status == MAMA_STATUS_OK) {
      const char* msgStr = mamaMsg_toString(req->getReply());
      fprintf(stderr, "reply=%s\n", msgStr);
   }
   else if (status == MAMA_STATUS_TIMEOUT) {
      fprintf(stderr, "timed out!\n");
   }
   else {
      fprintf(stderr, "error: %d (%s)!\n", status, mamaStatus_stringForStatus(status));
   }

   return 0;
}
