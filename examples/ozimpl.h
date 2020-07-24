// minimal wrapper for OpenMAMA API

#include <string>

#include <mama/mama.h>

namespace oz {

class connection;
class session;

///////////////////////////////////////////////////////////////////////
class publisher
{
public:
   static publisher* create(connection* pConnection, std::string topic);
   virtual mama_status destroy(void);

   mama_status publish(mamaMsg msg);

protected:
   connection*          pConn_;
   mamaPublisher        pub_;
   string               topic_;

   publisher(connection* pConnection, std::string topic);
   virtual ~publisher();
};


///////////////////////////////////////////////////////////////////////
class subscriber
{
public:
   static subscriber* create(session* pSession, std::string topic);
   virtual mama_status destroy();

   mama_status subscribe();

   virtual void MAMACALLTYPE onCreate(void) ;
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) ;
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) ;

protected:
   session*             pSession_;
   mamaSubscription     sub_;
   string               topic_;

   subscriber(session* pSession, std::string topic);
   virtual ~subscriber();

   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
   static void MAMACALLTYPE destroyCB(mamaSubscription subscription, void* closure);
};


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
};

void hangout(void);

}

