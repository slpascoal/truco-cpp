#include "core/royale.hpp"

#include <random>
#include <set>
#include <stdexcept>

#include "test_framework.hpp"

using namespace truco;

namespace {

int cardKey(Card c) {
    return commonStrength(c.rank) * 4 + suitOrder(c.suit);
}

}  // namespace

TEST(royale_initial_state_has_points_and_hands) {
    std::mt19937 rng(1);
    RoyaleMatch match(3, rng);
    CHECK(match.playerCount() == 3);
    CHECK(match.phase() == RoyalePhase::Playing);
    for (int seat = 0; seat < 3; ++seat) {
        CHECK_EQ(match.seat(seat).points, 12);
        CHECK(match.seat(seat).inHand);
        CHECK(!match.seat(seat).eliminated);
    }
    CHECK(match.currentTurn() == match.mao());
}

TEST(royale_rejects_invalid_player_counts) {
    std::mt19937 rng(2);
    bool threwLow = false;
    try {
        RoyaleMatch match(2, rng);
    } catch (const std::exception&) {
        threwLow = true;
    }
    CHECK(threwLow);
    bool threwHigh = false;
    try {
        RoyaleMatch match(14, rng);
    } catch (const std::exception&) {
        threwHigh = true;
    }
    CHECK(threwHigh);
}

TEST(royale_thirteen_players_deal_thirty_nine_distinct_cards) {
    std::mt19937 rng(3);
    RoyaleMatch match(13, rng);
    std::set<int> keys;
    for (int seat = 0; seat < 13; ++seat) {
        for (const Card& card : match.seat(seat).hand) {
            keys.insert(cardKey(card));
        }
    }
    CHECK_EQ(keys.size(), size_t{39});
}

TEST(royale_turn_order_follows_seats_and_winner_leads) {
    std::mt19937 rng(4);
    RoyaleMatch match(3, rng);
    match.setHandForTesting(0, {Card{Rank::Three, Suit::Paus}, Card{Rank::Four, Suit::Ouros},
                                Card{Rank::Five, Suit::Ouros}});
    match.setHandForTesting(1, {Card{Rank::Five, Suit::Paus}, Card{Rank::Six, Suit::Paus},
                                Card{Rank::Seven, Suit::Paus}});
    match.setHandForTesting(2, {Card{Rank::Six, Suit::Ouros}, Card{Rank::Seven, Suit::Paus},
                                Card{Rank::Queen, Suit::Paus}});

    CHECK(match.currentTurn() == 0);
    match.playCard(0, 0);
    CHECK(match.currentTurn() == 1);
    match.playCard(1, 0);
    CHECK(match.currentTurn() == 2);
    match.playCard(2, 0);

    CHECK_EQ(match.tricksPlayed(), 1);
    CHECK_EQ(match.seat(0).tricksWon, 1);
    CHECK_EQ(match.seat(1).tricksWon, 0);
    CHECK_EQ(match.seat(2).tricksWon, 0);
    CHECK(match.currentTurn() == 0);
    CHECK(!match.seat(0).tableCard.has_value());
}

TEST(royale_tied_trick_has_no_winner_and_next_seat_leads) {
    std::mt19937 rng(5);
    RoyaleMatch match(3, rng);
    match.setHandForTesting(0, {Card{Rank::Three, Suit::Paus}, Card{Rank::Four, Suit::Ouros},
                                Card{Rank::Five, Suit::Ouros}});
    match.setHandForTesting(1, {Card{Rank::Three, Suit::Ouros}, Card{Rank::Five, Suit::Paus},
                                Card{Rank::Six, Suit::Paus}});
    match.setHandForTesting(2, {Card{Rank::Five, Suit::Ouros}, Card{Rank::Six, Suit::Ouros},
                                Card{Rank::Queen, Suit::Paus}});

    match.playCard(0, 0);
    match.playCard(1, 0);
    match.playCard(2, 0);

    CHECK_EQ(match.tricksPlayed(), 1);
    CHECK_EQ(match.seat(0).tricksWon + match.seat(1).tricksWon + match.seat(2).tricksWon, 0);
    CHECK(match.currentTurn() == 1);
}

TEST(royale_rejects_out_of_turn_and_replayed_cards) {
    std::mt19937 rng(6);
    RoyaleMatch match(3, rng);
    bool outOfTurn = false;
    try {
        match.playCard(1, 0);
    } catch (const std::exception&) {
        outOfTurn = true;
    }
    CHECK(outOfTurn);

    match.playCard(0, 0);
    match.playCard(1, 0);
    match.playCard(2, 0);
    const int leader = match.currentTurn();
    bool replay = false;
    try {
        match.playCard(leader, 0);
    } catch (const std::exception&) {
        replay = true;
    }
    CHECK(replay);
}
