#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/socket.hpp"

namespace truco {
namespace net {

// Multi-client TCP room used by the battle royale host. Seats are stable:
// a client keeps its index even after disconnecting.
class RoomServer {
public:
    RoomServer() = default;
    ~RoomServer();

    RoomServer(const RoomServer&) = delete;
    RoomServer& operator=(const RoomServer&) = delete;

    bool listen(uint16_t port, std::string& error);
    int acceptPending();
    int clientCount() const;
    bool connected(int client) const;
    uint16_t port() const { return port_; }

    std::vector<std::string> readLines(int client);
    void sendLine(int client, const std::string& line);
    void broadcast(const std::string& line);
    std::vector<int> connectedClients() const;
    void dropClient(int client);
    void shutdown();

private:
    struct Client {
        int fd = -1;
        bool disconnected = false;
        LineReader reader;
        std::string pending;
    };

    bool validClient(int client) const;

    int listenFd_ = -1;
    uint16_t port_ = 0;
    std::vector<std::unique_ptr<Client>> clients_;
};

}  // namespace net
}  // namespace truco
