#include <chrono>
#include <thread>

#include <poll.h>
#include <unistd.h>

#include "net/host_server.hpp"
#include "net/socket.hpp"
#include "test_framework.hpp"

using namespace truco;
using namespace truco::net;

namespace {

bool waitReadable(int fd, int timeoutMs) {
    pollfd descriptor{};
    descriptor.fd = fd;
    descriptor.events = POLLIN;
    return poll(&descriptor, 1, timeoutMs) > 0;
}

bool readUntilLine(int fd, LineReader& reader, const std::string& line, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        waitReadable(fd, 20);
        bool closed = false;
        const auto lines = readLines(fd, reader, closed);
        for (const std::string& candidate : lines) {
            if (candidate == line) {
                return true;
            }
        }
        if (closed) {
            return false;
        }
    }
    return false;
}

}  // namespace

TEST(host_listens_on_ephemeral_port_and_accepts) {
    HostServer server;
    std::string error;
    CHECK(server.listen(0, error));
    CHECK(server.port() > 0);
    CHECK(server.waiting());
    int client = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(client >= 0);
    CHECK(server.acceptPending());
    CHECK(server.connected());
    CHECK(!server.waiting());
    closeFd(client);
}

TEST(host_sends_and_receives_lines) {
    HostServer server;
    std::string error;
    CHECK(server.listen(0, error));
    int client = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(client >= 0);
    CHECK(server.acceptPending());

    server.sendLine("WELCOME 1\n");
    LineReader clientReader;
    CHECK(readUntilLine(client, clientReader, "WELCOME 1", 1000));

    std::string pending;
    bool closed = false;
    sendAll(client, "HELLO TRUCO 1\n", pending, closed);
    CHECK(!closed);
    bool found = false;
    for (int i = 0; i < 100 && !found; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        for (const std::string& line : server.readLines()) {
            found = found || line == "HELLO TRUCO 1";
        }
    }
    CHECK(found);
    closeFd(client);
}

TEST(host_refuses_second_client) {
    HostServer server;
    std::string error;
    CHECK(server.listen(0, error));
    int first = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(first >= 0);
    CHECK(server.acceptPending());

    int second = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(second >= 0);
    CHECK(!server.acceptPending());
    CHECK(server.connected());

    bool refused = false;
    LineReader reader;
    for (int i = 0; i < 200 && !refused; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        bool closed = false;
        readLines(second, reader, closed);
        refused = closed;
    }
    CHECK(refused);
    closeFd(second);
    closeFd(first);
}

TEST(host_reports_busy_port) {
    HostServer first;
    std::string error;
    CHECK(first.listen(0, error));
    HostServer second;
    CHECK(!second.listen(first.port(), error));
    CHECK(!error.empty());
}

TEST(host_detects_peer_disconnect) {
    HostServer server;
    std::string error;
    CHECK(server.listen(0, error));
    int client = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(client >= 0);
    CHECK(server.acceptPending());
    closeFd(client);
    bool detected = false;
    for (int i = 0; i < 300 && !detected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        server.readLines();
        detected = server.disconnected();
    }
    CHECK(detected);
    CHECK(!server.connected());
}

TEST(host_discards_oversized_line_but_keeps_connection) {
    HostServer server;
    std::string error;
    CHECK(server.listen(0, error));
    int client = connectTcp("127.0.0.1", server.port(), 1000, error);
    CHECK(client >= 0);
    CHECK(server.acceptPending());

    std::string pending;
    bool closed = false;
    sendAll(client, std::string(600, 'A') + "\nPLAY 1\n", pending, closed);

    bool found = false;
    bool sawOversized = false;
    for (int i = 0; i < 200 && !found; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        for (const std::string& line : server.readLines()) {
            found = found || line == "PLAY 1";
            sawOversized = sawOversized || line.size() > kMaxWireLine;
        }
    }
    CHECK(found);
    CHECK(!sawOversized);
    CHECK(!server.disconnected());
    closeFd(client);
}

TEST(host_lists_local_addresses) {
    HostServer server;
    const std::vector<std::string> addresses = server.localAddresses();
    CHECK(!addresses.empty());
}
