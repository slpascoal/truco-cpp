#include "net/room_server.hpp"

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

RoomServer::~RoomServer() {
    shutdown();
}

bool RoomServer::listen(uint16_t port, std::string& error) {
    shutdown();
    listenFd_ = net::listenTcp(port, error);
    if (listenFd_ < 0) {
        return false;
    }
    port_ = boundPort(listenFd_);
    return true;
}

int RoomServer::acceptPending() {
    if (listenFd_ < 0) {
        return 0;
    }
    int accepted = 0;
    while (true) {
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        const int incoming =
            accept(listenFd_, reinterpret_cast<sockaddr*>(&address), &length);
        if (incoming < 0) {
            break;
        }
        if (!setNonBlocking(incoming)) {
            int broken = incoming;
            closeFd(broken);
            continue;
        }
        auto client = std::make_unique<Client>();
        client->fd = incoming;
        clients_.push_back(std::move(client));
        ++accepted;
    }
    return accepted;
}

int RoomServer::clientCount() const {
    return static_cast<int>(clients_.size());
}

bool RoomServer::validClient(int client) const {
    return client >= 0 && client < clientCount();
}

bool RoomServer::connected(int client) const {
    if (!validClient(client)) {
        return false;
    }
    const Client& slot = *clients_.at(static_cast<size_t>(client));
    return slot.fd >= 0 && !slot.disconnected;
}

std::vector<std::string> RoomServer::readLines(int client) {
    if (!connected(client)) {
        return {};
    }
    Client& slot = *clients_.at(static_cast<size_t>(client));
    bool closed = false;
    std::vector<std::string> lines = net::readLines(slot.fd, slot.reader, closed);
    if (closed) {
        slot.disconnected = true;
        closeFd(slot.fd);
    }
    return lines;
}

void RoomServer::sendLine(int client, const std::string& line) {
    if (!connected(client)) {
        return;
    }
    Client& slot = *clients_.at(static_cast<size_t>(client));
    bool closed = false;
    sendAll(slot.fd, line, slot.pending, closed);
    if (closed) {
        slot.disconnected = true;
        closeFd(slot.fd);
    }
}

void RoomServer::broadcast(const std::string& line) {
    for (int client = 0; client < clientCount(); ++client) {
        sendLine(client, line);
    }
}

std::vector<int> RoomServer::connectedClients() const {
    std::vector<int> seats;
    for (int client = 0; client < clientCount(); ++client) {
        if (connected(client)) {
            seats.push_back(client);
        }
    }
    return seats;
}

void RoomServer::dropClient(int client) {
    if (!validClient(client)) {
        return;
    }
    Client& slot = *clients_.at(static_cast<size_t>(client));
    closeFd(slot.fd);
    slot.reader = LineReader{};
    slot.pending.clear();
    slot.disconnected = true;
}

void RoomServer::shutdown() {
    for (std::unique_ptr<Client>& client : clients_) {
        closeFd(client->fd);
    }
    clients_.clear();
    closeFd(listenFd_);
    port_ = 0;
}

std::vector<std::string> localAddresses() {
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
