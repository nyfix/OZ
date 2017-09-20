#define _GNU_SOURCE
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <wombat/wUuid.h>
#include <mama/log.h>

#include "util.h"

void wlock_noop(wLock lock ) {}


#define UUID_STRING_BUF_SIZE 37

const char* zmq_generate_uuid()
{
   wUuid tempUuid;
   wUuid_generate_time(tempUuid);
   char uuidStringBuffer[UUID_STRING_BUF_SIZE];
   wUuid_unparse(tempUuid, uuidStringBuffer);

   return strdup(uuidStringBuffer);
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
