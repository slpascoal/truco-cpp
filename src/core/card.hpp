#pragma once

#include <string>

namespace truco {

enum class Suit { Ouros, Espadas, Copas, Paus };

enum class Rank { Four, Five, Six, Seven, Queen, Jack, King, Ace, Two, Three };

struct Card {
    Rank rank;
    Suit suit;
};

bool operator==(Card a, Card b);

// 0..9 following the common card order 4 < 5 < 6 < 7 < Q < J < K < A < 2 < 3.
int commonStrength(Rank r);

// 0..3 following the manilha suit order Ouros < Espadas < Copas < Paus.
int suitOrder(Suit s);

// Rank immediately above the vira in the circular 4..3 sequence.
Rank manilhaRank(Rank vira);

bool isManilha(Card c, Rank vira);

// Common cards: 0..9. Manilhas always outrank them: 10..13.
int cardStrength(Card c, Rank vira);

std::string rankLabel(Rank r);
std::string suitLabel(Suit s);
std::string suitName(Suit s);

}  // namespace truco
