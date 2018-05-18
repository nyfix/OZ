#define _GNU_SOURCE
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <wombat/wUuid.h>
#include <mama/log.h>

#include <zmq.h>

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

const char* zmqBridge_generateUid(long long* id)
{
   long long next = __sync_add_and_fetch(id, 1);
   char temp[16+1];
   sprintf(temp, "%016llx", next);
   return strdup(temp);
}


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
