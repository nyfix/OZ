#ifndef OPENMAMA_ZMQ_UTIL_H
#define OPENMAMA_ZMQ_UTIL_H

// call a function that returns mama_status -- log an error and return if not MAMA_STATUS_OK
#define CALL_MAMA_FUNC(x)                                                                  \
   do {                                                                                    \
      mama_status s = (x);                                                                 \
      if (s != MAMA_STATUS_OK) {                                                           \
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", s, mamaStatus_stringForStatus(s)); \
         return s;                                                                         \
      }                                                                                    \
   } while(0)

// call a function that returns an int -- log an error and return if < 0
#define CALL_ZMQ_FUNC(x)                                                             \
   do {                                                                              \
      int rc = (x);                                                                  \
      if (rc < 0) {                                                                  \
         MAMA_LOG(MAMA_LOG_LEVEL_ERROR, "Error %d(%s)", errno, zmq_strerror(errno)); \
         return MAMA_STATUS_PLATFORM;                                                \
      }                                                                              \
   } while(0)



#define UUID_STRING_SIZE 36
const char* zmqBridge_generateUuid();
const char* zmqBridge_generateSerial(long long* id);

// logging
// NOTE: following code uses (deprecated) gMamaLogLevel directly, rather than calling mama_getLogLevel
// (which acquires a read lock on gMamaLogLevel)
#define MAMA_LOG(l, ...)                                                      \
   do {                                                                       \
      if (gMamaLogLevel >= l) {                                               \
         mama_log_helper(l, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__); \
      }                                                                       \
   } while(0)

void mama_log_helper (MamaLogLevel level, const char* function, const char* file, int lineno, const char *format, ...);

const char* get_zmqEventName(int event);
int get_zmqEventLogLevel(int event);
int get_zmqEventMask(int logLevel);

#endif