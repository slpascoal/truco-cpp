#include "core/card.hpp"

#include "test_framework.hpp"

using namespace truco;

TEST(card_common_strength_order) {
    CHECK(commonStrength(Rank::Four) < commonStrength(Rank::Five));
    CHECK(commonStrength(Rank::Five) < commonStrength(Rank::Six));
    CHECK(commonStrength(Rank::Six) < commonStrength(Rank::Seven));
    CHECK(commonStrength(Rank::Seven) < commonStrength(Rank::Queen));
    CHECK(commonStrength(Rank::Queen) < commonStrength(Rank::Jack));
    CHECK(commonStrength(Rank::Jack) < commonStrength(Rank::King));
    CHECK(commonStrength(Rank::King) < commonStrength(Rank::Ace));
    CHECK(commonStrength(Rank::Ace) < commonStrength(Rank::Two));
    CHECK(commonStrength(Rank::Two) < commonStrength(Rank::Three));
}

TEST(card_suit_order) {
    CHECK(suitOrder(Suit::Ouros) < suitOrder(Suit::Espadas));
    CHECK(suitOrder(Suit::Espadas) < suitOrder(Suit::Copas));
    CHECK(suitOrder(Suit::Copas) < suitOrder(Suit::Paus));
}

TEST(card_fixed_manilha_order) {
    CHECK(cardStrength(Card{Rank::Four, Suit::Paus}) >
          cardStrength(Card{Rank::Seven, Suit::Copas}));
    CHECK(cardStrength(Card{Rank::Seven, Suit::Copas}) >
          cardStrength(Card{Rank::Ace, Suit::Espadas}));
    CHECK(cardStrength(Card{Rank::Ace, Suit::Espadas}) >
          cardStrength(Card{Rank::Seven, Suit::Ouros}));
    CHECK(cardStrength(Card{Rank::Seven, Suit::Ouros}) >
          cardStrength(Card{Rank::Three, Suit::Paus}));
}

TEST(card_manilha_identification) {
    CHECK(isManilha(Card{Rank::Four, Suit::Paus}));
    CHECK(isManilha(Card{Rank::Seven, Suit::Copas}));
    CHECK(isManilha(Card{Rank::Ace, Suit::Espadas}));
    CHECK(isManilha(Card{Rank::Seven, Suit::Ouros}));
    CHECK(!isManilha(Card{Rank::Four, Suit::Ouros}));
    CHECK(!isManilha(Card{Rank::Seven, Suit::Paus}));
    CHECK(!isManilha(Card{Rank::Three, Suit::Paus}));
}

TEST(card_manilha_beats_every_common_card) {
    const Card weakestManilha{Rank::Seven, Suit::Ouros};
    const Card strongestCommon{Rank::Three, Suit::Paus};
    CHECK(cardStrength(weakestManilha) > cardStrength(strongestCommon));
}

TEST(card_suit_does_not_break_common_ties) {
    CHECK_EQ(cardStrength(Card{Rank::Three, Suit::Paus}),
             cardStrength(Card{Rank::Three, Suit::Ouros}));
}

TEST(card_labels) {
    CHECK_EQ(rankLabel(Rank::Queen), std::string("Q"));
    CHECK_EQ(rankLabel(Rank::Three), std::string("3"));
    CHECK_EQ(suitLabel(Suit::Copas), std::string("\xE2\x99\xA5"));
    CHECK_EQ(suitName(Suit::Paus), std::string("paus"));
}
