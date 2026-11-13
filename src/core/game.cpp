#include "core/game.hpp"

#include <algorithm>

namespace truco {

Game::Game(std::mt19937& rng) : rng_(rng) {}

Side Game::other(Side side) {
    return side == Side::Human ? Side::Cpu : Side::Human;
}

void Game::startMatch() {
    score_ = GameScore{};
    dealer_ = Side::Cpu;
    startHand();
}

void Game::setScoreForTesting(GameScore score) {
    score_ = score;
}

void Game::startHand() {
    if (matchOver()) {
        throw std::logic_error("match already finished");
    }
    dealer_ = other(dealer_);
    resetHand();
}

void Game::resetHand() {
    deal_ = truco::deal(rng_);
    kind_ = HandKind::Normal;
    stake_ = Stake::One;
    progress_ = HandProgress(dealer_);
    currentTurn_ = dealer_;
    played_ = {};
    trickCards_ = {};
    trickLeader_ = dealer_;
    pending_ = false;
    caller_ = Side::Human;
    pendingStake_ = Stake::One;
    awaitingEleven_ = false;
    elevenSide_ = Side::Human;
    finished_ = false;
    winner_ = Side::Human;
    revealed_ = false;

    const bool humanEleven = score_.human == 11;
    const bool cpuEleven = score_.cpu == 11;
    if (humanEleven && cpuEleven) {
        kind_ = HandKind::IronHand;
        stake_ = Stake::Three;
        revealed_ = true;
        const int humanCard = cardStrength(deal_.human[0], deal_.vira.rank);
        const int cpuCard = cardStrength(deal_.cpu[0], deal_.vira.rank);
        Side winner = humanCard > cpuCard ? Side::Human : Side::Cpu;
        if (humanCard == cpuCard) {
            winner = dealer_;
        }
        settle(winner);
        return;
    }
    if (humanEleven || cpuEleven) {
        kind_ = HandKind::HandOfEleven;
        elevenSide_ = humanEleven ? Side::Human : Side::Cpu;
        awaitingEleven_ = true;
    }
}

bool Game::matchOver() const {
    return score_.human >= 12 || score_.cpu >= 12;
}

Side Game::matchWinner() const {
    return score_.human >= 12 ? Side::Human : Side::Cpu;
}

const std::array<Card, 3>& Game::handOf(Side side) const {
    return side == Side::Human ? deal_.human : deal_.cpu;
}

bool Game::cardPlayed(Side side, int index) const {
    return played_[static_cast<int>(side)][static_cast<size_t>(index)];
}

std::optional<Card> Game::currentTrickCard(Side side) const {
    return trickCards_[static_cast<int>(side)];
}

Card Game::elevenCard() const {
    return handOf(elevenSide_)[0];
}

bool Game::canCallTruco(Side side) const {
    if (finished_ || pending_ || awaitingEleven_) {
        return false;
    }
    if (kind_ != HandKind::Normal || matchOver()) {
        return false;
    }
    if (side != currentTurn_) {
        return false;
    }
    return canRaise(stake_);
}

void Game::callTruco(Side side) {
    if (!canCallTruco(side)) {
        throw std::logic_error("truco not allowed now");
    }
    pending_ = true;
    caller_ = side;
    pendingStake_ = nextStake(stake_);
}

void Game::respondTruco(Side responder, bool accept) {
    if (!pending_) {
        throw std::logic_error("no truco pending");
    }
    if (responder != other(caller_)) {
        throw std::logic_error("only the opponent may answer truco");
    }
    if (accept) {
        stake_ = pendingStake_;
        pending_ = false;
        return;
    }
    const Side caller = caller_;
    pending_ = false;
    settle(caller);
}

void Game::runFromTruco(Side responder) {
    respondTruco(responder, false);
}

void Game::playCard(Side side, int index) {
    if (finished_ || awaitingEleven_) {
        throw std::logic_error("hand not accepting cards");
    }
    if (pending_) {
        throw std::logic_error("waiting for truco answer");
    }
    if (side != currentTurn_) {
        throw std::logic_error("not this side's turn");
    }
    if (index < 0 || index > 2) {
        throw std::out_of_range("card index out of range");
    }
    if (played_[static_cast<int>(side)][static_cast<size_t>(index)]) {
        throw std::logic_error("card already played");
    }

    const bool startingTrick = !trickCards_[0].has_value() && !trickCards_[1].has_value();
    if (startingTrick) {
        trickLeader_ = side;
    }
    played_[static_cast<int>(side)][static_cast<size_t>(index)] = true;
    trickCards_[static_cast<int>(side)] = handOf(side)[static_cast<size_t>(index)];

    if (trickCards_[0].has_value() && trickCards_[1].has_value()) {
        resolveTrick();
    } else {
        currentTurn_ = other(side);
    }
}

void Game::resolveTrick() {
    const Card first = *trickCards_[static_cast<int>(trickLeader_)];
    const Card second = *trickCards_[static_cast<int>(other(trickLeader_))];
    const TrickOutcome local = trickWinner(first, second, deal_.vira.rank);

    Side trickWinnerSide = dealer_;
    if (local != TrickOutcome::Tie) {
        trickWinnerSide = local == TrickOutcome::Human ? trickLeader_ : other(trickLeader_);
    }
    const TrickOutcome outcome =
        local == TrickOutcome::Tie
            ? TrickOutcome::Tie
            : (trickWinnerSide == Side::Human ? TrickOutcome::Human : TrickOutcome::Cpu);

    trickCards_ = {};
    progress_.playTrick(outcome);
    if (progress_.finished()) {
        settle(progress_.winner());
        return;
    }
    currentTurn_ = local == TrickOutcome::Tie ? dealer_ : trickWinnerSide;
}

void Game::settle(Side winner) {
    if (finished_) {
        return;
    }
    const int points = valuePoints(stake_);
    if (winner == Side::Human) {
        score_.human += points;
    } else {
        score_.cpu += points;
    }
    finished_ = true;
    winner_ = winner;
    revealed_ = true;
    awaitingEleven_ = false;
    pending_ = false;
}

void Game::chooseHandOfEleven(Side side, bool play) {
    if (!awaitingEleven_ || kind_ != HandKind::HandOfEleven) {
        throw std::logic_error("no mão de 11 awaiting choice");
    }
    if (side != elevenSide_) {
        throw std::logic_error("only the eleven holder chooses");
    }
    stake_ = play ? Stake::Three : Stake::One;
    awaitingEleven_ = false;
    revealed_ = true;
    resolveShowdown(side);
}

void Game::resolveShowdown(Side elevenSide) {
    const Card mine = handOf(elevenSide)[0];
    const Side opponent = other(elevenSide);
    int bestOpponent = -1;
    for (const Card& card : handOf(opponent)) {
        bestOpponent = std::max(bestOpponent, cardStrength(card, deal_.vira.rank));
    }
    const int mineStrength = cardStrength(mine, deal_.vira.rank);
    Side winner = elevenSide;
    if (bestOpponent > mineStrength) {
        winner = opponent;
    }
    settle(winner);
}

}  // namespace truco
