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

namespace {

void pumpBoth(Session& host, Session& guest, int rounds = 120, int stepMs = 20) {
    for (int i = 0; i < rounds; ++i) {
        host.advance(milliseconds(stepMs));
        guest.advance(milliseconds(stepMs));
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

bool startThreePlayerRoyale(Session& host, Session& guest, net::GuestClient& raw) {
    if (!host.openRoyaleHostForTesting(0, 3)) {
        return false;
    }
    if (!guest.openRoyaleJoinForTesting("127.0.0.1", host.royalePort())) {
        return false;
    }
    std::string error;
    raw.connectTo("127.0.0.1", host.royalePort(), 1000, error);
    raw.sendLine(encodeLine(MessageType::Hello, {"ROYALE", "1"}));
    pumpBoth(host, guest);
    host.activateAction(0);
    pumpBoth(host, guest);
    return host.state() == SessionState::RoyalePlay;
}

}  // namespace

TEST(royale_guest_receives_seat_and_own_view) {
    Session host(31);
    Session guest(32);
    host.start();
    guest.start();
    net::GuestClient raw;
    CHECK(startThreePlayerRoyale(host, guest, raw));
    CHECK(guest.royaleSeat() >= 1);
    CHECK(guest.state() == SessionState::RoyalePlay);
    CHECK_EQ(guest.royaleView().playerCount, 3);
    CHECK_EQ(guest.royaleView().you, guest.royaleSeat());
    CHECK(host.royaleMatch() != nullptr);
    for (int i = 0; i < 3; ++i) {
        CHECK(guest.royaleView().own[static_cast<size_t>(i)] ==
              host.royaleMatch()->seat(guest.royaleSeat()).hand[static_cast<size_t>(i)]);
    }
    for (int seat = 0; seat < 3; ++seat) {
        if (seat != guest.royaleSeat()) {
            CHECK(!guest.royaleView().seats.at(static_cast<size_t>(seat)).revealed.has_value());
        }
    }
}

TEST(royale_guest_play_reaches_host_engine) {
    Session host(33);
    Session guest(34);
    host.start();
    guest.start();
    net::GuestClient raw;
    CHECK(startThreePlayerRoyale(host, guest, raw));

    host.selectCard(0);
    host.confirmSelection();
    CHECK(host.royaleMatch()->seat(0).played[0]);

    for (int i = 0; i < 120 && !(guest.royaleView().turn == guest.royaleSeat() &&
                                 guest.royaleView().phase == RoyalePhase::Playing);
         ++i) {
        pumpBoth(host, guest, 1);
    }
    CHECK_EQ(guest.royaleView().turn, guest.royaleSeat());
    guest.selectCard(0);
    guest.confirmSelection();
    pumpBoth(host, guest);
    CHECK(host.royaleMatch()->seat(guest.royaleSeat()).played[0]);
}

TEST(royale_guest_disconnect_eliminates_on_host) {
    Session host(35);
    Session guest(36);
    host.start();
    guest.start();
    net::GuestClient raw;
    CHECK(startThreePlayerRoyale(host, guest, raw));
    CHECK_EQ(host.royaleMatch()->aliveCount(), 3);

    raw.close();
    bool eliminated = false;
    for (int i = 0; i < 120 && !eliminated; ++i) {
        pumpBoth(host, guest, 1);
        eliminated = host.royaleMatch()->seat(2).eliminated;
    }
    CHECK(eliminated);
    CHECK(host.royaleMatch()->aliveCount() < 3);
}
