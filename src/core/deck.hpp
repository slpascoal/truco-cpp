#pragma once

#include <array>
#include <random>

#include "core/card.hpp"

namespace truco {

std::array<Card, 40> buildDeck();

void shuffleDeck(std::array<Card, 40>& deck, std::mt19937& rng);

struct Deal {
    std::array<Card, 3> human;
    std::array<Card, 3> cpu;
};

// Shuffles a fresh deck: 0..2 the human hand, 3..5 the cpu hand.
Deal deal(std::mt19937& rng);

}  // namespace truco
