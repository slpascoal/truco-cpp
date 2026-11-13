#include "core/card.hpp"

namespace truco {
namespace {

constexpr Rank kRankOrder[] = {Rank::Four,  Rank::Five, Rank::Six,  Rank::Seven, Rank::Queen,
                               Rank::Jack,  Rank::King, Rank::Ace,  Rank::Two,   Rank::Three};

constexpr int kRankCount = static_cast<int>(sizeof(kRankOrder) / sizeof(kRankOrder[0]));

int rankIndex(Rank r) {
    for (int i = 0; i < kRankCount; ++i) {
        if (kRankOrder[i] == r) {
            return i;
        }
    }
    return 0;
}

int manilhaStrength(Card c) {
    if (c == Card{Rank::Four, Suit::Paus}) {
        return 13;
    }
    if (c == Card{Rank::Seven, Suit::Copas}) {
        return 12;
    }
    if (c == Card{Rank::Ace, Suit::Espadas}) {
        return 11;
    }
    if (c == Card{Rank::Seven, Suit::Ouros}) {
        return 10;
    }
    return -1;
}

}  // namespace

bool operator==(Card a, Card b) {
    return a.rank == b.rank && a.suit == b.suit;
}

int commonStrength(Rank r) {
    return rankIndex(r);
}

int suitOrder(Suit s) {
    switch (s) {
        case Suit::Ouros:
            return 0;
        case Suit::Espadas:
            return 1;
        case Suit::Copas:
            return 2;
        case Suit::Paus:
            return 3;
    }
    return 0;
}

bool isManilha(Card c) {
    return manilhaStrength(c) >= 0;
}

int cardStrength(Card c) {
    const int manilha = manilhaStrength(c);
    return manilha >= 0 ? manilha : commonStrength(c.rank);
}

std::string rankLabel(Rank r) {
    switch (r) {
        case Rank::Four:
            return "4";
        case Rank::Five:
            return "5";
        case Rank::Six:
            return "6";
        case Rank::Seven:
            return "7";
        case Rank::Queen:
            return "Q";
        case Rank::Jack:
            return "J";
        case Rank::King:
            return "K";
        case Rank::Ace:
            return "A";
        case Rank::Two:
            return "2";
        case Rank::Three:
            return "3";
    }
    return "?";
}

std::string suitLabel(Suit s) {
    switch (s) {
        case Suit::Ouros:
            return "\xE2\x99\xA6";  // ♦
        case Suit::Espadas:
            return "\xE2\x99\xA0";  // ♠
        case Suit::Copas:
            return "\xE2\x99\xA5";  // ♥
        case Suit::Paus:
            return "\xE2\x99\xA3";  // ♣
    }
    return "?";
}

std::string suitName(Suit s) {
    switch (s) {
        case Suit::Ouros:
            return "ouros";
        case Suit::Espadas:
            return "espadas";
        case Suit::Copas:
            return "copas";
        case Suit::Paus:
            return "paus";
    }
    return "?";
}

}  // namespace truco
