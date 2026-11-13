#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/card.hpp"
#include "core/game.hpp"

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
    int scoreHuman = 0;
    int scoreCpu = 0;
    HandKind handKind = HandKind::Normal;
    Card vira{};
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

}  // namespace truco
