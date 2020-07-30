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
class subscriberEvents
{
public:
   virtual void MAMACALLTYPE onCreate(void) {}
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) {}
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) {}

};



///////////////////////////////////////////////////////////////////////
class subscriber
{
public:
   static subscriber* create(session* pSession, std::string topic);
   virtual mama_status destroy();

   mama_status subscribe(subscriberEvents* pSink = nullptr);

protected:
   session*             pSession_;
   mamaSubscription     sub_;
   subscriberEvents*    pSink_;
   string               topic_;

   subscriber(session* pSession, std::string topic);
   virtual ~subscriber();

   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
   static void MAMACALLTYPE destroyCB(mamaSubscription subscription, void* closure);
};

auto subscriber_deleter = [](subscriber* psubscriber)
{
   psubscriber->destroy();
};

template<typename... Ts>
std::unique_ptr<subscriber, decltype(subscriber_deleter)> makesubscriber(Ts&&... args)
{
  std::unique_ptr<subscriber, decltype(subscriber_deleter)> psubscriber(nullptr, subscriber_deleter);
  psubscriber.reset(subscriber::create(std::forward<Ts>(args)...));
  return psubscriber;
}

///////////////////////////////////////////////////////////////////////
class session
{
public:
   static oz::session* create(oz::connection* conn);
   virtual mama_status destroy(void);

   mama_status start(void);
   mama_status stop(void);

   mamaQueue queue(void)                { return queue_; }
   oz::connection* connection(void)     { return pConn_; }

protected:
   session(oz::connection* conn);
   virtual ~session() {}

   mama_status          status_;
   oz::connection*      pConn_;
   mamaQueue            queue_;
   mamaDispatcher       dispatcher_;
};


///////////////////////////////////////////////////////////////////////
class connection
{
public:
   static connection* create(std::string mw, std::string payload, std::string name);
   virtual mama_status destroy(void);

   mama_status start(void);
   mama_status stop(void);

   mamaTransport transport(void)       { return transport_; }
   mamaBridge bridge(void)             { return bridge_; }
   std::string mw(void)                { return mw_; }

   publisher* getPublisher(std::string topic);

protected:
   connection(std::string mw, std::string payload, std::string name);
   virtual ~connection() {}

   mama_status          status_;
   string               mw_;
   string               payload_;
   string               name_;
   mamaBridge           bridge_;
   mamaQueue            queue_;
   mamaTransport        transport_;
   mamaPayloadBridge    payloadBridge_;

   static void MAMACALLTYPE onStop(mama_status status, mamaBridge bridge, void* closure);

private:
   std::unordered_map<std::string, publisher*>   pubs_;
};

void hangout(void);

}

