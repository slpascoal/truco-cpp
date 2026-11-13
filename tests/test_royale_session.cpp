#include <chrono>
#include <thread>

#include "app/session.hpp"
#include "net/guest_client.hpp"
#include "net/protocol.hpp"
#include "test_framework.hpp"

using namespace truco;
using std::chrono::milliseconds;

namespace {

void pumpSession(Session& host, int rounds = 120, int stepMs = 20) {
    for (int i = 0; i < rounds; ++i) {
        host.advance(milliseconds(stepMs));
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

void joinRoyale(net::GuestClient& guest, uint16_t port) {
    std::string error;
    guest.connectTo("127.0.0.1", port, 1000, error);
    guest.sendLine(encodeLine(MessageType::Hello, {"ROYALE", "1"}));
}

}  // namespace

TEST(royale_host_opens_lobby_and_starts_with_two_guests) {
    Session host(21);
    host.start();
    CHECK(host.openRoyaleHostForTesting(0, 4));
    CHECK(host.state() == SessionState::RoyaleLobby);

    net::GuestClient g1;
    net::GuestClient g2;
    joinRoyale(g1, host.royalePort());
    joinRoyale(g2, host.royalePort());
    pumpSession(host);

    host.activateAction(0);
    CHECK(host.state() == SessionState::RoyalePlay);
    CHECK(host.royaleMatch() != nullptr);
    CHECK_EQ(host.royaleMatch()->playerCount(), 3);
    CHECK_EQ(host.royaleMatch()->aliveCount(), 3);
}

TEST(royale_host_applies_guest_play_intent) {
    Session host(22);
    host.start();
    CHECK(host.openRoyaleHostForTesting(0, 3));
    net::GuestClient g1;
    net::GuestClient g2;
    joinRoyale(g1, host.royalePort());
    joinRoyale(g2, host.royalePort());
    pumpSession(host);
    host.activateAction(0);
    CHECK(host.state() == SessionState::RoyalePlay);

    CHECK_EQ(host.royaleMatch()->currentTurn(), 0);
    host.selectCard(0);
    host.confirmSelection();
    CHECK(host.royaleMatch()->seat(0).played[0]);

    CHECK_EQ(host.royaleMatch()->currentTurn(), 1);
    g1.sendLine(encodeLine(MessageType::Play, {"1"}));
    pumpSession(host);
    CHECK(host.royaleMatch()->seat(1).played[0]);
}
