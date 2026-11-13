#pragma once

#include <string>

#include <termios.h>

#include "ui/size.hpp"

namespace truco {

// RAII owner of the interactive terminal state: raw mode, alternate screen,
// hidden cursor and SGR mouse tracking. Everything is restored on destruction.
class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    bool mouseSupported() const { return mouseSupported_; }
    Size size() const;
    void write(const std::string& text);

private:
    void enter();
    void leave();

    termios original_{};
    bool active_ = false;
    bool mouseSupported_ = false;
    bool mouseEnabled_ = false;
    bool handlerInstalled_ = false;
};

// True once after a SIGWINCH was received; consumed by the event loop.
bool consumeResizeFlag();

// True once after SIGINT/SIGTERM was received; the loop must exit cleanly.
bool consumeQuitFlag();

}  // namespace truco
