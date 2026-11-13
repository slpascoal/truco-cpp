#pragma once

#include <vector>

#include "core/card.hpp"

namespace truco {

enum class Side { Human, Cpu };

enum class TrickOutcome { Human, Cpu, Tie };

// `a` is the card of the player who led the trick.
TrickOutcome trickWinner(Card a, Card b, Rank vira);

// Resolves a hand from the trick outcomes already played.
// Rules: two wins take the hand; first trick tied is decided by the second;
// a tie in the deciding trick goes to the winner of the first trick;
// three ties go to the dealer (who is "mão").
Side resolveHandWinner(const std::vector<TrickOutcome>& outcomes, Side dealer);

class HandProgress {
public:
    explicit HandProgress(Side dealer = Side::Human);

    void playTrick(TrickOutcome outcome);

    int tricksWon(Side side) const;
    bool finished() const;
    Side winner() const;
    int trickIndex() const;
    TrickOutcome outcomeAt(int index) const;
    const std::vector<TrickOutcome>& outcomes() const;

private:
    void evaluate();

    Side dealer_;
    std::vector<TrickOutcome> outcomes_;
    bool finished_ = false;
    Side winner_ = Side::Human;
};

}  // namespace truco
