//
#include <string>
using namespace std;

#include <mama/mama.h>

namespace oz {

class connection;

class subscriber
{

public:
   subscriber(connection* pConnection) : pConn_(pConnection), sub_(nullptr)
   {
   }

   virtual ~subscriber();

   mama_status subscribe(std::string topic);

   virtual void MAMACALLTYPE onCreate(void);
   virtual void MAMACALLTYPE onError(mama_status status, void* platformError, const char* subject);
   virtual void MAMACALLTYPE onMsg(mamaMsg msg, void* itemClosure);

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

public:
   connection(std::string mw, std::string payload, std::string name);
   mama_status start(void);
   mama_status stop(void);
   subscriber* createSubscriber();

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

}

