// minimal wrapper for OpenMAMA API

#include <string.h>
#include <sys/errno.h>

#include <string>
using namespace std;

#include <wombat/wSemaphore.h>
#include <mama/mama.h>

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

mama_status oz::connection::destroy()
{
   CALL_MAMA_FUNC(status_ = mama_stop(bridge_));
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

   auto pub = shared_ptr<publisher>(new publisher(this, topic), publisherDeleter);
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

mama_status oz::session::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaDispatcher_destroy(dispatcher_));
   CALL_MAMA_FUNC(status_ = mamaQueue_destroyTimedWait(queue_, 100));
   delete this;
   return MAMA_STATUS_OK;
}


///////////////////////////////////////////////////////////////////////
// subscriber
subscriber::subscriber(session* pSession, string topic, subscriberEvents* pSink, wcType wcType)
   : pSession_(pSession), topic_(topic),  pSink_(pSink), wcType_(wcType)
{
}

mama_status subscriber::destroy()
{
   CALL_MAMA_FUNC(status_ = mamaSubscription_destroyEx(sub_));
   // Note: delete is done in destroyCB
   return MAMA_STATUS_OK;
}

subscriber::~subscriber() {}

mama_status subscriber::start()
{
   if (wcType_ == wcType::unspecified) {
      // try to guess
      const char* regexPos = strpbrk(topic_.c_str(), ".^$*+\[]");  // find wildcard regex?
      if (regexPos != nullptr) {
         wcType_ = wcType::POSIX;
      }
   }

   CALL_MAMA_FUNC(status_ = mamaSubscription_allocate(&sub_));

   origTopic_ = topic_;
   switch (wcType_) {
      case wcType::WS :
         // convert WS to posix, then fall thru
         CALL_MAMA_FUNC(ws2posix(origTopic_, topic_));
      case wcType::POSIX :
      {
         mamaWildCardMsgCallbacks cb;
         memset(&cb, 0, sizeof(cb));
         cb.onCreate  = createCB;
         cb.onError   = errorCB;
         cb.onMsg     = wcCB;
         cb.onDestroy = destroyCB;
         CALL_MAMA_FUNC(status_ = mamaSubscription_createBasicWildCard(sub_, pSession_->getConnection()->getTransport(), pSession_->getQueue(), &cb, nullptr, topic_.c_str(), this));
         break;
      }

      case wcType::unspecified :
      case wcType::none :
      {
         mamaMsgCallbacks cb;
         memset(&cb, 0, sizeof(cb));
         cb.onCreate       = createCB;
         cb.onError        = errorCB;
         cb.onMsg          = msgCB;
         cb.onDestroy      = destroyCB;
         CALL_MAMA_FUNC(status_ = mamaSubscription_createBasic(sub_, pSession_->getConnection()->getTransport(), pSession_->getQueue(), &cb, topic_.c_str(), this));
         break;
      }

   }

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
      pThis->pSink_->onMsg(pThis, pThis->topic_.c_str(), msg, itemClosure);
   }
}

void MAMACALLTYPE subscriber::wcCB(mamaSubscription subscription, mamaMsg msg, const char* topic, void* closure, void* itemClosure)
{
   subscriber* pThis = dynamic_cast<subscriber*>(static_cast<subscriber*>(closure));
   if ((pThis) && (pThis->pSink_)) {
      // TODO: find a better way ...
      pThis->pSink_->onMsg(pThis, topic, msg, itemClosure);
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
   auto publisher = pConn_->getPublisher(replyTopic);
   if (publisher == nullptr) {
      return MAMA_STATUS_PLATFORM;
   }

   return publisher->sendReply(msg, msg);
}

mama_status reply::send(mamaMsg request, mamaMsg reply)
{
   std::string replyTopic;
   CALL_MAMA_FUNC(getReplyTopic(request, replyTopic));
   auto publisher = pConn_->getPublisher(replyTopic);
   if (publisher == nullptr) {
      return MAMA_STATUS_PLATFORM;
   }

   return publisher->sendReply(request, reply);
}

mama_status reply::getReplyTopic(mamaMsg msg, std::string& replyTopic) const
{
   if (!mamaMsg_isFromInbox(msg)) {
      return MAMA_STATUS_INVALID_ARG;
   }

   // the topic used to publish inbox replies is irrelevant -- all
   // the bridge implementations ignore the specified topic and instead
   // use information encoded in the reply handle to route the reply
   replyTopic = "_INBOX";

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


///////////////////////////////////////////////////////////////////////
// signal handling
void ignoreSigHandler(int sig) {}
void hangout()
{
   signal(SIGINT, ignoreSigHandler);
   pause();
}


///////////////////////////////////////////////////////////////////////
// converts hierarchical topic (as per WS-Topic) to extended regular expression
mama_status ws2posix(const string& wsTopic, string& regex)
{
   mama_status status = MAMA_STATUS_INVALID_ARG;
   string inTopic = wsTopic;
   size_t q = 0;
   size_t p = inTopic.find("*");
   if (p != string::npos) {
      status = MAMA_STATUS_OK;
      if (p == 0) {
         regex = "^[^_][^/]*";               // anchor at beginning, omit subjects with beginning underscore
         q = regex.size();
         regex.append(inTopic.substr(1));
      }
      else {
         regex = "^";
         regex.append(inTopic);
      }
   }
   else {
      regex.append(inTopic);
   }

   p = regex.find("*", q);
   while (p != string::npos) {
      regex.replace(p, 1, "[^/]+");
      p = regex.find("*", p);
   }

   p = regex.rfind("/.");
   if (p == regex.length() -2) {
      if (p == 0) {
         regex = "^[^_].*";                  // anchor at beginning, omit subjects with beginning underscore
      }
      else {
         regex.replace(regex.length() -2, 2, ".*");
         if (regex[0] != '^') {
            regex.insert(0, 1, '^');         // anchor at beginning
         }
      }
      status = MAMA_STATUS_OK;
   }
   else {
      if (status == MAMA_STATUS_OK)
         regex.append("$");                  // anchor at end
   }

   mama_log(MAMA_LOG_LEVEL_NORMAL, "wsTopic=%s, regex=%s", inTopic.c_str(), regex.c_str());

   return status;
}


///////////////////////////////////////////////////////////////////////
// command-line parsing -- tedious but necessary
string cmdLine::getMw()
{
   string mw = "zmq";

   // first look on cmd line
   for (int i = 1; i < argc_; i++) {
      if (strncasecmp("-m", argv_[i], strlen(argv_[i])) == 0) {
         mw = argv_[++i];
      }
   }

   // if that didnt work, try env
   if (mw.empty()) {
      char* temp = getenv("MAMA_MW");
      if (temp) {
         mw = temp;
      }
   }

   return mw;
}

string cmdLine::getPayload()
{
   string payload = "omnmmsg";

   // first look on cmd line
   for (int i = 1; i < argc_; i++) {
      if (strncasecmp("-p", argv_[i], strlen(argv_[i])) == 0) {
         payload = argv_[++i];
      }
   }

   // if that didnt work, try env
   if (payload.empty()) {
      char* temp = getenv("MAMA_PAYLOAD");
      if (temp) {
         payload = temp;
      }
   }

   return payload;
}

string cmdLine::getTport()
{
   string tport = "oz";

   // first look on cmd line
   for (int i = 1; i < argc_; i++) {
      if (strncasecmp("-tport", argv_[i], strlen(argv_[i])) == 0) {
         tport = argv_[++i];
      }
   }

   return tport;
}

string cmdLine::getTportSub()
{
   // was it specified on cmd line?
   string tport = getTport();

   // if that didnt work, try env
   if (tport.empty()) {
      char* temp = getenv("MAMA_TPORT_SUB");
      if (temp) {
         tport = temp;
      }
   }

   return tport;
}


string cmdLine::getTportPub()
{
   // was it specified on cmd line?
   string tport = getTport();

   // if that didnt work, try env
   if (tport.empty()) {
      char* temp = getenv("MAMA_TPORT_PUB");
      if (temp) {
         tport = temp;
      }
   }

   return tport;
}

string cmdLine::getTopic(string defaultValue)
{
   string topic = defaultValue;

   if (argc_ < 2) {
      // no param
   }
   else if (argc_ == 2) {
      // only one argument, so it must be topic
      topic = argv_[1];
   }
   else {
      // if the next-to-last param doesn't start with a "-", take the last param
      if (argv_[argc_-2][0] != '-') {
         topic = argv_[argc_-1];
      }
   }

   return topic;
}

}
