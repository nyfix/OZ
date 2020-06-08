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
const char* zmqBridge_generateSerial(unsigned long long* id);

#define MAMA_LOG(l, ...)    mama_log(l, ##__VA_ARGS__)

const char* get_zmqEventName(uint64_t event);
int get_zmqEventLogLevel(uint64_t event);
uint64_t get_zmqEventMask(int logLevel);


MamaLogLevel getNamingLogLevel(const char mType);

uint64_t getMillis(void);

#endif