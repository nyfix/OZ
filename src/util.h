
// locking
#include <wlock.h>
void wlock_noop(wLock lock );

// uuid
// dont forget to add one for trailing null
#define UUID_STRING_SIZE 36
const char* zmq_generate_uuid();

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



