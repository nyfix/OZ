// minimal wrapper for OpenMAMA API

#include <string>
#include <unordered_map>
#include <memory>

#include <mama/mama.h>

namespace oz {

class connection;
class session;
class publisher;

///////////////////////////////////////////////////////////////////////////////
class timer;

class timerEvents
{
public:
   virtual void MAMACALLTYPE onTimer(void) {}
};

class timer
{
   friend class session;

public:
   timer(session* pSession, double interval, timerEvents* pSink);

   virtual mama_status destroy();

   mama_status start();

   session* getSession(void)  { return pSession_; }

   virtual void MAMACALLTYPE onTimer(void) {}

protected:
   virtual ~timer();

   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   session*             pSession_      {nullptr};
   double               interval_      {0};
   mamaTimer            timer_         {nullptr};
   timerEvents*         pSink_         {nullptr};

private:
   static void MAMACALLTYPE timerCB(mamaTimer timer, void* closure);
   static void MAMACALLTYPE destroyCB(mamaTimer timer, void* closure);
};

auto timer_deleter = [](timer* ptimer)
{
   ptimer->destroy();
};


///////////////////////////////////////////////////////////////////////////////
class reply
{
   friend class connection;

public:
   reply(connection* pConn);

   virtual mama_status destroy();

   mama_status send(mamaMsg reply);
   mama_status send(mamaMsg request, mamaMsg reply);

   mama_status getReplyTopic(mamaMsg msg, std::string& replyTopic);

protected:
   virtual ~reply();

   connection*                         pConn_         {nullptr};
   std::shared_ptr<publisher>          pub_;
};

auto reply_deleter = [](reply* pReply)
{
   pReply->destroy();
};


///////////////////////////////////////////////////////////////////////////////
class request;
class requestEvents
{
public:
   virtual void MAMACALLTYPE onError(request* pRequest, mama_status status) {}
   virtual void MAMACALLTYPE onReply(request* pRequest, mamaMsg msg) {}
};

class publisher;
class request
{
   friend class session;

public:
   request(session* pSession, string topic, requestEvents* pSink = nullptr);
   virtual mama_status destroy();

   mama_status send(mamaMsg msg);
   mama_status waitReply(double seconds);

   std::string getTopic(void) { return topic_; }

protected:
   virtual ~request();

   session*                            pSession_      {nullptr};
   string                              topic_;
   requestEvents*                      pSink_         {nullptr};
   std::shared_ptr<publisher>          pub_;
   mamaInbox                           inbox_         {nullptr};
   wsem_t                              replied_;

private:
   static void MAMACALLTYPE errorCB(mama_status status, void* closure);
   static void MAMACALLTYPE msgCB(mamaMsg msg, void* closure);
   static void MAMACALLTYPE destroyCB(mamaInbox inbox, void* closure);
};

auto request_deleter = [](request* prequest)
{
   prequest->destroy();
};


///////////////////////////////////////////////////////////////////////////////
class publisher
{
   friend class connection;

public:
   publisher(connection* pConnection, std::string topic);

   virtual mama_status destroy(void);

   mama_status publish(mamaMsg msg);
   mama_status sendRequest(mamaMsg msg, mamaInbox inbox);
   mama_status sendReply(mamaMsg request, mamaMsg reply);

   mamaPublisher getPublisher(void)    { return pub_; }

protected:
   virtual ~publisher();

   connection*          pConn_         {nullptr};
   mamaPublisher        pub_           {nullptr};
   string               topic_;
};

auto publisher_deleter = [](publisher* pPublisher)
{
   pPublisher->destroy();
};


///////////////////////////////////////////////////////////////////////////////
class subscriber;

class subscriberEvents
{
public:
   virtual void MAMACALLTYPE onCreate(subscriber* pSubscriber) {}
   virtual void MAMACALLTYPE onError(subscriber* pSubscriber, mama_status status, void* platformError, const char* subject) {}
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, mamaMsg msg, void* itemClosure) {}
};

class subscriber
{
   friend class session;

public:
   subscriber(session* pSession, std::string topic, subscriberEvents* pSink = nullptr);

   virtual mama_status destroy();

   mama_status subscribe();

   std::string getTopic(void) { return topic_; }

   session* getSession(void)  { return pSession_; }

protected:
   virtual ~subscriber();

   virtual void MAMACALLTYPE onCreate(void) {}
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) {}
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) {}

private:
   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
   static void MAMACALLTYPE destroyCB(mamaSubscription subscription, void* closure);

   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   session*             pSession_      {nullptr};
   mamaSubscription     sub_           {nullptr};
   subscriberEvents*    pSink_         {nullptr};
   string               topic_;
};

auto subscriber_deleter = [](subscriber* psubscriber)
{
   psubscriber->destroy();
};


///////////////////////////////////////////////////////////////////////////////
// Represents a callback thread consisting of a queue and dispatcher
class session
{
   friend class connection;
public:
   session(oz::connection* pConn) : pConn_(pConn) {}

   virtual mama_status destroy(void);

   mama_status start(void);
   mama_status stop(void);

   mamaQueue getQueue(void)                { return queue_; }
   oz::connection* getConnection(void)     { return pConn_; }

   std::unique_ptr<subscriber, decltype(subscriber_deleter)> createSubscriber(std::string topic, subscriberEvents* pSink = nullptr)
   {
      unique_ptr<subscriber, decltype(subscriber_deleter)> pSubscriber(nullptr, subscriber_deleter);
      pSubscriber.reset(new subscriber(this, topic, pSink));
      return pSubscriber;
   }

   std::unique_ptr<request, decltype(request_deleter)> createRequest(std::string topic, requestEvents* pSink = nullptr)
   {
      unique_ptr<request, decltype(request_deleter)> pRequest(nullptr, request_deleter);
      pRequest.reset(new request(this, topic, pSink));
      return pRequest;
   }

   std::unique_ptr<timer, decltype(timer_deleter)> createTimer(double interval, timerEvents* pSink = nullptr)
   {
      unique_ptr<timer, decltype(timer_deleter)> pTimer(nullptr, timer_deleter);
      pTimer.reset(new timer(this, interval, pSink));
      return pTimer;
   }

protected:
   virtual ~session() {}

   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   oz::connection*      pConn_         {nullptr};
   mamaQueue            queue_         {nullptr};
   mamaDispatcher       dispatcher_    {nullptr};
};

auto session_deleter = [](session* pSession)
{
   pSession->destroy();
};


///////////////////////////////////////////////////////////////////////////////
// Represents a middleware connection (i.e., transport), including payload
// library, identified by the tuple {middleware, payload, name}
class connection
{
public:
   virtual mama_status destroy(void);

   mama_status start(std::string mw, std::string payload, std::string name);
   mama_status stop(void);

   mamaTransport getTransport(void)       { return transport_; }
   mamaBridge getBridge(void)             { return bridge_; }
   std::string getMw(void)                { return mw_; }

   std::unique_ptr<session, decltype(session_deleter)> createSession(void)
   {
      unique_ptr<session, decltype(session_deleter)> pSession(nullptr, session_deleter);
      pSession.reset(new session(this));
      return pSession;
   }

   std::unique_ptr<reply, decltype(reply_deleter)> createReply(void)
   {
      unique_ptr<reply, decltype(reply_deleter)> pReply(nullptr, reply_deleter);
      pReply.reset(new reply(this));
      return pReply;
   }

   std::shared_ptr<publisher> getPublisher(std::string topic);
   void removePublisher(std::string topic);

protected:
   virtual ~connection() {}
   static void MAMACALLTYPE onStop(mama_status status, mamaBridge bridge, void* closure);

private:
   mama_status          status_           {MAMA_STATUS_INVALID_ARG};
   string               mw_;
   string               payload_;
   string               name_;
   mamaBridge           bridge_           {nullptr};
   mamaQueue            queue_            {nullptr};
   mamaTransport        transport_        {nullptr};
   mamaPayloadBridge    payloadBridge_    {nullptr};

   std::unordered_map<std::string, std::weak_ptr<publisher>>   pubs_;
};

auto connection_deleter = [](connection* pconnection)
{
   pconnection->destroy();
};

template<typename... Ts>
std::unique_ptr<connection, decltype(connection_deleter)> makeconnection(Ts&&... args)
{
  std::unique_ptr<connection, decltype(connection_deleter)> pconnection(nullptr, connection_deleter);
  pconnection.reset(new connection(std::forward<Ts>(args)...));
  return pconnection;
}


void hangout(void);

}

