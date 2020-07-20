//
#include <string>
#include <cstdio>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"
using namespace oz;

class mySubscriber : public subscriber
{
public:
   mySubscriber(connection* pConnection, std::string topic) : subscriber(pConnection, topic)
   {
   }
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure);
};

void MAMACALLTYPE mySubscriber::onMsg(mamaMsg msg, void* itemClosure)
{
   const char* msgStr = mamaMsg_toString(msg);
   printf("topic=%s,msg=%s\n", topic_.c_str(), msgStr);
}



int main(int argc, char** argv)
{
   connection* conn = new connection("zmq", "omnmmsg", "oz");
   mama_status status = conn->start();

   mySubscriber* sub = new mySubscriber(conn, "topic");
   status = sub->subscribe();

   hangout();

   status = conn->stop();

   return 0;
}
