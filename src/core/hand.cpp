#include "core/hand.hpp"

#include <stdexcept>

namespace truco {
namespace {

int countOutcome(const std::vector<TrickOutcome>& outcomes, TrickOutcome wanted) {
    int total = 0;
    for (TrickOutcome outcome : outcomes) {
        if (outcome == wanted) {
            ++total;
        }
    }
    return total;
}

}  // namespace

TrickOutcome trickWinner(Card a, Card b) {
    const int left = cardStrength(a);
    const int right = cardStrength(b);
    if (left > right) {
        return TrickOutcome::Human;
    }
    if (right > left) {
        return TrickOutcome::Cpu;
    }
    return TrickOutcome::Tie;
}

Side resolveHandWinner(const std::vector<TrickOutcome>& outcomes, Side dealer) {
    const int human = countOutcome(outcomes, TrickOutcome::Human);
    const int cpu = countOutcome(outcomes, TrickOutcome::Cpu);

    if (human > cpu) {
        return Side::Human;
    }
    if (cpu > human) {
        return Side::Cpu;
    }
    if (human == 0 && cpu == 0) {
        return dealer;
    }
    // One win each with a tie somewhere: the first trick decides the tie.
    if (outcomes.empty()) {
        return dealer;
    }
    if (outcomes.front() == TrickOutcome::Human) {
        return Side::Human;
    }
    if (outcomes.front() == TrickOutcome::Cpu) {
        return Side::Cpu;
    }
    return dealer;
}

HandProgress::HandProgress(Side dealer) : dealer_(dealer) {}

void HandProgress::playTrick(TrickOutcome outcome) {
    if (finished_) {
        throw std::logic_error("trick played after hand finished");
    }
    outcomes_.push_back(outcome);
    evaluate();
}

void HandProgress::evaluate() {
    const int n = static_cast<int>(outcomes_.size());
    const int human = countOutcome(outcomes_, TrickOutcome::Human);
    const int cpu = countOutcome(outcomes_, TrickOutcome::Cpu);

    if (human == 2 || cpu == 2) {
        finished_ = true;
        winner_ = human == 2 ? Side::Human : Side::Cpu;
        return;
    }
    if (n == 2) {
        if (human == 1 && cpu == 0) {
            finished_ = true;
            winner_ = Side::Human;
        } else if (cpu == 1 && human == 0) {
            finished_ = true;
            winner_ = Side::Cpu;
        }
        return;
    }
    if (n >= 3) {
        finished_ = true;
        winner_ = resolveHandWinner(outcomes_, dealer_);
    }
}

int HandProgress::tricksWon(Side side) const {
    return countOutcome(outcomes_, side == Side::Human ? TrickOutcome::Human : TrickOutcome::Cpu);
}

bool HandProgress::finished() const {
    return finished_;
}

Side HandProgress::winner() const {
    return winner_;
}

int HandProgress::trickIndex() const {
    return static_cast<int>(outcomes_.size());
}

TrickOutcome HandProgress::outcomeAt(int index) const {
    return outcomes_.at(static_cast<size_t>(index));
}

const std::vector<TrickOutcome>& HandProgress::outcomes() const {
    return outcomes_;
}

}  // namespace truco
