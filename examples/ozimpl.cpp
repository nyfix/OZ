//
#include <string>
#include <iostream>
using namespace std;

#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"

namespace oz {


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

subscriber* connection::createSubscriber()
{
   subscriber* pSub = new subscriber(this);
   return pSub;
}

subscriber::~subscriber() {}

mama_status subscriber::subscribe(std::string topic)
{
   mamaMsgCallbacks cb;
   memset(&cb, 0, sizeof(cb));
   cb.onCreate       = createCB;
   cb.onError        = errorCB;
   cb.onMsg          = msgCB;
   cb.onQuality      = nullptr;
   cb.onGap          = nullptr;
   cb.onRecapRequest = nullptr;
   cb.onDestroy      = nullptr;

   CALL_MAMA_FUNC(mamaSubscription_allocate(&sub_));
   CALL_MAMA_FUNC(mamaSubscription_createBasic(sub_, pConn_->transport_, pConn_->queue_, &cb, topic.c_str(), this));
   return MAMA_STATUS_OK;
}

void MAMACALLTYPE subscriber::createCB(mamaSubscription subscription, void* closure)
{
   subscriber* cb = static_cast<subscriber*>(closure);
   if (cb) {
      cb->onCreate();
   }
}

void MAMACALLTYPE subscriber::errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure)
{
   subscriber* cb = static_cast<subscriber*>(closure);
   if (cb) {
      cb->onError(status, platformError, subject);
   }
}

void MAMACALLTYPE subscriber::msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure)
{
   subscriber* cb = static_cast<subscriber*>(closure);
   if (cb) {
      cb->onMsg(msg, itemClosure);
   }
}

// no-op definitions
void MAMACALLTYPE subscriber::onCreate(void) {}
void MAMACALLTYPE subscriber::onError(mama_status status, void* platformError, const char* subject) {}
void MAMACALLTYPE subscriber::onMsg(mamaMsg msg, void* itemClosure) {}


}