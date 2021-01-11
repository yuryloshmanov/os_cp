#include <netdb.h>
#include <unistd.h>
#include <stdexcept>
#include <arpa/inet.h>


#include "../networking.hpp"


auto getIP() -> std::string {
    char buffer[256];
    char *ipBuffer;
    struct hostent *host_entry;

    if (gethostname(buffer, sizeof buffer) == -1) {
        throw std::runtime_error("can't get hostname");
    }

    host_entry = gethostbyname(buffer);
    if (!host_entry) {
        throw std::runtime_error("can't get host");
    }

    ipBuffer = inet_ntoa(*((struct in_addr *) host_entry->h_addr_list[0]));
    if (!ipBuffer) {
        throw std::runtime_error("can't get ip");
    }

    std::string result(ipBuffer);
    return result;
}
