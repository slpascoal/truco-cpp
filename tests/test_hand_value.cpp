#include "core/hand_value.hpp"

#include "test_framework.hpp"

using namespace truco;

TEST(stake_ladder_progression) {
    CHECK(nextStake(Stake::One) == Stake::Three);
    CHECK(nextStake(Stake::Three) == Stake::Six);
    CHECK(nextStake(Stake::Six) == Stake::Nine);
    CHECK(nextStake(Stake::Nine) == Stake::Twelve);
}

TEST(stake_cannot_raise_at_twelve) {
    CHECK(canRaise(Stake::One));
    CHECK(canRaise(Stake::Three));
    CHECK(canRaise(Stake::Six));
    CHECK(canRaise(Stake::Nine));
    CHECK(!canRaise(Stake::Twelve));
}

TEST(stake_accept_moves_to_next_value) {
    CHECK(valueAfterAccept(Stake::One) == Stake::Three);
    CHECK(valueAfterAccept(Stake::Nine) == Stake::Twelve);
}

TEST(stake_points) {
    CHECK_EQ(valuePoints(Stake::One), 1);
    CHECK_EQ(valuePoints(Stake::Three), 3);
    CHECK_EQ(valuePoints(Stake::Six), 6);
    CHECK_EQ(valuePoints(Stake::Nine), 9);
    CHECK_EQ(valuePoints(Stake::Twelve), 12);
}

TEST(stake_call_labels) {
    CHECK_EQ(std::string(callLabel(Stake::One)), std::string("Truco"));
    CHECK_EQ(std::string(callLabel(Stake::Three)), std::string("Seis"));
    CHECK_EQ(std::string(callLabel(Stake::Six)), std::string("Nove"));
    CHECK_EQ(std::string(callLabel(Stake::Nine)), std::string("Doze"));
}
