#include "core/hand.hpp"

#include "test_framework.hpp"

using namespace truco;

namespace {

std::vector<TrickOutcome> outcomes(std::initializer_list<TrickOutcome> list) {
    return std::vector<TrickOutcome>(list);
}

}  // namespace

TEST(trick_higher_card_wins) {
    const Rank vira = Rank::Four;
    CHECK(trickWinner(Card{Rank::Three, Suit::Ouros}, Card{Rank::Two, Suit::Ouros}, vira) ==
          TrickOutcome::Human);
    CHECK(trickWinner(Card{Rank::Two, Suit::Ouros}, Card{Rank::Three, Suit::Ouros}, vira) ==
          TrickOutcome::Cpu);
}

TEST(trick_manilha_beats_common) {
    const Rank vira = Rank::Four;
    CHECK(trickWinner(Card{Rank::Five, Suit::Ouros}, Card{Rank::Three, Suit::Paus}, vira) ==
          TrickOutcome::Human);
}

TEST(trick_manilha_suit_breaks_tie) {
    const Rank vira = Rank::Four;
    CHECK(trickWinner(Card{Rank::Five, Suit::Paus}, Card{Rank::Five, Suit::Copas}, vira) ==
          TrickOutcome::Human);
    CHECK(trickWinner(Card{Rank::Five, Suit::Copas}, Card{Rank::Five, Suit::Paus}, vira) ==
          TrickOutcome::Cpu);
}

TEST(trick_equal_common_cards_tie) {
    const Rank vira = Rank::Four;
    CHECK(trickWinner(Card{Rank::Three, Suit::Paus}, Card{Rank::Three, Suit::Ouros}, vira) ==
          TrickOutcome::Tie);
}

TEST(hand_two_straight_wins) {
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Human, TrickOutcome::Human}), Side::Cpu) ==
          Side::Human);
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Cpu, TrickOutcome::Cpu}), Side::Human) ==
          Side::Cpu);
}

TEST(hand_wins_after_three_tricks) {
    CHECK(resolveHandWinner(
              outcomes({TrickOutcome::Human, TrickOutcome::Cpu, TrickOutcome::Human}),
              Side::Cpu) == Side::Human);
    CHECK(resolveHandWinner(
              outcomes({TrickOutcome::Cpu, TrickOutcome::Human, TrickOutcome::Cpu}),
              Side::Human) == Side::Cpu);
}

TEST(hand_first_tie_second_decides) {
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Tie, TrickOutcome::Human}), Side::Cpu) ==
          Side::Human);
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Tie, TrickOutcome::Cpu}), Side::Human) ==
          Side::Cpu);
}

TEST(hand_first_won_second_tied_goes_to_first_winner) {
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Human, TrickOutcome::Tie}), Side::Cpu) ==
          Side::Human);
    CHECK(resolveHandWinner(outcomes({TrickOutcome::Cpu, TrickOutcome::Tie}), Side::Human) ==
          Side::Cpu);
}

TEST(hand_second_tie_third_tie_goes_to_first_winner) {
    CHECK(resolveHandWinner(
              outcomes({TrickOutcome::Cpu, TrickOutcome::Tie, TrickOutcome::Tie}),
              Side::Human) == Side::Cpu);
}

TEST(hand_three_ties_goes_to_dealer) {
    CHECK(resolveHandWinner(
              outcomes({TrickOutcome::Tie, TrickOutcome::Tie, TrickOutcome::Tie}),
              Side::Cpu) == Side::Cpu);
    CHECK(resolveHandWinner(
              outcomes({TrickOutcome::Tie, TrickOutcome::Tie, TrickOutcome::Tie}),
              Side::Human) == Side::Human);
}

TEST(hand_progress_finishes_early_on_two_wins) {
    HandProgress p(Side::Human);
    p.playTrick(TrickOutcome::Cpu);
    CHECK(!p.finished());
    p.playTrick(TrickOutcome::Cpu);
    CHECK(p.finished());
    CHECK(p.winner() == Side::Cpu);
    CHECK_EQ(p.tricksWon(Side::Cpu), 2);
    CHECK_EQ(p.trickIndex(), 2);
}

TEST(hand_progress_finishes_on_first_won_second_tied) {
    HandProgress p(Side::Human);
    p.playTrick(TrickOutcome::Human);
    p.playTrick(TrickOutcome::Tie);
    CHECK(p.finished());
    CHECK(p.winner() == Side::Human);
}

TEST(hand_progress_needs_third_after_tie_tie) {
    HandProgress p(Side::Human);
    p.playTrick(TrickOutcome::Tie);
    p.playTrick(TrickOutcome::Tie);
    CHECK(!p.finished());
    p.playTrick(TrickOutcome::Tie);
    CHECK(p.finished());
    CHECK(p.winner() == Side::Human);
}

TEST(hand_progress_tie_first_then_win_finishes) {
    HandProgress p(Side::Cpu);
    p.playTrick(TrickOutcome::Tie);
    p.playTrick(TrickOutcome::Human);
    CHECK(p.finished());
    CHECK(p.winner() == Side::Human);
}
