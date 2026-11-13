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
    Card vira;
};

// Shuffles a fresh deck: index 0 is the vira, 1..3 the human hand, 4..6 the cpu hand.
Deal deal(std::mt19937& rng);

}  // namespace truco
