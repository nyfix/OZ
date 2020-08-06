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
mama_status oz::connection::start(string mw, string payload, string name)
{
   mw_ = mw;
   payload_ = payload;
   name_ = name;

   CALL_MAMA_FUNC(status_ = mama_loadBridge(&bridge_, mw_.c_str()));
   CALL_MAMA_FUNC(status_ = mama_loadPayloadBridge(&payloadBridge_, payload_.c_str()));
   CALL_MAMA_FUNC(status_ = mama_open());
   CALL_MAMA_FUNC(status_ = mama_getDefaultEventQueue(bridge_, &queue_));
   CALL_MAMA_FUNC(status_ = mamaTransport_allocate(&transport_));
   CALL_MAMA_FUNC(status_ = mamaTransport_create(transport_, name_.c_str(), bridge_));
   CALL_MAMA_FUNC(status_ = mama_startBackgroundEx(bridge_, onStop, this));
   return MAMA_STATUS_OK;
}

mama_status oz::connection::stop(void)
{
   CALL_MAMA_FUNC(status_ = mama_stop(bridge_));
   return MAMA_STATUS_OK;
}

mama_status oz::connection::destroy(void)
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
mama_status oz::session::start(void)
{
   CALL_MAMA_FUNC(status_ = mamaQueue_create(&queue_, pConn_->getBridge()));
   CALL_MAMA_FUNC(status_ = mamaDispatcher_create(&dispatcher_, queue_));
   return MAMA_STATUS_OK;
}

mama_status oz::session::stop(void)
{
   CALL_MAMA_FUNC(status_ = mamaDispatcher_destroy(dispatcher_));
   return MAMA_STATUS_OK;
}

mama_status oz::session::destroy(void)
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
   if (pSink == nullptr) {
      pSink = dynamic_cast<subscriberEvents*>(this);
   }
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

mama_status publisher::destroy(void)
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
void hangout(void)
{
   signal(SIGINT, ignoreSigHandler);
   pause();
}


///////////////////////////////////////////////////////////////////////
// request
request::request(session* pSession, string topic, requestEvents* pSink)
   : pSession_(pSession), topic_(topic), pSink_(pSink)
{
   wsem_init(&replied_, 0, 0);
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

  return pub_->sendRequest(msg, inbox_);
}

mama_status request::waitReply(mamaMsg& reply, double seconds)
{
   int millis = seconds / 1000.0;
   int rc = wsem_timedwait(&replied_, millis);
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

mama_status reply::destroy(void)
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

   if (pConn_->getMw() == "zmq") {
      //
      // OZ needs a reply topic
      // this is a fugly hack, but there is no way to get reply address using public API
      //
      typedef struct mamaMsgReplyImpl_
      {
          void* mBridgeImpl;
          void* replyHandle;
      } mamaMsgReplyImpl;
      mamaMsgReply replyHandle;
      CALL_MAMA_FUNC(mamaMsg_getReplyHandle(msg, &replyHandle));
      mamaMsgReplyImpl* pMamaReplyImpl = reinterpret_cast<mamaMsgReplyImpl*>(replyHandle);
      #if 1
      replyTopic = std::string((char*) pMamaReplyImpl->replyHandle);
      #else
      #define ZMQ_REPLYHANDLE_SIZE              81
      #define ZMQ_REPLYHANDLE_INBOXNAME_INDEX   43
      char  zmqReplyAddr[ZMQ_REPLYHANDLE_SIZE];
      strcpy(zmqReplyAddr, (const char*) pMamaReplyImpl->replyHandle);
      zmqReplyAddr[ZMQ_REPLYHANDLE_INBOXNAME_INDEX-1] = '\0';
      replyTopic = zmqReplyAddr;
      #endif
   }
   else {
      // TODO: AFAIK none of the other transports care about topic for replies?
      replyTopic = "_INBOX";
   }

   return MAMA_STATUS_OK;
}




}
