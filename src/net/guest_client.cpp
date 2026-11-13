#include "net/guest_client.hpp"

namespace truco {
namespace net {

GuestClient::~GuestClient() {
    close();
}

bool GuestClient::connectTo(const std::string& ip, uint16_t port, int timeoutMs,
                            std::string& error) {
    close();
    fd_ = net::connectTcp(ip, port, timeoutMs, error);
    if (fd_ < 0) {
        return false;
    }
    disconnected_ = false;
    reader_ = LineReader{};
    pending_.clear();
    return true;
}

bool GuestClient::connected() const {
    return fd_ >= 0 && !disconnected_;
}

void GuestClient::sendLine(const std::string& line) {
    if (fd_ < 0 || disconnected_) {
        return;
    }
    bool closed = false;
    sendAll(fd_, line, pending_, closed);
    if (closed) {
        disconnected_ = true;
        closeFd(fd_);
    }
}

std::vector<std::string> GuestClient::readLines() {
    if (fd_ < 0) {
        return {};
    }
    bool closed = false;
    std::vector<std::string> lines = net::readLines(fd_, reader_, closed);
    if (closed) {
        disconnected_ = true;
        closeFd(fd_);
    }
    return lines;
}

void GuestClient::close() {
    closeFd(fd_);
    reader_ = LineReader{};
    pending_.clear();
}

}  // namespace net
}  // namespace truco
