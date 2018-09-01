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
   // this appears to be the "most" unique of all the uuid_generate functions
   // (it better be ;-)
   wUuid_generate(tempUuid);
   char uuidStringBuffer[UUID_STRING_SIZE+ 1];
   wUuid_unparse(tempUuid, uuidStringBuffer);

   return strdup(uuidStringBuffer);
}


const char* zmqBridge_generateSerial(long long* id)
{
   long long next = __sync_add_and_fetch(id, 1);
   char temp[ZMQ_REPLYHANDLE_INBOXNAME_SIZE +1];
   sprintf(temp, "%016llx", next);
   return strdup(temp);
}

#ifndef  USE_MAMA_LOG
#define MAX_LOG_MSG_SIZE 1024
// TODO: figure out a way to do this without needing to call printf twice
void mama_log_helper (MamaLogLevel level, const char* function, const char* file, int lineno, const char *format, ...)
{
   char temp[MAX_LOG_MSG_SIZE +1] = "";
   if (format) {
      va_list ap;
      va_start(ap, format);
      // NOTE: vsnprintf behaves differently on Windows & Linux, but following construct should work for both
      if (vsnprintf(temp, MAX_LOG_MSG_SIZE, format, ap) == -1)
         temp[MAX_LOG_MSG_SIZE] = '\0';
      va_end(ap);
   }

   // TODO: is there a better way than calling basename?
   mama_log(level, "%s:%s (%s:%d)", function, temp, basename(file), lineno);
}
#endif

const char* get_zmqEventName(int event)
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

int get_zmqEventLogLevel(int event)
{
   switch(event) {
      case ZMQ_EVENT_CONNECTED                  : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_CONNECT_DELAYED            : return MAMA_LOG_LEVEL_FINEST;
      case ZMQ_EVENT_CONNECT_RETRIED            : return MAMA_LOG_LEVEL_FINEST;
      case ZMQ_EVENT_LISTENING                  : return MAMA_LOG_LEVEL_FINER;
      case ZMQ_EVENT_BIND_FAILED                : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_ACCEPTED                   : return MAMA_LOG_LEVEL_NORMAL;
      case ZMQ_EVENT_ACCEPT_FAILED              : return MAMA_LOG_LEVEL_ERROR;
      case ZMQ_EVENT_CLOSED                     : return MAMA_LOG_LEVEL_FINE;
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
int get_zmqEventMask(int logLevel)
{
   int eventMask = 0;

   switch(logLevel) {

      case MAMA_LOG_LEVEL_FINEST:
         eventMask |= ZMQ_EVENT_CONNECT_DELAYED;
         eventMask |= ZMQ_EVENT_CONNECT_RETRIED;

      case MAMA_LOG_LEVEL_FINER:
         eventMask |= ZMQ_EVENT_LISTENING;

      case MAMA_LOG_LEVEL_FINE:
         eventMask |= ZMQ_EVENT_CLOSED;
         eventMask |= ZMQ_EVENT_MONITOR_STOPPED;

      case MAMA_LOG_LEVEL_NORMAL:
         eventMask |= ZMQ_EVENT_CONNECTED;
         eventMask |= ZMQ_EVENT_ACCEPTED;
         eventMask |= ZMQ_EVENT_DISCONNECTED;
         eventMask |= ZMQ_EVENT_HANDSHAKE_SUCCEEDED;

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

#define MAMA_LOG_LEVEL_BEACON MAMA_LOG_LEVEL_FINEST
//#define MAMA_LOG_LEVEL_BEACON MAMA_LOG_LEVEL_NORMAL

MamaLogLevel getNamingLogLevel(const char mType)
{
   if (mType == 'c')
      return MAMA_LOG_LEVEL_BEACON;
   else
      return MAMA_LOG_LEVEL_NORMAL;
}


uint64_t getMillis(void)
{
    //  Use POSIX gettimeofday function to get precise time.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * (uint64_t) 1000) + (tv.tv_usec / 1000));
}