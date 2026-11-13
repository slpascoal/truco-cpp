#pragma once

#include <array>
#include <optional>
#include <random>

#include "core/game.hpp"

namespace truco {

// Everything the CPU may know. It intentionally has no field holding the
// opponent's cards: the AI cannot cheat by construction.
struct PublicView {
    std::array<Card, 3> hand;
    std::array<bool, 3> played{};
    std::optional<Card> opponentCardInTrick;
    Stake stake = Stake::One;
    HandProgress progress{Side::Human};
    GameScore score;
    Side turn = Side::Cpu;
    bool trucoPending = false;
};

int chooseCardIndex(const PublicView& view, std::mt19937& rng);

bool shouldCallTruco(const PublicView& view, std::mt19937& rng);

bool shouldAcceptTruco(const PublicView& view, std::mt19937& rng);

bool shouldPlayHandOfEleven(Card hidden, std::mt19937& rng);

}  // namespace truco
