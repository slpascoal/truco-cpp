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

TEST(card_manilha_follows_vira) {
    CHECK(manilhaRank(Rank::Four) == Rank::Five);
    CHECK(manilhaRank(Rank::Seven) == Rank::Queen);
    CHECK(manilhaRank(Rank::Three) == Rank::Four);
    CHECK(manilhaRank(Rank::Ace) == Rank::Two);
}

TEST(card_manilha_identification) {
    CHECK(isManilha(Card{Rank::Five, Suit::Paus}, Rank::Four));
    CHECK(!isManilha(Card{Rank::Four, Suit::Paus}, Rank::Four));
    CHECK(isManilha(Card{Rank::Four, Suit::Ouros}, Rank::Three));
}

TEST(card_manilha_beats_every_common_card) {
    const Card weakestManilha{Rank::Five, Suit::Ouros};
    const Card strongestCommon{Rank::Three, Suit::Paus};
    CHECK(cardStrength(weakestManilha, Rank::Four) >
          cardStrength(strongestCommon, Rank::Four));
}

TEST(card_manilha_order_by_suit) {
    const Rank vira = Rank::Four;
    CHECK(cardStrength(Card{Rank::Five, Suit::Paus}, vira) >
          cardStrength(Card{Rank::Five, Suit::Copas}, vira));
    CHECK(cardStrength(Card{Rank::Five, Suit::Copas}, vira) >
          cardStrength(Card{Rank::Five, Suit::Espadas}, vira));
    CHECK(cardStrength(Card{Rank::Five, Suit::Espadas}, vira) >
          cardStrength(Card{Rank::Five, Suit::Ouros}, vira));
}

TEST(card_suit_does_not_break_common_ties) {
    CHECK_EQ(cardStrength(Card{Rank::Three, Suit::Paus}, Rank::Four),
             cardStrength(Card{Rank::Three, Suit::Ouros}, Rank::Four));
}

TEST(card_labels) {
    CHECK_EQ(rankLabel(Rank::Queen), std::string("Q"));
    CHECK_EQ(rankLabel(Rank::Three), std::string("3"));
    CHECK_EQ(suitLabel(Suit::Copas), std::string("\xE2\x99\xA5"));
    CHECK_EQ(suitName(Suit::Paus), std::string("paus"));
}
