#include <cstdio>
#include "net/protocol.hpp"

#include "test_framework.hpp"

using namespace truco;

TEST(protocol_encodes_and_parses_messages) {
    const std::string line = encodeLine(MessageType::Play, {"2"});
    CHECK_EQ(line, std::string("PLAY 2\n"));
    const Message message = parseLine(line);
    CHECK(message.type == MessageType::Play);
    CHECK_EQ(message.fields.size(), size_t{1});
    CHECK_EQ(message.fields[0], std::string("2"));
}

TEST(protocol_parses_hello_and_welcome) {
    const Message hello = parseLine("HELLO TRUCO 1\n");
    CHECK(hello.type == MessageType::Hello);
    CHECK_EQ(hello.fields.size(), size_t{2});
    CHECK_EQ(hello.fields[0], std::string("TRUCO"));
    CHECK_EQ(hello.fields[1], std::string("1"));

    const Message welcome = parseLine("WELCOME 1\r\n");
    CHECK(welcome.type == MessageType::Welcome);
    CHECK_EQ(welcome.fields[0], std::string("1"));
}

TEST(protocol_parses_text_messages) {
    const Message message = parseLine("MSG Sua vez de jogar\n");
    CHECK(message.type == MessageType::Msg);
    CHECK_EQ(message.text, std::string("Sua vez de jogar"));

    const Message error = parseLine("ERROR VERSION versao incompativel\n");
    CHECK(error.type == MessageType::Error);
    CHECK_EQ(error.fields[0], std::string("VERSION"));
    CHECK_EQ(error.text, std::string("versao incompativel"));
}

TEST(protocol_rejects_unknown_and_malformed) {
    CHECK(parseLine("FOO bar").type == MessageType::Unknown);
    CHECK(parseLine("").type == MessageType::Unknown);
    const std::string huge(kMaxLine + 10, 'A');
    CHECK(parseLine(huge).type == MessageType::Unknown);
}

TEST(protocol_card_tokens_roundtrip) {
    const Card cards[] = {
        {Rank::Four, Suit::Ouros}, {Rank::Three, Suit::Paus}, {Rank::Queen, Suit::Copas},
        {Rank::Ace, Suit::Espadas}, {Rank::Seven, Suit::Paus},
    };
    for (Card card : cards) {
        Card parsed{};
        CHECK(cardFromToken(cardToken(card), parsed));
        CHECK(parsed == card);
    }
    Card invalid{};
    CHECK(!cardFromToken("XX", invalid));
    CHECK(!cardFromToken("3", invalid));
    CHECK(!cardFromToken("10C", invalid));
}

TEST(protocol_side_tokens) {
    Side side = Side::Human;
    CHECK(sideFromToken("J1", side));
    CHECK(side == Side::Human);
    CHECK(sideFromToken("J2", side));
    CHECK(side == Side::Cpu);
    CHECK(!sideFromToken("J3", side));
    CHECK_EQ(sideToken(Side::Human), std::string("J1"));
    CHECK_EQ(sideToken(Side::Cpu), std::string("J2"));
}

namespace {
SyncState sampleSync(bool revealed) {
    SyncState sync;
    sync.seq = 7;
    sync.you = Side::Cpu;
    sync.turn = Side::Cpu;
    sync.phase = NetPhase::Play;
    sync.stake = 3;
    sync.scoreHuman = 5;
    sync.scoreCpu = 9;
    sync.handKind = HandKind::Normal;
    sync.vira = Card{Rank::Four, Suit::Ouros};
    sync.own = {Card{Rank::Three, Suit::Paus}, Card{Rank::Seven, Suit::Copas},
                Card{Rank::Ace, Suit::Espadas}};
    sync.playedOwn = {true, false, false};
    sync.playedOpponent = {false, true, false};
    sync.trickOwn = Card{Rank::Two, Suit::Copas};
    sync.trickOpponent = std::nullopt;
    sync.revealed = revealed;
    sync.elevenSide = Side::Human;
    sync.opponent = {Card{Rank::Five, Suit::Paus}, Card{Rank::Six, Suit::Ouros},
                     Card{Rank::King, Suit::Espadas}};
    return sync;
}
}  // namespace

TEST(protocol_sync_roundtrip_revealed) {
    const SyncState original = sampleSync(true);
    const std::string line = encodeSync(original);
    std::printf("SYNC: %s", line.c_str());
    SyncState parsed;
    CHECK(decodeSync(line, parsed));
    CHECK_EQ(parsed.seq, original.seq);
    CHECK(parsed.you == original.you);
    CHECK(parsed.turn == original.turn);
    CHECK(parsed.phase == original.phase);
    CHECK_EQ(parsed.stake, original.stake);
    CHECK_EQ(parsed.scoreHuman, original.scoreHuman);
    CHECK_EQ(parsed.scoreCpu, original.scoreCpu);
    CHECK(parsed.handKind == original.handKind);
    CHECK(parsed.vira == original.vira);
    for (int i = 0; i < 3; ++i) {
        CHECK(parsed.own[static_cast<size_t>(i)] == original.own[static_cast<size_t>(i)]);
        CHECK_EQ(parsed.playedOwn[static_cast<size_t>(i)],
                 original.playedOwn[static_cast<size_t>(i)]);
        CHECK(parsed.opponent[static_cast<size_t>(i)] ==
              original.opponent[static_cast<size_t>(i)]);
    }
    CHECK(parsed.trickOwn.has_value());
    CHECK(*parsed.trickOwn == *original.trickOwn);
    CHECK(!parsed.trickOpponent.has_value());
    CHECK(parsed.revealed);
}

TEST(protocol_sync_hides_opponent_hand_until_revealed) {
    SyncState hidden = sampleSync(false);
    const std::string line = encodeSync(hidden);
    // None of the opponent's cards may appear in the wire format.
    CHECK(line.find(cardToken(Card{Rank::Five, Suit::Paus})) == std::string::npos);
    CHECK(line.find(cardToken(Card{Rank::King, Suit::Espadas})) == std::string::npos);
    SyncState parsed;
    CHECK(decodeSync(line, parsed));
    CHECK(!parsed.revealed);
    CHECK((parsed.opponent[0] == Card{Rank::Four, Suit::Ouros}));
}

TEST(protocol_sync_rejects_bad_fields) {
    SyncState parsed;
    CHECK(!decodeSync("SYNC 1", parsed));
    CHECK(!decodeSync("SYNC 1 J1 J1 PLAY 3 0 0 NORMAL 4O 3P 7C AE 1 2 - - 0 J1 - - - extra",
                      parsed));
    CHECK(!decodeSync("SYNC 1 J9 J1 PLAY 3 0 0 NORMAL 4O 3P 7C AE 1 2 - - 0 J1 - - -", parsed));
    CHECK(!decodeSync("SYNC 1 J1 J1 PLAY 3 0 0 NORMAL XX 3P 7C AE 1 2 - - 0 J1 - - -", parsed));
}

TEST(protocol_phase_tokens_roundtrip) {
    const NetPhase phases[] = {NetPhase::Play,      NetPhase::TrucoAnswer, NetPhase::Eleven,
                               NetPhase::Wait,      NetPhase::HandEnd,    NetPhase::MatchEnd};
    for (NetPhase phase : phases) {
        NetPhase parsed = NetPhase::Wait;
        CHECK(phaseFromToken(phaseToken(phase), parsed));
        CHECK(parsed == phase);
    }
}
