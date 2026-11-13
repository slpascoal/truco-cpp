#include <chrono>
#include <string>
#include <thread>

#include "app/session.hpp"
#include "net/guest_client.hpp"
#include "net/protocol.hpp"
#include "test_framework.hpp"

using namespace truco;
using std::chrono::milliseconds;

namespace {

struct RoyaleFixture {
    Session host{41};
    net::GuestClient g1;
    net::GuestClient g2;
    uint16_t port = 0;

    bool start() {
        host.start();
        if (!host.openRoyaleHostForTesting(0, 3)) {
            return false;
        }
        port = host.royalePort();
        std::string error;
        if (!g1.connectTo("127.0.0.1", port, 1000, error)) {
            return false;
        }
        g1.sendLine(encodeLine(MessageType::Hello, {"ROYALE", "1"}));
        if (!g2.connectTo("127.0.0.1", port, 1000, error)) {
            return false;
        }
        g2.sendLine(encodeLine(MessageType::Hello, {"ROYALE", "1"}));
        pump(120);
        host.activateAction(0);
        pump(20);
        return host.state() == SessionState::RoyalePlay;
    }

    void pump(int rounds, int stepMs = 20) {
        for (int i = 0; i < rounds; ++i) {
            host.advance(milliseconds(stepMs));
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    net::GuestClient& guestFor(int seat) {
        return seat == 1 ? g1 : g2;
    }
};

int firstUnplayed(const RoyaleMatch& match, int seat) {
    for (int i = 0; i < 3; ++i) {
        if (!match.seat(seat).played[static_cast<size_t>(i)]) {
            return i;
        }
    }
    return -1;
}

}  // namespace

TEST(royale_integration_three_players_complete_a_hand) {
    RoyaleFixture fixture;
    CHECK(fixture.start());
    const RoyaleMatch* match = fixture.host.royaleMatch();
    CHECK(match != nullptr);
    CHECK_EQ(match->playerCount(), 3);

    int guard = 0;
    while (match->phase() == RoyalePhase::Playing && guard++ < 120) {
        const int turn = match->currentTurn();
        if (turn < 0) {
            break;
        }
        const int card = firstUnplayed(*match, turn);
        if (card < 0) {
            break;
        }
        if (turn == 0) {
            fixture.host.selectCard(card);
            fixture.host.confirmSelection();
        } else {
            fixture.guestFor(turn).sendLine(
                encodeLine(MessageType::Play, {std::to_string(card + 1)}));
        }
        fixture.pump(3);
    }

    CHECK(match->phase() == RoyalePhase::HandEnd ||
          match->phase() == RoyalePhase::MatchEnd);
    int totalPoints = 0;
    int winners = 0;
    for (int seat = 0; seat < 3; ++seat) {
        totalPoints += match->seat(seat).points;
        if (match->seat(seat).points == 12) {
            ++winners;
        }
    }
    CHECK_EQ(totalPoints, 34);
    CHECK_EQ(winners, 1);
}

TEST(royale_integration_next_hand_starts_after_reveal) {
    RoyaleFixture fixture;
    CHECK(fixture.start());
    const RoyaleMatch* match = fixture.host.royaleMatch();
    int guard = 0;
    while (match->phase() == RoyalePhase::Playing && guard++ < 120) {
        const int turn = match->currentTurn();
        const int card = firstUnplayed(*match, turn);
        if (card < 0) {
            break;
        }
        if (turn == 0) {
            fixture.host.selectCard(card);
            fixture.host.confirmSelection();
        } else {
            fixture.guestFor(turn).sendLine(
                encodeLine(MessageType::Play, {std::to_string(card + 1)}));
        }
        fixture.pump(3);
    }
    CHECK(match->phase() == RoyalePhase::HandEnd);
    fixture.pump(90, 20);
    CHECK(match->phase() == RoyalePhase::Playing);
    CHECK_EQ(match->tricksPlayed(), 0);
}

TEST(royale_integration_disconnects_end_the_match) {
    RoyaleFixture fixture;
    CHECK(fixture.start());
    fixture.g1.close();
    fixture.g2.close();
    bool over = false;
    for (int i = 0; i < 200 && !over; ++i) {
        fixture.pump(1);
        over = fixture.host.state() == SessionState::RoyaleResult;
    }
    CHECK(over);
    CHECK(fixture.host.royaleMatch() != nullptr);
    CHECK(fixture.host.royaleMatch()->matchOver());
    CHECK_EQ(fixture.host.royaleMatch()->matchWinner(), 0);
}
