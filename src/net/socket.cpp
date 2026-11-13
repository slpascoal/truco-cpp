#include "net/socket.hpp"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace truco {
namespace net {
namespace {

}  // namespace

bool setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

int listenTcp(uint16_t port, std::string& error) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = "nao foi possivel criar o socket";
        return -1;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = "porta indisponivel";
        close(fd);
        return -1;
    }
    if (listen(fd, 4) != 0) {
        error = "nao foi possivel escutar na porta";
        close(fd);
        return -1;
    }
    if (!setNonBlocking(fd)) {
        error = "nao foi possivel configurar o socket";
        close(fd);
        return -1;
    }
    return fd;
}

uint16_t boundPort(int fd) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }
    return ntohs(address.sin_port);
}

int connectTcp(const std::string& ip, uint16_t port, int timeoutMs, std::string& error) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1) {
        error = "endereco IP invalido";
        return -1;
    }
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = "nao foi possivel criar o socket";
        return -1;
    }
    if (!setNonBlocking(fd)) {
        error = "nao foi possivel configurar o socket";
        close(fd);
        return -1;
    }
    const int result = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result != 0 && errno != EINPROGRESS) {
        error = "nao foi possivel conectar";
        close(fd);
        return -1;
    }
    if (result != 0) {
        pollfd descriptor{};
        descriptor.fd = fd;
        descriptor.events = POLLOUT;
        const int ready = poll(&descriptor, 1, timeoutMs);
        if (ready <= 0) {
            error = "tempo esgotado ao conectar";
            close(fd);
            return -1;
        }
        int socketError = 0;
        socklen_t length = sizeof(socketError);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &length) != 0 ||
            socketError != 0) {
            error = "conexao recusada";
            close(fd);
            return -1;
        }
    }
    return fd;
}

void closeFd(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

std::vector<std::string> readLines(int fd, LineReader& reader, bool& closed) {
    std::vector<std::string> lines;
    if (fd < 0) {
        return lines;
    }
    char incoming[1024];
    for (;;) {
        const ssize_t got = recv(fd, incoming, sizeof(incoming), 0);
        if (got > 0) {
            reader.buffer.append(incoming, static_cast<size_t>(got));
            continue;
        }
        if (got == 0) {
            closed = true;
            return lines;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        closed = true;
        return lines;
    }

    for (;;) {
        const size_t newline = reader.buffer.find('\n');
        if (newline == std::string::npos) {
            if (reader.buffer.size() > kMaxWireLine) {
                reader.discarding = true;
                reader.buffer.clear();
            }
            break;
        }
        std::string line = reader.buffer.substr(0, newline);
        reader.buffer.erase(0, newline + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (reader.discarding) {
            reader.discarding = false;
            continue;
        }
        if (line.size() > kMaxWireLine) {
            continue;
        }
        lines.push_back(line);
    }
    return lines;
}

bool sendAll(int fd, const std::string& data, std::string& pending, bool& closed) {
    if (fd < 0) {
        closed = true;
        return false;
    }
    pending += data;
    while (!pending.empty()) {
        const ssize_t sent =
            send(fd, pending.data(), pending.size(), MSG_NOSIGNAL);
        if (sent > 0) {
            pending.erase(0, static_cast<size_t>(sent));
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            return true;
        }
        closed = true;
        return false;
    }
    return true;
}

}  // namespace net
}  // namespace truco
