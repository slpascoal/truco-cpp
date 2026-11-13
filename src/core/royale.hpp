#pragma once

#include <array>
#include <optional>
#include <random>
#include <vector>

#include "core/card.hpp"
#include "core/hand_value.hpp"

namespace truco {

constexpr int kRoyaleMaxPlayers = 13;
constexpr int kRoyaleStartPoints = 12;
constexpr int kRoyaleCardsPerPlayer = 3;

enum class RoyalePhase { Playing, TrucoAnswer, HandEnd, MatchEnd };

struct RoyaleSeat {
    int points = kRoyaleStartPoints;
    bool eliminated = false;
    bool inHand = false;
    std::array<Card, 3> hand{};
    std::array<bool, 3> played{};
    std::optional<Card> tableCard;
    int tricksWon = 0;
    int bestCard = -1;
};

class RoyaleMatch {
public:
    RoyaleMatch(int playerCount, std::mt19937& rng);

    int playerCount() const { return playerCount_; }
    RoyalePhase phase() const { return phase_; }
    const RoyaleSeat& seat(int index) const;
    int aliveCount() const;
    int inHandCount() const;

    int currentTurn() const { return currentTurn_; }
    bool canPlay(int seat) const;
    void playCard(int seat, int index);
    int tricksPlayed() const { return tricksPlayed_; }
    int mao() const { return mao_; }

    int handWinner() const { return handWinner_; }
    int matchWinner() const { return matchWinner_; }
    int handValue() const { return valuePoints(handValue_); }
    bool matchOver() const { return phase_ == RoyalePhase::MatchEnd; }

    void startHand();
    void removePlayer(int seat);

    // Test seams: plant hands/points so multi-player rules can be exercised
    // deterministically without depending on shuffle output.
    void setHandForTesting(int seat, const std::array<Card, 3>& cards);
    void setPointsForTesting(int seat, int points);

private:
    int nextAlive(int seat) const;
    int nextInHand(int seat) const;
    bool seatValid(int seat) const;
    void resetSeatsForHand();
    void dealHands();
    void resolveTrick();
    void finishHandFromTricks();
    void finishHand(int winner);
    void checkMatchEnd();
    void clearTable();

    std::mt19937& rng_;
    int playerCount_ = 0;
    std::array<RoyaleSeat, kRoyaleMaxPlayers> seats_{};
    RoyalePhase phase_ = RoyalePhase::Playing;
    int mao_ = 0;
    int trickLeader_ = 0;
    int currentTurn_ = -1;
    int tricksPlayed_ = 0;
    bool handStarted_ = false;
    int handWinner_ = -1;
    int matchWinner_ = -1;
    Stake handValue_ = Stake::One;
};

}  // namespace truco
