#include "app/session.hpp"

#include <algorithm>
#include <cstdlib>

#include "ui/terminal.hpp"
#include "net/room_server.hpp"

namespace truco {
namespace {

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

std::string cardLabel(Card card) {
    return rankLabel(card.rank) + suitLabel(card.suit);
}

int clampScore(int score) {
    return score > 12 ? 12 : score;
}

Side other(Side side) {
    return side == Side::Human ? Side::Cpu : Side::Human;
}

bool parsePort(const std::string& text, uint16_t& out) {
    if (text.empty() || text.size() > 5) {
        return false;
    }
    long value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    if (value < 1 || value > 65535) {
        return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
}

}  // namespace

std::string sessionStateName(SessionState state) {
    switch (state) {
        case SessionState::Menu:
            return "Menu";
        case SessionState::Handoff:
            return "Handoff";
        case SessionState::Dealing:
            return "Dealing";
        case SessionState::HumanTurn:
            return "HumanTurn";
        case SessionState::CpuTurn:
            return "CpuTurn";
        case SessionState::TrucoDialog:
            return "TrucoDialog";
        case SessionState::ElevenChoice:
            return "ElevenChoice";
        case SessionState::TrickReveal:
            return "TrickReveal";
        case SessionState::HandResult:
            return "HandResult";
        case SessionState::MatchResult:
            return "MatchResult";
        case SessionState::LanConfig:
            return "LanConfig";
        case SessionState::LanWaiting:
            return "LanWaiting";
        case SessionState::LanError:
            return "LanError";
        case SessionState::RoyaleConfig:
            return "RoyaleConfig";
        case SessionState::RoyaleLobby:
            return "RoyaleLobby";
        case SessionState::RoyaleWaiting:
            return "RoyaleWaiting";
        case SessionState::RoyalePlay:
            return "RoyalePlay";
        case SessionState::RoyaleResult:
            return "RoyaleResult";
    }
    return "Unknown";
}

Session::Session(uint32_t seed)
    : rng_(seed), aiRng_(seed ^ 0x5bd1e995u), game_(rng_) {}

void Session::start() {
    now_ = Clock::now();
    until_ = now_;
    state_ = SessionState::Menu;
    mode_ = GameMode::Cpu;
    message_ = "Escolha um modo de jogo";
    refreshView();
}

void Session::setMouseWarning(bool value) {
    mouseWarning_ = value;
    refreshView();
}

std::string Session::sideName(Side side) const {
    if (mode_ != GameMode::Cpu) {
        return side == Side::Human ? "Jogador 1" : "Jogador 2";
    }
    return side == Side::Human ? "Voce" : "Computador";
}

std::string Session::sideShortName(Side side) const {
    if (mode_ != GameMode::Cpu) {
        return side == Side::Human ? "J1" : "J2";
    }
    return side == Side::Human ? "Voce" : "CPU";
}

void Session::setModeForTesting(GameMode mode) {
    mode_ = mode;
}

void Session::setScoreForTesting(GameScore score) {
    game_.setScoreForTesting(score);
}

void Session::beginHotseatWithScoreForTesting(GameScore score) {
    now_ = Clock::now();
    mode_ = GameMode::Hotseat;
    activeSide_ = Side::Human;
    pendingSide_ = Side::Human;
    handoffDoneThisHand_ = false;
    game_.setScoreForTesting(score);
    game_.startHand();
    message_ = "Nova partida!";
    enterHandStart();
}

bool Session::openLanHostForTesting(uint16_t port) {
    now_ = Clock::now();
    host_ = std::make_unique<net::HostServer>();
    std::string error;
    if (!host_->listen(port, error)) {
        host_.reset();
        message_ = "Erro: " + error;
        refreshView();
        return false;
    }
    mode_ = GameMode::LanHost;
    lanHandshakeDone_ = false;
    lanMatchActive_ = false;
    lanLastMessage_.clear();
    state_ = SessionState::LanWaiting;
    message_ = "Aguardando convidado";
    refreshView();
    return true;
}

bool Session::openLanConnectForTesting(const std::string& ip, uint16_t port) {
    now_ = Clock::now();
    guest_ = std::make_unique<net::GuestClient>();
    std::string error;
    if (!guest_->connectTo(ip, port, 1500, error)) {
        guest_.reset();
        message_ = "Erro: " + error;
        refreshView();
        return false;
    }
    mode_ = GameMode::LanGuest;
    lanHandshakeDone_ = false;
    lanMatchActive_ = false;
    guest_->sendLine(encodeLine(MessageType::Hello, {"TRUCO", "2"}));
    state_ = SessionState::LanWaiting;
    message_ = "Conectando...";
    refreshView();
    return true;
}

uint16_t Session::lanPort() const {
    return host_ ? host_->port() : 0;
}

bool Session::lanConnected() const {
    if (host_) {
        return host_->connected();
    }
    if (guest_) {
        return guest_->connected();
    }
    return false;
}

void Session::beginMatch() {
    activeSide_ = Side::Human;
    pendingSide_ = Side::Human;
    handoffDoneThisHand_ = false;
    game_.startMatch();
    message_ = "Nova partida!";
    enterHandStart();
}

void Session::startLanHostMatch() {
    activeSide_ = Side::Human;
    handoffDoneThisHand_ = false;
    game_.startMatch();
    lanMatchActive_ = true;
    lanLastMessage_.clear();
    message_ = "Partida iniciada!";
    enterHandStart();
}

void Session::enterHandStart() {
    selectedCard_ = -1;
    selectedAction_ = 0;
    handoffDoneThisHand_ = false;
    if (game_.handFinished()) {
        endHand();
        return;
    }
    if (game_.awaitingElevenChoice()) {
        if (hotseat()) {
            requestHandoff(game_.elevenSide(), HandoffPurpose::ElevenChoice,
                           "Mao de 11 para " + sideName(game_.elevenSide()));
        } else if (lanHost()) {
            state_ = SessionState::ElevenChoice;
            activeSide_ = Side::Human;
            until_ = now_;
            message_ = game_.elevenSide() == Side::Human ? "Voce esta na mao de 11"
                                                         : "Jogador 2 esta na mao de 11...";
            refreshView();
        } else {
            enterElevenSide(game_.elevenSide());
        }
        return;
    }
    state_ = SessionState::Dealing;
    until_ = now_ + milliseconds(800);
    message_ = "Distribuindo as cartas...";
    refreshView();
}

void Session::requestHandoff(Side next, HandoffPurpose purpose, const std::string& text) {
    pendingSide_ = next;
    handoffPurpose_ = purpose;
    state_ = SessionState::Handoff;
    until_ = now_;
    selectedCard_ = -1;
    selectedAction_ = 0;
    message_ = text;
    refreshView();
}

void Session::confirmHandoff() {
    if (state_ != SessionState::Handoff) {
        return;
    }
    activeSide_ = pendingSide_;
    switch (handoffPurpose_) {
        case HandoffPurpose::Turn:
            handoffDoneThisHand_ = true;
            enterTurnSide(activeSide_);
            return;
        case HandoffPurpose::TrucoAnswer:
            state_ = SessionState::TrucoDialog;
            selectedAction_ = 0;
            message_ = sideName(activeSide_) + ": aceitar ou correr?";
            refreshView();
            return;
        case HandoffPurpose::ElevenChoice:
            enterElevenSide(activeSide_);
            return;
    }
}

void Session::enterTurnSide(Side side) {
    activeSide_ = side;
    selectedCard_ = -1;
    if (side == Side::Human) {
        state_ = SessionState::HumanTurn;
        message_ = hotseat() || lanMode() ? "Vez de " + sideName(side) : "Sua vez";
        until_ = now_;
    } else {
        state_ = SessionState::CpuTurn;
        if (hotseat() || lanHost()) {
            message_ = "Vez de " + sideName(side);
            until_ = lanHost() ? now_ + milliseconds(200) : now_;
        } else {
            message_ = "Vez do computador...";
            until_ = now_ + milliseconds(700);
        }
    }
    refreshView();
}

void Session::enterElevenSide(Side side) {
    activeSide_ = side;
    state_ = SessionState::ElevenChoice;
    if (hotseat() || lanHost() || side == Side::Human) {
        until_ = now_;
        message_ = "Mao de 11 para " + sideName(side);
    } else {
        until_ = now_ + milliseconds(900);
        message_ = "O computador esta na mao de 11...";
    }
    refreshView();
}

void Session::beginTurn() {
    if (game_.handFinished()) {
        endHand();
        return;
    }
    if (game_.awaitingElevenChoice()) {
        if (hotseat()) {
            if (game_.elevenSide() != activeSide_) {
                requestHandoff(game_.elevenSide(), HandoffPurpose::ElevenChoice,
                               "Mao de 11 para " + sideName(game_.elevenSide()));
            } else {
                enterElevenSide(game_.elevenSide());
            }
        } else if (lanHost()) {
            state_ = SessionState::ElevenChoice;
            until_ = now_;
            refreshView();
        } else {
            enterElevenSide(game_.elevenSide());
        }
        return;
    }
    const Side side = game_.currentTurn();
    if (hotseat() && (!handoffDoneThisHand_ || side != activeSide_)) {
        requestHandoff(side, HandoffPurpose::Turn, "Passe o controle para " + sideName(side));
        return;
    }
    enterTurnSide(side);
}

void Session::endHand() {
    state_ = SessionState::HandResult;
    until_ = now_ + milliseconds(1500);
    const int points = valuePoints(game_.stake());
    message_ = sideName(game_.handWinner()) + " ganhou a mao (+" + std::to_string(points) + ")";
    refreshView();
}

void Session::tick(Clock::time_point now) {
    if (now > now_) {
        now_ = now;
    }
    if (royaleHost()) {
        if (state_ == SessionState::RoyaleLobby) {
            pumpRoyaleLobby();
        } else if (state_ == SessionState::RoyalePlay ||
                   state_ == SessionState::RoyaleResult) {
            pumpRoyaleHost();
        }
    } else if (royaleGuest()) {
        pumpRoyaleGuest();
    } else if (lanHost()) {
        if (lanMatchActive_) {
            pumpHost();
        } else {
            pumpLanWaiting();
        }
    } else if (lanGuest()) {
        pumpGuest();
    }

    switch (state_) {
        case SessionState::Dealing:
            if (now_ >= until_) {
                beginTurn();
            }
            break;
        case SessionState::CpuTurn:
            if (!hotseat() && !lanMode() && now_ >= until_) {
                handleCpuTurn();
            }
            break;
        case SessionState::TrucoDialog:
            if (cpuAnsweringTruco_ && now_ >= until_) {
                cpuAnsweringTruco_ = false;
                if (shouldAcceptTruco(buildCpuView(), aiRng_)) {
                    game_.respondTruco(Side::Cpu, true);
                    message_ = "O computador aceitou!";
                    enterTurnSide(game_.currentTurn());
                } else {
                    game_.respondTruco(Side::Cpu, false);
                    message_ = "O computador correu. Voce leva os pontos!";
                    endHand();
                }
            }
            break;
        case SessionState::ElevenChoice:
            if (!hotseat() && !lanMode() && game_.elevenSide() == Side::Cpu &&
                now_ >= until_) {
                const bool play = shouldPlayHandOfEleven(game_.elevenCard(), aiRng_);
                game_.chooseHandOfEleven(Side::Cpu, play);
                message_ = play ? "O computador jogou a mao de 11 (3 pontos)."
                                : "O computador escondeu a mao de 11 (1 ponto).";
                endHand();
            }
            break;
        case SessionState::TrickReveal:
            if (now_ >= until_) {
                if (game_.handFinished()) {
                    endHand();
                } else {
                    beginTurn();
                }
            }
            break;
        case SessionState::HandResult:
            if (!lanGuest() && now_ >= until_) {
                if (game_.matchOver()) {
                    state_ = SessionState::MatchResult;
                    selectedAction_ = 0;
                    message_ = sideName(game_.matchWinner()) + " venceu a partida!";
                    until_ = now_;
                    refreshView();
                } else {
                    game_.startHand();
                    enterHandStart();
                }
            }
            break;
        default:
            break;
    }
}

void Session::advance(milliseconds delta) {
    tick(now_ + delta);
}

void Session::handleCpuTurn() {
    PublicView view = buildCpuView();
    if (game_.canCallTruco(Side::Cpu) && shouldCallTruco(view, aiRng_)) {
        const std::string label = callLabel(game_.stake());
        game_.callTruco(Side::Cpu);
        activeSide_ = Side::Human;
        state_ = SessionState::TrucoDialog;
        selectedAction_ = 0;
        message_ = "O computador pediu " + label + "!";
        refreshView();
        return;
    }
    const int index = chooseCardIndex(view, aiRng_);
    if (index < 0) {
        return;
    }
    const Card played = game_.handOf(Side::Cpu)[static_cast<size_t>(index)];
    game_.playCard(Side::Cpu, index);
    message_ = "Computador jogou " + cardLabel(played);
    state_ = SessionState::TrickReveal;
    until_ = now_ + milliseconds(700);
    refreshView();
}

void Session::playForSide(Side side, int index) {
    if (game_.cardPlayed(side, index)) {
        return;
    }
    const Card played = game_.handOf(side)[static_cast<size_t>(index)];
    game_.playCard(side, index);
    selectedCard_ = -1;
    message_ = sideName(side) + " jogou " + cardLabel(played);
    state_ = SessionState::TrickReveal;
    until_ = now_ + milliseconds(700);
    refreshView();
}

bool Session::canSelectCards() const {
    if (royaleHost()) {
        return state_ == SessionState::RoyalePlay && royale_ && royale_->canPlay(0);
    }
    if (royaleGuest()) {
        return state_ == SessionState::RoyalePlay &&
               royaleView_.phase == RoyalePhase::Playing &&
               royaleView_.turn == royaleSeat_;
    }
    if (lanGuest()) {
        return lanMatchActive_ && !guestWaitingAck_ &&
               guestView_.phase == NetPhase::Play && guestView_.turn == localSide();
    }
    if (state_ == SessionState::HumanTurn) {
        return true;
    }
    return hotseat() && state_ == SessionState::CpuTurn;
}

void Session::selectCard(int index) {
    if (!canSelectCards()) {
        return;
    }
    if (index < 0 || index > 2) {
        return;
    }
    if (royaleHost()) {
        if (royale_->seat(0).played[static_cast<size_t>(index)]) {
            return;
        }
    } else if (royaleGuest()) {
        if (royaleView_.playedOwn[static_cast<size_t>(index)]) {
            return;
        }
    } else if (lanGuest()) {
        if (guestView_.playedOwn[static_cast<size_t>(index)]) {
            return;
        }
    } else if (game_.cardPlayed(activeSide_, index)) {
        return;
    }
    if (selectedCard_ == index) {
        confirmSelection();
        return;
    }
    selectedCard_ = index;
    refreshView();
}

void Session::confirmSelection() {
    if (!canSelectCards()) {
        return;
    }
    if (selectedCard_ < 0 || selectedCard_ > 2) {
        return;
    }
    if (royaleHost()) {
        royaleHostPlay(selectedCard_);
        return;
    }
    if (royaleGuest()) {
        if (royaleView_.playedOwn[static_cast<size_t>(selectedCard_)]) {
            return;
        }
        guest_->sendLine(encodeLine(MessageType::Play,
                                    {std::to_string(selectedCard_ + 1)}));
        selectedCard_ = -1;
        refreshView();
        return;
    }
    if (lanGuest()) {
        if (guestView_.playedOwn[static_cast<size_t>(selectedCard_)]) {
            return;
        }
        sendGuest(MessageType::Play, {std::to_string(selectedCard_ + 1)});
        selectedCard_ = -1;
        refreshView();
        return;
    }
    if (!game_.cardPlayed(activeSide_, selectedCard_)) {
        playForSide(activeSide_, selectedCard_);
    }
}

void Session::cancelSelection() {
    if (selectedCard_ >= 0) {
        selectedCard_ = -1;
        refreshView();
    }
}

void Session::activateAction(int index) {
    if (royaleGuest()) {
        const RoyaleView& view = royaleView_;
        if (state_ == SessionState::RoyaleResult) {
            resetToMenu();
            return;
        }
        if (view.phase == RoyalePhase::Playing && view.turn == royaleSeat_) {
            if (index == 0) {
                confirmSelection();
            } else if (index == 1 && view.canRaise) {
                guest_->sendLine(encodeLine(MessageType::Truco));
            }
        } else if (view.phase == RoyalePhase::TrucoAnswer &&
                   view.responder == royaleSeat_) {
            if (index == 0) {
                guest_->sendLine(encodeLine(MessageType::Accept));
            } else if (index == 1) {
                guest_->sendLine(encodeLine(MessageType::Run));
            }
        }
        return;
    }
    if (royaleHost()) {
        royaleHostAction(index);
        return;
    }
    if (lanGuest()) {
        switch (guestView_.phase) {
            case NetPhase::Play:
                if (guestView_.turn != localSide()) break;
                if (index == 0) {
                    confirmSelection();
                } else if (index == 1 && guestView_.stake < 12) {
                    sendGuest(MessageType::Truco);
                }
                break;
            case NetPhase::TrucoAnswer:
                if (guestView_.turn != localSide()) break;
                if (index == 0) sendGuest(MessageType::Accept);
                if (index == 1) sendGuest(MessageType::Run);
                break;
            case NetPhase::Eleven:
                if (guestView_.elevenSide != localSide()) break;
                if (index == 0) sendGuest(MessageType::Eleven, {"PLAY"});
                if (index == 1) sendGuest(MessageType::Eleven, {"HIDE"});
                break;
            case NetPhase::MatchEnd:
                if (index == 0) {
                    sendGuest(MessageType::Rematch);
                } else if (index == 1) {
                    if (guest_) guest_->close();
                    resetToMenu();
                }
                break;
            default:
                break;
        }
        return;
    }

    switch (state_) {
        case SessionState::Menu:
            if (index == 0 || index == 1) {
                mode_ = index == 0 ? GameMode::Cpu : GameMode::Hotseat;
                beginMatch();
            } else if (index == 2) {
                openLanConfig(true);
            } else if (index == 3) {
                openLanConfig(false);
            } else if (index == 4) {
                running_ = false;
            }
            break;
        case SessionState::LanConfig:
            if (index == 0) {
                confirmLanConfig();
            } else if (index == 1) {
                cancelLan();
            }
            break;
        case SessionState::LanWaiting:
            if (index == 0) {
                cancelLan();
            }
            break;
        case SessionState::LanError:
            if (index == 0) {
                resetToMenu();
            }
            break;
        case SessionState::Handoff:
            if (index == 0) {
                confirmHandoff();
            }
            break;
        case SessionState::HumanTurn:
        case SessionState::CpuTurn:
            if (lanHost() && state_ == SessionState::CpuTurn) {
                break;
            }
            if (hotseat() && !canSelectCards()) {
                break;
            }
            if (index == 0) {
                confirmSelection();
            } else if (index == 1 && game_.canCallTruco(activeSide_)) {
                const std::string label = callLabel(game_.stake());
                game_.callTruco(activeSide_);
                if (hotseat()) {
                    requestHandoff(other(activeSide_), HandoffPurpose::TrucoAnswer,
                                   sideName(activeSide_) + " pediu " + label +
                                       ". Passe para " + sideName(other(activeSide_)));
                } else if (lanHost()) {
                    state_ = SessionState::TrucoDialog;
                    selectedAction_ = 0;
                    message_ = "Voce pediu " + label + "...";
                    refreshView();
                } else {
                    cpuAnsweringTruco_ = true;
                    state_ = SessionState::TrucoDialog;
                    until_ = now_ + milliseconds(900);
                    message_ = "Voce pediu " + label + "...";
                    refreshView();
                }
            }
            break;
        case SessionState::TrucoDialog:
            if (!humanMustAnswerTruco()) {
                break;
            }
            if (index == 0) {
                try {
                    game_.respondTruco(activeSide_, true);
                } catch (const std::exception&) {
                    break;
                }
                message_ = sideName(activeSide_) + " aceitou. A mao vale " +
                           std::to_string(valuePoints(game_.stake())) + " pontos.";
                if (hotseat()) {
                    requestHandoff(game_.currentTurn(), HandoffPurpose::Turn,
                                   "Passe o controle para " + sideName(game_.currentTurn()));
                } else {
                    enterTurnSide(game_.currentTurn());
                }
            } else if (index == 1) {
                try {
                    game_.respondTruco(activeSide_, false);
                } catch (const std::exception&) {
                    break;
                }
                message_ = sideName(activeSide_) + " correu. " +
                           sideName(other(activeSide_)) + " leva os pontos.";
                endHand();
            }
            break;
        case SessionState::ElevenChoice:
            if (game_.elevenSide() != activeSide_) {
                break;
            }
            if (lanHost() && activeSide_ != Side::Human) {
                break;
            }
            if (index == 0 || index == 1) {
                try {
                    game_.chooseHandOfEleven(activeSide_, index == 0);
                } catch (const std::exception&) {
                    break;
                }
                message_ = index == 0 ? sideName(activeSide_) + " jogou a mao de 11 (3 pontos)."
                                      : sideName(activeSide_) + " escondeu a mao de 11 (1 ponto).";
                endHand();
            }
            break;
        case SessionState::MatchResult:
            if (index == 0) {
                if (lanHost()) {
                    startLanHostMatch();
                } else if (lanGuest()) {
                    sendGuest(MessageType::Rematch);
                } else {
                    beginMatch();
                }
            } else if (index == 1) {
                running_ = false;
            }
            break;
        default:
            break;
    }
}

bool Session::humanMustAnswerTruco() const {
    if (!game_.trucoPending()) {
        return false;
    }
    if (lanHost()) {
        return game_.trucoCaller() == Side::Cpu;
    }
    if (hotseat()) {
        return game_.trucoCaller() != activeSide_;
    }
    return game_.trucoCaller() == Side::Cpu;
}

PublicView Session::buildCpuView() const {
    PublicView view;
    view.hand = game_.handOf(Side::Cpu);
    for (int i = 0; i < 3; ++i) {
        view.played[static_cast<size_t>(i)] = game_.cardPlayed(Side::Cpu, i);
    }
    view.opponentCardInTrick = game_.currentTrickCard(Side::Human);
    view.stake = game_.stake();
    view.progress = game_.progress();
    view.score = game_.score();
    view.turn = Side::Cpu;
    view.trucoPending = game_.trucoPending();
    return view;
}

void Session::openLanConfig(bool hostMode) {
    lanConfigHost_ = hostMode;
    state_ = SessionState::LanConfig;
    lanFocus_ = 0;
    lanPort_.set("4000");
    lanIp_.set("");
    message_.clear();
    selectedAction_ = 0;
    refreshView();
}

void Session::confirmLanConfig() {
    uint16_t port = 0;
    if (!parsePort(lanPort_.value(), port)) {
        message_ = "Porta invalida: use 1 a 65535";
        refreshView();
        return;
    }
    if (lanConfigHost_) {
        host_ = std::make_unique<net::HostServer>();
        std::string error;
        if (!host_->listen(port, error)) {
            host_.reset();
            message_ = "Erro: " + error;
            refreshView();
            return;
        }
        mode_ = GameMode::LanHost;
        lanHandshakeDone_ = false;
        lanMatchActive_ = false;
        lanLastMessage_.clear();
        state_ = SessionState::LanWaiting;
        message_ = "Aguardando convidado";
        refreshView();
        return;
    }
    if (lanIp_.empty()) {
        message_ = "Informe o IP do host";
        refreshView();
        return;
    }
    guest_ = std::make_unique<net::GuestClient>();
    std::string error;
    if (!guest_->connectTo(lanIp_.value(), port, 3000, error)) {
        guest_.reset();
        message_ = "Erro: " + error;
        refreshView();
        return;
    }
    mode_ = GameMode::LanGuest;
    lanHandshakeDone_ = false;
    lanMatchActive_ = false;
    guest_->sendLine(encodeLine(MessageType::Hello, {"TRUCO", "2"}));
    state_ = SessionState::LanWaiting;
    message_ = "Conectando...";
    refreshView();
}

void Session::cancelLan() {
    if (host_) host_->shutdown();
    if (guest_) guest_->close();
    resetToMenu();
}

void Session::resetToMenu() {
    host_.reset();
    guest_.reset();
    if (room_) room_->shutdown();
    room_.reset();
    royale_.reset();
    royaleJoined_.clear();
    royaleRevealScheduled_ = false;
    royaleSeat_ = -1;
    royaleView_ = RoyaleView{};
    lanHandshakeDone_ = false;
    lanMatchActive_ = false;
    mode_ = GameMode::Cpu;
    state_ = SessionState::Menu;
    selectedAction_ = 0;
    message_ = "Escolha um modo de jogo";
    refreshView();
}

void Session::enterLanError(const std::string& reason) {
    if (host_) host_->shutdown();
    if (guest_) guest_->close();
    host_.reset();
    guest_.reset();
    lanHandshakeDone_ = false;
    lanMatchActive_ = false;
    state_ = SessionState::LanError;
    selectedAction_ = 0;
    message_ = reason;
    refreshView();
}

void Session::pumpLanWaiting() {
    if (!host_) {
        return;
    }
    if (host_->waiting()) {
        host_->acceptPending();
        return;
    }
    if (!host_->connected()) {
        return;
    }
    for (const std::string& line : host_->readLines()) {
        const Message message = parseLine(line);
        if (message.type != MessageType::Hello) {
            continue;
        }
        const bool valid = message.fields.size() >= 2 && message.fields[0] == "TRUCO" &&
                           message.fields[1] == "2";
        if (valid) {
            host_->sendLine(encodeLine(MessageType::Welcome, {"2"}));
            lanHandshakeDone_ = true;
            startLanHostMatch();
        } else {
            host_->sendLine(encodeLine(MessageType::Error, {"VERSION"}, "versao incompativel"));
            host_->dropClient();
        }
        return;
    }
    if (host_->disconnected()) {
        host_->dropClient();
    }
}

void Session::pumpHost() {
    if (!host_) {
        return;
    }
    if (!host_->connected()) {
        if (lanMatchActive_) {
            enterLanError("Conexao perdida com o adversario");
        }
        return;
    }
    bool received = false;
    for (const std::string& line : host_->readLines()) {
        received = true;
        applyGuestIntent(parseLine(line));
    }
    if (received) {
        refreshView();
    }
    if (host_->disconnected() && lanMatchActive_) {
        enterLanError("Conexao perdida com o adversario");
    }
}

void Session::pumpGuest() {
    if (!guest_) {
        return;
    }
    if (guest_->disconnected()) {
        enterLanError("Conexao perdida com o host");
        return;
    }
    for (const std::string& line : guest_->readLines()) {
        const Message message = parseLine(line);
        if (message.type == MessageType::Welcome) {
            if (!message.fields.empty() && message.fields[0] == "2") {
                lanHandshakeDone_ = true;
                message_ = "Conectado. Aguardando o host iniciar...";
                refreshView();
            } else {
                enterLanError("Versao de protocolo incompativel");
            }
            continue;
        }
        if (message.type == MessageType::Sync) {
            SyncState sync;
            if (decodeSync(line, sync)) {
                applyGuestSync(sync);
            }
            continue;
        }
        if (message.type == MessageType::Msg) {
            guestMessage_ = message.text;
            continue;
        }
        if (message.type == MessageType::Error) {
            enterLanError(message.text.empty() ? "Erro do servidor" : message.text);
            return;
        }
    }
    if (guest_->disconnected()) {
        enterLanError("Conexao perdida com o host");
    }
}

void Session::applyGuestIntent(const Message& message) {
    if (!lanHost() || !lanMatchActive_) {
        return;
    }
    try {
        switch (message.type) {
            case MessageType::Play: {
                if (message.fields.size() != 1) break;
                const int value = std::atoi(message.fields[0].c_str());
                if (value < 1 || value > 3) break;
                if (game_.currentTurn() != Side::Cpu) break;
                if (game_.trucoPending() || game_.handFinished() ||
                    game_.awaitingElevenChoice()) {
                    break;
                }
                if (state_ != SessionState::CpuTurn) break;
                playForSide(Side::Cpu, value - 1);
                break;
            }
            case MessageType::Truco:
                if (!game_.canCallTruco(Side::Cpu)) break;
                game_.callTruco(Side::Cpu);
                activeSide_ = Side::Human;
                state_ = SessionState::TrucoDialog;
                selectedAction_ = 0;
                message_ = sideName(Side::Cpu) + " pediu " + callLabel(game_.stake()) + "!";
                refreshView();
                break;
            case MessageType::Accept:
            case MessageType::Run: {
                if (!game_.trucoPending() || game_.trucoCaller() != Side::Human) break;
                const bool accept = message.type == MessageType::Accept;
                game_.respondTruco(Side::Cpu, accept);
                if (accept) {
                    enterTurnSide(game_.currentTurn());
                } else {
                    endHand();
                }
                break;
            }
            case MessageType::Eleven: {
                if (!game_.awaitingElevenChoice() || game_.elevenSide() != Side::Cpu) break;
                if (message.fields.empty()) break;
                const bool play = message.fields[0] == "PLAY";
                if (!play && message.fields[0] != "HIDE") break;
                game_.chooseHandOfEleven(Side::Cpu, play);
                message_ = play ? sideName(Side::Cpu) + " jogou a mao de 11 (3 pontos)."
                                : sideName(Side::Cpu) + " escondeu a mao de 11 (1 ponto).";
                endHand();
                break;
            }
            case MessageType::Rematch:
                if (state_ == SessionState::MatchResult) {
                    startLanHostMatch();
                }
                break;
            default:
                break;
        }
    } catch (const std::exception&) {
        // Invalid intent: state stays untouched.
    }
}

void Session::applyGuestSync(const SyncState& sync) {
    guestWaitingAck_ = false;
    guestView_ = sync;
    lanMatchActive_ = true;
    mode_ = GameMode::LanGuest;
    activeSide_ = localSide();
    switch (sync.phase) {
        case NetPhase::Play:
            state_ = sync.turn == localSide() ? SessionState::HumanTurn
                                              : SessionState::CpuTurn;
            break;
        case NetPhase::TrucoAnswer:
            state_ = SessionState::TrucoDialog;
            break;
        case NetPhase::Eleven:
            state_ = SessionState::ElevenChoice;
            break;
        case NetPhase::HandEnd:
            state_ = SessionState::HandResult;
            break;
        case NetPhase::MatchEnd:
            state_ = SessionState::MatchResult;
            break;
        case NetPhase::Wait:
            state_ = SessionState::CpuTurn;
            break;
    }
    message_ = guestMessage_;
    if (message_.empty()) {
        if (sync.phase == NetPhase::MatchEnd) {
            message_ = sync.scoreCpu >= 12 ? "Jogador 2 venceu a partida!"
                                           : "Jogador 1 venceu a partida!";
        } else if (sync.phase == NetPhase::TrucoAnswer) {
            message_ = "Decisao de truco";
        } else if (sync.phase == NetPhase::Eleven) {
            message_ = "Mao de 11";
        }
    }
    refreshView();
}

NetPhase Session::hostPhase() const {
    switch (state_) {
        case SessionState::TrucoDialog:
            return NetPhase::TrucoAnswer;
        case SessionState::ElevenChoice:
            return NetPhase::Eleven;
        case SessionState::HandResult:
            return NetPhase::HandEnd;
        case SessionState::MatchResult:
            return NetPhase::MatchEnd;
        case SessionState::Dealing:
        case SessionState::Handoff:
        case SessionState::LanConfig:
        case SessionState::LanWaiting:
        case SessionState::LanError:
        case SessionState::Menu:
            return NetPhase::Wait;
        default:
            return NetPhase::Play;
    }
}

Side Session::hostTurnSide() const {
    if (state_ == SessionState::TrucoDialog && game_.trucoPending()) {
        return other(game_.trucoCaller());
    }
    if (state_ == SessionState::ElevenChoice) {
        return game_.elevenSide();
    }
    return game_.currentTurn();
}

SyncState Session::makeSync(Side viewer) const {
    SyncState sync;
    sync.seq = lanSeq_;
    sync.you = viewer;
    sync.phase = hostPhase();
    sync.turn = hostTurnSide();
    sync.stake = valuePoints(game_.stake());
    sync.tricksHuman = game_.progress().tricksWon(Side::Human);
    sync.tricksCpu = game_.progress().tricksWon(Side::Cpu);
    sync.scoreHuman = game_.score().human;
    sync.scoreCpu = game_.score().cpu;
    sync.handKind = game_.handKind();
    sync.canRaise = game_.canCallTruco(viewer);
    sync.own = game_.handOf(viewer);
    for (int i = 0; i < 3; ++i) {
        sync.playedOwn[static_cast<size_t>(i)] = game_.cardPlayed(viewer, i);
        sync.playedOpponent[static_cast<size_t>(i)] = game_.cardPlayed(other(viewer), i);
    }
    sync.trickOwn = game_.currentTrickCard(viewer);
    sync.trickOpponent = game_.currentTrickCard(other(viewer));
    sync.revealed = game_.revealed();
    sync.elevenSide = game_.elevenSide();
    if (sync.revealed) {
        sync.opponent = game_.handOf(other(viewer));
    }
    return sync;
}

void Session::sendGuest(MessageType type, const std::vector<std::string>& fields) {
    if (!guest_ || !guest_->connected()) {
        return;
    }
    guestWaitingAck_ = true;
    guest_->sendLine(encodeLine(type, fields));
}

void Session::broadcastIfHost() {
    if (!lanHost() || !lanHandshakeDone_ || !lanMatchActive_ || !host_ ||
        !host_->connected()) {
        return;
    }
    ++lanSeq_;
    host_->sendLine(encodeSync(makeSync(Side::Cpu)));
    if (message_ != lanLastMessage_) {
        host_->sendLine(encodeLine(MessageType::Msg, {}, message_));
        lanLastMessage_ = message_;
    }
}

void Session::refreshView() {
    view_ = ViewModel{};
    view_.mouseWarning = mouseWarning_;
    view_.humanScore = lanGuest() ? clampScore(guestView_.scoreHuman)
                                  : clampScore(game_.score().human);
    view_.cpuScore = lanGuest() ? clampScore(guestView_.scoreCpu)
                                : clampScore(game_.score().cpu);
    view_.message = message_;
    view_.selectedCard = selectedCard_;
    view_.selectedAction = selectedAction_;
    const bool lanNames = mode_ != GameMode::Cpu;
    view_.humanLabel = lanNames ? "JOGADOR 1" : "VOCE";
    view_.cpuLabel = lanNames ? "JOGADOR 2" : "COMPUTADOR";

    if (royaleMode()) {
        view_.humanScore = 0;
        view_.cpuScore = 0;
        view_.humanLabel = "J" + std::to_string((royaleHost() ? 0 : royaleSeat_) + 1);
        view_.cpuLabel = "BATTLE ROYALE";
        switch (state_) {
            case SessionState::RoyaleConfig: {
                view_.screen = ViewModel::Screen::LanConfig;
                view_.lanTitle = "Battle Royale (LAN)";
                view_.lanFieldLabel[0] = "Jogadores (3-13)";
                view_.lanFieldValue[0] = royalePlayers_.value();
                view_.lanFieldLabel[1] = "Porta";
                view_.lanFieldValue[1] = lanPort_.value();
                view_.lanFieldFocus = 0;
                view_.lanHint = "Jogadores 3-13 | Enter inicia | ESC volta";
                view_.actions = {"Hospedar", "Voltar"};
                break;
            }
            case SessionState::RoyaleLobby: {
                view_.screen = ViewModel::Screen::LanWaiting;
                view_.lanTitle = "Sala battle royale";
                std::string addresses;
                for (const std::string& address : net::localAddresses()) {
                    if (!addresses.empty()) addresses += " / ";
                    addresses += address;
                }
                view_.lanAddressText = "IP: " + addresses +
                                       "  Porta: " + std::to_string(royalePort());
                view_.actions = {"Iniciar", "Cancelar"};
                break;
            }
            case SessionState::RoyaleWaiting: {
                view_.screen = ViewModel::Screen::LanWaiting;
                view_.lanTitle = "Entrando na sala...";
                view_.actions = {"Cancelar"};
                break;
            }
            case SessionState::RoyaleResult: {
                view_.screen = ViewModel::Screen::MatchResult;
                view_.matchResultText = message_;
                view_.actions = {"Voltar ao menu"};
                break;
            }
            case SessionState::RoyalePlay:
            default: {
                view_.screen = ViewModel::Screen::Game;
                if (royaleHost() && royale_) {
                    const RoyaleSeat& own = royale_->seat(0);
                    view_.humanCards = own.hand;
                    view_.humanPlayed = own.played;
                    view_.stakeText = "MAO VALE " +
                                      std::to_string(royale_->pendingValue());
                    view_.turnText = royale_->currentTurn() == 0
                                         ? "Sua vez"
                                         : "Vez do adversario";
                    view_.trickText = "Vazas jogadas: " +
                                      std::to_string(royale_->tricksPlayed());
                    if (royale_->phase() == RoyalePhase::Playing &&
                        royale_->currentTurn() == 0) {
                        view_.actions = royale_->canCallTruco(0)
                                            ? std::vector<std::string>{"Jogar", "Truco!"}
                                            : std::vector<std::string>{"Jogar"};
                    } else if (royale_->phase() == RoyalePhase::TrucoAnswer &&
                               royale_->trucoResponder() == 0) {
                        view_.actions = {"Aceitar", "Correr"};
                    }
                } else {
                    view_.humanCards = royaleView_.own;
                    view_.humanPlayed = royaleView_.playedOwn;
                    view_.stakeText = "MAO VALE " + std::to_string(royaleView_.stake);
                    view_.turnText = royaleView_.turn == royaleSeat_
                                         ? "Sua vez"
                                         : "Vez do adversario";
                    view_.trickText = "Vazas jogadas: " +
                                      std::to_string(royaleView_.tricksPlayed);
                    if (royaleView_.phase == RoyalePhase::Playing &&
                        royaleView_.turn == royaleSeat_) {
                        view_.actions = royaleView_.canRaise
                                            ? std::vector<std::string>{"Jogar", "Truco!"}
                                            : std::vector<std::string>{"Jogar"};
                    } else if (royaleView_.phase == RoyalePhase::TrucoAnswer &&
                               royaleView_.responder == royaleSeat_) {
                        view_.actions = {"Aceitar", "Correr"};
                    }
                }
                break;
            }
        }
        return;
    }

    switch (state_) {
        case SessionState::Menu:
            view_.screen = ViewModel::Screen::Menu;
            view_.actions = {"Jogar vs CPU", "Jogar 2 jogadores", "Hospedar partida (LAN)",
                             "Conectar em partida (LAN)", "Sair"};
            break;
        case SessionState::LanConfig: {
            view_.screen = ViewModel::Screen::LanConfig;
            view_.lanTitle = lanConfigHost_ ? "Hospedar partida (LAN)"
                                            : "Conectar em partida (LAN)";
            if (lanConfigHost_) {
                view_.lanFieldLabel[0] = "Porta";
                view_.lanFieldValue[0] = lanPort_.value();
                view_.lanFieldFocus = 0;
            } else {
                view_.lanFieldLabel[0] = "IP do host";
                view_.lanFieldValue[0] = lanIp_.value();
                view_.lanFieldLabel[1] = "Porta";
                view_.lanFieldValue[1] = lanPort_.value();
                view_.lanFieldFocus = lanFocus_;
            }
            view_.lanHint = lanConfigHost_ ? "Digite a porta | Enter inicia | ESC volta"
                                           : "Tab alterna campo | Enter conecta | ESC volta";
            view_.actions = lanConfigHost_ ? std::vector<std::string>{"Hospedar", "Voltar"}
                                           : std::vector<std::string>{"Conectar", "Voltar"};
            break;
        }
        case SessionState::LanWaiting: {
            view_.screen = ViewModel::Screen::LanWaiting;
            view_.lanTitle = lanHost() ? "Hospedando partida" : "Conectando...";
            if (host_) {
                std::string addresses;
                for (const std::string& address : host_->localAddresses()) {
                    if (!addresses.empty()) addresses += " / ";
                    addresses += address;
                }
                view_.lanAddressText = "IP: " + addresses +
                                       "  Porta: " + std::to_string(host_->port());
            }
            view_.actions = {"Cancelar"};
            break;
        }
        case SessionState::LanError:
            view_.screen = ViewModel::Screen::LanError;
            view_.lanErrorText = message_;
            view_.actions = {"Voltar ao menu"};
            break;
        case SessionState::Handoff:
            view_.screen = ViewModel::Screen::Handoff;
            view_.handoffTitle = "Passe o controle para " + sideName(pendingSide_);
            view_.handoffText = message_;
            view_.actions = {"Continuar"};
            break;
        case SessionState::MatchResult:
            view_.screen = ViewModel::Screen::MatchResult;
            view_.matchResultText = message_;
            view_.actions = {"Jogar novamente", "Sair"};
            break;
        default:
            view_.screen = ViewModel::Screen::Game;
            break;
    }

    if (view_.screen == ViewModel::Screen::Menu ||
        view_.screen == ViewModel::Screen::LanConfig ||
        view_.screen == ViewModel::Screen::LanWaiting ||
        view_.screen == ViewModel::Screen::LanError ||
        view_.screen == ViewModel::Screen::Handoff ||
        view_.screen == ViewModel::Screen::MatchResult) {
        if (lanHost()) broadcastIfHost();
        return;
    }

    if (lanGuest()) {
        const SyncState& sync = guestView_;
        view_.humanCards = sync.own;
        view_.cpuCards = sync.opponent;
        for (int i = 0; i < 3; ++i) {
            view_.humanPlayed[static_cast<size_t>(i)] = sync.playedOwn[static_cast<size_t>(i)];
            view_.cpuPlayed[static_cast<size_t>(i)] = sync.playedOpponent[static_cast<size_t>(i)];
        }
        view_.humanTrickCard = sync.trickOwn;
        view_.cpuTrickCard = sync.trickOpponent;
        view_.humanTrickLabel = sideShortName(sync.you);
        view_.cpuTrickLabel = sideShortName(other(sync.you));
        view_.revealCpu = sync.revealed;
        view_.trickText = "Vazas: " + sideShortName(Side::Human) + " " +
                          std::to_string(sync.tricksHuman) + " x " +
                          std::to_string(sync.tricksCpu) + " " + sideShortName(Side::Cpu);
        if (sync.handKind == HandKind::IronHand) {
            view_.stakeText = "MAO DE FERRO VALE 3";
        } else if (sync.handKind == HandKind::HandOfEleven) {
            view_.stakeText = "MAO DE 11";
        } else {
            view_.stakeText = "MAO VALE " + std::to_string(sync.stake);
        }
        view_.turnText = sync.turn == localSide() ? "Sua vez" : "Vez do adversario";
        switch (sync.phase) {
            case NetPhase::Play:
                if (sync.turn == localSide()) {
                    view_.actions = sync.canRaise
                                        ? std::vector<std::string>{"Jogar", "Truco!"}
                                        : std::vector<std::string>{"Jogar"};
                }
                break;
            case NetPhase::TrucoAnswer:
                view_.turnText = "Decisao de truco";
                if (sync.turn == localSide()) {
                    view_.actions = {"Aceitar", "Correr"};
                }
                break;
            case NetPhase::Eleven:
                view_.turnText = "Mao de 11";
                view_.showEleven = true;
                view_.elevenText = "Jogar = 3 pontos | Esconder = 1 ponto";
                if (sync.elevenSide == localSide()) {
                    view_.actions = {"Jogar (3)", "Esconder (1)"};
                }
                break;
            default:
                break;
        }
        return;
    }

    const Side bottom = hotseat() ? activeSide_ : Side::Human;
    const Side top = other(bottom);
    view_.humanCards = game_.handOf(bottom);
    view_.cpuCards = game_.handOf(top);
    for (int i = 0; i < 3; ++i) {
        view_.humanPlayed[static_cast<size_t>(i)] = game_.cardPlayed(bottom, i);
        view_.cpuPlayed[static_cast<size_t>(i)] = game_.cardPlayed(top, i);
    }
    view_.humanTrickCard = game_.currentTrickCard(Side::Human);
    view_.cpuTrickCard = game_.currentTrickCard(Side::Cpu);
    view_.humanTrickLabel = sideShortName(Side::Human);
    view_.cpuTrickLabel = sideShortName(Side::Cpu);
    view_.revealCpu = game_.revealed();
    view_.trickText = "Vazas: " + sideShortName(Side::Human) + " " +
                      std::to_string(game_.progress().tricksWon(Side::Human)) + " x " +
                      std::to_string(game_.progress().tricksWon(Side::Cpu)) + " " +
                      sideShortName(Side::Cpu);

    if (game_.handKind() == HandKind::IronHand) {
        view_.stakeText = "MAO DE FERRO VALE 3";
    } else if (game_.handKind() == HandKind::HandOfEleven) {
        view_.stakeText = "MAO DE 11";
    } else {
        view_.stakeText = "MAO VALE " + std::to_string(valuePoints(game_.stake()));
    }

    const bool canAct = state_ == SessionState::HumanTurn ||
                        (hotseat() && state_ == SessionState::CpuTurn);
    switch (state_) {
        case SessionState::HumanTurn:
        case SessionState::CpuTurn:
            if (!canAct) {
                break;
            }
            view_.turnText = "Vez de " + sideName(activeSide_);
            view_.actions = game_.canCallTruco(activeSide_)
                                ? std::vector<std::string>{"Jogar", "Truco!"}
                                : std::vector<std::string>{"Jogar"};
            break;
        case SessionState::TrucoDialog:
            view_.turnText = "Decisao de truco";
            if (humanMustAnswerTruco()) {
                view_.actions = {"Aceitar", "Correr"};
            }
            break;
        case SessionState::ElevenChoice:
            view_.turnText = "Mao de 11";
            view_.showEleven = true;
            view_.elevenText = "Jogar = 3 pontos | Esconder = 1 ponto";
            if (game_.elevenSide() == activeSide_ &&
                (hotseat() || activeSide_ == Side::Human)) {
                view_.actions = {"Jogar (3)", "Esconder (1)"};
            }
            break;
        default:
            break;
    }

    if (lanHost()) {
        broadcastIfHost();
    }
}

uint16_t Session::royalePort() const {
    return room_ ? room_->port() : 0;
}

void Session::setRoyalePlayersForTesting(int players) {
    royaleMax_ = players;
}

void Session::openRoyaleConfig() {
    mode_ = GameMode::RoyaleHost;
    state_ = SessionState::RoyaleConfig;
    royalePlayers_.set(std::to_string(royaleMax_));
    lanPort_.set("4000");
    message_.clear();
    selectedAction_ = 0;
    refreshView();
}

void Session::confirmRoyaleConfig() {
    int players = 0;
    for (char ch : royalePlayers_.value()) {
        if (ch < '0' || ch > '9') {
            players = -1;
            break;
        }
        players = players * 10 + (ch - '0');
    }
    if (players < 3 || players > kRoyaleMaxPlayers) {
        message_ = "Jogadores deve ser de 3 a 13";
        refreshView();
        return;
    }
    uint16_t port = 0;
    if (!parsePort(lanPort_.value(), port)) {
        message_ = "Porta invalida: use 1 a 65535";
        refreshView();
        return;
    }
    if (!openRoyaleHostForTesting(port, players)) {
        return;
    }
}

bool Session::openRoyaleHostForTesting(uint16_t port, int maxPlayers) {
    royaleMax_ = std::max(3, std::min(kRoyaleMaxPlayers, maxPlayers));
    room_ = std::make_unique<net::RoomServer>();
    std::string error;
    if (!room_->listen(port, error)) {
        room_.reset();
        message_ = "Erro: " + error;
        refreshView();
        return false;
    }
    royale_ = nullptr;
    royaleJoined_.clear();
    royaleRevealScheduled_ = false;
    royaleSeat_ = 0;
    mode_ = GameMode::RoyaleHost;
    state_ = SessionState::RoyaleLobby;
    message_ = "Aguardando jogadores";
    selectedAction_ = 0;
    refreshView();
    return true;
}

bool Session::openRoyaleJoinForTesting(const std::string& ip, uint16_t port) {
    guest_ = std::make_unique<net::GuestClient>();
    std::string error;
    if (!guest_->connectTo(ip, port, 3000, error)) {
        guest_.reset();
        message_ = "Erro: " + error;
        refreshView();
        return false;
    }
    mode_ = GameMode::RoyaleGuest;
    state_ = SessionState::RoyaleWaiting;
    royaleSeat_ = -1;
    royaleView_ = RoyaleView{};
    guest_->sendLine(encodeLine(MessageType::Hello, {"ROYALE", "1"}));
    message_ = "Conectando...";
    selectedAction_ = 0;
    refreshView();
    return true;
}

void Session::pumpRoyaleLobby() {
    if (!room_) {
        return;
    }
    room_->acceptPending();
    while (royaleJoined_.size() < static_cast<size_t>(room_->clientCount())) {
        royaleJoined_.push_back(false);
    }
    for (int client = 0; client < room_->clientCount(); ++client) {
        if (!room_->connected(client)) {
            continue;
        }
        for (const std::string& line : room_->readLines(client)) {
            const Message message = parseLine(line);
            if (message.type == MessageType::Hello) {
                const bool valid = message.fields.size() >= 2 &&
                                   message.fields[0] == "ROYALE" &&
                                   message.fields[1] == "1";
                if (!valid) {
                    room_->sendLine(client, encodeLine(MessageType::Error, {"VERSION"},
                                                       "versao incompativel"));
                    room_->dropClient(client);
                    continue;
                }
                room_->sendLine(client, encodeLine(MessageType::Welcome,
                                                   {"ROYALE", "1",
                                                    std::to_string(client + 1)}));
                royaleJoined_[static_cast<size_t>(client)] = true;
            }
        }
    }
    std::vector<std::string> names{"J1"};
    int connected = 0;
    for (int client = 0; client < room_->clientCount(); ++client) {
        if (royaleJoined_[static_cast<size_t>(client)] && room_->connected(client)) {
            ++connected;
            names.push_back("J" + std::to_string(client + 2));
        }
    }
    room_->broadcast(encodeLobby(names));
    message_ = "Jogadores: " + std::to_string(connected + 1) + "/" +
               std::to_string(royaleMax_);
    refreshView();
}

void Session::startRoyaleMatch() {
    if (!room_) {
        return;
    }
    int joined = 0;
    for (int client = 0; client < room_->clientCount(); ++client) {
        if (!royaleJoined_[static_cast<size_t>(client)] || !room_->connected(client)) {
            if (room_->connected(client)) {
                room_->dropClient(client);
            }
            continue;
        }
        ++joined;
    }
    if (joined + 1 < 3) {
        message_ = "Conecte pelo menos 3 jogadores";
        refreshView();
        return;
    }
    royale_ = std::make_unique<RoyaleMatch>(joined + 1, rng_);
    royaleRevealScheduled_ = false;
    state_ = SessionState::RoyalePlay;
    message_ = "Partida iniciada";
    until_ = now_;
    room_->broadcast(encodeLine(MessageType::Start, {"ROYALE"}));
    broadcastRoyale();
    refreshView();
}

RoyaleView Session::makeRoyaleView(int seat) const {
    RoyaleView view;
    view.seq = royaleSeq_;
    view.you = seat;
    view.phase = royale_->phase();
    view.turn = royale_->currentTurn();
    view.stake = royale_->pendingValue();
    view.caller = royale_->trucoCaller();
    view.responder = royale_->trucoResponder();
    view.handWinner = royale_->handWinner();
    view.matchWinner = royale_->matchWinner();
    view.tricksPlayed = royale_->tricksPlayed();
    view.playerCount = royale_->playerCount();
    view.canRaise = royale_->canCallTruco(seat);
    const bool revealAll = royale_->phase() == RoyalePhase::HandEnd ||
                           royale_->phase() == RoyalePhase::MatchEnd;
    for (int i = 0; i < view.playerCount; ++i) {
        const RoyaleSeat& source = royale_->seat(i);
        RoyaleSeatView& target = view.seats.at(static_cast<size_t>(i));
        target.points = source.points;
        target.eliminated = source.eliminated;
        target.inHand = source.inHand;
        target.tableCard = source.tableCard;
        if (revealAll || !source.inHand) {
            target.revealed = source.hand;
        }
    }
    const RoyaleSeat& own = royale_->seat(seat);
    view.own = own.hand;
    view.playedOwn = own.played;
    return view;
}

void Session::broadcastRoyale() {
    if (!room_ || !royale_) {
        return;
    }
    ++royaleSeq_;
    for (int client = 0; client < room_->clientCount(); ++client) {
        if (!room_->connected(client)) {
            continue;
        }
        room_->sendLine(client, encodeRoyaleState(makeRoyaleView(client + 1)));
    }
}

void Session::applyRoyaleIntent(int seat, const Message& message) {
    if (!royale_) {
        return;
    }
    try {
        switch (message.type) {
            case MessageType::Play: {
                if (message.fields.size() != 1) break;
                const int value = std::atoi(message.fields[0].c_str());
                if (value < 1 || value > 3) break;
                if (!royale_->canPlay(seat)) break;
                royale_->playCard(seat, value - 1);
                break;
            }
            case MessageType::Truco:
                if (royale_->canCallTruco(seat)) {
                    royale_->callTruco(seat);
                }
                break;
            case MessageType::Accept:
            case MessageType::Run:
                if (royale_->phase() != RoyalePhase::TrucoAnswer ||
                    royale_->trucoResponder() != seat) {
                    break;
                }
                royale_->respondTruco(message.type == MessageType::Accept);
                break;
            default:
                break;
        }
    } catch (const std::exception&) {
    }
}

void Session::pumpRoyaleHost() {
    if (!room_ || !royale_) {
        return;
    }
    room_->acceptPending();
    for (int client = 0; client < room_->clientCount(); ++client) {
        const size_t index = static_cast<size_t>(client);
        if (index >= royaleJoined_.size() || !royaleJoined_[index] ||
            !room_->connected(client)) {
            if (room_->connected(client) && index >= royaleJoined_.size()) {
                room_->sendLine(client, encodeLine(MessageType::Error, {"STARTED"},
                                                   "partida ja iniciada"));
                room_->dropClient(client);
            }
            if (index < royaleJoined_.size() && royaleJoined_[index] &&
                !room_->connected(client)) {
                royaleJoined_[index] = false;
                royale_->removePlayer(client + 1);
            }
            continue;
        }
        for (const std::string& line : room_->readLines(client)) {
            applyRoyaleIntent(client + 1, parseLine(line));
        }
    }

    if (royale_->phase() == RoyalePhase::HandEnd) {
        if (!royaleRevealScheduled_) {
            royaleRevealScheduled_ = true;
            until_ = now_ + std::chrono::milliseconds(1500);
        } else if (now_ >= until_) {
            royaleRevealScheduled_ = false;
            royale_->startHand();
        }
    }
    if (royale_->matchOver()) {
        state_ = SessionState::RoyaleResult;
        message_ = "Jogador " + std::to_string(royale_->matchWinner() + 1) +
                   " venceu!";
    }
    broadcastRoyale();
    refreshView();
}

void Session::applyRoyaleState(const RoyaleView& view) {
    royaleView_ = view;
    royaleSeat_ = view.you;
    if (view.phase == RoyalePhase::MatchEnd) {
        state_ = SessionState::RoyaleResult;
        message_ = view.matchWinner >= 0
                       ? "Jogador " + std::to_string(view.matchWinner + 1) + " venceu!"
                       : "Partida encerrada";
    } else {
        state_ = SessionState::RoyalePlay;
        if (view.phase == RoyalePhase::TrucoAnswer) {
            message_ = "Decisao de truco";
        } else if (view.phase == RoyalePhase::HandEnd) {
            message_ = "Fim da mao";
        } else {
            message_ = view.turn == royaleSeat_ ? "Sua vez" : "Vez do adversario";
        }
    }
    refreshView();
}

void Session::pumpRoyaleGuest() {
    if (!guest_) {
        return;
    }
    if (guest_->disconnected()) {
        enterRoyaleError("Conexao perdida com o host");
        return;
    }
    for (const std::string& line : guest_->readLines()) {
        const Message message = parseLine(line);
        switch (message.type) {
            case MessageType::Welcome: {
                const bool valid = message.fields.size() >= 3 &&
                                   message.fields[0] == "ROYALE" &&
                                   message.fields[1] == "1";
                if (!valid) {
                    enterRoyaleError("Versao de protocolo incompativel");
                    return;
                }
                royaleSeat_ = std::atoi(message.fields[2].c_str());
                state_ = SessionState::RoyalePlay;
                message_ = "Conectado como J" + std::to_string(royaleSeat_ + 1);
                refreshView();
                break;
            }
            case MessageType::Start:
                state_ = SessionState::RoyalePlay;
                break;
            case MessageType::RState: {
                RoyaleView view;
                if (decodeRoyaleState(line, view)) {
                    applyRoyaleState(view);
                }
                break;
            }
            case MessageType::Error:
                enterRoyaleError(message.text.empty() ? "Erro do host" : message.text);
                return;
            case MessageType::Msg:
                message_ = message.text;
                refreshView();
                break;
            default:
                break;
        }
    }
}

void Session::enterRoyaleError(const std::string& reason) {
    if (room_) {
        room_->shutdown();
        room_.reset();
    }
    if (guest_) {
        guest_->close();
        guest_.reset();
    }
    royale_ = nullptr;
    state_ = SessionState::LanError;
    selectedAction_ = 0;
    message_ = reason;
    refreshView();
}

void Session::royaleHostPlay(int index) {
    if (!royale_ || !royale_->canPlay(0)) {
        return;
    }
    try {
        royale_->playCard(0, index);
    } catch (const std::exception&) {
        return;
    }
    selectedCard_ = -1;
    message_ = "Voce jogou";
    broadcastRoyale();
    refreshView();
}

void Session::royaleHostAction(int index) {
    switch (state_) {
        case SessionState::RoyaleConfig:
            if (index == 0) {
                confirmRoyaleConfig();
            } else {
                resetToMenu();
            }
            break;
        case SessionState::RoyaleLobby:
            if (index == 0) {
                startRoyaleMatch();
            } else {
                resetToMenu();
            }
            break;
        case SessionState::RoyalePlay:
            if (!royale_) {
                break;
            }
            if (royale_->phase() == RoyalePhase::TrucoAnswer) {
                if (royale_->trucoResponder() != 0) {
                    break;
                }
                try {
                    royale_->respondTruco(index == 0);
                } catch (const std::exception&) {
                    break;
                }
            } else if (royale_->phase() == RoyalePhase::Playing) {
                if (royale_->currentTurn() != 0) {
                    break;
                }
                if (index == 0) {
                    confirmSelection();
                } else if (index == 1 && royale_->canCallTruco(0)) {
                    royale_->callTruco(0);
                    message_ = "Voce pediu truco";
                }
            }
            broadcastRoyale();
            refreshView();
            break;
        case SessionState::RoyaleResult:
            resetToMenu();
            break;
        default:
            break;
    }
}

void Session::handleEvent(const InputEvent& event) {
    switch (event.kind) {
        case InputEvent::Kind::Quit:
            running_ = false;
            return;
        case InputEvent::Kind::Resize:
            refreshView();
            return;
        case InputEvent::Kind::MouseClick:
            return;
        case InputEvent::Kind::MouseMove:
        case InputEvent::Kind::None:
            return;
        case InputEvent::Kind::Key:
            break;
    }

    const char key = event.key;

    if (state_ == SessionState::LanConfig) {
        if (key == '\t') {
            if (!lanConfigHost_) {
                lanFocus_ = lanFocus_ == 0 ? 1 : 0;
                refreshView();
            }
            return;
        }
        if (key == '\r') {
            confirmLanConfig();
            return;
        }
        if (key == 27) {
            cancelLan();
            return;
        }
        TextInput& field = (lanConfigHost_ || lanFocus_ == 1) ? lanPort_ : lanIp_;
        if (field.handleKey(key)) {
            refreshView();
        }
        return;
    }

    if (key == 'q' || key == 'Q') {
        running_ = false;
        return;
    }
    if (key == 27) {
        cancelSelection();
        return;
    }
    if (key == '\r') {
        if (canSelectCards()) {
            confirmSelection();
        } else {
            activateAction(selectedAction_);
        }
        return;
    }
    if (key >= '1' && key <= '5') {
        const int index = key - '1';
        if (canSelectCards() && index < 3) {
            selectCard(index);
        } else {
            activateAction(index);
        }
    }
}

int Session::run() {
    Terminal terminal;
    Renderer renderer(terminal);
    setMouseWarning(!terminal.mouseSupported());
    start();

    while (running_) {
        const InputEvent event = pollEvent(30);
        if (event.kind == InputEvent::Kind::MouseClick) {
            for (const HitTarget& target : renderer.hitTargets()) {
                if (target.rect.contains(event.x, event.y)) {
                    if (target.actionId >= 100) {
                        activateAction(target.actionId - 100);
                    } else if (target.actionId >= 0 && target.actionId < 3) {
                        selectCard(target.actionId);
                    }
                    break;
                }
            }
        } else {
            handleEvent(event);
        }
        tick(Clock::now());
        if (!running_) {
            break;
        }
        renderer.draw(view_, terminal.size());
    }
    return 0;
}

}  // namespace truco
