#include "app/session.hpp"

#include <algorithm>

#include "ui/terminal.hpp"

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
    }
    return "Unknown";
}

Session::Session(uint32_t seed)
    : rng_(seed), aiRng_(seed ^ 0x5bd1e995u), game_(rng_) {}

void Session::start() {
    now_ = Clock::now();
    until_ = now_;
    state_ = SessionState::Menu;
    message_ = "Escolha um modo de jogo";
    refreshView();
}

void Session::setMouseWarning(bool value) {
    mouseWarning_ = value;
    refreshView();
}

std::string Session::sideName(Side side) const {
    if (hotseat()) {
        return side == Side::Human ? "Jogador 1" : "Jogador 2";
    }
    return side == Side::Human ? "Voce" : "Computador";
}

std::string Session::sideShortName(Side side) const {
    if (hotseat()) {
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

void Session::beginMatch() {
    activeSide_ = Side::Human;
    pendingSide_ = Side::Human;
    handoffDoneThisHand_ = false;
    game_.startMatch();
    message_ = "Nova partida!";
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
        message_ = hotseat() ? "Vez de " + sideName(side) : "Sua vez";
        until_ = now_;
    } else {
        state_ = SessionState::CpuTurn;
        if (hotseat()) {
            message_ = "Vez de " + sideName(side);
            until_ = now_;
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
    if (hotseat() || side == Side::Human) {
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
    switch (state_) {
        case SessionState::Dealing:
            if (now_ >= until_) {
                beginTurn();
            }
            break;
        case SessionState::CpuTurn:
            if (!hotseat() && now_ >= until_) {
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
            if (!hotseat() && game_.elevenSide() == Side::Cpu && now_ >= until_) {
                const bool play = shouldPlayHandOfEleven(game_.elevenCard(), game_.deal().vira.rank,
                                                         aiRng_);
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
            if (now_ >= until_) {
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
    if (state_ == SessionState::HumanTurn) {
        return true;
    }
    return hotseat() && state_ == SessionState::CpuTurn;
}

void Session::selectCard(int index) {
    if (!canSelectCards()) {
        return;
    }
    if (index < 0 || index > 2 || game_.cardPlayed(activeSide_, index)) {
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
    if (selectedCard_ >= 0 && selectedCard_ <= 2 &&
        !game_.cardPlayed(activeSide_, selectedCard_)) {
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
    switch (state_) {
        case SessionState::Menu:
            if (index == 0 || index == 1) {
                mode_ = index == 0 ? GameMode::Cpu : GameMode::Hotseat;
                beginMatch();
            } else if (index == 2) {
                running_ = false;
            }
            break;
        case SessionState::Handoff:
            if (index == 0) {
                confirmHandoff();
            }
            break;
        case SessionState::HumanTurn:
        case SessionState::CpuTurn:
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
                game_.respondTruco(activeSide_, true);
                message_ = sideName(activeSide_) + " aceitou. A mao vale " +
                           std::to_string(valuePoints(game_.stake())) + " pontos.";
                if (hotseat()) {
                    requestHandoff(game_.currentTurn(), HandoffPurpose::Turn,
                                   "Passe o controle para " +
                                       sideName(game_.currentTurn()));
                } else {
                    enterTurnSide(game_.currentTurn());
                }
            } else if (index == 1) {
                game_.respondTruco(activeSide_, false);
                message_ = sideName(activeSide_) + " correu. " +
                           sideName(other(activeSide_)) + " leva os pontos.";
                endHand();
            }
            break;
        case SessionState::ElevenChoice:
            if (game_.elevenSide() != activeSide_) {
                break;
            }
            if (index == 0 || index == 1) {
                game_.chooseHandOfEleven(activeSide_, index == 0);
                message_ = index == 0 ? sideName(activeSide_) + " jogou a mao de 11 (3 pontos)."
                                      : sideName(activeSide_) + " escondeu a mao de 11 (1 ponto).";
                endHand();
            }
            break;
        case SessionState::MatchResult:
            if (index == 0) {
                beginMatch();
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
    view.vira = game_.deal().vira;
    view.stake = game_.stake();
    view.progress = game_.progress();
    view.score = game_.score();
    view.turn = Side::Cpu;
    view.trucoPending = game_.trucoPending();
    return view;
}

void Session::refreshView() {
    view_ = ViewModel{};
    view_.mouseWarning = mouseWarning_;
    view_.humanScore = clampScore(game_.score().human);
    view_.cpuScore = clampScore(game_.score().cpu);
    view_.message = message_;
    view_.selectedCard = selectedCard_;
    view_.selectedAction = selectedAction_;
    view_.humanLabel = hotseat() ? "JOGADOR 1" : "VOCE";
    view_.cpuLabel = hotseat() ? "JOGADOR 2" : "COMPUTADOR";

    switch (state_) {
        case SessionState::Menu:
            view_.screen = ViewModel::Screen::Menu;
            view_.actions = {"Jogar vs CPU", "Jogar 2 jogadores", "Sair"};
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

    if (view_.screen != ViewModel::Screen::Game) {
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
    view_.vira = game_.deal().vira;
    view_.humanTrickCard = game_.currentTrickCard(Side::Human);
    view_.cpuTrickCard = game_.currentTrickCard(Side::Cpu);
    view_.humanTrickLabel = sideShortName(Side::Human);
    view_.cpuTrickLabel = sideShortName(Side::Cpu);
    view_.revealCpu = game_.revealed();
    view_.manilhaText = "MANILHA: " + rankLabel(manilhaRank(game_.deal().vira.rank));
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
            if (game_.elevenSide() == activeSide_ && (hotseat() || activeSide_ == Side::Human)) {
                view_.actions = {"Jogar (3)", "Esconder (1)"};
            }
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
    if (key >= '1' && key <= '3') {
        const int index = key - '1';
        if (canSelectCards()) {
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
