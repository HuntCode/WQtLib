#include "ESUtils.h"

#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace hhcast {

uint32_t IPToStreamID(const std::string& ipAddress)
{
    struct in_addr addr;

    if (inet_pton(AF_INET, ipAddress.c_str(), &addr) == 1) {
        return ntohl(addr.s_addr);
    }

    return 0;
}

} // namespace hhcast