#include "net/protocol.hpp"

#include <cctype>

namespace truco {
namespace {

struct TypeEntry {
    const char* token;
    MessageType type;
};

constexpr TypeEntry kTypes[] = {
    {"HELLO", MessageType::Hello}, {"WELCOME", MessageType::Welcome},
    {"ERROR", MessageType::Error}, {"SYNC", MessageType::Sync},
    {"MSG", MessageType::Msg},     {"PLAY", MessageType::Play},
    {"TRUCO", MessageType::Truco}, {"ACCEPT", MessageType::Accept},
    {"RUN", MessageType::Run},     {"ELEVEN", MessageType::Eleven},
    {"REMATCH", MessageType::Rematch}, {"JOIN", MessageType::Join},
    {"LOBBY", MessageType::Lobby}, {"START", MessageType::Start},
    {"RSTATE", MessageType::RState},
};

const char* typeToken(MessageType type) {
    for (const TypeEntry& entry : kTypes) {
        if (entry.type == type) {
            return entry.token;
        }
    }
    return "";
}

MessageType typeFromToken(const std::string& token) {
    for (const TypeEntry& entry : kTypes) {
        if (token == entry.token) {
            return entry.type;
        }
    }
    return MessageType::Unknown;
}

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::vector<std::string> splitTokens(const std::string& value) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < value.size()) {
        while (i < value.size() && value[i] == ' ') {
            ++i;
        }
        const size_t start = i;
        while (i < value.size() && value[i] != ' ') {
            ++i;
        }
        if (i > start) {
            tokens.push_back(value.substr(start, i - start));
        }
    }
    return tokens;
}

bool parseInt(const std::string& token, int min, int max, int& out) {
    if (token.empty() || token.size() > 6) {
        return false;
    }
    int value = 0;
    for (char ch : token) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    if (value < min || value > max) {
        return false;
    }
    out = value;
    return true;
}

bool parseSigned(const std::string& token, int min, int max, int& out) {
    if (token.empty()) {
        return false;
    }
    if (token[0] == '-') {
        int value = 0;
        if (!parseInt(token.substr(1), 0, 99, value)) {
            return false;
        }
        value = -value;
        if (value < min || value > max) {
            return false;
        }
        out = value;
        return true;
    }
    return parseInt(token, min, max, out);
}

std::string kindToken(HandKind kind) {
    switch (kind) {
        case HandKind::Normal:
            return "NORMAL";
        case HandKind::HandOfEleven:
            return "ELEVEN";
        case HandKind::IronHand:
            return "IRON";
    }
    return "NORMAL";
}

bool kindFromToken(const std::string& token, HandKind& out) {
    if (token == "NORMAL") {
        out = HandKind::Normal;
        return true;
    }
    if (token == "ELEVEN") {
        out = HandKind::HandOfEleven;
        return true;
    }
    if (token == "IRON") {
        out = HandKind::IronHand;
        return true;
    }
    return false;
}

std::string maskToken(const std::array<bool, 3>& played) {
    int mask = 0;
    for (int i = 0; i < 3; ++i) {
        if (played[static_cast<size_t>(i)]) {
            mask |= 1 << i;
        }
    }
    return std::to_string(mask);
}

bool parseMask(const std::string& token, std::array<bool, 3>& out) {
    int mask = 0;
    if (!parseInt(token, 0, 7, mask)) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        out[static_cast<size_t>(i)] = (mask & (1 << i)) != 0;
    }
    return true;
}

std::string trickToken(const std::optional<Card>& card) {
    return card.has_value() ? cardToken(*card) : "-";
}

bool parseTrick(const std::string& token, std::optional<Card>& out) {
    if (token == "-") {
        out.reset();
        return true;
    }
    Card card{};
    if (!cardFromToken(token, card)) {
        return false;
    }
    out = card;
    return true;
}

}  // namespace

std::string sideToken(Side side) {
    return side == Side::Human ? "J1" : "J2";
}

bool sideFromToken(const std::string& token, Side& out) {
    if (token == "J1") {
        out = Side::Human;
        return true;
    }
    if (token == "J2") {
        out = Side::Cpu;
        return true;
    }
    return false;
}

std::string cardToken(Card card) {
    const std::string rank = rankLabel(card.rank);
    char suit = 'O';
    switch (card.suit) {
        case Suit::Ouros:
            suit = 'O';
            break;
        case Suit::Espadas:
            suit = 'E';
            break;
        case Suit::Copas:
            suit = 'C';
            break;
        case Suit::Paus:
            suit = 'P';
            break;
    }
    return rank + std::string(1, suit);
}

bool cardFromToken(const std::string& token, Card& out) {
    if (token.size() != 2) {
        return false;
    }
    Rank rank = Rank::Four;
    switch (token[0]) {
        case '4':
            rank = Rank::Four;
            break;
        case '5':
            rank = Rank::Five;
            break;
        case '6':
            rank = Rank::Six;
            break;
        case '7':
            rank = Rank::Seven;
            break;
        case 'Q':
            rank = Rank::Queen;
            break;
        case 'J':
            rank = Rank::Jack;
            break;
        case 'K':
            rank = Rank::King;
            break;
        case 'A':
            rank = Rank::Ace;
            break;
        case '2':
            rank = Rank::Two;
            break;
        case '3':
            rank = Rank::Three;
            break;
        default:
            return false;
    }
    Suit suit = Suit::Ouros;
    switch (token[1]) {
        case 'O':
            suit = Suit::Ouros;
            break;
        case 'E':
            suit = Suit::Espadas;
            break;
        case 'C':
            suit = Suit::Copas;
            break;
        case 'P':
            suit = Suit::Paus;
            break;
        default:
            return false;
    }
    out = Card{rank, suit};
    return true;
}

std::string phaseToken(NetPhase phase) {
    switch (phase) {
        case NetPhase::Play:
            return "PLAY";
        case NetPhase::TrucoAnswer:
            return "TRUCO_ANSWER";
        case NetPhase::Eleven:
            return "ELEVEN";
        case NetPhase::Wait:
            return "WAIT";
        case NetPhase::HandEnd:
            return "HANDEND";
        case NetPhase::MatchEnd:
            return "MATCHEND";
    }
    return "WAIT";
}

bool phaseFromToken(const std::string& token, NetPhase& out) {
    if (token == "PLAY") {
        out = NetPhase::Play;
        return true;
    }
    if (token == "TRUCO_ANSWER") {
        out = NetPhase::TrucoAnswer;
        return true;
    }
    if (token == "ELEVEN") {
        out = NetPhase::Eleven;
        return true;
    }
    if (token == "WAIT") {
        out = NetPhase::Wait;
        return true;
    }
    if (token == "HANDEND") {
        out = NetPhase::HandEnd;
        return true;
    }
    if (token == "MATCHEND") {
        out = NetPhase::MatchEnd;
        return true;
    }
    return false;
}

Message parseLine(const std::string& raw) {
    Message message;
    std::string line = raw;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    if (line.empty() || line.size() > kMaxLine) {
        return message;
    }
    const size_t space = line.find(' ');
    const std::string head = space == std::string::npos ? line : line.substr(0, space);
    message.type = typeFromToken(head);
    if (message.type == MessageType::Unknown) {
        return message;
    }
    if (space == std::string::npos) {
        return message;
    }
    const std::string rest = line.substr(space + 1);
    if (message.type == MessageType::Msg) {
        message.text = trim(rest);
        return message;
    }
    if (message.type == MessageType::Error) {
        const std::vector<std::string> tokens = splitTokens(rest);
        if (tokens.empty()) {
            message.type = MessageType::Unknown;
            return message;
        }
        message.fields.push_back(tokens[0]);
        size_t consumed = rest.find(tokens[0]) + tokens[0].size();
        message.text = trim(rest.substr(consumed));
        return message;
    }
    message.fields = splitTokens(rest);
    return message;
}

std::string encodeLine(MessageType type, const std::vector<std::string>& fields,
                       const std::string& text) {
    std::string line = typeToken(type);
    for (const std::string& field : fields) {
        line += ' ';
        line += field;
    }
    if (!text.empty()) {
        line += ' ';
        line += text;
    }
    line += '\n';
    return line;
}

std::string encodeSync(const SyncState& sync) {
    std::string line = "SYNC";
    line += ' ' + std::to_string(sync.seq);
    line += ' ' + sideToken(sync.you);
    line += ' ' + sideToken(sync.turn);
    line += ' ' + phaseToken(sync.phase);
    line += ' ' + std::to_string(sync.stake);
    line += ' ' + std::to_string(sync.tricksHuman);
    line += ' ' + std::to_string(sync.tricksCpu);
    line += ' ' + std::to_string(sync.scoreHuman);
    line += ' ' + std::to_string(sync.scoreCpu);
    line += ' ' + kindToken(sync.handKind);
    line += ' ' + std::string(sync.canRaise ? "1" : "0");
    for (int i = 0; i < 3; ++i) {
        line += ' ' + cardToken(sync.own[static_cast<size_t>(i)]);
    }
    line += ' ' + maskToken(sync.playedOwn);
    line += ' ' + maskToken(sync.playedOpponent);
    line += ' ' + trickToken(sync.trickOwn);
    line += ' ' + trickToken(sync.trickOpponent);
    line += ' ' + std::string(sync.revealed ? "1" : "0");
    line += ' ' + sideToken(sync.elevenSide);
    for (int i = 0; i < 3; ++i) {
        line += ' ';
        line += sync.revealed ? cardToken(sync.opponent[static_cast<size_t>(i)]) : "-";
    }
    line += '\n';
    return line;
}

bool decodeSync(const std::string& line, SyncState& out) {
    const Message message = parseLine(line);
    if (message.type != MessageType::Sync || message.fields.size() != 23) {
        return false;
    }
    const std::vector<std::string>& f = message.fields;
    SyncState sync;
    int seq = 0;
    if (!parseInt(f[0], 0, 999999, seq)) return false;
    sync.seq = static_cast<uint32_t>(seq);
    if (!sideFromToken(f[1], sync.you)) return false;
    if (!sideFromToken(f[2], sync.turn)) return false;
    if (!phaseFromToken(f[3], sync.phase)) return false;
    int stake = 0;
    if (!parseInt(f[4], 1, 12, stake)) return false;
    sync.stake = stake;
    if (!parseInt(f[5], 0, 3, sync.tricksHuman)) return false;
    if (!parseInt(f[6], 0, 3, sync.tricksCpu)) return false;
    if (!parseInt(f[7], 0, 99, sync.scoreHuman)) return false;
    if (!parseInt(f[8], 0, 99, sync.scoreCpu)) return false;
    if (!kindFromToken(f[9], sync.handKind)) return false;
    int canRaise = 0;
    if (!parseInt(f[10], 0, 1, canRaise)) return false;
    sync.canRaise = canRaise == 1;
    for (int i = 0; i < 3; ++i) {
        if (!cardFromToken(f[11 + static_cast<size_t>(i)],
                           sync.own[static_cast<size_t>(i)])) {
            return false;
        }
    }
    if (!parseMask(f[14], sync.playedOwn)) return false;
    if (!parseMask(f[15], sync.playedOpponent)) return false;
    if (!parseTrick(f[16], sync.trickOwn)) return false;
    if (!parseTrick(f[17], sync.trickOpponent)) return false;
    int revealed = 0;
    if (!parseInt(f[18], 0, 1, revealed)) return false;
    sync.revealed = revealed == 1;
    if (!sideFromToken(f[19], sync.elevenSide)) return false;
    for (int i = 0; i < 3; ++i) {
        const std::string& token = f[20 + static_cast<size_t>(i)];
        if (sync.revealed) {
            if (!cardFromToken(token, sync.opponent[static_cast<size_t>(i)])) {
                return false;
            }
        } else if (token != "-") {
            return false;
        }
    }
    out = sync;
    return true;
}

std::string royalePhaseToken(RoyalePhase phase) {
    switch (phase) {
        case RoyalePhase::Playing:
            return "PLAY";
        case RoyalePhase::TrucoAnswer:
            return "TRUCO_ANSWER";
        case RoyalePhase::HandEnd:
            return "HANDEND";
        case RoyalePhase::MatchEnd:
            return "MATCHEND";
    }
    return "PLAY";
}

bool royalePhaseFromToken(const std::string& token, RoyalePhase& out) {
    if (token == "PLAY") {
        out = RoyalePhase::Playing;
        return true;
    }
    if (token == "TRUCO_ANSWER") {
        out = RoyalePhase::TrucoAnswer;
        return true;
    }
    if (token == "HANDEND") {
        out = RoyalePhase::HandEnd;
        return true;
    }
    if (token == "MATCHEND") {
        out = RoyalePhase::MatchEnd;
        return true;
    }
    return false;
}

std::string encodeRoyaleState(const RoyaleView& view) {
    std::string line = "RSTATE";
    line += ' ' + std::to_string(view.seq);
    line += ' ' + std::to_string(view.you);
    line += ' ' + royalePhaseToken(view.phase);
    line += ' ' + std::to_string(view.turn);
    line += ' ' + std::to_string(view.stake);
    line += ' ' + std::to_string(view.caller);
    line += ' ' + std::to_string(view.responder);
    line += ' ' + std::to_string(view.handWinner);
    line += ' ' + std::to_string(view.matchWinner);
    line += ' ' + std::to_string(view.tricksPlayed);
    line += ' ' + std::to_string(view.playerCount);
    for (int i = 0; i < view.playerCount; ++i) {
        const RoyaleSeatView& seat = view.seats.at(static_cast<size_t>(i));
        line += ' ' + std::to_string(seat.points);
        line += ' ' + std::string(seat.eliminated ? "1" : "0");
        line += ' ' + std::string(seat.inHand ? "1" : "0");
        line += ' ' + trickToken(seat.tableCard);
        if (seat.revealed.has_value()) {
            line += " 1";
            for (int card = 0; card < 3; ++card) {
                line += ' ' + cardToken(seat.revealed->at(static_cast<size_t>(card)));
            }
        } else {
            line += " 0 - - -";
        }
    }
    for (int card = 0; card < 3; ++card) {
        line += ' ' + cardToken(view.own.at(static_cast<size_t>(card)));
    }
    line += ' ' + maskToken(view.playedOwn);
    line += '\n';
    return line;
}

bool decodeRoyaleState(const std::string& line, RoyaleView& out) {
    const Message message = parseLine(line);
    if (message.type != MessageType::RState) {
        return false;
    }
    const std::vector<std::string>& f = message.fields;
    RoyaleView view;
    size_t index = 0;
    int value = 0;
    if (f.size() < 11 || !parseInt(f[index++], 0, 999999, value)) return false;
    view.seq = static_cast<uint32_t>(value);
    if (!parseInt(f[index++], 0, kRoyaleMaxPlayers - 1, view.you)) return false;
    if (!royalePhaseFromToken(f[index++], view.phase)) return false;
    if (!parseSigned(f[index++], -1, kRoyaleMaxPlayers - 1, view.turn)) return false;
    if (!parseInt(f[index++], 1, 12, view.stake)) return false;
    if (!parseSigned(f[index++], -1, kRoyaleMaxPlayers - 1, view.caller)) return false;
    if (!parseSigned(f[index++], -1, kRoyaleMaxPlayers - 1, view.responder)) return false;
    if (!parseSigned(f[index++], -1, kRoyaleMaxPlayers - 1, view.handWinner)) return false;
    if (!parseSigned(f[index++], -1, kRoyaleMaxPlayers - 1, view.matchWinner)) return false;
    if (!parseInt(f[index++], 0, 3, view.tricksPlayed)) return false;
    if (!parseInt(f[index++], 3, kRoyaleMaxPlayers, view.playerCount)) return false;

    for (int i = 0; i < view.playerCount; ++i) {
        RoyaleSeatView& seat = view.seats.at(static_cast<size_t>(i));
        if (index >= f.size()) return false;
        if (!parseSigned(f[index++], -99, 99, seat.points)) return false;
        int flag = 0;
        if (index >= f.size() || !parseInt(f[index++], 0, 1, flag)) return false;
        seat.eliminated = flag == 1;
        if (index >= f.size() || !parseInt(f[index++], 0, 1, flag)) return false;
        seat.inHand = flag == 1;
        if (index >= f.size() || !parseTrick(f[index++], seat.tableCard)) return false;
        if (index >= f.size() || !parseInt(f[index++], 0, 1, flag)) return false;
        if (flag == 1) {
            std::array<Card, 3> cards{};
            for (int card = 0; card < 3; ++card) {
                if (index >= f.size() ||
                    !cardFromToken(f[index++], cards.at(static_cast<size_t>(card)))) {
                    return false;
                }
            }
            seat.revealed = cards;
        } else {
            for (int card = 0; card < 3; ++card) {
                if (index >= f.size() || f[index++] != "-") {
                    return false;
                }
            }
        }
    }
    for (int card = 0; card < 3; ++card) {
        if (index >= f.size() || !cardFromToken(f[index++], view.own.at(static_cast<size_t>(card)))) {
            return false;
        }
    }
    if (index >= f.size() || !parseMask(f[index++], view.playedOwn)) return false;
    if (index != f.size()) {
        return false;
    }
    out = view;
    return true;
}

std::string encodeLobby(const std::vector<std::string>& names) {
    std::string line = "LOBBY " + std::to_string(names.size());
    for (const std::string& name : names) {
        line += ' ' + name;
    }
    line += '\n';
    return line;
}

bool decodeLobby(const std::string& line, std::vector<std::string>& names) {
    const Message message = parseLine(line);
    if (message.type != MessageType::Lobby || message.fields.empty()) {
        return false;
    }
    int count = 0;
    if (!parseInt(message.fields.at(0), 0, kRoyaleMaxPlayers, count)) {
        return false;
    }
    if (static_cast<int>(message.fields.size()) != count + 1) {
        return false;
    }
    names.assign(message.fields.begin() + 1, message.fields.end());
    return true;
}

}  // namespace truco
