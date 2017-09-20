#include <wlock.h>

void mama_log_helper (MamaLogLevel level, const char* function, const char* file, int lineno, const char *format, ...);


const char* zmq_generate_uuid();

void wlock_noop(wLock lock );
