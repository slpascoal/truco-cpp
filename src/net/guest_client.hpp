#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/socket.hpp"

namespace truco {
namespace net {

class GuestClient {
public:
    GuestClient() = default;
    ~GuestClient();

    GuestClient(const GuestClient&) = delete;
    GuestClient& operator=(const GuestClient&) = delete;

    bool connectTo(const std::string& ip, uint16_t port, int timeoutMs, std::string& error);
    bool connected() const;
    void sendLine(const std::string& line);
    std::vector<std::string> readLines();
    bool disconnected() const { return disconnected_; }
    void close();

private:
    int fd_ = -1;
    bool disconnected_ = false;
    LineReader reader_;
    std::string pending_;
};

}  // namespace net
}  // namespace truco
