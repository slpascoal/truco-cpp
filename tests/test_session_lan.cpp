#include <chrono>
#include <string>
#include <thread>

#include "net/guest_client.hpp"
#include "net/protocol.hpp"
#include "app/session.hpp"
#include "test_framework.hpp"

using namespace truco;
using std::chrono::milliseconds;

namespace {

struct LanPair {
    Session host{101};
    Session guest{202};
    bool started = false;

    bool start() {
        host.start();
        guest.start();
        if (!host.openLanHostForTesting(0)) {
            return false;
        }
        const uint16_t port = host.lanPort();
        if (port == 0) {
            return false;
        }
        if (!guest.openLanConnectForTesting("127.0.0.1", port)) {
            return false;
        }
        started = true;
        return true;
    }

    void pump(int rounds = 40, int stepMs = 40) {
        for (int i = 0; i < rounds; ++i) {
            host.advance(milliseconds(stepMs));
            guest.advance(milliseconds(stepMs));
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    bool pumpUntil(bool (*predicate)(const Session&, const Session&), int rounds = 300) {
        for (int i = 0; i < rounds; ++i) {
            pump(1);
            if (predicate(host, guest)) {
                return true;
            }
        }
        return false;
    }
};

bool hostTurnPlays(const Session& host, const Session&) {
    return host.state() == SessionState::HumanTurn;
}

bool guestTurnPlays(const Session& host, const Session& guest) {
    return host.state() == SessionState::CpuTurn &&
           guest.guestSync().phase == NetPhase::Play &&
           guest.guestSync().turn == Side::Cpu;
}

int firstUnplayed(const Session& session, Side side) {
    for (int i = 0; i < 3; ++i) {
        if (!session.game().cardPlayed(side, i)) {
            return i;
        }
    }
    return -1;
}

}  // namespace

TEST(lan_host_waiting_screen_shows_address_and_port) {
    Session host(1);
    host.start();
    CHECK(host.openLanHostForTesting(0));
    CHECK(host.state() == SessionState::LanWaiting);
    CHECK(host.view().screen == ViewModel::Screen::LanWaiting);
    CHECK(host.view().lanAddressText.find("Porta:") != std::string::npos);
    CHECK_EQ(host.view().actions.size(), size_t{1});
}

TEST(lan_config_rejects_invalid_port) {
    Session session(2);
    session.start();
    session.activateAction(2);
    CHECK(session.state() == SessionState::LanConfig);
    for (int i = 0; i < 6; ++i) {
        InputEvent backspace;
        backspace.kind = InputEvent::Kind::Key;
        backspace.key = 127;
        session.handleEvent(backspace);
    }
    InputEvent zero;
    zero.kind = InputEvent::Kind::Key;
    zero.key = '0';
    session.handleEvent(zero);
    InputEvent enter;
    enter.kind = InputEvent::Kind::Key;
    enter.key = '\r';
    session.handleEvent(enter);
    CHECK(session.state() == SessionState::LanConfig);
    CHECK(session.view().message.find("Porta invalida") != std::string::npos);
}

TEST(lan_config_rejects_invalid_ip) {
    Session session(3);
    session.start();
    session.activateAction(3);
    CHECK(session.state() == SessionState::LanConfig);
    CHECK_EQ(session.view().actions.size(), size_t{2});
    const std::string ip = "999.999.999.999";
    for (char ch : ip) {
        InputEvent key;
        key.kind = InputEvent::Kind::Key;
        key.key = ch;
        session.handleEvent(key);
    }
    InputEvent enter;
    enter.kind = InputEvent::Kind::Key;
    enter.key = '\r';
    session.handleEvent(enter);
    CHECK(session.state() == SessionState::LanConfig);
    CHECK(session.view().message.find("Erro") != std::string::npos);
}

TEST(lan_handshake_starts_match_on_both_sides) {
    LanPair pair;
    CHECK(pair.start());
    CHECK(pair.pumpUntil(hostTurnPlays));
    CHECK(pair.host.state() == SessionState::HumanTurn);
    CHECK(pair.guest.guestSync().phase == NetPhase::Play);
    CHECK(!pair.guest.guestSync().revealed);
    CHECK_EQ(pair.host.game().score().human, 0);
    CHECK_EQ(pair.host.game().score().cpu, 0);
}

TEST(lan_guest_sees_only_own_hand_until_reveal) {
    LanPair pair;
    CHECK(pair.start());
    CHECK(pair.pumpUntil(hostTurnPlays));
    CHECK(!pair.guest.guestSync().revealed);
    CHECK(!pair.guest.view().revealCpu);
    for (int i = 0; i < 3; ++i) {
        CHECK(pair.guest.view().humanCards[static_cast<size_t>(i)] ==
              pair.guest.guestSync().own[static_cast<size_t>(i)]);
    }
}

TEST(lan_guest_plays_its_turn_and_host_applies) {
    LanPair pair;
    CHECK(pair.start());
    CHECK(pair.pumpUntil(hostTurnPlays));
    pair.host.selectCard(0);
    pair.host.confirmSelection();
    CHECK(pair.pumpUntil(guestTurnPlays));
    pair.guest.selectCard(1);
    pair.guest.confirmSelection();
    pair.pump(10);
    CHECK(pair.host.game().cardPlayed(Side::Cpu, 1));
}

TEST(lan_host_rejects_out_of_turn_card) {
    Session host(4);
    host.start();
    CHECK(host.openLanHostForTesting(0));
    net::GuestClient raw;
    std::string error;
    CHECK(raw.connectTo("127.0.0.1", host.lanPort(), 1000, error));
    raw.sendLine(encodeLine(MessageType::Hello, {"TRUCO", "2"}));
    for (int i = 0; i < 120 && host.state() != SessionState::HumanTurn; ++i) {
        host.advance(milliseconds(30));
    }
    CHECK(host.state() == SessionState::HumanTurn);
    raw.sendLine(encodeLine(MessageType::Play, {"2"}));
    for (int i = 0; i < 20; ++i) {
        host.advance(milliseconds(30));
    }
    CHECK(!host.game().cardPlayed(Side::Cpu, 0));
    CHECK(host.state() == SessionState::HumanTurn);
}

TEST(lan_version_mismatch_is_rejected) {
    Session host(5);
    host.start();
    CHECK(host.openLanHostForTesting(0));
    net::GuestClient raw;
    std::string error;
    CHECK(raw.connectTo("127.0.0.1", host.lanPort(), 1000, error));
    raw.sendLine(encodeLine(MessageType::Hello, {"TRUCO", "999"}));
    bool sawError = false;
    for (int i = 0; i < 150 && !sawError; ++i) {
        host.advance(milliseconds(20));
        for (const std::string& line : raw.readLines()) {
            const Message message = parseLine(line);
            sawError = message.type == MessageType::Error &&
                       !message.fields.empty() && message.fields[0] == "VERSION";
        }
    }
    CHECK(sawError);
    CHECK(host.state() == SessionState::LanWaiting);
}

TEST(lan_disconnect_moves_host_to_error_screen) {
    Session host(6);
    host.start();
    CHECK(host.openLanHostForTesting(0));
    net::GuestClient raw;
    std::string error;
    CHECK(raw.connectTo("127.0.0.1", host.lanPort(), 1000, error));
    raw.sendLine(encodeLine(MessageType::Hello, {"TRUCO", "2"}));
    for (int i = 0; i < 150 && host.state() != SessionState::HumanTurn; ++i) {
        host.advance(milliseconds(30));
    }
    raw.close();
    for (int i = 0; i < 200 && host.state() != SessionState::LanError; ++i) {
        host.advance(milliseconds(30));
    }
    CHECK(host.state() == SessionState::LanError);
    CHECK(host.view().message.find("Conexao") != std::string::npos);
    CHECK_EQ(host.view().actions.size(), size_t{1});
    host.activateAction(0);
    CHECK(host.state() == SessionState::Menu);
}

TEST(lan_guest_truco_intent_reaches_host_dialog) {
    LanPair pair;
    CHECK(pair.start());
    CHECK(pair.pumpUntil(hostTurnPlays));
    pair.host.selectCard(0);
    pair.host.confirmSelection();
    CHECK(pair.pumpUntil(guestTurnPlays));
    pair.guest.activateAction(1);
    bool hostDialog = false;
    for (int i = 0; i < 200 && !hostDialog; ++i) {
        pair.pump(1);
        hostDialog = pair.host.state() == SessionState::TrucoDialog &&
                     !pair.host.view().actions.empty();
    }
    CHECK(hostDialog);
    pair.host.activateAction(0);
    pair.pump(10);
    CHECK_EQ(static_cast<int>(pair.host.game().stake()), 3);
}

TEST(lan_guest_cannot_raise_after_own_truco_is_accepted) {
    LanPair pair;
    CHECK(pair.start());
    CHECK(pair.pumpUntil(hostTurnPlays));
    pair.host.selectCard(0);
    pair.host.confirmSelection();
    CHECK(pair.pumpUntil(guestTurnPlays));
    pair.guest.activateAction(1);
    bool hostDialog = false;
    for (int i = 0; i < 200 && !hostDialog; ++i) {
        pair.pump(1);
        hostDialog = pair.host.state() == SessionState::TrucoDialog &&
                     !pair.host.view().actions.empty();
    }
    CHECK(hostDialog);
    pair.host.activateAction(0);
    bool guestPlaying = false;
    for (int i = 0; i < 200 && !guestPlaying; ++i) {
        pair.pump(1);
        guestPlaying = pair.guest.state() == SessionState::HumanTurn &&
                       pair.guest.guestSync().phase == NetPhase::Play;
    }
    CHECK(guestPlaying);
    CHECK(!pair.guest.guestSync().canRaise);
    for (const std::string& action : pair.guest.view().actions) {
        CHECK(action != "Truco!");
    }
}

TEST(lan_full_match_reaches_match_end_on_both_sides) {
    LanPair pair;
    CHECK(pair.start());
    int guard = 0;
    while (!(pair.host.state() == SessionState::MatchResult &&
             pair.guest.state() == SessionState::MatchResult) &&
           guard++ < 8000) {
        pair.pump(1);
        if (pair.host.state() == SessionState::HumanTurn) {
            const int index = firstUnplayed(pair.host, Side::Human);
            if (index >= 0) {
                pair.host.selectCard(index);
                pair.host.confirmSelection();
            }
        }
        if (!pair.host.view().actions.empty() &&
            (pair.host.state() == SessionState::TrucoDialog ||
             pair.host.state() == SessionState::ElevenChoice)) {
            pair.host.activateAction(0);
        }
        if (pair.guest.guestSync().phase == NetPhase::Play &&
            pair.guest.guestSync().turn == Side::Cpu) {
            int index = -1;
            for (int i = 0; i < 3; ++i) {
                if (!pair.guest.guestSync().playedOwn[static_cast<size_t>(i)]) {
                    index = i;
                    break;
                }
            }
            if (index >= 0) {
                pair.guest.selectCard(index);
                pair.guest.confirmSelection();
            }
        }
        if (!pair.guest.view().actions.empty() &&
            (pair.guest.state() == SessionState::TrucoDialog ||
             pair.guest.state() == SessionState::ElevenChoice)) {
            pair.guest.activateAction(0);
        }
    }
    pair.pump(20);
    CHECK(pair.host.state() == SessionState::MatchResult);
    CHECK(pair.guest.state() == SessionState::MatchResult);
    CHECK(pair.host.game().matchOver());
    CHECK_EQ(pair.guest.guestSync().phase, NetPhase::MatchEnd);
    CHECK_EQ(pair.guest.view().humanScore, pair.host.view().humanScore);
    CHECK_EQ(pair.guest.view().cpuScore, pair.host.view().cpuScore);
}
