#include "core/deck.hpp"

#include <algorithm>

namespace truco {
namespace {

constexpr Rank kRanks[] = {Rank::Four,  Rank::Five, Rank::Six,  Rank::Seven, Rank::Queen,
                           Rank::Jack,  Rank::King, Rank::Ace,  Rank::Two,   Rank::Three};

constexpr Suit kSuits[] = {Suit::Ouros, Suit::Espadas, Suit::Copas, Suit::Paus};

}  // namespace

std::array<Card, 40> buildDeck() {
    std::array<Card, 40> deck{};
    size_t index = 0;
    for (Suit suit : kSuits) {
        for (Rank rank : kRanks) {
            deck[index++] = Card{rank, suit};
        }
    }
    return deck;
}

void shuffleDeck(std::array<Card, 40>& deck, std::mt19937& rng) {
    std::shuffle(deck.begin(), deck.end(), rng);
}

Deal deal(std::mt19937& rng) {
    auto deck = buildDeck();
    shuffleDeck(deck, rng);
    return Deal{
        {deck[0], deck[1], deck[2]},
        {deck[3], deck[4], deck[5]},
    };
}

}  // namespace truco
