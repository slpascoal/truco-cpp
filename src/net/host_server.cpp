#include "net/host_server.hpp"

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
    const int incoming = acceptClient(listenFd_);
    if (incoming < 0) {
        return false;
    }
    if (clientFd_ >= 0) {
        int refused = incoming;
        closeFd(refused);
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
    return net::localAddresses();
}

}  // namespace net
}  // namespace truco
