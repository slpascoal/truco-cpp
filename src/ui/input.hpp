#pragma once

#include <string>
#include <vector>

namespace truco {

struct InputEvent {
    enum class Kind { None, Key, MouseClick, MouseMove, Resize, Quit };

    Kind kind = Kind::None;
    char key = 0;  // '1'..'3', '\r', ' ', 27 (Esc); arrows map to U/D/L/R
    int x = 0;     // zero-based column
    int y = 0;     // zero-based row
};

// Parses a raw byte buffer (SGR 1006 and X10 mouse, control keys, arrows).
// Unknown, malformed or truncated sequences are ignored.
std::vector<InputEvent> parseInput(const std::string& buffer);

InputEvent pollEvent(int timeoutMs);

}  // namespace truco
