#include <random>

#include "core/game.hpp"
#include "test_framework.hpp"

using namespace truco;

namespace {

Game makeGame(std::mt19937& rng) {
    Game game(rng);
    game.startMatch();
    return game;
}

int firstUnplayed(const Game& game, Side side) {
    for (int i = 0; i < 3; ++i) {
        if (!game.cardPlayed(side, i)) {
            return i;
        }
    }
    return -1;
}

int bestCardIndex(const Game& game, Side side) {
    int best = 0;
    for (int i = 1; i < 3; ++i) {
        if (cardStrength(game.handOf(side)[i]) > cardStrength(game.handOf(side)[best])) {
            best = i;
        }
    }
    return best;
}

}  // namespace

TEST(game_starts_clean) {
    std::mt19937 rng(1);
    Game game = makeGame(rng);
    CHECK_EQ(game.score().human, 0);
    CHECK_EQ(game.score().cpu, 0);
    CHECK(game.dealer() == Side::Human);
    CHECK(game.currentTurn() == Side::Human);
    CHECK(game.handKind() == HandKind::Normal);
    CHECK_EQ(static_cast<int>(game.stake()), 1);
    CHECK(!game.handFinished());
    CHECK(!game.matchOver());
}

TEST(game_dealer_alternates_each_hand) {
    std::mt19937 rng(2);
    Game game = makeGame(rng);
    CHECK(game.dealer() == Side::Human);
    game.startHand();
    CHECK(game.dealer() == Side::Cpu);
    game.startHand();
    CHECK(game.dealer() == Side::Human);
}

TEST(game_rejects_out_of_turn_play) {
    std::mt19937 rng(3);
    Game game = makeGame(rng);
    bool threw = false;
    try {
        game.playCard(Side::Cpu, 0);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(game_rejects_replaying_same_card) {
    std::mt19937 rng(4);
    Game game = makeGame(rng);
    game.playCard(Side::Human, 0);
    game.playCard(Side::Cpu, 0);
    bool threw = false;
    try {
        game.playCard(Side::Human, 0);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(game_hand_completes_and_awards_one_point) {
    std::mt19937 rng(5);
    Game game = makeGame(rng);
    int guard = 0;
    while (!game.handFinished() && guard++ < 10) {
        const Side turn = game.currentTurn();
        game.playCard(turn, firstUnplayed(game, turn));
    }
    CHECK(game.handFinished());
    CHECK_EQ(game.score().human + game.score().cpu, 1);
    CHECK(game.revealed());
}

TEST(game_truco_accept_raises_stake) {
    std::mt19937 rng(6);
    Game game = makeGame(rng);
    CHECK(game.canCallTruco(Side::Human));
    game.callTruco(Side::Human);
    CHECK(game.trucoPending());
    CHECK(game.trucoCaller() == Side::Human);
    CHECK_EQ(static_cast<int>(game.pendingStake()), 3);
    game.respondTruco(Side::Cpu, true);
    CHECK(!game.trucoPending());
    CHECK_EQ(static_cast<int>(game.stake()), 3);
    CHECK(!game.handFinished());
}

TEST(game_truco_run_awards_previous_stake_to_caller) {
    std::mt19937 rng(7);
    Game game = makeGame(rng);
    game.callTruco(Side::Human);
    game.respondTruco(Side::Cpu, false);
    CHECK(game.handFinished());
    CHECK_EQ(game.score().human, 1);
    CHECK_EQ(game.score().cpu, 0);
    CHECK(game.revealed());
}

TEST(game_escalates_to_twelve_then_blocks) {
    std::mt19937 rng(8);
    Game game = makeGame(rng);
    const int expected[] = {3, 6, 9, 12};
    for (int value : expected) {
        CHECK(game.canCallTruco(Side::Human));
        game.callTruco(Side::Human);
        game.respondTruco(Side::Cpu, true);
        CHECK_EQ(static_cast<int>(game.stake()), value);
    }
    CHECK(!game.canCallTruco(Side::Human));
    bool threw = false;
    try {
        game.callTruco(Side::Human);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(game_cannot_call_truco_out_of_turn) {
    std::mt19937 rng(9);
    Game game = makeGame(rng);
    bool threw = false;
    try {
        game.callTruco(Side::Cpu);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(game_hand_of_eleven_requires_choice) {
    std::mt19937 rng(10);
    Game game(rng);
    game.setScoreForTesting(GameScore{11, 5});
    game.startHand();
    CHECK(game.handKind() == HandKind::HandOfEleven);
    CHECK(game.awaitingElevenChoice());
    CHECK(game.elevenSide() == Side::Human);
    CHECK(!game.canCallTruco(Side::Human));
}

TEST(game_hand_of_eleven_play_worth_three) {
    std::mt19937 rng(11);
    Game game(rng);
    game.setScoreForTesting(GameScore{11, 5});
    game.startHand();
    const int elevenStrength = cardStrength(game.handOf(Side::Human)[0]);
    const int cpuStrength = cardStrength(game.handOf(Side::Cpu)[bestCardIndex(game, Side::Cpu)]);
    game.chooseHandOfEleven(Side::Human, true);
    CHECK(game.handFinished());
    CHECK_EQ(static_cast<int>(game.stake()), 3);
    if (elevenStrength >= cpuStrength) {
        CHECK_EQ(game.score().human, 14);
        CHECK_EQ(game.score().cpu, 5);
    } else {
        CHECK_EQ(game.score().human, 11);
        CHECK_EQ(game.score().cpu, 8);
    }
    CHECK_EQ(game.matchOver(), game.score().human >= 12);
}

TEST(game_hand_of_eleven_hide_worth_one) {
    std::mt19937 rng(12);
    Game game(rng);
    game.setScoreForTesting(GameScore{11, 5});
    game.startHand();
    game.chooseHandOfEleven(Side::Human, false);
    CHECK(game.handFinished());
    CHECK_EQ(static_cast<int>(game.stake()), 1);
    CHECK_EQ(game.score().human + game.score().cpu, 17);
}

TEST(game_iron_hand_worth_three_without_choice) {
    std::mt19937 rng(13);
    Game game(rng);
    game.setScoreForTesting(GameScore{11, 11});
    game.startHand();
    CHECK(game.handKind() == HandKind::IronHand);
    CHECK(!game.awaitingElevenChoice());
    CHECK(game.handFinished());
    CHECK_EQ(static_cast<int>(game.stake()), 3);
    CHECK(game.revealed());
    CHECK_EQ(game.score().human + game.score().cpu, 25);
}

TEST(game_no_eleven_hand_at_ten_points) {
    std::mt19937 rng(14);
    Game game(rng);
    game.setScoreForTesting(GameScore{10, 5});
    game.startHand();
    CHECK(game.handKind() == HandKind::Normal);
    CHECK(!game.awaitingElevenChoice());
}

TEST(game_play_after_finish_throws) {
    std::mt19937 rng(15);
    Game game = makeGame(rng);
    game.callTruco(Side::Human);
    game.respondTruco(Side::Cpu, false);
    bool threw = false;
    try {
        game.playCard(Side::Human, 0);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(game_cpu_eleven_choice_is_pending_for_cpu) {
    std::mt19937 rng(16);
    Game game(rng);
    game.setScoreForTesting(GameScore{5, 11});
    game.startHand();
    CHECK(game.handKind() == HandKind::HandOfEleven);
    CHECK(game.awaitingElevenChoice());
    CHECK(game.elevenSide() == Side::Cpu);
    bool threw = false;
    try {
        game.chooseHandOfEleven(Side::Human, true);
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}
