#pragma once

#include <string>

class TextInput {
public:
    TextInput() = default;
    explicit TextInput(std::string allowed, std::string initial = {});

    bool handleKey(char key);          // 8/127 backspace; printable allowed chars
    void moveCursor(int delta);
    void set(std::string value);

    const std::string& value() const { return value_; }
    size_t cursor() const { return cursor_; }
    bool empty() const { return value_.empty(); }

private:
    std::string allowed_;
    std::string value_;
    size_t cursor_ = 0;
};
