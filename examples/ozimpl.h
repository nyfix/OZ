// minimal wrapper for OpenMAMA API

#include <string>
#include <unordered_map>
#include <memory>

#include <mama/mama.h>

namespace oz {

class connection;
class session;

///////////////////////////////////////////////////////////////////////
class reply
{
public:
   static reply* create(connection* pConn);
   virtual mama_status destroy();

   mama_status send(mamaMsg reply);
   mama_status send(mamaMsg request, mamaMsg reply);

   mama_status getReplyTopic(mamaMsg msg, std::string& replyTopic);


protected:
   reply(connection* pConn);
   virtual ~reply();

   connection*          pConn_;
   mamaPublisher        pub_;
};

auto reply_deleter = [](reply* pReply)
{
   pReply->destroy();
};

template<typename... Ts>
std::unique_ptr<reply, decltype(reply_deleter)> makeReply(Ts&&... args)
{
  std::unique_ptr<reply, decltype(reply_deleter)> pReply(nullptr, reply_deleter);
  pReply.reset(reply::create(std::forward<Ts>(args)...));
  return pReply;
}


///////////////////////////////////////////////////////////////////////
class publisher;
class request
{
public:
   static request* create(session* pSession, std::string topic);
   virtual mama_status destroy();

   mama_status send(mamaMsg msg);
   mama_status waitReply(mamaMsg& reply, double seconds);

   virtual void MAMACALLTYPE onError(mama_status status) ;
   virtual void MAMACALLTYPE onReply(mamaMsg msg);

protected:
   session*             pSession_;
   publisher*           pub_;
   mamaInbox            inbox_;
   string               topic_;
   wsem_t               replied_;

   request(session* pSession, std::string topic);
   virtual ~request();

   static void MAMACALLTYPE errorCB(mama_status status, void* closure);
   static void MAMACALLTYPE msgCB(mamaMsg msg, void* closure);
   static void MAMACALLTYPE destroyCB(mamaInbox inbox, void* closure);
};


///////////////////////////////////////////////////////////////////////
class publisher
{
public:
   static publisher* create(connection* pConnection, std::string topic);
   virtual mama_status destroy(void);

   mama_status publish(mamaMsg msg);
   mama_status sendRequest(mamaMsg msg, mamaInbox inbox);
   mama_status sendReply(mamaMsg request, mamaMsg reply);

   mamaPublisher getPublisher(void)    { return pub_; }

protected:
   connection*          pConn_;
   mamaPublisher        pub_;
   string               topic_;

   publisher(connection* pConnection, std::string topic);
   virtual ~publisher();
};

///////////////////////////////////////////////////////////////////////
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
   virtual mama_status destroy();

   mama_status subscribe(std::string topic);

   std::string topic(void) { return topic_; }

   virtual void MAMACALLTYPE onCreate(void) {}
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) {}
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) {}

protected:
   subscriber(session* pSession, subscriberEvents* pSink);
   virtual ~subscriber();

   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
   static void MAMACALLTYPE destroyCB(mamaSubscription subscription, void* closure);

private:
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


///////////////////////////////////////////////////////////////////////
class session
{
   friend class connection;
public:
   virtual mama_status destroy(void);

   mama_status start(void);
   mama_status stop(void);

   mamaQueue queue(void)                { return queue_; }
   oz::connection* connection(void)     { return pConn_; }

   std::unique_ptr<subscriber, decltype(subscriber_deleter)> createSubscriber(subscriberEvents* pSink = nullptr)
   {
      unique_ptr<subscriber, decltype(subscriber_deleter)> pSubscriber(nullptr, subscriber_deleter);
      pSubscriber.reset(new subscriber(this, pSink));
      return pSubscriber;
   }

protected:
   session(oz::connection* pConn) : pConn_(pConn) {}
   virtual ~session() {}

private:
   mama_status          status_        {MAMA_STATUS_INVALID_ARG};
   oz::connection*      pConn_         {nullptr};
   mamaQueue            queue_         {nullptr};
   mamaDispatcher       dispatcher_    {nullptr};
};

auto session_deleter = [](session* pSession)
{
   pSession->destroy();
};


///////////////////////////////////////////////////////////////////////
class connection
{
public:
   virtual mama_status destroy(void);

   mama_status start(std::string mw, std::string payload, std::string name);
   mama_status stop(void);

   mamaTransport transport(void)       { return transport_; }
   mamaBridge bridge(void)             { return bridge_; }
   std::string mw(void)                { return mw_; }

   std::unique_ptr<session, decltype(session_deleter)> createSession(void)
   {
      unique_ptr<session, decltype(session_deleter)> pSession(nullptr, session_deleter);
      pSession.reset(new session(this));
      return pSession;
   }

   publisher* getPublisher(std::string topic);

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
   std::unordered_map<std::string, publisher*>   pubs_;
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

