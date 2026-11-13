#include "net/room_server.hpp"

#include <chrono>
#include <thread>

#include "net/guest_client.hpp"
#include "test_framework.hpp"

using namespace truco;
using namespace truco::net;
using std::chrono::milliseconds;

namespace {

bool waitForLines(RoomServer& server, int client, int count, std::vector<std::string>& out,
                  int timeoutMs = 1500) {
    for (int waited = 0; waited < timeoutMs; waited += 10) {
        std::vector<std::string> lines = server.readLines(client);
        if (!lines.empty()) {
            out.insert(out.end(), lines.begin(), lines.end());
            if (static_cast<int>(out.size()) >= count) {
                return true;
            }
        }
        std::this_thread::sleep_for(milliseconds(10));
    }
    return static_cast<int>(out.size()) >= count;
}

bool waitForLine(GuestClient& guest, const std::string& expected, int timeoutMs = 1500) {
    for (int waited = 0; waited < timeoutMs; waited += 10) {
        for (const std::string& line : guest.readLines()) {
            if (line == expected) {
                return true;
            }
        }
        std::this_thread::sleep_for(milliseconds(10));
    }
    return false;
}

}  // namespace

TEST(room_server_accepts_multiple_clients_and_routes_lines) {
    RoomServer server;
    std::string error;
    CHECK(server.listen(0, error));
    CHECK(server.clientCount() == 0);

    GuestClient a;
    GuestClient b;
    GuestClient c;
    CHECK(a.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(b.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(c.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK_EQ(server.acceptPending(), 3);
    CHECK_EQ(server.clientCount(), 3);
    CHECK(server.connected(0));
    CHECK(server.connected(1));
    CHECK(server.connected(2));

    a.sendLine("A1\n");
    b.sendLine("B1\n");
    c.sendLine("C1\n");
    std::vector<std::string> linesA;
    std::vector<std::string> linesB;
    std::vector<std::string> linesC;
    CHECK(waitForLines(server, 0, 1, linesA));
    CHECK(waitForLines(server, 1, 1, linesB));
    CHECK(waitForLines(server, 2, 1, linesC));
    CHECK_EQ(linesA.at(0), std::string("A1"));
    CHECK_EQ(linesB.at(0), std::string("B1"));
    CHECK_EQ(linesC.at(0), std::string("C1"));

    server.sendLine(1, "TO-B\n");
    CHECK(waitForLine(b, "TO-B"));
}

TEST(room_server_broadcast_reaches_every_client) {
    RoomServer server;
    std::string error;
    CHECK(server.listen(0, error));
    GuestClient a;
    GuestClient b;
    GuestClient c;
    CHECK(a.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(b.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(c.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK_EQ(server.acceptPending(), 3);

    server.broadcast("STATE\n");
    CHECK(waitForLine(a, "STATE"));
    CHECK(waitForLine(b, "STATE"));
    CHECK(waitForLine(c, "STATE"));
}

TEST(room_server_detects_disconnect_and_keeps_seats) {
    RoomServer server;
    std::string error;
    CHECK(server.listen(0, error));
    GuestClient a;
    GuestClient b;
    GuestClient c;
    CHECK(a.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(b.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK(c.connectTo("127.0.0.1", server.port(), 1000, error));
    CHECK_EQ(server.acceptPending(), 3);

    b.close();
    for (int waited = 0; waited < 1500 && server.connected(1); waited += 10) {
        server.readLines(1);
        std::this_thread::sleep_for(milliseconds(10));
    }
    CHECK(!server.connected(1));
    CHECK(server.connected(0));
    CHECK(server.connected(2));
    const std::vector<int> connected = server.connectedClients();
    CHECK_EQ(connected.size(), size_t{2});
    CHECK_EQ(connected.at(0), 0);
    CHECK_EQ(connected.at(1), 2);
}
