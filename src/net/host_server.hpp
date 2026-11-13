#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/socket.hpp"

namespace truco {
namespace net {

class HostServer {
public:
    HostServer() = default;
    ~HostServer();

    HostServer(const HostServer&) = delete;
    HostServer& operator=(const HostServer&) = delete;

    bool listen(uint16_t port, std::string& error);
    bool waiting() const;
    bool acceptPending();
    bool connected() const;
    uint16_t port() const { return port_; }

    void sendLine(const std::string& line);
    std::vector<std::string> readLines();
    bool disconnected() const { return disconnected_; }

    void dropClient();
    void shutdown();

    std::vector<std::string> localAddresses() const;

private:
    int listenFd_ = -1;
    int clientFd_ = -1;
    uint16_t port_ = 0;
    bool disconnected_ = false;
    LineReader reader_;
    std::string pending_;
};

}  // namespace net
}  // namespace truco
