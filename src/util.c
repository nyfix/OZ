#include <string.h>
#include <wombat/wUuid.h>

#define UUID_STRING_BUF_SIZE 37

const char* zmq_generate_uuid()
{
    wUuid tempUuid;
    wUuid_generate_time(tempUuid);
    char uuidStringBuffer[UUID_STRING_BUF_SIZE];
    wUuid_unparse(tempUuid, uuidStringBuffer);

    return strdup(uuidStringBuffer);
}
