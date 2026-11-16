#include "net/room_server.hpp"

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
        const int incoming = acceptClient(listenFd_);
        if (incoming == -1) {
            break;
        }
        if (incoming < 0) {
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

}  // namespace net
}  // namespace truco
