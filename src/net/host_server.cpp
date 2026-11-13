#include "net/host_server.hpp"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace truco {
namespace net {

HostServer::~HostServer() {
    shutdown();
}

bool HostServer::listen(uint16_t port, std::string& error) {
    shutdown();
    listenFd_ = net::listenTcp(port, error);
    if (listenFd_ < 0) {
        return false;
    }
    port_ = boundPort(listenFd_);
    return true;
}

bool HostServer::waiting() const {
    return listenFd_ >= 0 && clientFd_ < 0;
}

bool HostServer::acceptPending() {
    if (listenFd_ < 0) {
        return false;
    }
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    const int incoming =
        accept(listenFd_, reinterpret_cast<sockaddr*>(&address), &length);
    if (incoming < 0) {
        return false;
    }
    if (clientFd_ >= 0) {
        int refused = incoming;
        closeFd(refused);
        return false;
    }
    if (!setNonBlocking(incoming)) {
        int broken = incoming;
        closeFd(broken);
        return false;
    }
    clientFd_ = incoming;
    disconnected_ = false;
    reader_ = LineReader{};
    pending_.clear();
    return true;
}

bool HostServer::connected() const {
    return clientFd_ >= 0 && !disconnected_;
}

void HostServer::sendLine(const std::string& line) {
    if (clientFd_ < 0 || disconnected_) {
        return;
    }
    bool closed = false;
    sendAll(clientFd_, line, pending_, closed);
    if (closed) {
        disconnected_ = true;
        closeFd(clientFd_);
    }
}

std::vector<std::string> HostServer::readLines() {
    if (clientFd_ < 0) {
        return {};
    }
    bool closed = false;
    std::vector<std::string> lines = net::readLines(clientFd_, reader_, closed);
    if (closed) {
        disconnected_ = true;
        closeFd(clientFd_);
    }
    return lines;
}

void HostServer::dropClient() {
    closeFd(clientFd_);
    reader_ = LineReader{};
    pending_.clear();
}

void HostServer::shutdown() {
    dropClient();
    closeFd(listenFd_);
    port_ = 0;
    disconnected_ = false;
}

std::vector<std::string> HostServer::localAddresses() const {
    std::vector<std::string> addresses;
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) {
        addresses.push_back("127.0.0.1");
        return addresses;
    }
    for (ifaddrs* entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if ((entry->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        char buffer[INET_ADDRSTRLEN] = {0};
        const sockaddr_in* address = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
        if (inet_ntop(AF_INET, &address->sin_addr, buffer, sizeof(buffer)) != nullptr) {
            const std::string text(buffer);
            bool alreadyThere = false;
            for (const std::string& existing : addresses) {
                alreadyThere = alreadyThere || existing == text;
            }
            if (!alreadyThere) {
                addresses.push_back(text);
            }
        }
    }
    freeifaddrs(interfaces);
    if (addresses.empty()) {
        addresses.push_back("127.0.0.1");
    }
    return addresses;
}

}  // namespace net
}  // namespace truco
