#include "ai/cpu_player.hpp"

#include <random>

#include "test_framework.hpp"

using namespace truco;

namespace {

PublicView baseView(std::array<Card, 3> hand, Rank vira) {
    PublicView view;
    view.hand = hand;
    view.vira = Card{vira, Suit::Ouros};
    view.turn = Side::Cpu;
    return view;
}

int countCalls(const PublicView& view, unsigned seed, int rounds) {
    std::mt19937 rng(seed);
    int total = 0;
    for (int i = 0; i < rounds; ++i) {
        if (shouldCallTruco(view, rng)) ++total;
    }
    return total;
}

int countAccepts(const PublicView& view, unsigned seed, int rounds) {
    std::mt19937 rng(seed);
    int total = 0;
    for (int i = 0; i < rounds; ++i) {
        if (shouldAcceptTruco(view, rng)) ++total;
    }
    return total;
}

}  // namespace

TEST(ai_chooses_legal_card) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Three, Suit::Paus}, Card{Rank::Five, Suit::Paus}, Card{Rank::Four, Suit::Ouros}},
        vira);
    view.played = {true, false, false};
    std::mt19937 rng(1);
    for (int i = 0; i < 20; ++i) {
        const int idx = chooseCardIndex(view, rng);
        CHECK(idx >= 1 && idx <= 2);
        CHECK(!view.played[static_cast<size_t>(idx)]);
    }
}

TEST(ai_wins_cheaply_when_opponent_played_low) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Three, Suit::Paus}, Card{Rank::Five, Suit::Copas}, Card{Rank::Six, Suit::Ouros}},
        vira);
    view.played = {false, false, false};
    view.opponentCardInTrick = Card{Rank::Four, Suit::Ouros};
    std::mt19937 rng(2);
    const int idx = chooseCardIndex(view, rng);
    CHECK_EQ(idx, 2);  // 6♦ is the weakest card that beats 4♦
}

TEST(ai_throws_weakest_when_cannot_win) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Six, Suit::Ouros}, Card{Rank::Seven, Suit::Ouros}, Card{Rank::Queen, Suit::Copas}},
        vira);
    view.played = {false, false, false};
    view.opponentCardInTrick = Card{Rank::Three, Suit::Paus};
    std::mt19937 rng(3);
    const int idx = chooseCardIndex(view, rng);
    CHECK_EQ(idx, 0);  // nothing beats 3♣, so dump the weakest (6♦)
}

TEST(ai_leads_low_when_leading) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Three, Suit::Paus}, Card{Rank::Four, Suit::Ouros}, Card{Rank::Seven, Suit::Ouros}},
        vira);
    std::mt19937 rng(4);
    const int idx = chooseCardIndex(view, rng);
    CHECK_EQ(idx, 1);  // 4♦ is the weakest non-manilha... 4 < 7, and 4 is not manilha
}

TEST(ai_strong_hand_calls_truco_more_than_weak) {
    const Rank vira = Rank::Four;
    PublicView strong = baseView(
        {Card{Rank::Five, Suit::Paus}, Card{Rank::Five, Suit::Copas}, Card{Rank::Three, Suit::Paus}},
        vira);
    PublicView weak = baseView(
        {Card{Rank::Four, Suit::Ouros}, Card{Rank::Five, Suit::Ouros}, Card{Rank::Six, Suit::Ouros}},
        vira);
    const int strongCalls = countCalls(strong, 10, 400);
    const int weakCalls = countCalls(weak, 10, 400);
    CHECK(strongCalls > weakCalls);
    CHECK(strongCalls > 100);
    CHECK(weakCalls < 120);
    CHECK(weakCalls > 0);  // limited bluffing still exists
}

TEST(ai_strong_hand_accepts_more_than_weak) {
    const Rank vira = Rank::Four;
    PublicView strong = baseView(
        {Card{Rank::Five, Suit::Paus}, Card{Rank::Five, Suit::Copas}, Card{Rank::Three, Suit::Paus}},
        vira);
    PublicView weak = baseView(
        {Card{Rank::Four, Suit::Ouros}, Card{Rank::Five, Suit::Ouros}, Card{Rank::Six, Suit::Ouros}},
        vira);
    strong.stake = Stake::Three;
    weak.stake = Stake::Three;
    strong.trucoPending = true;
    weak.trucoPending = true;
    const int strongAccepts = countAccepts(strong, 11, 400);
    const int weakAccepts = countAccepts(weak, 11, 400);
    CHECK(strongAccepts > weakAccepts);
    CHECK(weakAccepts < 200);
    CHECK(weakAccepts > 0);
}

TEST(ai_never_accepts_when_no_truco_pending) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Five, Suit::Paus}, Card{Rank::Five, Suit::Copas}, Card{Rank::Three, Suit::Paus}},
        vira);
    std::mt19937 rng(12);
    CHECK(!shouldAcceptTruco(view, rng));
}

TEST(ai_eleven_hand_plays_manilha_most_of_time) {
    std::mt19937 rng(13);
    int plays = 0;
    for (int i = 0; i < 200; ++i) {
        if (shouldPlayHandOfEleven(Card{Rank::Five, Suit::Paus}, Rank::Four, rng)) ++plays;
    }
    CHECK(plays > 150);
}

TEST(ai_eleven_hand_hides_weak_card_most_of_time) {
    std::mt19937 rng(14);
    int plays = 0;
    for (int i = 0; i < 200; ++i) {
        if (shouldPlayHandOfEleven(Card{Rank::Four, Suit::Ouros}, Rank::Four, rng)) ++plays;
    }
    CHECK(plays < 80);
}

TEST(ai_is_deterministic_with_seed) {
    const Rank vira = Rank::Four;
    PublicView view = baseView(
        {Card{Rank::Five, Suit::Paus}, Card{Rank::Seven, Suit::Copas}, Card{Rank::Three, Suit::Ouros}},
        vira);
    std::mt19937 a(99);
    std::mt19937 b(99);
    for (int i = 0; i < 50; ++i) {
        CHECK_EQ(chooseCardIndex(view, a), chooseCardIndex(view, b));
    }
}
