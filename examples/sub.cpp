//
#include <string>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

namespace oz {

class connection {

public:
   connection(std::string mw, std::string payload, std::string name);
   mama_status start(void);
   mama_status stop(void);

private:
   mama_status          status_;
   string               mw_;
   string               payload_;
   string               name_;
   mamaBridge           bridge_;
   mamaQueue            queue_;
   mamaTransport        transport_;
   mamaPayloadBridge    payloadBridge_;

   static void MAMACALLTYPE onStop(mama_status status, mamaBridge bridge, void* closure);

};


connection::connection(string mw, string payload, string name) :
   status_(MAMA_STATUS_INVALID_ARG), bridge_(nullptr), queue_(nullptr), transport_(nullptr), payloadBridge_(nullptr)
{
   mw_ = mw;
   payload_ = payload;
   name_ = name;
}


mama_status connection::start(void)
{
   CALL_MAMA_FUNC(status_ = mama_loadBridge(&bridge_, mw_.c_str()));
   CALL_MAMA_FUNC(status_ = mama_loadPayloadBridge(&payloadBridge_, payload_.c_str()));
   CALL_MAMA_FUNC(status_ = mama_open());
   CALL_MAMA_FUNC(status_ = mama_getDefaultEventQueue(bridge_, &queue_));
   CALL_MAMA_FUNC(status_ = mamaTransport_allocate(&transport_));
   CALL_MAMA_FUNC(status_ = mamaTransport_create(transport_, name_.c_str(), bridge_));
   CALL_MAMA_FUNC(status_ = mama_startBackgroundEx(bridge_, onStop, this));

   return MAMA_STATUS_OK;
}

mama_status connection::stop(void)
{
   CALL_MAMA_FUNC(status_ = mama_stop(bridge_));
   CALL_MAMA_FUNC(status_ = mamaTransport_destroy(transport_));

   return MAMA_STATUS_OK;
}

void MAMACALLTYPE connection::onStop(mama_status status, mamaBridge bridge, void* closure)
{
   connection* pThis = static_cast<connection*>(closure);
}


}



int main(int argc, char** argv)
{

   oz::connection* conn = new oz::connection("zmq", "omnmmsg", "oz");
   mama_status status = conn->start();

   sleep(2);

   status = conn->stop();

   return 0;
}