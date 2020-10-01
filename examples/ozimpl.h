// minimal wrapper for OpenMAMA API
#include <signal.h>


#include <string>
#include <unordered_map>
#include <memory>

#include <mama/mama.h>

// call a function that returns mama_status -- log an error and return if not MAMA_STATUS_OK
#define CALL_MAMA_FUNC(x)                                                                  \
   do {                                                                                    \
      mama_status s = (x);                                                                 \
      if (s != MAMA_STATUS_OK) {                                                           \
         mama_log(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", s, mamaStatus_stringForStatus(s)); \
         return s;                                                                         \
      }                                                                                    \
   } while(0)

#define TRY_MAMA_FUNC(x)                                                                  \
   do {                                                                                    \
      mama_status s = (x);                                                                 \
      if (s != MAMA_STATUS_OK) {                                                           \
         mama_log(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", s, mamaStatus_stringForStatus(s)); \
         throw s;                                                                         \
      }                                                                                    \
   } while(0)


namespace oz {

class connection;
class session;
class publisher;

///////////////////////////////////////////////////////////////////////////////
class timer;

class timerEvents
{
public:
   virtual void MAMACALLTYPE onTimer() {}
};

class timer
{
   friend class session;

public:
   timer(session* pSession, double interval, timerEvents* pSink);

   virtual mama_status destroy();

   mama_status start();

   session* getSession() const  { return pSession_; }

   virtual void MAMACALLTYPE onTimer() {}

   // un-implemented
   timer() = delete;
   timer(const timer&) = delete;
   timer(timer&&) = delete;
   timer& operator=(const timer&) = delete;
   timer& operator=(timer&&) = delete;

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

auto timerDeleter = [](timer* pTimer)
{
   pTimer->destroy();
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

   mama_status getReplyTopic(mamaMsg msg, std::string& replyTopic) const;

   // un-implemented
   reply() = delete;
   reply(const reply&) = delete;
   reply(reply&&) = delete;
   reply& operator=(const reply&) = delete;
   reply& operator=(reply&&) = delete;

protected:
   virtual ~reply();

   connection*                         pConn_         {nullptr};
   std::shared_ptr<publisher>          pub_;
};

auto replyDeleter = [](reply* pReply)
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

   std::string getTopic() const { return topic_; }

   // un-implemented
   request() = delete;
   request(const request&) = delete;
   request(request&&) = delete;
   request& operator=(const request&) = delete;
   request& operator=(request&&) = delete;

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

auto requestDeleter = [](request* pRequest)
{
   pRequest->destroy();
};


///////////////////////////////////////////////////////////////////////////////
class publisher
{
   friend class connection;

public:
   publisher(connection* pConnection, std::string topic);

   virtual mama_status destroy();

   mama_status publish(mamaMsg msg);
   mama_status sendRequest(mamaMsg msg, mamaInbox inbox);
   mama_status sendReply(mamaMsg request, mamaMsg reply);

   mamaPublisher getPublisher() const     { return pub_; }

   std::string getTopic() const           { return topic_; }

   // un-implemented
   publisher() = delete;
   publisher(const publisher&) = delete;
   publisher(publisher&&) = delete;
   publisher& operator=(const publisher&) = delete;
   publisher& operator=(publisher&&) = delete;

protected:
   virtual ~publisher();

   connection*          pConn_         {nullptr};
   mamaPublisher        pub_           {nullptr};
   string               topic_;
};

auto publisherDeleter = [](publisher* pPublisher)
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
   virtual void MAMACALLTYPE onMsg(subscriber* pSubscriber, const char* topic, mamaMsg msg, void* itemClosure) {}
};

enum class wcType { unspecified = -1, none, POSIX, WS };

class subscriber
{
   friend class session;

public:
   subscriber(session* pSession, std::string topic, subscriberEvents* pSink = nullptr, wcType wcType = wcType::unspecified);

   virtual mama_status destroy();

   mama_status start();

   std::string getTopic() const           { return topic_; }
   std::string getOrigTopic() const       { return origTopic_; }

   session* getSession()  const           { return pSession_; }

   // un-implemented
   subscriber() = delete;
   subscriber(const subscriber&) = delete;
   subscriber(subscriber&&) = delete;
   subscriber& operator=(const subscriber&) = delete;
   subscriber& operator=(subscriber&&) = delete;

protected:
   virtual ~subscriber();

   virtual void MAMACALLTYPE onCreate() {}
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) {}
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) {}

private:
   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
   static void MAMACALLTYPE wcCB(mamaSubscription subscription, mamaMsg msg, const char* topic, void* closure, void* itemClosure);
   static void MAMACALLTYPE destroyCB(mamaSubscription subscription, void* closure);

   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   session*             pSession_      {nullptr};
   mamaSubscription     sub_           {nullptr};
   subscriberEvents*    pSink_         {nullptr};
   string               topic_;
   string               origTopic_;
   wcType               wcType_        {wcType::unspecified};
};

auto subscriberDeleter = [](subscriber* pSubscriber)
{
   pSubscriber->destroy();
};


///////////////////////////////////////////////////////////////////////////////
// Represents a callback thread consisting of a queue and dispatcher
class session
{
   friend class connection;
public:
   session(oz::connection* pConn) : pConn_(pConn) {}

   virtual mama_status destroy();

   mama_status start();

   mamaQueue getQueue() const               { return queue_; }
   oz::connection* getConnection() const    { return pConn_; }

   template<typename... Ts>
   std::unique_ptr<subscriber, decltype(subscriberDeleter)> createSubscriber(Ts&&... args)
   {
      unique_ptr<subscriber, decltype(subscriberDeleter)> pSubscriber(nullptr, subscriberDeleter);
      pSubscriber.reset(new subscriber(this, std::forward<Ts>(args)...));
      return pSubscriber;
   }

   template<typename... Ts>
   std::unique_ptr<request, decltype(requestDeleter)> createRequest(Ts&&... args)
   {
      unique_ptr<request, decltype(requestDeleter)> pRequest(nullptr, requestDeleter);
      pRequest.reset(new request(this, std::forward<Ts>(args)...));
      return pRequest;
   }

   template<typename... Ts>
   std::unique_ptr<timer, decltype(timerDeleter)> createTimer(Ts&&... args)
   {
      unique_ptr<timer, decltype(timerDeleter)> pTimer(nullptr, timerDeleter);
      pTimer.reset(new timer(this, std::forward<Ts>(args)...));
      return pTimer;
   }

   // un-implemented
   session() = delete;
   session(const session&) = delete;
   session(session&&) = delete;
   session& operator=(session&) = delete;
   session& operator=(session&&) = delete;

protected:
   virtual ~session() {}

   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   oz::connection*      pConn_         {nullptr};
   mamaQueue            queue_         {nullptr};
   mamaDispatcher       dispatcher_    {nullptr};
};

auto sessionDeleter = [](session* pSession)
{
   pSession->destroy();
};


///////////////////////////////////////////////////////////////////////////////
// Represents a middleware connection (i.e., transport), including payload
// library, identified by the tuple {middleware, payload, name}
class connection
{
public:
   connection(std::string mw, std::string payload, std::string name)
      : mw_(mw), payload_(payload), name_(name)
   {}

   virtual mama_status destroy();

   mama_status start();

   mamaTransport getTransport() const      { return transport_; }
   mamaBridge getBridge() const            { return bridge_; }
   std::string getMw() const               { return mw_; }

   template<typename... Ts>
   std::unique_ptr<session, decltype(sessionDeleter)> createSession(Ts&&... args)
   {
      unique_ptr<session, decltype(sessionDeleter)> pSession(nullptr, sessionDeleter);
      pSession.reset(new session(this, std::forward<Ts>(args)...));
      return pSession;
   }

   template<typename... Ts>
   std::unique_ptr<reply, decltype(replyDeleter)> createReply(Ts&&... args)
   {
      unique_ptr<reply, decltype(replyDeleter)> pReply(nullptr, replyDeleter);
      pReply.reset(new reply(this, std::forward<Ts>(args)...));
      return pReply;
   }

   std::shared_ptr<publisher> getPublisher(std::string topic);
   void removePublisher(std::string topic);

   // un-implemented
   connection() = delete;
   connection(const connection&) = delete;
   connection(connection&&) = delete;
   connection& operator=(const connection&) = delete;
   connection& operator=(connection&&) = delete;

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

auto connectionDeleter = [](connection* pConnection)
{
   pConnection->destroy();
};

template<typename... Ts>
std::unique_ptr<connection, decltype(connectionDeleter)> createConnection(Ts&&... args)
{
  std::unique_ptr<connection, decltype(connectionDeleter)> pconnection(nullptr, connectionDeleter);
  pconnection.reset(new connection(std::forward<Ts>(args)...));
  return pconnection;
}


void hangout();

mama_status ws2posix(const string& wsTopic, string& regex);

class cmdLine
{
public:
   cmdLine(int argc, char** argv)
      : argc_(argc), argv_(argv)
   {}

   std::string getMw();
   std::string getPayload();
   std::string getTopic(std::string defaultValue);
   std::string getTport();
   std::string getTportPub();
   std::string getTportSub();

private:
   int      argc_;
   char**   argv_;
};

}

