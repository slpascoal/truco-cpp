#include "net/protocol.hpp"

#include <string>

#include "test_framework.hpp"

using namespace truco;

namespace {

RoyaleView makeView(int players, bool revealAll) {
    RoyaleView view;
    view.seq = 42;
    view.you = 1;
    view.phase = RoyalePhase::Playing;
    view.turn = 2;
    view.stake = 3;
    view.caller = 0;
    view.responder = -1;
    view.handWinner = -1;
    view.matchWinner = -1;
    view.tricksPlayed = 1;
    view.playerCount = players;
    view.canRaise = true;
    for (int i = 0; i < players; ++i) {
        view.seats[static_cast<size_t>(i)].points = 12 - i;
        view.seats[static_cast<size_t>(i)].eliminated = i == players - 1;
        view.seats[static_cast<size_t>(i)].inHand = i != players - 1;
        if (i == 0) {
            view.seats[static_cast<size_t>(i)].tableCard = Card{Rank::Seven, Suit::Ouros};
        }
        if (revealAll && i != 1) {
            view.seats[static_cast<size_t>(i)].revealed = std::array<Card, 3>{
                Card{Rank::Three, Suit::Paus}, Card{Rank::Five, Suit::Copas},
                Card{Rank::Ace, Suit::Espadas}};
        }
    }
    view.own = {Card{Rank::Two, Suit::Ouros}, Card{Rank::Queen, Suit::Paus},
                Card{Rank::King, Suit::Copas}};
    view.playedOwn = {true, false, false};
    return view;
}

}  // namespace

TEST(royale_state_roundtrip_three_players) {
    const RoyaleView original = makeView(3, false);
    const std::string line = encodeRoyaleState(original);
    CHECK(line.size() <= kMaxLine);
    RoyaleView parsed;
    CHECK(decodeRoyaleState(line, parsed));
    CHECK_EQ(parsed.seq, original.seq);
    CHECK_EQ(parsed.you, original.you);
    CHECK(parsed.phase == original.phase);
    CHECK_EQ(parsed.turn, original.turn);
    CHECK_EQ(parsed.stake, original.stake);
    CHECK_EQ(parsed.caller, original.caller);
    CHECK_EQ(parsed.responder, original.responder);
    CHECK_EQ(parsed.tricksPlayed, original.tricksPlayed);
    CHECK_EQ(parsed.playerCount, original.playerCount);
    CHECK_EQ(parsed.canRaise, original.canRaise);
    CHECK_EQ(parsed.seats.at(0).points, original.seats.at(0).points);
    CHECK(parsed.seats.at(0).eliminated == original.seats.at(0).eliminated);
    CHECK(parsed.seats.at(0).inHand == original.seats.at(0).inHand);
    CHECK(parsed.seats.at(0).tableCard.has_value());
    CHECK(*parsed.seats.at(0).tableCard == *original.seats.at(0).tableCard);
    CHECK(!parsed.seats.at(0).revealed.has_value());
    CHECK(parsed.seats.at(2).eliminated);
    CHECK(parsed.own.at(0) == original.own.at(0));
    CHECK_EQ(parsed.playedOwn.at(1), original.playedOwn.at(1));
}

TEST(royale_state_roundtrip_thirteen_players_all_revealed) {
    const RoyaleView original = makeView(13, true);
    const std::string line = encodeRoyaleState(original);
    CHECK(line.size() <= kMaxLine);
    RoyaleView parsed;
    CHECK(decodeRoyaleState(line, parsed));
    CHECK_EQ(parsed.playerCount, 13);
    CHECK(parsed.seats.at(0).revealed.has_value());
    CHECK(parsed.seats.at(0).revealed->at(2) ==
          original.seats.at(0).revealed->at(2));
    CHECK(!parsed.seats.at(1).revealed.has_value());
}

TEST(royale_state_rejects_invalid_counts_and_tokens) {
    RoyaleView parsed;
    CHECK(!decodeRoyaleState("RSTATE", parsed));
    CHECK(!decodeRoyaleState(
        "RSTATE 1 0 PLAY 0 1 -1 -1 -1 -1 0 2 12 0 1 - 0 - - - 2O QP KC 0", parsed));

    RoyaleView view = makeView(3, false);
    std::string line = encodeRoyaleState(view);
    const size_t card = line.find("7O");
    CHECK(card != std::string::npos);
    line[card] = 'X';
    CHECK(!decodeRoyaleState(line, parsed));

    std::string extra = encodeRoyaleState(view);
    extra.insert(extra.size() - 1, " EXTRA");
    CHECK(!decodeRoyaleState(extra, parsed));
}

TEST(royale_lobby_roundtrip) {
    const std::vector<std::string> names = {"J1", "J2", "J3", "J4"};
    const std::string line = encodeLobby(names);
    std::vector<std::string> parsed;
    CHECK(decodeLobby(line, parsed));
    CHECK_EQ(parsed.size(), names.size());
    CHECK_EQ(parsed.at(3), std::string("J4"));
    CHECK(!decodeLobby("LOBBY 2 J1", parsed));
}

TEST(royale_phase_tokens_are_stable) {
    RoyalePhase phase = RoyalePhase::Playing;
    CHECK(royalePhaseFromToken("PLAY", phase));
    CHECK(phase == RoyalePhase::Playing);
    CHECK(royalePhaseFromToken("TRUCO_ANSWER", phase));
    CHECK(phase == RoyalePhase::TrucoAnswer);
    CHECK(royalePhaseFromToken("HANDEND", phase));
    CHECK(phase == RoyalePhase::HandEnd);
    CHECK(royalePhaseFromToken("MATCHEND", phase));
    CHECK(phase == RoyalePhase::MatchEnd);
}
