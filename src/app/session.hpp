#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <random>

#include "ai/cpu_player.hpp"
#include "core/game.hpp"
#include "net/guest_client.hpp"
#include "net/host_server.hpp"
#include "net/protocol.hpp"
#include "ui/input.hpp"
#include "ui/render.hpp"
#include "ui/text_input.hpp"

namespace truco {

enum class GameMode { Cpu, Hotseat, LanHost, LanGuest };

enum class SessionState {
    Menu,
    Handoff,
    Dealing,
    HumanTurn,
    CpuTurn,
    TrucoDialog,
    ElevenChoice,
    TrickReveal,
    HandResult,
    MatchResult,
    LanConfig,
    LanWaiting,
    LanError
};

class Session {
public:
    explicit Session(uint32_t seed = std::random_device{}());

    void start();
    int run();

    void handleEvent(const InputEvent& event);
    void tick(std::chrono::steady_clock::time_point now);
    void advance(std::chrono::milliseconds delta);

    const ViewModel& view() const { return view_; }
    SessionState state() const { return state_; }
    GameMode mode() const { return mode_; }
    Side activeSide() const { return activeSide_; }
    bool running() const { return running_; }
    bool mouseWarning() const { return mouseWarning_; }
    void setMouseWarning(bool value);
    const Game& game() const { return game_; }

    std::string sideName(Side side) const;
    std::string sideShortName(Side side) const;

    void selectCard(int index);
    void confirmSelection();
    void cancelSelection();
    void confirmHandoff();
    void activateAction(int index);

    void setModeForTesting(GameMode mode);
    void setScoreForTesting(GameScore score);
    void beginHotseatWithScoreForTesting(GameScore score);
    bool openLanHostForTesting(uint16_t port);
    bool openLanConnectForTesting(const std::string& ip, uint16_t port);
    uint16_t lanPort() const;
    bool lanConnected() const;
    const SyncState& guestSync() const { return guestView_; }

    std::chrono::steady_clock::time_point now() const { return now_; }

private:
    enum class HandoffPurpose { Turn, TrucoAnswer, ElevenChoice };

    bool hotseat() const { return mode_ == GameMode::Hotseat; }
    bool lanHost() const { return mode_ == GameMode::LanHost; }
    bool lanGuest() const { return mode_ == GameMode::LanGuest; }
    bool lanMode() const { return lanHost() || lanGuest(); }
    Side localSide() const { return lanGuest() ? Side::Cpu : Side::Human; }

    bool canSelectCards() const;
    void beginMatch();
    void startLanHostMatch();
    void enterHandStart();
    void beginTurn();
    void endHand();
    void requestHandoff(Side next, HandoffPurpose purpose, const std::string& text);
    void enterTurnSide(Side side);
    void enterElevenSide(Side side);
    void handleCpuTurn();
    void playForSide(Side side, int index);
    void refreshView();
    bool humanMustAnswerTruco() const;
    PublicView buildCpuView() const;

    void openLanConfig(bool hostMode);
    void confirmLanConfig();
    void cancelLan();
    void enterLanError(const std::string& reason);
    void pumpLanWaiting();
    void pumpHost();
    void pumpGuest();
    void applyGuestIntent(const Message& message);
    void applyGuestSync(const SyncState& sync);
    void broadcastIfHost();
    NetPhase hostPhase() const;
    Side hostTurnSide() const;
    SyncState makeSync(Side viewer) const;
    void sendGuest(MessageType type, const std::vector<std::string>& fields = {});
    void resetToMenu();

    std::mt19937 rng_;
    std::mt19937 aiRng_;
    Game game_;

    SessionState state_ = SessionState::Menu;
    GameMode mode_ = GameMode::Cpu;
    Side activeSide_ = Side::Human;
    Side pendingSide_ = Side::Human;
    HandoffPurpose handoffPurpose_ = HandoffPurpose::Turn;
    bool handoffDoneThisHand_ = false;
    std::chrono::steady_clock::time_point now_{};
    std::chrono::steady_clock::time_point until_{};
    bool running_ = true;
    bool mouseWarning_ = false;
    bool cpuAnsweringTruco_ = false;
    int selectedCard_ = -1;
    int selectedAction_ = 0;
    std::string message_;
    ViewModel view_;

    bool lanConfigHost_ = false;
    TextInput lanIp_{"0123456789.", ""};
    TextInput lanPort_{"0123456789", "4000"};
    int lanFocus_ = 0;
    std::unique_ptr<net::HostServer> host_;
    std::unique_ptr<net::GuestClient> guest_;
    bool lanHandshakeDone_ = false;
    bool lanMatchActive_ = false;
    bool guestWaitingAck_ = false;
    uint32_t lanSeq_ = 0;
    std::string lanLastMessage_;
    SyncState guestView_;
    std::string guestMessage_;
};

std::string sessionStateName(SessionState state);

}  // namespace truco
