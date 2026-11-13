#include "ui/input.hpp"

#include <cctype>
#include <cstdlib>

#include <poll.h>
#include <unistd.h>

#include "ui/terminal.hpp"

namespace truco {
namespace {

int parseNumber(const std::string& buffer, size_t& index) {
    int value = 0;
    size_t digits = 0;
    while (index < buffer.size() &&
           std::isdigit(static_cast<unsigned char>(buffer[index])) != 0) {
        value = value * 10 + (buffer[index] - '0');
        ++index;
        ++digits;
    }
    return digits > 0 ? value : -1;
}

size_t utf8Length(unsigned char lead) {
    if ((lead & 0x80) == 0) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

void pushMouse(std::vector<InputEvent>& events, int button, int column, int row) {
    if ((button & 64) != 0) {
        return;  // wheel
    }
    if ((button & 32) != 0) {
        InputEvent event;
        event.kind = InputEvent::Kind::MouseMove;
        event.x = column - 1;
        event.y = row - 1;
        events.push_back(event);
        return;
    }
    if ((button & 3) != 0) {
        return;  // middle/right/release
    }
    InputEvent event;
    event.kind = InputEvent::Kind::MouseClick;
    event.x = column - 1;
    event.y = row - 1;
    events.push_back(event);
}

}  // namespace

std::vector<InputEvent> parseInput(const std::string& buffer) {
    std::vector<InputEvent> events;
    size_t i = 0;
    while (i < buffer.size()) {
        const unsigned char byte = static_cast<unsigned char>(buffer[i]);

        if (byte == 0x1b) {
            if (i + 1 < buffer.size() && buffer[i + 1] == '[') {
                const size_t third = i + 2;
                if (third >= buffer.size()) {
                    break;  // truncated CSI
                }
                if (buffer[third] == '<') {
                    size_t cursor = third + 1;
                    const int button = parseNumber(buffer, cursor);
                    if (button < 0 || cursor >= buffer.size() || buffer[cursor] != ';') {
                        break;
                    }
                    ++cursor;
                    const int column = parseNumber(buffer, cursor);
                    if (column < 0 || cursor >= buffer.size() || buffer[cursor] != ';') {
                        break;
                    }
                    ++cursor;
                    const int row = parseNumber(buffer, cursor);
                    if (row < 0 || cursor >= buffer.size()) {
                        break;
                    }
                    const char terminator = buffer[cursor];
                    if (terminator == 'M') {
                        pushMouse(events, button, column, row);
                    }
                    i = cursor + 1;
                    continue;
                }
                if (buffer[third] == 'M') {
                    if (i + 5 >= buffer.size()) {
                        break;  // truncated X10 packet
                    }
                    const int button = static_cast<unsigned char>(buffer[i + 3]) - 32;
                    const int column = static_cast<unsigned char>(buffer[i + 4]) - 32;
                    const int row = static_cast<unsigned char>(buffer[i + 5]) - 32;
                    pushMouse(events, button, column, row);
                    i += 6;
                    continue;
                }
                char key = 0;
                switch (buffer[third]) {
                    case 'A':
                        key = 'U';
                        break;
                    case 'B':
                        key = 'D';
                        break;
                    case 'C':
                        key = 'R';
                        break;
                    case 'D':
                        key = 'L';
                        break;
                    default:
                        break;
                }
                if (key != 0) {
                    InputEvent event;
                    event.kind = InputEvent::Kind::Key;
                    event.key = key;
                    events.push_back(event);
                }
                i = third + 1;
                continue;
            }
            InputEvent event;
            event.kind = InputEvent::Kind::Key;
            event.key = 27;
            events.push_back(event);
            ++i;
            continue;
        }

        if (byte == 0x03) {
            InputEvent event;
            event.kind = InputEvent::Kind::Quit;
            events.push_back(event);
            ++i;
            continue;
        }

        if (byte >= 0x80) {
            i += utf8Length(byte);
            continue;
        }

        if (byte == '\r' || byte == '\n') {
            const bool continuation = byte == '\n' && !events.empty() &&
                                      events.back().kind == InputEvent::Kind::Key &&
                                      events.back().key == '\r';
            if (!continuation) {
                InputEvent event;
                event.kind = InputEvent::Kind::Key;
                event.key = '\r';
                events.push_back(event);
            }
            ++i;
            continue;
        }

        if (byte == ' ' || (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z')) {
            InputEvent event;
            event.kind = InputEvent::Kind::Key;
            event.key = static_cast<char>(byte);
            events.push_back(event);
        }
        ++i;
    }
    return events;
}

InputEvent pollEvent(int timeoutMs) {
    static std::vector<InputEvent> queue;
    static std::string pending;

    if (!queue.empty()) {
        const InputEvent event = queue.front();
        queue.erase(queue.begin());
        return event;
    }

    struct pollfd descriptor {};
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    const int ready = poll(&descriptor, 1, timeoutMs);
    if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
        char buffer[256];
        const ssize_t got = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (got > 0) {
            pending.append(buffer, static_cast<size_t>(got));
            queue = parseInput(pending);
            pending.clear();
        }
    }

    if (consumeQuitFlag()) {
        queue.clear();
        InputEvent event;
        event.kind = InputEvent::Kind::Quit;
        return event;
    }
    if (consumeResizeFlag()) {
        InputEvent event;
        event.kind = InputEvent::Kind::Resize;
        return event;
    }
    if (!queue.empty()) {
        const InputEvent event = queue.front();
        queue.erase(queue.begin());
        return event;
    }
    return InputEvent{};
}

}  // namespace truco
