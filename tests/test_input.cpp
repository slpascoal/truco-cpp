#include "ui/input.hpp"
#include "ui/terminal.hpp"

#include "test_framework.hpp"

using namespace truco;

TEST(parse_digit_keys) {
    const auto events = parseInput("1");
    CHECK_EQ(events.size(), size_t{1});
    CHECK(events[0].kind == InputEvent::Kind::Key);
    CHECK_EQ(events[0].key, '1');
}

TEST(parse_enter_normalizes_newline) {
    const auto events = parseInput("\r\n");
    CHECK_EQ(events.size(), size_t{1});
    CHECK_EQ(events[0].key, '\r');
}

TEST(parse_escape_alone) {
    const auto events = parseInput("\x1b");
    CHECK_EQ(events.size(), size_t{1});
    CHECK_EQ(events[0].key, 27);
}

TEST(parse_arrows_map_to_internal_keys) {
    CHECK_EQ(parseInput("\x1b[A")[0].key, 'U');
    CHECK_EQ(parseInput("\x1b[B")[0].key, 'D');
    CHECK_EQ(parseInput("\x1b[C")[0].key, 'R');
    CHECK_EQ(parseInput("\x1b[D")[0].key, 'L');
}

TEST(parse_sgr_mouse_press_is_zero_based) {
    const auto events = parseInput("\x1b[<0;10;5M");
    CHECK_EQ(events.size(), size_t{1});
    CHECK(events[0].kind == InputEvent::Kind::MouseClick);
    CHECK_EQ(events[0].x, 9);
    CHECK_EQ(events[0].y, 4);
}

TEST(parse_sgr_mouse_release_is_ignored) {
    const auto events = parseInput("\x1b[<0;10;5m");
    CHECK_EQ(events.size(), size_t{0});
}

TEST(parse_x10_mouse_press_is_zero_based) {
    std::string buffer = "\x1b[M";
    buffer.push_back(static_cast<char>(32 + 0));
    buffer.push_back(static_cast<char>(32 + 5));
    buffer.push_back(static_cast<char>(32 + 3));
    const auto events = parseInput(buffer);
    CHECK_EQ(events.size(), size_t{1});
    CHECK(events[0].kind == InputEvent::Kind::MouseClick);
    CHECK_EQ(events[0].x, 4);
    CHECK_EQ(events[0].y, 2);
}

TEST(parse_multiple_events_in_one_buffer) {
    const auto events = parseInput("1\x1b[<0;3;4M2");
    CHECK_EQ(events.size(), size_t{3});
    CHECK_EQ(events[0].key, '1');
    CHECK(events[1].kind == InputEvent::Kind::MouseClick);
    CHECK_EQ(events[1].x, 2);
    CHECK_EQ(events[1].y, 3);
    CHECK_EQ(events[2].key, '2');
}

TEST(parse_ignores_garbage_and_utf8) {
    std::string buffer = "\xFF";
    buffer += "\xE2\x99\xA5";
    buffer += "q";
    const auto events = parseInput(buffer);
    CHECK_EQ(events.size(), size_t{1});
    CHECK_EQ(events[0].key, 'q');
}

TEST(parse_ignores_truncated_sequences) {
    CHECK_EQ(parseInput("\x1b[<0;1").size(), size_t{0});
    CHECK_EQ(parseInput("\x1b[").size(), size_t{0});
    CHECK_EQ(parseInput("\x1b[M\x01").size(), size_t{0});
}

TEST(terminal_is_safe_without_tty) {
    Terminal terminal;
    CHECK(!terminal.mouseSupported());
    const Size size = terminal.size();
    CHECK(size.cols > 0);
    CHECK(size.rows > 0);
}
