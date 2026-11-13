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

TEST(royale_hand_end_charges_losers_and_keeps_winner) {
    std::mt19937 rng(7);
    RoyaleMatch match(3, rng);
    match.setHandForTesting(0, {Card{Rank::Three, Suit::Paus}, Card{Rank::Three, Suit::Copas},
                                Card{Rank::Ace, Suit::Espadas}});
    match.setHandForTesting(1, {Card{Rank::Four, Suit::Ouros}, Card{Rank::Five, Suit::Ouros},
                                Card{Rank::Six, Suit::Ouros}});
    match.setHandForTesting(2, {Card{Rank::Seven, Suit::Paus}, Card{Rank::Queen, Suit::Paus},
                                Card{Rank::Jack, Suit::Paus}});

    for (int trick = 0; trick < 3; ++trick) {
        for (int seat = 0; seat < 3; ++seat) {
            match.playCard(seat, trick);
        }
    }

    CHECK(match.phase() == RoyalePhase::HandEnd);
    CHECK_EQ(match.handWinner(), 0);
    CHECK_EQ(match.seat(0).points, 12);
    CHECK_EQ(match.seat(1).points, 11);
    CHECK_EQ(match.seat(2).points, 11);
}

TEST(royale_hand_tiebreak_uses_highest_card) {
    std::mt19937 rng(8);
    RoyaleMatch match(3, rng);
    match.setHandForTesting(0, {Card{Rank::Two, Suit::Paus}, Card{Rank::Four, Suit::Ouros},
                                Card{Rank::Six, Suit::Ouros}});
    match.setHandForTesting(1, {Card{Rank::Ace, Suit::Ouros}, Card{Rank::Three, Suit::Copas},
                                Card{Rank::Five, Suit::Ouros}});
    match.setHandForTesting(2, {Card{Rank::King, Suit::Paus}, Card{Rank::Jack, Suit::Paus},
                                Card{Rank::Ace, Suit::Paus}});

    match.playCard(0, 0);
    match.playCard(1, 0);
    match.playCard(2, 0);
    match.playCard(0, 1);
    match.playCard(1, 1);
    match.playCard(2, 1);
    match.playCard(1, 2);
    match.playCard(2, 2);
    match.playCard(0, 2);

    CHECK(match.phase() == RoyalePhase::HandEnd);
    CHECK_EQ(match.seat(0).tricksWon, 1);
    CHECK_EQ(match.seat(1).tricksWon, 1);
    CHECK_EQ(match.seat(2).tricksWon, 1);
    CHECK_EQ(match.handWinner(), 1);
}

TEST(royale_players_reaching_zero_are_eliminated_and_match_ends) {
    std::mt19937 rng(9);
    RoyaleMatch match(3, rng);
    match.setPointsForTesting(1, 1);
    match.setPointsForTesting(2, 1);
    match.setHandForTesting(0, {Card{Rank::Three, Suit::Paus}, Card{Rank::Three, Suit::Copas},
                                Card{Rank::Ace, Suit::Espadas}});
    match.setHandForTesting(1, {Card{Rank::Four, Suit::Ouros}, Card{Rank::Five, Suit::Ouros},
                                Card{Rank::Six, Suit::Ouros}});
    match.setHandForTesting(2, {Card{Rank::Seven, Suit::Paus}, Card{Rank::Queen, Suit::Paus},
                                Card{Rank::Jack, Suit::Paus}});

    for (int trick = 0; trick < 3; ++trick) {
        for (int seat = 0; seat < 3; ++seat) {
            match.playCard(seat, trick);
        }
    }

    CHECK(match.seat(1).eliminated);
    CHECK(match.seat(2).eliminated);
    CHECK_EQ(match.aliveCount(), 1);
    CHECK(match.phase() == RoyalePhase::MatchEnd);
    CHECK_EQ(match.matchWinner(), 0);
}

TEST(royale_new_hand_rotates_mao) {
    std::mt19937 rng(10);
    RoyaleMatch match(3, rng);
    CHECK_EQ(match.mao(), 0);
    match.setHandForTesting(0, {Card{Rank::Three, Suit::Paus}, Card{Rank::Three, Suit::Copas},
                                Card{Rank::Ace, Suit::Espadas}});
    match.setHandForTesting(1, {Card{Rank::Four, Suit::Ouros}, Card{Rank::Five, Suit::Ouros},
                                Card{Rank::Six, Suit::Ouros}});
    match.setHandForTesting(2, {Card{Rank::Seven, Suit::Paus}, Card{Rank::Queen, Suit::Paus},
                                Card{Rank::Jack, Suit::Paus}});
    for (int trick = 0; trick < 3; ++trick) {
        for (int seat = 0; seat < 3; ++seat) {
            match.playCard(seat, trick);
        }
    }
    CHECK(match.phase() == RoyalePhase::HandEnd);
    match.startHand();
    CHECK(match.phase() == RoyalePhase::Playing);
    CHECK_EQ(match.mao(), 1);
    CHECK_EQ(match.handValue(), 1);
    for (int seat = 0; seat < 3; ++seat) {
        CHECK(match.seat(seat).inHand);
        CHECK_EQ(match.seat(seat).tricksWon, 0);
    }
}

TEST(royale_disconnect_eliminates_without_charge) {
    std::mt19937 rng(11);
    RoyaleMatch match(3, rng);
    match.removePlayer(1);
    CHECK(match.seat(1).eliminated);
    CHECK_EQ(match.seat(1).points, 12);
    CHECK(!match.seat(1).inHand);
    CHECK_EQ(match.aliveCount(), 2);
    CHECK(match.phase() == RoyalePhase::Playing);

    match.removePlayer(2);
    CHECK(match.phase() == RoyalePhase::MatchEnd);
    CHECK_EQ(match.matchWinner(), 0);
}

TEST(royale_truco_asks_each_opponent_and_grants_raise_rights) {
    std::mt19937 rng(12);
    RoyaleMatch match(3, rng);
    CHECK(match.canCallTruco(0));
    CHECK(!match.canCallTruco(1));
    match.callTruco(0);
    CHECK(match.trucoPending());
    CHECK_EQ(match.trucoCaller(), 0);
    CHECK_EQ(match.trucoResponder(), 1);
    CHECK_EQ(match.pendingValue(), 3);
    match.respondTruco(true);
    CHECK_EQ(match.trucoResponder(), 2);
    match.respondTruco(true);
    CHECK(!match.trucoPending());
    CHECK(match.phase() == RoyalePhase::Playing);
    CHECK_EQ(match.handValue(), 3);
    CHECK(!match.raiseAllowed(0));
    CHECK(match.raiseAllowed(1));
    CHECK(match.raiseAllowed(2));
}

TEST(royale_running_pays_pending_value_and_leaves_hand) {
    std::mt19937 rng(13);
    RoyaleMatch match(3, rng);
    match.callTruco(0);
    match.respondTruco(false);
    CHECK_EQ(match.seat(1).points, 9);
    CHECK(!match.seat(1).inHand);
    CHECK(!match.seat(1).eliminated);
    CHECK_EQ(match.trucoResponder(), 2);
}

TEST(royale_running_can_eliminate_and_answers_continue) {
    std::mt19937 rng(14);
    RoyaleMatch match(3, rng);
    match.setPointsForTesting(1, 2);
    match.callTruco(0);
    match.respondTruco(false);
    CHECK(match.seat(1).eliminated);
    CHECK_EQ(match.aliveCount(), 2);
    CHECK_EQ(match.trucoResponder(), 2);
    CHECK(match.phase() == RoyalePhase::TrucoAnswer);
}

TEST(royale_all_opponents_running_ends_hand_for_caller) {
    std::mt19937 rng(15);
    RoyaleMatch match(3, rng);
    match.callTruco(0);
    match.respondTruco(false);
    match.respondTruco(false);
    CHECK(match.phase() == RoyalePhase::HandEnd);
    CHECK_EQ(match.handWinner(), 0);
    CHECK_EQ(match.seat(1).points, 9);
    CHECK_EQ(match.seat(2).points, 9);
    CHECK_EQ(match.seat(0).points, 12);
}

TEST(royale_caller_cannot_raise_but_acceptors_can) {
    std::mt19937 rng(16);
    RoyaleMatch match(3, rng);
    match.callTruco(0);
    match.respondTruco(true);
    match.respondTruco(true);
    match.playCard(0, 0);
    CHECK_EQ(match.currentTurn(), 1);
    CHECK(!match.canCallTruco(0));
    CHECK(match.canCallTruco(1));

    match.callTruco(1);
    CHECK_EQ(match.pendingValue(), 6);
    CHECK_EQ(match.trucoResponder(), 2);
    match.respondTruco(true);
    CHECK_EQ(match.trucoResponder(), 0);
    match.respondTruco(true);
    CHECK_EQ(match.handValue(), 6);
    CHECK(!match.raiseAllowed(1));
    CHECK(match.raiseAllowed(0));
    CHECK(match.raiseAllowed(2));
}

TEST(royale_play_is_blocked_while_truco_pending) {
    std::mt19937 rng(17);
    RoyaleMatch match(3, rng);
    match.callTruco(0);
    bool threw = false;
    try {
        match.playCard(0, 0);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(match.phase() == RoyalePhase::TrucoAnswer);
}

TEST(royale_runner_does_not_get_raise_right) {
    std::mt19937 rng(18);
    RoyaleMatch match(3, rng);
    match.callTruco(0);
    match.respondTruco(false);
    match.respondTruco(true);
    CHECK_EQ(match.handValue(), 3);
    CHECK(!match.raiseAllowed(1));
    CHECK(!match.raiseAllowed(0));
    CHECK(match.raiseAllowed(2));
}
