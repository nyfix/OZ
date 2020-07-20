//
#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"
using namespace oz;

class mySubscriber : public oz::subscriber
{
public:
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure);
};

void MAMACALLTYPE mySubscriber::onMsg(mamaMsg msg, void* itemClosure)
{
   cout << topic_ << endl;
}


int main(int argc, char** argv)
{
   connection* conn = new connection("zmq", "omnmmsg", "oz");
   mama_status status = conn->start();



   sleep(2);

   status = conn->stop();

   return 0;
}
