#include "ai/cpu_player.hpp"

#include <algorithm>
#include <vector>

namespace truco {
namespace {

double normalizedStrength(const PublicView& view) {
    int total = 0;
    for (int i = 0; i < 3; ++i) {
        if (!view.played[static_cast<size_t>(i)]) {
            total += cardStrength(view.hand[static_cast<size_t>(i)]);
        }
    }
    return static_cast<double>(total) / 39.0;
}

double unit(std::mt19937& rng) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

std::vector<int> availableCards(const PublicView& view) {
    std::vector<int> indices;
    for (int i = 0; i < 3; ++i) {
        if (!view.played[static_cast<size_t>(i)]) {
            indices.push_back(i);
        }
    }
    return indices;
}

int weakest(const PublicView& view, const std::vector<int>& indices) {
    int best = indices.front();
    for (int idx : indices) {
        if (cardStrength(view.hand[static_cast<size_t>(idx)]) <
            cardStrength(view.hand[static_cast<size_t>(best)])) {
            best = idx;
        }
    }
    return best;
}

int strongest(const PublicView& view, const std::vector<int>& indices) {
    int best = indices.front();
    for (int idx : indices) {
        if (cardStrength(view.hand[static_cast<size_t>(idx)]) >
            cardStrength(view.hand[static_cast<size_t>(best)])) {
            best = idx;
        }
    }
    return best;
}

}  // namespace

int chooseCardIndex(const PublicView& view, std::mt19937& rng) {
    (void)rng;
    const std::vector<int> indices = availableCards(view);
    if (indices.empty()) {
        return -1;
    }

    if (view.opponentCardInTrick.has_value()) {
        const int opponent = cardStrength(*view.opponentCardInTrick);
        int cheapestWinner = -1;
        for (int idx : indices) {
            const int strength =
                cardStrength(view.hand[static_cast<size_t>(idx)]);
            if (strength > opponent &&
                (cheapestWinner < 0 || strength <
                                           cardStrength(view.hand[static_cast<size_t>(
                                               cheapestWinner)]))) {
                cheapestWinner = idx;
            }
        }
        if (cheapestWinner >= 0) {
            return cheapestWinner;
        }
        return weakest(view, indices);
    }

    // Leading: if this trick decides the hand, go with the strongest card.
    if (view.progress.trickIndex() == 1) {
        const int mine = view.progress.tricksWon(Side::Cpu);
        const int theirs = view.progress.tricksWon(Side::Human);
        if (mine == 1 || theirs == 1) {
            return strongest(view, indices);
        }
    }

    // Otherwise lead low, preferring a non-manilha.
    std::vector<int> commons;
    for (int idx : indices) {
        if (cardStrength(view.hand[static_cast<size_t>(idx)]) < 10) {
            commons.push_back(idx);
        }
    }
    return weakest(view, commons.empty() ? indices : commons);
}

bool shouldCallTruco(const PublicView& view, std::mt19937& rng) {
    if (view.trucoPending || !canRaise(view.stake)) {
        return false;
    }
    const double strength = normalizedStrength(view);
    const double roll = unit(rng);
    if (strength >= 0.55) {
        return roll < 0.70;
    }
    if (strength >= 0.40) {
        return roll < 0.35;
    }
    return roll < 0.08;
}

bool shouldAcceptTruco(const PublicView& view, std::mt19937& rng) {
    if (!view.trucoPending) {
        return false;
    }
    const double strength = normalizedStrength(view);
    const double roll = unit(rng);
    if (strength >= 0.60) {
        return roll < 0.90;
    }
    if (strength >= 0.45) {
        return roll < 0.60;
    }
    if (strength >= 0.30) {
        return roll < 0.35;
    }
    return roll < 0.12;
}

bool shouldPlayHandOfEleven(Card hidden, std::mt19937& rng) {
    const int strength = cardStrength(hidden);
    const double roll = unit(rng);
    if (strength >= 10) {
        return roll < 0.95;
    }
    if (strength >= 8) {
        return roll < 0.70;
    }
    if (strength >= 6) {
        return roll < 0.50;
    }
    if (strength >= 4) {
        return roll < 0.35;
    }
    return roll < 0.20;
}

}  // namespace truco
