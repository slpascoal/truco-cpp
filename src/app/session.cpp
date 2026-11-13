#include "app/session.hpp"

#include <algorithm>
#include <thread>

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

std::string winnerPointsText(const Game& game) {
    const int points = valuePoints(game.stake());
    if (game.handWinner() == Side::Human) {
        return "Voce ganhou a mao (+" + std::to_string(points) + ")";
    }
    return "Computador ganhou a mao (+" + std::to_string(points) + ")";
}

}  // namespace

std::string sessionStateName(SessionState state) {
    switch (state) {
        case SessionState::Menu:
            return "Menu";
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
    message_ = "Escolha Jogar para comecar";
    refreshView();
}

void Session::setMouseWarning(bool value) {
    mouseWarning_ = value;
    refreshView();
}

void Session::beginMatch() {
    game_.startMatch();
    message_ = "Nova partida!";
    enterHandStart();
}

void Session::enterHandStart() {
    selectedCard_ = -1;
    selectedAction_ = 0;
    if (game_.handFinished()) {
        endHand();
        return;
    }
    if (game_.awaitingElevenChoice()) {
        state_ = SessionState::ElevenChoice;
        if (game_.elevenSide() == Side::Human) {
            message_ = "Mao de 11! Jogar vale 3, esconder vale 1.";
            until_ = now_;
        } else {
            message_ = "O computador esta na mao de 11...";
            until_ = now_ + milliseconds(900);
        }
        refreshView();
        return;
    }
    state_ = SessionState::Dealing;
    until_ = now_ + milliseconds(800);
    message_ = "Distribuindo as cartas...";
    refreshView();
}

void Session::beginTurn() {
    if (game_.handFinished()) {
        endHand();
        return;
    }
    if (game_.awaitingElevenChoice()) {
        state_ = SessionState::ElevenChoice;
        until_ = game_.elevenSide() == Side::Human ? now_ : now_ + milliseconds(800);
        refreshView();
        return;
    }
    selectedCard_ = -1;
    if (game_.currentTurn() == Side::Human) {
        state_ = SessionState::HumanTurn;
        message_ = "Sua vez";
    } else {
        state_ = SessionState::CpuTurn;
        until_ = now_ + milliseconds(700);
        message_ = "Vez do computador...";
    }
    refreshView();
}

void Session::endHand() {
    state_ = SessionState::HandResult;
    until_ = now_ + milliseconds(1500);
    message_ = winnerPointsText(game_);
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
            if (now_ >= until_) {
                handleCpuTurn();
            }
            break;
        case SessionState::TrucoDialog:
            if (cpuAnsweringTruco_ && now_ >= until_) {
                cpuAnsweringTruco_ = false;
                if (shouldAcceptTruco(buildCpuView(), aiRng_)) {
                    game_.respondTruco(Side::Cpu, true);
                    message_ = "O computador aceitou!";
                    state_ = SessionState::HumanTurn;
                    refreshView();
                } else {
                    game_.respondTruco(Side::Cpu, false);
                    message_ = "O computador correu. Voce leva os pontos!";
                    endHand();
                }
            }
            break;
        case SessionState::ElevenChoice:
            if (game_.elevenSide() == Side::Cpu && now_ >= until_) {
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
                    message_ = game_.matchWinner() == Side::Human ? "Voce venceu a partida!"
                                                                  : "O computador venceu a partida.";
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

void Session::humanPlays(int index) {
    if (game_.cardPlayed(Side::Human, index)) {
        return;
    }
    const Card played = game_.handOf(Side::Human)[static_cast<size_t>(index)];
    game_.playCard(Side::Human, index);
    selectedCard_ = -1;
    message_ = "Voce jogou " + cardLabel(played);
    state_ = SessionState::TrickReveal;
    until_ = now_ + milliseconds(700);
    refreshView();
}

void Session::selectCard(int index) {
    if (state_ != SessionState::HumanTurn) {
        return;
    }
    if (index < 0 || index > 2 || game_.cardPlayed(Side::Human, index)) {
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
    if (state_ != SessionState::HumanTurn) {
        return;
    }
    if (selectedCard_ >= 0 && selectedCard_ <= 2 &&
        !game_.cardPlayed(Side::Human, selectedCard_)) {
        humanPlays(selectedCard_);
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
            if (index == 0) {
                beginMatch();
            } else if (index == 1) {
                running_ = false;
            }
            break;
        case SessionState::HumanTurn:
            if (index == 0) {
                confirmSelection();
            } else if (index == 1 && game_.canCallTruco(Side::Human)) {
                const std::string label = callLabel(game_.stake());
                game_.callTruco(Side::Human);
                cpuAnsweringTruco_ = true;
                state_ = SessionState::TrucoDialog;
                until_ = now_ + milliseconds(900);
                message_ = "Voce pediu " + label + "...";
                refreshView();
            }
            break;
        case SessionState::TrucoDialog:
            if (!humanMustAnswerTruco()) {
                break;
            }
            if (index == 0) {
                game_.respondTruco(Side::Human, true);
                message_ = "Voce aceitou. A mao vale " +
                           std::to_string(valuePoints(game_.stake())) + " pontos.";
                state_ = SessionState::CpuTurn;
                until_ = now_ + milliseconds(600);
                refreshView();
            } else if (index == 1) {
                game_.respondTruco(Side::Human, false);
                message_ = "Voce correu. O computador leva os pontos.";
                endHand();
            }
            break;
        case SessionState::ElevenChoice:
            if (game_.elevenSide() != Side::Human) {
                break;
            }
            if (index == 0 || index == 1) {
                game_.chooseHandOfEleven(Side::Human, index == 0);
                message_ = index == 0 ? "Voce jogou a mao de 11 (3 pontos)."
                                      : "Voce escondeu a mao de 11 (1 ponto).";
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
    return game_.trucoPending() && game_.trucoCaller() == Side::Cpu;
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

    switch (state_) {
        case SessionState::Menu:
            view_.screen = ViewModel::Screen::Menu;
            view_.actions = {"Jogar", "Sair"};
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

    view_.humanCards = game_.handOf(Side::Human);
    view_.cpuCards = game_.handOf(Side::Cpu);
    for (int i = 0; i < 3; ++i) {
        view_.humanPlayed[static_cast<size_t>(i)] = game_.cardPlayed(Side::Human, i);
        view_.cpuPlayed[static_cast<size_t>(i)] = game_.cardPlayed(Side::Cpu, i);
    }
    view_.vira = game_.deal().vira;
    view_.humanTrickCard = game_.currentTrickCard(Side::Human);
    view_.cpuTrickCard = game_.currentTrickCard(Side::Cpu);
    view_.revealCpu = game_.revealed();

    if (game_.handKind() == HandKind::IronHand) {
        view_.stakeText = "MAO DE FERRO VALE 3";
    } else if (game_.handKind() == HandKind::HandOfEleven) {
        view_.stakeText = "MAO DE 11";
    } else {
        view_.stakeText = "MAO VALE " + std::to_string(valuePoints(game_.stake()));
    }

    switch (state_) {
        case SessionState::HumanTurn:
            view_.turnText = "Sua vez";
            view_.actions = game_.canCallTruco(Side::Human) ? std::vector<std::string>{"Jogar", "Truco!"}
                                                            : std::vector<std::string>{"Jogar"};
            break;
        case SessionState::CpuTurn:
            view_.turnText = "Vez do computador";
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
            if (game_.elevenSide() == Side::Human) {
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
        if (state_ == SessionState::HumanTurn) {
            confirmSelection();
        } else {
            activateAction(selectedAction_);
        }
        return;
    }
    if (key >= '1' && key <= '3') {
        const int index = key - '1';
        if (state_ == SessionState::HumanTurn) {
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
