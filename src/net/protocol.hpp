#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/card.hpp"
#include "core/game.hpp"
#include "core/royale.hpp"

namespace truco {

constexpr size_t kMaxLine = 512;

enum class MessageType {
    Hello,
    Welcome,
    Error,
    Sync,
    Msg,
    Play,
    Truco,
    Accept,
    Run,
    Eleven,
    Rematch,
    Join,
    Lobby,
    Start,
    RState,
    Unknown
};

struct Message {
    MessageType type = MessageType::Unknown;
    std::vector<std::string> fields;
    std::string text;
};

Message parseLine(const std::string& line);
std::string encodeLine(MessageType type, const std::vector<std::string>& fields = {},
                       const std::string& text = {});

std::string sideToken(Side side);
bool sideFromToken(const std::string& token, Side& out);

std::string cardToken(Card card);
bool cardFromToken(const std::string& token, Card& out);

enum class NetPhase { Play, TrucoAnswer, Eleven, Wait, HandEnd, MatchEnd };

std::string phaseToken(NetPhase phase);
bool phaseFromToken(const std::string& token, NetPhase& out);

struct SyncState {
    uint32_t seq = 0;
    Side you = Side::Human;
    Side turn = Side::Human;
    NetPhase phase = NetPhase::Wait;
    int stake = 1;
    int tricksHuman = 0;
    int tricksCpu = 0;
    int scoreHuman = 0;
    int scoreCpu = 0;
    HandKind handKind = HandKind::Normal;
    bool canRaise = false;
    std::array<Card, 3> own{};
    std::array<bool, 3> playedOwn{};
    std::array<bool, 3> playedOpponent{};
    std::optional<Card> trickOwn;
    std::optional<Card> trickOpponent;
    bool revealed = false;
    Side elevenSide = Side::Human;
    std::array<Card, 3> opponent{};
};

// Serializes per-viewer state. The opponent's hand is never written unless
// revealed is true; the encoder has no other path to those cards.
std::string encodeSync(const SyncState& sync);
bool decodeSync(const std::string& line, SyncState& out);

std::string royalePhaseToken(RoyalePhase phase);
bool royalePhaseFromToken(const std::string& token, RoyalePhase& out);

struct RoyaleSeatView {
    int points = kRoyaleStartPoints;
    bool eliminated = false;
    bool inHand = false;
    std::optional<Card> tableCard;
    std::optional<std::array<Card, 3>> revealed;
};

struct RoyaleView {
    uint32_t seq = 0;
    int you = 0;
    RoyalePhase phase = RoyalePhase::Playing;
    int turn = -1;
    int stake = 1;
    int caller = -1;
    int responder = -1;
    int handWinner = -1;
    int matchWinner = -1;
    int tricksPlayed = 0;
    int playerCount = 0;
    bool canRaise = false;
    std::array<RoyaleSeatView, kRoyaleMaxPlayers> seats{};
    std::array<Card, 3> own{};
    std::array<bool, 3> playedOwn{};
};

std::string encodeRoyaleState(const RoyaleView& view);
bool decodeRoyaleState(const std::string& line, RoyaleView& out);

std::string encodeLobby(const std::vector<std::string>& names);
bool decodeLobby(const std::string& line, std::vector<std::string>& names);

}  // namespace truco
