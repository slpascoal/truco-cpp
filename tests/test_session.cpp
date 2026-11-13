#include <chrono>
#include <string>

#include "app/session.hpp"
#include "test_framework.hpp"

using namespace truco;
using std::chrono::milliseconds;

namespace {

int firstUnplayed(const Session& session, Side side) {
    for (int i = 0; i < 3; ++i) {
        if (!session.game().cardPlayed(side, i)) {
            return i;
        }
    }
    return -1;
}

void reachHumanTurn(Session& session) {
    session.activateAction(0);
    for (int i = 0; i < 10 && session.state() != SessionState::HumanTurn; ++i) {
        session.advance(milliseconds(1000));
    }
}

}  // namespace

TEST(session_starts_at_menu) {
    Session session(7);
    session.start();
    CHECK(session.state() == SessionState::Menu);
    CHECK_EQ(session.view().actions.size(), size_t{2});
    CHECK_EQ(session.view().actions[0], std::string("Jogar"));
}

TEST(session_menu_exit_stops_running) {
    Session session(8);
    session.start();
    session.activateAction(1);
    CHECK(!session.running());
}

TEST(session_starts_dealing_then_human_turn) {
    Session session(9);
    session.start();
    reachHumanTurn(session);
    CHECK(session.state() == SessionState::HumanTurn);
    CHECK_EQ(session.view().humanCards.size(), size_t{3});
    CHECK(session.view().vira.has_value());
}

TEST(session_card_needs_confirmation) {
    Session session(10);
    session.start();
    reachHumanTurn(session);
    session.selectCard(0);
    CHECK_EQ(session.view().selectedCard, 0);
    CHECK(!session.game().cardPlayed(Side::Human, 0));
    session.selectCard(0);
    CHECK(session.game().cardPlayed(Side::Human, 0));
    CHECK(session.state() == SessionState::TrickReveal);
}

TEST(session_escape_cancels_selection) {
    Session session(11);
    session.start();
    reachHumanTurn(session);
    session.selectCard(1);
    InputEvent event;
    event.kind = InputEvent::Kind::Key;
    event.key = 27;
    session.handleEvent(event);
    CHECK_EQ(session.view().selectedCard, -1);
}

TEST(session_keyboard_selects_and_confirms_card) {
    Session session(12);
    session.start();
    reachHumanTurn(session);
    InputEvent digit;
    digit.kind = InputEvent::Kind::Key;
    digit.key = '2';
    session.handleEvent(digit);
    CHECK_EQ(session.view().selectedCard, 1);
    InputEvent enter;
    enter.kind = InputEvent::Kind::Key;
    enter.key = '\r';
    session.handleEvent(enter);
    CHECK(session.game().cardPlayed(Side::Human, 1));
}

TEST(session_human_truco_opens_dialog) {
    Session session(13);
    session.start();
    reachHumanTurn(session);
    CHECK_EQ(session.view().actions.size(), size_t{2});
    session.activateAction(1);
    CHECK(session.state() == SessionState::TrucoDialog);
    CHECK(session.game().trucoPending());
    session.advance(milliseconds(1200));
    CHECK(session.state() == SessionState::HumanTurn ||
          session.state() == SessionState::HandResult ||
          session.state() == SessionState::MatchResult);
}

TEST(session_quit_event_stops_running) {
    Session session(14);
    session.start();
    InputEvent event;
    event.kind = InputEvent::Kind::Quit;
    session.handleEvent(event);
    CHECK(!session.running());
}

TEST(session_mouse_warning_is_exposed) {
    Session session(15);
    session.start();
    session.setMouseWarning(true);
    CHECK(session.mouseWarning());
    CHECK(session.view().mouseWarning);
}

TEST(session_full_match_reaches_result) {
    Session session(4242);
    session.start();
    int guard = 0;
    while (session.state() != SessionState::MatchResult && guard++ < 4000) {
        session.advance(milliseconds(1600));
        switch (session.state()) {
            case SessionState::HumanTurn: {
                const int index = firstUnplayed(session, Side::Human);
                if (index >= 0) {
                    session.selectCard(index);
                    session.confirmSelection();
                }
                break;
            }
            case SessionState::TrucoDialog:
                if (!session.view().actions.empty()) {
                    session.activateAction(0);
                }
                break;
            case SessionState::ElevenChoice:
                if (!session.view().actions.empty()) {
                    session.activateAction(0);
                }
                break;
            case SessionState::Menu:
                session.activateAction(0);
                break;
            case SessionState::HandResult:
                session.advance(milliseconds(2000));
                break;
            default:
                break;
        }
    }
    CHECK(session.state() == SessionState::MatchResult);
    CHECK(session.game().matchOver());
    CHECK_EQ(session.view().actions.size(), size_t{2});
}

TEST(session_resize_event_keeps_state) {
    Session session(16);
    session.start();
    reachHumanTurn(session);
    const SessionState before = session.state();
    const int humanScore = session.view().humanScore;
    InputEvent resize;
    resize.kind = InputEvent::Kind::Resize;
    session.handleEvent(resize);
    CHECK(session.state() == before);
    CHECK_EQ(session.view().humanScore, humanScore);
}

TEST(session_dealing_animation_state_is_visible) {
    Session session(17);
    session.start();
    session.activateAction(0);
    CHECK(session.state() == SessionState::Dealing);
    session.advance(milliseconds(100));
    CHECK(session.state() == SessionState::Dealing);
    session.advance(milliseconds(1000));
    CHECK(session.state() == SessionState::HumanTurn);
}
