// minimal wrapper for OpenMAMA API
#include <string>

#include <mama/mama.h>

namespace oz {

class connection;


class publisher
{

public:
   publisher(connection* pConnection, std::string topic) : pConn_(pConnection), pub_(nullptr), topic_(topic)
   {
   }

   virtual ~publisher();

   mama_status publish(mamaMsg msg);

protected:
   connection*          pConn_;
   mamaPublisher        pub_;
   string               topic_;
};

class subscriber
{

public:
   subscriber(connection* pConnection, std::string topic) : pConn_(pConnection), sub_(nullptr), topic_(topic)
   {
   }

   virtual ~subscriber();

   mama_status subscribe();

   virtual void MAMACALLTYPE onCreate(void) ;
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject) ;
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure) ;

protected:
   connection*          pConn_;
   mamaSubscription     sub_;
   string               topic_;

   static void MAMACALLTYPE createCB(mamaSubscription subscription, void* closure);
   static void MAMACALLTYPE errorCB(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
   static void MAMACALLTYPE msgCB(mamaSubscription subscription, mamaMsg msg, void* closure, void* itemClosure);
};

class connection {

friend class subscriber;
friend class publisher;

public:
   connection(std::string mw, std::string payload, std::string name);
   mama_status start(void);
   mama_status stop(void);

private:
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

