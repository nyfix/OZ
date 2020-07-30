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
oz::connection* oz::connection::create(string mw, string payload, string name)
{
   oz::connection* pConn = new oz::connection(mw, payload, name);
   return pConn;
}

oz::connection::connection(string mw, string payload, string name)
   : status_(MAMA_STATUS_INVALID_ARG), bridge_(nullptr), queue_(nullptr), transport_(nullptr), payloadBridge_(nullptr)
{
   mw_ = mw;
   payload_ = payload;
   name_ = name;
}

mama_status oz::connection::start(void)
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

publisher* oz::connection::getPublisher(std::string topic)
{
   auto temp = pubs_.find(topic);
   if (temp != pubs_.end()) {
      return temp->second;
   }

   publisher* pub = publisher::create(this, topic);
   if (pub) {
      pubs_[topic] = pub;
   }

   return pub;
}


///////////////////////////////////////////////////////////////////////
// session
oz::session* oz::session::create(oz::connection* pConn)
{
   oz::session* pSession = new oz::session(pConn);
   return pSession;
}

oz::session::session(oz::connection* pConn)
   : pConn_(pConn)
{
}

mama_status oz::session::start(void)
{
   CALL_MAMA_FUNC(status_ = mamaQueue_create(&queue_, pConn_->bridge()));
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
   CALL_MAMA_FUNC(status_ = mamaQueue_destroy(queue_));
   delete this;
   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////
// subscriber
subscriber* subscriber::create(session* pSession, std::string topic)
{
   return new subscriber(pSession, topic);
}

subscriber::subscriber(session* pSession, std::string topic)
   : pSession_(pSession), sub_(nullptr), pSink_(nullptr), topic_(topic)
{
}

mama_status subscriber::destroy()
{
   CALL_MAMA_FUNC(mamaSubscription_destroyEx(sub_));
   // Note: delete is done in destroyCB
   return MAMA_STATUS_OK;
}

subscriber::~subscriber() {}

mama_status subscriber::subscribe(subscriberEvents* pSink)
{
   if (pSink) {
      pSink_ = pSink;
   }
   else {
      pSink_ = dynamic_cast<subscriberEvents*>(this);
   }

   mamaMsgCallbacks cb;
   memset(&cb, 0, sizeof(cb));
   cb.onCreate       = createCB;
   cb.onError        = errorCB;
   cb.onMsg          = msgCB;
   cb.onQuality      = nullptr;
   cb.onGap          = nullptr;
   cb.onRecapRequest = nullptr;
   cb.onDestroy      = destroyCB;

   CALL_MAMA_FUNC(mamaSubscription_allocate(&sub_));
   CALL_MAMA_FUNC(mamaSubscription_createBasic(sub_, pSession_->connection()->transport(), pSession_->queue(), &cb, topic_.c_str(), pSink_));
   return MAMA_STATUS_OK;
}

void MAMACALLTYPE subscriber::createCB(mamaSubscription subscription, void* closure)
{
   subscriberEvents* pThis = dynamic_cast<subscriberEvents*>(static_cast<subscriberEvents*>(closure));
   if (pThis) {
      pThis->onCreate();
   }
}

void MAMACALLTYPE subscriber::errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure)
{
   subscriberEvents* pThis = dynamic_cast<subscriberEvents*>(static_cast<subscriberEvents*>(closure));
   if (pThis) {
      pThis->onError(status, platformError, subject);
   }
}

void MAMACALLTYPE subscriber::msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure)
{
   subscriberEvents* pThis = dynamic_cast<subscriberEvents*>(static_cast<subscriberEvents*>(closure));
   if (pThis) {
      pThis->onMsg(msg, itemClosure);
   }
}

void MAMACALLTYPE subscriber::destroyCB(mamaSubscription subscription, void* closure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if (pThis) {
      delete pThis;
   }
}


///////////////////////////////////////////////////////////////////////
// publisher
publisher* publisher::create(connection* pConnection, std::string topic)
{
   return new publisher(pConnection, topic);
}

publisher::publisher(connection* pConnection, std::string topic)
   : pConn_(pConnection), pub_(nullptr), topic_(topic)
{
}

mama_status publisher::destroy(void)
{
   delete this;
   return MAMA_STATUS_OK;
}

publisher::~publisher() {}

mama_status publisher::publish(mamaMsg msg)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->transport(), topic_.c_str(), NULL, NULL));
   }

   return mamaPublisher_send(pub_, msg);
}

mama_status publisher::sendRequest(mamaMsg msg, mamaInbox inbox)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->transport(), topic_.c_str(), NULL, NULL));
   }

   return mamaPublisher_sendFromInbox(pub_, inbox, msg);
}

mama_status publisher::sendReply(mamaMsg request, mamaMsg reply)
{
   if (pub_ == nullptr) {
      CALL_MAMA_FUNC(mamaPublisher_create(&pub_, pConn_->transport(), topic_.c_str(), NULL, NULL));
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
request* request::create(session* pSession, std::string topic)
{
   return new request(pSession, topic);
}

request::request(session* pSession, std::string topic)
   : pSession_(pSession), inbox_(nullptr), pub_(nullptr), topic_(topic)
{
   wsem_init(&replied_, 0, 0);
}

mama_status request::destroy()
{
   CALL_MAMA_FUNC(mamaInbox_destroy(inbox_));
   // Note: delete is done in destroyCB
   return MAMA_STATUS_OK;
}

request::~request()
{
   wsem_destroy(&replied_);
}

mama_status request::send(mamaMsg msg)
{
   if (inbox_ == nullptr) {
      CALL_MAMA_FUNC(mamaInbox_create2(&inbox_, pSession_->connection()->transport(), pSession_->queue(), msgCB, errorCB, destroyCB, this));
   }
   if (pub_ == nullptr) {
      pub_ = pSession_->connection()->getPublisher(topic_);
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
   if (pThis) {
      pThis->onError(status);
   }
}

void MAMACALLTYPE request::msgCB(mamaMsg msg, void* closure)
{
   request* pThis = dynamic_cast<request*>(static_cast<request*>(closure));
   if (pThis) {
      wsem_post(&pThis->replied_);
      pThis->onReply(msg);
   }
}

void MAMACALLTYPE request::destroyCB(mamaInbox inbox, void* closure)
{
   request* pThis = dynamic_cast<request*>(static_cast<request*>(closure));
   if (pThis) {
      delete pThis;
   }
}

// no-op definitions
void MAMACALLTYPE request::onError(mama_status status) {}
void MAMACALLTYPE request::onReply(mamaMsg msg) {}


///////////////////////////////////////////////////////////////////////
// reply
reply* reply::create(connection* pConnection)
{
   return new reply(pConnection);
}

reply::reply(connection* pConnection)
   : pConn_(pConnection), pub_(nullptr)
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
   publisher* pPublisher = pConn_->getPublisher(replyTopic);
   if (pPublisher == nullptr) {
      return MAMA_STATUS_PLATFORM;
   }

   return pPublisher->sendReply(msg, msg);
}

mama_status reply::send(mamaMsg request, mamaMsg reply)
{
   std::string replyTopic;
   CALL_MAMA_FUNC(getReplyTopic(request, replyTopic));
   publisher* pPublisher = pConn_->getPublisher(replyTopic);
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

   if (pConn_->mw() == "zmq") {
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
      replyTopic = std::string((char*) pMamaReplyImpl->replyHandle);
   }
   else {
      // TODO: AFAIK none of the other transports care about topic for replies?
      replyTopic = "_INBOX";
   }

   return MAMA_STATUS_OK;
}




}
