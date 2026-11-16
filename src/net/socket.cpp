#include "net/socket.hpp"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace truco {
namespace net {
namespace {

#ifdef _WIN32

using SocketLength = int;
using IoResult = int;
constexpr int kSendFlags = 0;

// Winsock handles are kernel object indexes that fit in an int; the rest of the
// code keeps the POSIX convention of "negative means invalid".
int toFd(SOCKET handle) {
    return handle == INVALID_SOCKET ? -1 : static_cast<int>(handle);
}

SOCKET toSocket(int fd) {
    return static_cast<SOCKET>(fd);
}

void ensureSockets() {
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
}

void closeSocket(int fd) {
    closesocket(toSocket(fd));
}

bool wouldBlock() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}

bool interrupted() {
    return WSAGetLastError() == WSAEINTR;
}

bool connectInProgress() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}

void setListenOptions(int fd) {
    // SO_REUSEADDR on Windows lets a second process steal a bound port, so the
    // "porta indisponivel" check needs exclusive use instead.
    BOOL exclusive = TRUE;
    setsockopt(toSocket(fd), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
}

// Winsock reports a failed non-blocking connect through the except set.
bool waitSocket(int fd, bool forWrite, int timeoutMs) {
    fd_set primary;
    fd_set failed;
    FD_ZERO(&primary);
    FD_ZERO(&failed);
    FD_SET(toSocket(fd), &primary);
    FD_SET(toSocket(fd), &failed);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    return select(0, forWrite ? nullptr : &primary, forWrite ? &primary : nullptr, &failed,
                  &timeout) > 0;
}

int socketError(int fd) {
    int value = 0;
    int length = sizeof(value);
    if (getsockopt(toSocket(fd), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&value),
                   &length) != 0) {
        return -1;
    }
    return value;
}

#else

using SocketLength = socklen_t;
using IoResult = ssize_t;
constexpr int kSendFlags = MSG_NOSIGNAL;

int toFd(int handle) {
    return handle;
}

int toSocket(int fd) {
    return fd;
}

void ensureSockets() {}

void closeSocket(int fd) {
    ::close(fd);
}

bool wouldBlock() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool interrupted() {
    return errno == EINTR;
}

bool connectInProgress() {
    return errno == EINPROGRESS;
}

void setListenOptions(int fd) {
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

bool waitSocket(int fd, bool forWrite, int timeoutMs) {
    pollfd descriptor{};
    descriptor.fd = fd;
    descriptor.events = forWrite ? POLLOUT : POLLIN;
    return poll(&descriptor, 1, timeoutMs) > 0;
}

int socketError(int fd) {
    int value = 0;
    socklen_t length = sizeof(value);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &value, &length) != 0) {
        return -1;
    }
    return value;
}

#endif

void addUnique(std::vector<std::string>& addresses, const in_addr& address) {
    char buffer[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &address, buffer, sizeof(buffer)) == nullptr) {
        return;
    }
    const std::string text(buffer);
    for (const std::string& existing : addresses) {
        if (existing == text) {
            return;
        }
    }
    addresses.push_back(text);
}

}  // namespace

bool setNonBlocking(int fd) {
#ifdef _WIN32
    u_long enabled = 1;
    return ioctlsocket(toSocket(fd), FIONBIO, &enabled) == 0;
#else
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool waitReadable(int fd, int timeoutMs) {
    return fd >= 0 && waitSocket(fd, false, timeoutMs);
}

int listenTcp(uint16_t port, std::string& error) {
    ensureSockets();
    const int fd = toFd(socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) {
        error = "nao foi possivel criar o socket";
        return -1;
    }
    setListenOptions(fd);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(toSocket(fd), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = "porta indisponivel";
        closeSocket(fd);
        return -1;
    }
    if (listen(toSocket(fd), 4) != 0) {
        error = "nao foi possivel escutar na porta";
        closeSocket(fd);
        return -1;
    }
    if (!setNonBlocking(fd)) {
        error = "nao foi possivel configurar o socket";
        closeSocket(fd);
        return -1;
    }
    return fd;
}

uint16_t boundPort(int fd) {
    sockaddr_in address{};
    SocketLength length = sizeof(address);
    if (getsockname(toSocket(fd), reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }
    return ntohs(address.sin_port);
}

int connectTcp(const std::string& ip, uint16_t port, int timeoutMs, std::string& error) {
    ensureSockets();
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1) {
        error = "endereco IP invalido";
        return -1;
    }
    const int fd = toFd(socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) {
        error = "nao foi possivel criar o socket";
        return -1;
    }
    if (!setNonBlocking(fd)) {
        error = "nao foi possivel configurar o socket";
        closeSocket(fd);
        return -1;
    }
    const int result =
        connect(toSocket(fd), reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result != 0 && !connectInProgress()) {
        error = "nao foi possivel conectar";
        closeSocket(fd);
        return -1;
    }
    if (result != 0) {
        if (!waitSocket(fd, true, timeoutMs)) {
            error = "tempo esgotado ao conectar";
            closeSocket(fd);
            return -1;
        }
        if (socketError(fd) != 0) {
            error = "conexao recusada";
            closeSocket(fd);
            return -1;
        }
    }
    return fd;
}

int acceptClient(int listenFd) {
    sockaddr_in address{};
    SocketLength length = sizeof(address);
    int incoming =
        toFd(accept(toSocket(listenFd), reinterpret_cast<sockaddr*>(&address), &length));
    if (incoming < 0) {
        return -1;
    }
    if (!setNonBlocking(incoming)) {
        closeFd(incoming);
        return -2;
    }
    return incoming;
}

void closeFd(int& fd) {
    if (fd >= 0) {
        closeSocket(fd);
        fd = -1;
    }
}

std::vector<std::string> localAddresses() {
    ensureSockets();
    std::vector<std::string> addresses;
#ifdef _WIN32
    char name[256] = {0};
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    if (gethostname(name, sizeof(name) - 1) == 0 &&
        getaddrinfo(name, nullptr, &hints, &results) == 0) {
        for (addrinfo* entry = results; entry != nullptr; entry = entry->ai_next) {
            if (entry->ai_addr == nullptr || entry->ai_family != AF_INET) {
                continue;
            }
            const sockaddr_in* address = reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
            if ((ntohl(address->sin_addr.s_addr) >> 24) == 127) {
                continue;
            }
            addUnique(addresses, address->sin_addr);
        }
        freeaddrinfo(results);
    }
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        for (ifaddrs* entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
            if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((entry->ifa_flags & IFF_LOOPBACK) != 0) {
                continue;
            }
            addUnique(addresses, reinterpret_cast<const sockaddr_in*>(entry->ifa_addr)->sin_addr);
        }
        freeifaddrs(interfaces);
    }
#endif
    if (addresses.empty()) {
        addresses.push_back("127.0.0.1");
    }
    return addresses;
}

std::vector<std::string> readLines(int fd, LineReader& reader, bool& closed) {
    std::vector<std::string> lines;
    if (fd < 0) {
        return lines;
    }
    char incoming[1024];
    bool atEnd = false;
    for (;;) {
        const IoResult got = recv(toSocket(fd), incoming, sizeof(incoming), 0);
        if (got > 0) {
            reader.buffer.append(incoming, static_cast<size_t>(got));
            continue;
        }
        if (got == 0) {
            atEnd = true;
            break;
        }
        if (wouldBlock()) {
            break;
        }
        if (interrupted()) {
            continue;
        }
        atEnd = true;
        break;
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
    if (atEnd) {
        closed = true;
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
        const IoResult sent = send(toSocket(fd), pending.data(),
                                   static_cast<int>(pending.size()), kSendFlags);
        if (sent > 0) {
            pending.erase(0, static_cast<size_t>(sent));
            continue;
        }
        if (sent < 0 && (wouldBlock() || interrupted())) {
            return true;
        }
        closed = true;
        return false;
    }
    return true;
}

}  // namespace net
}  // namespace truco
