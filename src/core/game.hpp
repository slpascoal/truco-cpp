#pragma once

#include <array>
#include <optional>
#include <random>
#include <stdexcept>

#include "core/deck.hpp"
#include "core/hand.hpp"
#include "core/hand_value.hpp"

namespace truco {

enum class HandKind { Normal, HandOfEleven, IronHand };

struct GameScore {
    int human = 0;
    int cpu = 0;
};

class Game {
public:
    explicit Game(std::mt19937& rng);

    void startMatch();
    void startHand();

    // Test seam: plants a score so mão de 11 / mão de ferro can be exercised
    // deterministically without playing dozens of hands.
    void setScoreForTesting(GameScore score);

    const Deal& deal() const { return deal_; }
    HandKind handKind() const { return kind_; }
    Stake stake() const { return stake_; }
    Side dealer() const { return dealer_; }
    GameScore score() const { return score_; }
    Side currentTurn() const { return currentTurn_; }
    bool matchOver() const;
    Side matchWinner() const;

    const std::array<Card, 3>& handOf(Side side) const;
    bool cardPlayed(Side side, int index) const;
    std::optional<Card> currentTrickCard(Side side) const;

    const HandProgress& progress() const { return progress_; }
    HandProgress& progress() { return progress_; }

    bool handFinished() const { return finished_; }
    Side handWinner() const { return winner_; }
    bool revealed() const { return revealed_; }
    void revealAll() { revealed_ = true; }

    bool awaitingElevenChoice() const { return awaitingEleven_; }
    Side elevenSide() const { return elevenSide_; }
    Card elevenCard() const;

    bool trucoPending() const { return pending_; }
    Side trucoCaller() const { return caller_; }
    Stake pendingStake() const { return pendingStake_; }
    bool canCallTruco(Side side) const;
    void callTruco(Side side);
    void respondTruco(Side responder, bool accept);
    void runFromTruco(Side responder);

    void playCard(Side side, int index);
    void chooseHandOfEleven(Side side, bool play);

private:
    static Side other(Side side);
    void resetHand();
    void settle(Side winner);
    void resolveShowdown(Side elevenSide);
    void resolveTrick();

    std::mt19937& rng_;
    GameScore score_;
    Side dealer_ = Side::Cpu;
    Deal deal_{};
    HandKind kind_ = HandKind::Normal;
    Stake stake_ = Stake::One;
    HandProgress progress_{Side::Human};
    Side currentTurn_ = Side::Human;
    std::array<std::array<bool, 3>, 2> played_{};
    std::array<std::optional<Card>, 2> trickCards_{};
    Side trickLeader_ = Side::Human;
    bool pending_ = false;
    Side caller_ = Side::Human;
    Stake pendingStake_ = Stake::One;
    bool awaitingEleven_ = false;
    Side elevenSide_ = Side::Human;
    bool finished_ = true;
    Side winner_ = Side::Human;
    bool revealed_ = false;
};

}  // namespace truco
