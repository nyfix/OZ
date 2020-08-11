// minimal wrapper for OpenMAMA API

#include <sys/errno.h>
#include <string>
using namespace std;

#include <wombat/wSemaphore.h>
#include <mama/mama.h>

#include "../src/util.h"

#include "ozimpl.h"

namespace oz {

///////////////////////////////////////////////////////////////////////
// connection
mama_status oz::connection::start()
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

mama_status oz::connection::stop()
{
   CALL_MAMA_FUNC(status_ = mama_stop(bridge_));
   return MAMA_STATUS_OK;
}

mama_status oz::connection::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaTransport_destroy(transport_));
   CALL_MAMA_FUNC(status_ = mama_close());
   delete this;
   return MAMA_STATUS_OK;
}

void MAMACALLTYPE oz::connection::onStop(mama_status status, mamaBridge bridge, void* closure)
{
   connection* pThis = static_cast<connection*>(closure);
}

shared_ptr<publisher> oz::connection::getPublisher(std::string topic)
{
   auto temp = pubs_.find(topic);
   if (temp != pubs_.end()) {
      return temp->second.lock();
   }

   auto pub = shared_ptr<publisher>(new publisher(this, topic), publisher_deleter);
   if (pub) {
      pubs_[topic] = pub;
   }

   return pub;
}

void oz::connection::removePublisher(std::string topic)
{
   pubs_.erase(topic);
}


///////////////////////////////////////////////////////////////////////
// session
mama_status oz::session::start()
{
   CALL_MAMA_FUNC(status_ = mamaQueue_create(&queue_, pConn_->getBridge()));
   CALL_MAMA_FUNC(status_ = mamaDispatcher_create(&dispatcher_, queue_));
   return MAMA_STATUS_OK;
}

mama_status oz::session::stop()
{
   CALL_MAMA_FUNC(status_ = mamaDispatcher_destroy(dispatcher_));
   return MAMA_STATUS_OK;
}

mama_status oz::session::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaQueue_destroyTimedWait(queue_, 100));
   delete this;
   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////
// subscriber
subscriber::subscriber(session* pSession, string topic, subscriberEvents* pSink)
   : pSession_(pSession), topic_(topic),  pSink_(pSink)
{
}

mama_status subscriber::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaSubscription_destroyEx(sub_));
   // Note: delete is done in destroyCB
   return MAMA_STATUS_OK;
}

subscriber::~subscriber() {}

mama_status subscriber::subscribe()
{
   mamaMsgCallbacks cb;
   memset(&cb, 0, sizeof(cb));
   cb.onCreate       = createCB;
   cb.onError        = errorCB;
   cb.onMsg          = msgCB;
   cb.onQuality      = nullptr;
   cb.onGap          = nullptr;
   cb.onRecapRequest = nullptr;
   cb.onDestroy      = destroyCB;

   CALL_MAMA_FUNC(status_ = mamaSubscription_allocate(&sub_));
   CALL_MAMA_FUNC(status_ = mamaSubscription_createBasic(sub_, pSession_->getConnection()->getTransport(), pSession_->getQueue(), &cb, topic_.c_str(), this));
   return MAMA_STATUS_OK;
}

void MAMACALLTYPE subscriber::createCB(mamaSubscription subscription, void* closure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      pThis->pSink_->onCreate(pThis);
   }
}

void MAMACALLTYPE subscriber::errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      pThis->pSink_->onError(pThis, status, platformError, subject);
   }
}

void MAMACALLTYPE subscriber::msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      // TODO: find a better way ...
      pThis->pSink_->onMsg(pThis, msg, itemClosure);
   }
}

void MAMACALLTYPE subscriber::destroyCB(mamaSubscription subscription, void* closure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if (pThis) {
      mamaSubscription_deallocate(pThis->sub_);
      delete pThis;
   }
}


///////////////////////////////////////////////////////////////////////
// publisher
publisher::publisher(connection* pConnection, std::string topic)
   : pConn_(pConnection), topic_(topic)
{
}

mama_status publisher::destroy()
{
   pConn_->removePublisher(topic_);
   delete this;
   return MAMA_STATUS_OK;
}

publisher::~publisher() {}

mama_status publisher::publish(mamaMsg msg)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->getTransport(), topic_.c_str(), NULL, NULL));
   }

   return mamaPublisher_send(pub_, msg);
}

mama_status publisher::sendRequest(mamaMsg msg, mamaInbox inbox)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->getTransport(), topic_.c_str(), NULL, NULL));
   }

   return mamaPublisher_sendFromInbox(pub_, inbox, msg);
}

mama_status publisher::sendReply(mamaMsg request, mamaMsg reply)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->getTransport(), topic_.c_str(), NULL, NULL));
   }

   return mamaPublisher_sendReplyToInbox(pub_, request, reply);
}


///////////////////////////////////////////////////////////////////////
// signal handling
void ignoreSigHandler(int sig) {}
void hangout()
{
   signal(SIGINT, ignoreSigHandler);
   pause();
}


///////////////////////////////////////////////////////////////////////
// request
request::request(session* pSession, string topic, requestEvents* pSink)
   : pSession_(pSession), topic_(topic), pSink_(pSink)
{
}

mama_status request::destroy()
{
   if (inbox_) {
      // Note: delete is done in destroyCB
      CALL_MAMA_FUNC(mamaInbox_destroy(inbox_));
   }

   return MAMA_STATUS_OK;
}

request::~request()
{
   wsem_destroy(&replied_);
}

mama_status request::send(mamaMsg msg)
{
    if (inbox_ == nullptr) {
       CALL_MAMA_FUNC(mamaInbox_create2(&inbox_, pSession_->getConnection()->getTransport(), pSession_->getQueue(), msgCB, errorCB, destroyCB, this));
    }
    if (pub_ == nullptr) {
       pub_ = pSession_->getConnection()->getPublisher(topic_);
   }

   wsem_init(&replied_, 0, 0);
   return pub_->sendRequest(msg, inbox_);
}

mama_status request::waitReply(double seconds)
{
   int rc = wsem_timedwait(&replied_, (int) (seconds * 1000.0));
   if (rc < 0) {
      switch (errno) {
         case ETIMEDOUT:      return MAMA_STATUS_TIMEOUT;
         case EINVAL:         return MAMA_STATUS_INVALID_ARG;
         default:             return MAMA_STATUS_SYSTEM_ERROR;
      }
   }

   return MAMA_STATUS_OK;
}

void MAMACALLTYPE request::errorCB(mama_status status, void* closure)
{
   request* pThis = dynamic_cast<request*>(static_cast<request*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      pThis->pSink_->onError(pThis, status);
   }
}

void MAMACALLTYPE request::msgCB(mamaMsg msg, void* closure)
{
   request* pThis = dynamic_cast<request*>(static_cast<request*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      wsem_post(&pThis->replied_);
      pThis->pSink_->onReply(pThis, msg);
   }
}

void MAMACALLTYPE request::destroyCB(mamaInbox inbox, void* closure)
{
   request* pThis = dynamic_cast<request*>(static_cast<request*>(closure));
   if (pThis) {
      delete pThis;
   }
}


///////////////////////////////////////////////////////////////////////
// reply
reply::reply(connection* pConnection)
   : pConn_(pConnection)
{
}

mama_status reply::destroy()
{
   delete this;
   return MAMA_STATUS_OK;
}

reply::~reply() {}

mama_status reply::send(mamaMsg msg)
{
   std::string replyTopic;
   CALL_MAMA_FUNC(getReplyTopic(msg, replyTopic));
   auto pPublisher = pConn_->getPublisher(replyTopic);
   if (pPublisher == nullptr) {
      return MAMA_STATUS_PLATFORM;
   }

   return pPublisher->sendReply(msg, msg);
}

mama_status reply::send(mamaMsg request, mamaMsg reply)
{
   std::string replyTopic;
   CALL_MAMA_FUNC(getReplyTopic(request, replyTopic));
   auto pPublisher = pConn_->getPublisher(replyTopic);
   if (pPublisher == nullptr) {
      return MAMA_STATUS_PLATFORM;
   }

   return pPublisher->sendReply(request, reply);
}

mama_status reply::getReplyTopic(mamaMsg msg, std::string& replyTopic)
{
   if (!mamaMsg_isFromInbox(msg)) {
      return MAMA_STATUS_INVALID_ARG;
   }

   #if 0
   if (pConn_->getMw() == "zmq") {
      // this is a fugly hack, but there is no way to get reply address using public API
      typedef struct mamaMsgReplyImpl_
      {
          void* mBridgeImpl;
          void* replyHandle;
      } mamaMsgReplyImpl;
      mamaMsgReply replyHandle;
      CALL_MAMA_FUNC(mamaMsg_getReplyHandle(msg, &replyHandle));
      mamaMsgReplyImpl* pMamaReplyImpl = reinterpret_cast<mamaMsgReplyImpl*>(replyHandle);
      // another fugly hack -- we need a topic to create the publisher for OZ, but there is no
      // way to do that using public API
      #define ZMQ_REPLYHANDLE_INBOXNAME_INDEX   43
      replyTopic = std::string((char*) pMamaReplyImpl->replyHandle).substr(0,ZMQ_REPLYHANDLE_INBOXNAME_INDEX);
      CALL_MAMA_FUNC(mamaMsg_destroyReplyHandle(replyHandle));
   }
   else {
      // TODO: AFAIK none of the other transports care about topic for replies?
      replyTopic = "_INBOX";
   }
   #else
   replyTopic = "_INBOX";
   #endif

   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////
// timer
timer::timer(session* pSession, double interval, timerEvents* pSink)
   : pSession_(pSession), interval_(interval),  pSink_(pSink)
{
}

mama_status timer::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaTimer_destroy(timer_));
   // Note: delete is done in destroyCB
   return MAMA_STATUS_OK;
}

timer::~timer() {}

mama_status timer::start()
{
   CALL_MAMA_FUNC(status_ = mamaTimer_create2(&timer_, pSession_->getQueue(), &timerCB, &destroyCB, interval_, this));
   return MAMA_STATUS_OK;
}

void MAMACALLTYPE timer::timerCB(mamaTimer timer, void* closure)
{
   oz::timer* pThis = dynamic_cast<oz::timer*>(static_cast<oz::timer*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      pThis->pSink_->onTimer();
   }
}

void MAMACALLTYPE timer::destroyCB(mamaTimer timer, void* closure)
{
   oz::timer* pThis = dynamic_cast<oz::timer*>(static_cast<oz::timer*>(closure));
   if (pThis) {
      delete pThis;
   }
}



}
