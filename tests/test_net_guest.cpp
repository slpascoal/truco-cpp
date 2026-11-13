#include <chrono>
#include <thread>

#include "net/guest_client.hpp"
#include "net/host_server.hpp"
#include "net/protocol.hpp"
#include "test_framework.hpp"

using namespace truco;
using namespace truco::net;

namespace {

bool pumpUntilLine(GuestClient& guest, const std::string& wanted, int timeoutMs,
                   Message* captured = nullptr) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const std::string& line : guest.readLines()) {
            const Message message = parseLine(line);
            if (captured != nullptr) {
                *captured = message;
            }
            if (line == wanted || (captured != nullptr && message.type != MessageType::Unknown)) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool pumpHostLine(HostServer& host, const std::string& wanted, int timeoutMs,
                  Message* captured = nullptr) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const std::string& line : host.readLines()) {
            const Message message = parseLine(line);
            if (captured != nullptr) {
                *captured = message;
            }
            if (line == wanted || (captured != nullptr && message.type != MessageType::Unknown)) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

}  // namespace

TEST(guest_connects_to_host) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    GuestClient guest;
    CHECK(guest.connectTo("127.0.0.1", host.port(), 1000, error));
    CHECK(guest.connected());
    CHECK(host.acceptPending());
    CHECK(host.connected());
}

TEST(guest_rejects_invalid_ip) {
    GuestClient guest;
    std::string error;
    CHECK(!guest.connectTo("999.999.999.999", 4000, 200, error));
    CHECK(!error.empty());
    CHECK(!guest.connected());
}

TEST(guest_times_out_on_closed_port) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    const uint16_t port = host.port();
    host.shutdown();
    GuestClient guest;
    CHECK(!guest.connectTo("127.0.0.1", port, 300, error));
    CHECK(!error.empty());
}

TEST(guest_sends_and_receives) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    GuestClient guest;
    CHECK(guest.connectTo("127.0.0.1", host.port(), 1000, error));
    CHECK(host.acceptPending());

    guest.sendLine("HELLO TRUCO 1\n");
    Message received;
    CHECK(pumpHostLine(host, "HELLO TRUCO 1", 1000, &received));
    CHECK(received.type == MessageType::Hello);

    host.sendLine("WELCOME 1\n");
    Message reply;
    CHECK(pumpUntilLine(guest, "WELCOME 1", 1000, &reply));
    CHECK(reply.type == MessageType::Welcome);
}

TEST(handshake_accepts_matching_version) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    GuestClient guest;
    CHECK(guest.connectTo("127.0.0.1", host.port(), 1000, error));
    CHECK(host.acceptPending());

    guest.sendLine(encodeLine(MessageType::Hello, {"TRUCO", "1"}));
    Message hello;
    CHECK(pumpHostLine(host, "", 1000, &hello));
    CHECK(hello.type == MessageType::Hello);
    CHECK_EQ(hello.fields.at(1), std::string("1"));
    host.sendLine(encodeLine(MessageType::Welcome, {"1"}));
    Message welcome;
    CHECK(pumpUntilLine(guest, "", 1000, &welcome));
    CHECK(welcome.type == MessageType::Welcome);
}

TEST(handshake_rejects_mismatched_version) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    GuestClient guest;
    CHECK(guest.connectTo("127.0.0.1", host.port(), 1000, error));
    CHECK(host.acceptPending());

    guest.sendLine(encodeLine(MessageType::Hello, {"TRUCO", "999"}));
    Message hello;
    CHECK(pumpHostLine(host, "", 1000, &hello));
    CHECK(hello.type == MessageType::Hello);
    CHECK(hello.fields.at(1) != "1");
    host.sendLine(encodeLine(MessageType::Error, {"VERSION"}, "versao incompativel"));
    Message reply;
    CHECK(pumpUntilLine(guest, "", 1000, &reply));
    CHECK(reply.type == MessageType::Error);
    CHECK_EQ(reply.fields.at(0), std::string("VERSION"));
}

TEST(guest_detects_host_disconnect) {
    HostServer host;
    std::string error;
    CHECK(host.listen(0, error));
    GuestClient guest;
    CHECK(guest.connectTo("127.0.0.1", host.port(), 1000, error));
    CHECK(host.acceptPending());
    host.dropClient();
    bool detected = false;
    for (int i = 0; i < 300 && !detected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        guest.readLines();
        detected = guest.disconnected();
    }
    CHECK(detected);
    CHECK(!guest.connected());
}
