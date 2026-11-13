#include "ui/text_input.hpp"

#include "test_framework.hpp"

TEST(text_input_accepts_allowed_characters) {
    TextInput input("0123456789.", "");
    CHECK(input.handleKey('1'));
    CHECK(input.handleKey('9'));
    CHECK(input.handleKey('2'));
    CHECK(input.handleKey('.'));
    CHECK(input.handleKey('1'));
    CHECK_EQ(input.value(), std::string("192.1"));
    CHECK_EQ(input.cursor(), size_t{5});
}

TEST(text_input_rejects_disallowed_characters) {
    TextInput input("0123456789", "");
    CHECK(!input.handleKey('a'));
    CHECK(!input.handleKey(' '));
    CHECK(input.empty());
}

TEST(text_input_backspace_removes_before_cursor) {
    TextInput input("0123456789", "4000");
    CHECK(input.handleKey(127));
    CHECK_EQ(input.value(), std::string("400"));
    CHECK(input.handleKey(8));
    CHECK_EQ(input.value(), std::string("40"));
    CHECK_EQ(input.cursor(), size_t{2});
}

TEST(text_input_backspace_at_start_is_noop) {
    TextInput input("0123456789", "12");
    input.moveCursor(-5);
    CHECK_EQ(input.cursor(), size_t{0});
    CHECK(!input.handleKey(127));
    CHECK_EQ(input.value(), std::string("12"));
}

TEST(text_input_inserts_at_cursor_position) {
    TextInput input("0123456789.", "192168");
    input.moveCursor(-3);
    CHECK(input.handleKey('.'));
    CHECK_EQ(input.value(), std::string("192.168"));
    CHECK_EQ(input.cursor(), size_t{4});
}

TEST(text_input_move_cursor_bounds) {
    TextInput input("0123456789", "12345");
    input.moveCursor(10);
    CHECK_EQ(input.cursor(), size_t{5});
    input.moveCursor(-2);
    CHECK_EQ(input.cursor(), size_t{3});
}

TEST(text_input_set_repositions_cursor) {
    TextInput input("0123456789", "1");
    input.set("1234");
    CHECK_EQ(input.value(), std::string("1234"));
    CHECK_EQ(input.cursor(), size_t{4});
}
