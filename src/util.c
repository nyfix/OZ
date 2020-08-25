#define _GNU_SOURCE
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <wombat/wUuid.h>
#include <mama/log.h>

#include <zmq.h>

#include "zmqdefs.h"

// TODO: these values are in a later version of zmq.h
/*  DRAFT 0MQ socket events and monitoring                                    */
/*  Unspecified system errors during handshake. Event value is an errno.      */
#define ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL   0x0800
/*  Handshake complete successfully with successful authentication (if        *
 *  enabled). Event value is unused.                                          */
#define ZMQ_EVENT_HANDSHAKE_SUCCEEDED          0x1000
/*  Protocol errors between ZMTP peers or between server and ZAP handler.     *
 *  Event value is one of ZMQ_PROTOCOL_ERROR_*                                */
#define ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL    0x2000
/*  Failed authentication requests. Event value is the numeric ZAP status     *
 *  code, i.e. 300, 400 or 500.                                               */
#define ZMQ_EVENT_HANDSHAKE_FAILED_AUTH        0x4000



#include "util.h"


const char* zmqBridge_generateUuid()
{
   wUuid tempUuid;
   #ifdef wUuid_generate_time_safe
   if (wUuid_generate_time_safe(tempUuid) == -1) {
      return NULL;
   }
   #else
   wUuid_generate(tempUuid);
   #endif

   char uuidStringBuffer[UUID_STRING_SIZE+ 1];
   wUuid_unparse(tempUuid, uuidStringBuffer);

   return strdup(uuidStringBuffer);
}


const char* zmqBridge_generateSerial(unsigned long long* id)
{
   long long next = __sync_add_and_fetch(id, 1);
   char temp[ZMQ_REPLYHANDLE_INBOXNAME_SIZE +1];
   sprintf(temp, "%016llx", next);
   return strdup(temp);
}

const char* get_zmqEventName(uint64_t event)
{
   switch(event) {
      case ZMQ_EVENT_CONNECTED                  : return "CONNECTED";
      case ZMQ_EVENT_CONNECT_DELAYED            : return "CONNECT_DELAYED";
      case ZMQ_EVENT_CONNECT_RETRIED            : return "CONNECT_RETRIED";
      case ZMQ_EVENT_LISTENING                  : return "LISTENING";
      case ZMQ_EVENT_BIND_FAILED                : return "BIND_FAILED";
      case ZMQ_EVENT_ACCEPTED                   : return "ACCEPTED";
      case ZMQ_EVENT_ACCEPT_FAILED              : return "ACCEPT_FAILED";
      case ZMQ_EVENT_CLOSED                     : return "CLOSED";
      case ZMQ_EVENT_CLOSE_FAILED               : return "CLOSE_FAILED";
      case ZMQ_EVENT_DISCONNECTED               : return "DISCONNECTED";
      case ZMQ_EVENT_MONITOR_STOPPED            : return "MONITOR_STOPPED";
      case ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL : return "HANDSHAKE_FAILED_NO_DETAIL";
      case ZMQ_EVENT_HANDSHAKE_SUCCEEDED        : return "HANDSHAKE_SUCCEEDED";
      case ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL  : return "HANDSHAKE_FAILED_PROTOCOL";
      case ZMQ_EVENT_HANDSHAKE_FAILED_AUTH      : return "HANDSHAKE_FAILED_AUTH";
      default                                   : return "UNKNOWN";
   }
}

// Note: this code must be synchronized w/get_zmqEventMask
int get_zmqEventLogLevel(uint64_t event)
{
   switch(event) {
      case ZMQ_EVENT_CONNECTED                  : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_CONNECT_DELAYED            : return MAMA_LOG_LEVEL_FINER;
      case ZMQ_EVENT_CONNECT_RETRIED            : return MAMA_LOG_LEVEL_FINE;
      case ZMQ_EVENT_LISTENING                  : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_BIND_FAILED                : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_ACCEPTED                   : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_ACCEPT_FAILED              : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_CLOSED                     : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_CLOSE_FAILED               : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_DISCONNECTED               : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_MONITOR_STOPPED            : return MAMA_LOG_LEVEL_FINE;
      case ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_HANDSHAKE_SUCCEEDED        : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL  : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_HANDSHAKE_FAILED_AUTH      : return MAMA_LOG_LEVEL_ERROR;
      default                                   : return MAMA_LOG_LEVEL_ERROR;
   }
}


// Returns a bit-mask of events that are logged at specified log level.
// Ensures that we don't ask for more events than we care about
// Note: this code must be synchronized w/get_zmqEventLogLevel
uint64_t get_zmqEventMask(int logLevel)
{
   uint64_t eventMask = 0;

   switch(logLevel) {

      case MAMA_LOG_LEVEL_FINEST:

      case MAMA_LOG_LEVEL_FINER:
         eventMask |= ZMQ_EVENT_CONNECT_DELAYED;

      case MAMA_LOG_LEVEL_FINE:
         eventMask |= ZMQ_EVENT_CONNECT_RETRIED;
         eventMask |= ZMQ_EVENT_CLOSED;
         eventMask |= ZMQ_EVENT_MONITOR_STOPPED;

      case MAMA_LOG_LEVEL_NORMAL:
         eventMask |= ZMQ_EVENT_CONNECTED;
         eventMask |= ZMQ_EVENT_ACCEPTED;
         eventMask |= ZMQ_EVENT_DISCONNECTED;
         eventMask |= ZMQ_EVENT_HANDSHAKE_SUCCEEDED;
         eventMask |= ZMQ_EVENT_LISTENING;

      case MAMA_LOG_LEVEL_ERROR:
         eventMask |= ZMQ_EVENT_BIND_FAILED;
         eventMask |= ZMQ_EVENT_ACCEPT_FAILED;
         eventMask |= ZMQ_EVENT_CLOSE_FAILED;
         eventMask |= ZMQ_EVENT_HANDSHAKE_FAILED_NO_DETAIL;
         eventMask |= ZMQ_EVENT_HANDSHAKE_FAILED_PROTOCOL;
         eventMask |= ZMQ_EVENT_HANDSHAKE_FAILED_AUTH;
   }

   return eventMask;
}


MamaLogLevel getNamingLogLevel(const char mType)
{
   if (mType == 'c')
      return log_level_beacon;
   else
      return log_level_naming;
}


uint64_t getMillis(void)
{
    //  Use POSIX gettimeofday function to get precise time.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * (uint64_t) 1000) + (tv.tv_usec / 1000));
}
