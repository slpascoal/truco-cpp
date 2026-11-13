#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace truco {
namespace net {

constexpr size_t kMaxWireLine = 512;

struct LineReader {
    std::string buffer;
    bool discarding = false;
};

// Blocking listen with SO_REUSEADDR and non-blocking client sockets.
// Passing port 0 asks the kernel for an ephemeral port; discover it via
// getsockname-based helper below.
int listenTcp(uint16_t port, std::string& error);
uint16_t boundPort(int fd);

int connectTcp(const std::string& ip, uint16_t port, int timeoutMs, std::string& error);

void closeFd(int& fd);

bool setNonBlocking(int fd);

// Appends available bytes, returns complete lines (without '\n'). Lines longer
// than kMaxWireLine are discarded up to the next newline. Sets closed on EOF or
// socket error.
std::vector<std::string> readLines(int fd, LineReader& reader, bool& closed);

// Sends data plus any pending remainder. On EAGAIN keeps the remainder in
// pending. Sets closed on hard error.
bool sendAll(int fd, const std::string& data, std::string& pending, bool& closed);

}  // namespace net
}  // namespace truco
