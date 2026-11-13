#include "ui/text_input.hpp"

#include <algorithm>

TextInput::TextInput(std::string allowed, std::string initial)
    : allowed_(std::move(allowed)), value_(std::move(initial)), cursor_(value_.size()) {}

bool TextInput::handleKey(char key) {
    if (key == 127 || key == 8) {
        if (cursor_ == 0 || value_.empty()) {
            return false;
        }
        value_.erase(cursor_ - 1, 1);
        --cursor_;
        return true;
    }
    if (key < 0x20 || key > 0x7e) {
        return false;
    }
    if (allowed_.find(key) == std::string::npos) {
        return false;
    }
    value_.insert(cursor_, 1, key);
    ++cursor_;
    return true;
}

void TextInput::moveCursor(int delta) {
    if (delta < 0) {
        const size_t amount = static_cast<size_t>(-delta);
        cursor_ = amount > cursor_ ? 0 : cursor_ - amount;
        return;
    }
    cursor_ = std::min(value_.size(), cursor_ + static_cast<size_t>(delta));
}

void TextInput::set(std::string value) {
    value_ = std::move(value);
    cursor_ = value_.size();
}
