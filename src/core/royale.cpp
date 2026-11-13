#include "core/royale.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/deck.hpp"

namespace truco {

RoyaleMatch::RoyaleMatch(int playerCount, std::mt19937& rng)
    : rng_(rng), playerCount_(playerCount) {
    if (playerCount < 3 || playerCount > kRoyaleMaxPlayers) {
        throw std::out_of_range("royale player count must be 3..13");
    }
    startHand();
}

const RoyaleSeat& RoyaleMatch::seat(int index) const {
    if (!seatValid(index)) {
        throw std::out_of_range("seat out of range");
    }
    return seats_.at(static_cast<size_t>(index));
}

int RoyaleMatch::aliveCount() const {
    int alive = 0;
    for (int i = 0; i < playerCount_; ++i) {
        if (!seats_[static_cast<size_t>(i)].eliminated) {
            ++alive;
        }
    }
    return alive;
}

int RoyaleMatch::inHandCount() const {
    int count = 0;
    for (int i = 0; i < playerCount_; ++i) {
        if (seats_[static_cast<size_t>(i)].inHand) {
            ++count;
        }
    }
    return count;
}

bool RoyaleMatch::seatValid(int seat) const {
    return seat >= 0 && seat < playerCount_;
}

int RoyaleMatch::nextAlive(int seat) const {
    for (int step = 1; step <= playerCount_; ++step) {
        const int candidate = (seat + step) % playerCount_;
        if (!seats_[static_cast<size_t>(candidate)].eliminated) {
            return candidate;
        }
    }
    return -1;
}

int RoyaleMatch::nextInHand(int seat) const {
    for (int step = 1; step <= playerCount_; ++step) {
        const int candidate = (seat + step) % playerCount_;
        if (seats_[static_cast<size_t>(candidate)].inHand) {
            return candidate;
        }
    }
    return -1;
}

void RoyaleMatch::startHand() {
    if (aliveCount() <= 1) {
        phase_ = RoyalePhase::MatchEnd;
        return;
    }
    if (handStarted_) {
        mao_ = nextAlive(mao_);
    }
    handStarted_ = true;
    resetSeatsForHand();
    dealHands();
    phase_ = RoyalePhase::Playing;
    trickLeader_ = mao_;
    currentTurn_ = mao_;
    tricksPlayed_ = 0;
    handWinner_ = -1;
    handValue_ = Stake::One;
}

void RoyaleMatch::resetSeatsForHand() {
    for (int i = 0; i < playerCount_; ++i) {
        RoyaleSeat& s = seats_[static_cast<size_t>(i)];
        s.inHand = !s.eliminated;
        s.played = {false, false, false};
        s.tableCard.reset();
        s.tricksWon = 0;
        s.bestCard = -1;
    }
}

void RoyaleMatch::dealHands() {
    auto deck = buildDeck();
    shuffleDeck(deck, rng_);
    size_t next = 0;
    for (int i = 0; i < playerCount_; ++i) {
        RoyaleSeat& s = seats_[static_cast<size_t>(i)];
        if (!s.inHand) {
            continue;
        }
        for (int card = 0; card < kRoyaleCardsPerPlayer; ++card) {
            s.hand[static_cast<size_t>(card)] = deck[next++];
        }
    }
}

bool RoyaleMatch::canPlay(int s) const {
    if (phase_ != RoyalePhase::Playing || !seatValid(s)) {
        return false;
    }
    const RoyaleSeat& player = seats_[static_cast<size_t>(s)];
    return player.inHand && !player.eliminated && s == currentTurn_;
}

void RoyaleMatch::playCard(int s, int index) {
    if (!canPlay(s)) {
        throw std::logic_error("seat cannot play now");
    }
    if (index < 0 || index > 2) {
        throw std::out_of_range("card index out of range");
    }
    RoyaleSeat& player = seats_[static_cast<size_t>(s)];
    if (player.played[static_cast<size_t>(index)]) {
        throw std::logic_error("card already played");
    }
    player.played[static_cast<size_t>(index)] = true;
    player.tableCard = player.hand[static_cast<size_t>(index)];
    player.bestCard =
        std::max(player.bestCard, cardStrength(player.hand[static_cast<size_t>(index)]));

    bool allPlayed = true;
    for (int i = 0; i < playerCount_; ++i) {
        const RoyaleSeat& other = seats_[static_cast<size_t>(i)];
        if (other.inHand && !other.tableCard.has_value()) {
            allPlayed = false;
            break;
        }
    }
    if (allPlayed) {
        resolveTrick();
        return;
    }
    currentTurn_ = nextInHand(s);
}

void RoyaleMatch::resolveTrick() {
    int bestStrength = -1;
    int bestSeat = -1;
    int bestCount = 0;
    for (int i = 0; i < playerCount_; ++i) {
        const RoyaleSeat& player = seats_[static_cast<size_t>(i)];
        if (!player.inHand || !player.tableCard.has_value()) {
            continue;
        }
        const int strength = cardStrength(*player.tableCard);
        if (strength > bestStrength) {
            bestStrength = strength;
            bestSeat = i;
            bestCount = 1;
        } else if (strength == bestStrength) {
            ++bestCount;
        }
    }
    const int winner = bestCount == 1 ? bestSeat : -1;
    if (winner >= 0) {
        ++seats_[static_cast<size_t>(winner)].tricksWon;
        trickLeader_ = winner;
    } else {
        trickLeader_ = nextInHand(trickLeader_);
    }
    ++tricksPlayed_;
    clearTable();
    if (tricksPlayed_ >= kRoyaleCardsPerPlayer) {
        finishHandFromTricks();
        return;
    }
    currentTurn_ = trickLeader_;
}

void RoyaleMatch::finishHandFromTricks() {
    int bestTricks = -1;
    for (int i = 0; i < playerCount_; ++i) {
        const RoyaleSeat& player = seats_[static_cast<size_t>(i)];
        if (player.inHand) {
            bestTricks = std::max(bestTricks, player.tricksWon);
        }
    }

    int winner = -1;
    for (int i = 0; i < playerCount_; ++i) {
        const RoyaleSeat& player = seats_[static_cast<size_t>(i)];
        if (!player.inHand || player.tricksWon != bestTricks) {
            continue;
        }
        if (winner < 0) {
            winner = i;
            continue;
        }
        const RoyaleSeat& current = seats_[static_cast<size_t>(winner)];
        if (player.bestCard > current.bestCard) {
            winner = i;
        } else if (player.bestCard == current.bestCard) {
            const int distance = (i - mao_ + playerCount_) % playerCount_;
            const int currentDistance = (winner - mao_ + playerCount_) % playerCount_;
            if (distance < currentDistance) {
                winner = i;
            }
        }
    }
    finishHand(winner);
}

void RoyaleMatch::finishHand(int winner) {
    if (phase_ == RoyalePhase::HandEnd || phase_ == RoyalePhase::MatchEnd) {
        return;
    }
    handWinner_ = winner;
    const int value = valuePoints(handValue_);
    for (int i = 0; i < playerCount_; ++i) {
        RoyaleSeat& player = seats_[static_cast<size_t>(i)];
        if (!player.inHand || i == winner) {
            continue;
        }
        player.points -= value;
        if (player.points <= 0) {
            player.eliminated = true;
            player.inHand = false;
        }
    }
    phase_ = RoyalePhase::HandEnd;
    currentTurn_ = -1;
    clearTable();
    checkMatchEnd();
}

void RoyaleMatch::checkMatchEnd() {
    const int alive = aliveCount();
    if (alive > 1) {
        return;
    }
    phase_ = RoyalePhase::MatchEnd;
    currentTurn_ = -1;
    matchWinner_ = -1;
    for (int i = 0; i < playerCount_; ++i) {
        if (!seats_[static_cast<size_t>(i)].eliminated) {
            matchWinner_ = i;
            break;
        }
    }
}

void RoyaleMatch::removePlayer(int s) {
    if (!seatValid(s) || seats_[static_cast<size_t>(s)].eliminated) {
        return;
    }
    RoyaleSeat& removed = seats_[static_cast<size_t>(s)];
    removed.eliminated = true;
    removed.inHand = false;
    removed.tableCard.reset();

    if (phase_ == RoyalePhase::Playing) {
        if (currentTurn_ == s) {
            currentTurn_ = nextInHand(s);
        }
        if (inHandCount() == 1) {
            const int remaining = nextInHand(s);
            finishHand(remaining);
            return;
        }
        bool allPlayed = inHandCount() > 0;
        for (int i = 0; i < playerCount_; ++i) {
            const RoyaleSeat& other = seats_[static_cast<size_t>(i)];
            if (other.inHand && !other.tableCard.has_value()) {
                allPlayed = false;
                break;
            }
        }
        if (allPlayed) {
            resolveTrick();
        }
    }
    checkMatchEnd();
}

void RoyaleMatch::clearTable() {
    for (int i = 0; i < playerCount_; ++i) {
        seats_[static_cast<size_t>(i)].tableCard.reset();
    }
}

void RoyaleMatch::setHandForTesting(int s, const std::array<Card, 3>& cards) {
    if (!seatValid(s)) {
        throw std::out_of_range("seat out of range");
    }
    seats_[static_cast<size_t>(s)].hand = cards;
}

void RoyaleMatch::setPointsForTesting(int s, int points) {
    if (!seatValid(s)) {
        throw std::out_of_range("seat out of range");
    }
    seats_[static_cast<size_t>(s)].points = points;
}

}  // namespace truco
