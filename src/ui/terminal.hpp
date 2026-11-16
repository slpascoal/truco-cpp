#pragma once

#include <string>

#ifndef _WIN32
#include <termios.h>
#endif

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

#ifdef _WIN32
    unsigned long originalInputMode_ = 0;
    unsigned long originalOutputMode_ = 0;
    unsigned int originalOutputCodePage_ = 0;
#else
    termios original_{};
#endif
    bool active_ = false;
    bool mouseSupported_ = false;
    bool mouseEnabled_ = false;
    bool handlerInstalled_ = false;
};

// True once after the terminal was resized; consumed by the event loop.
bool consumeResizeFlag();

// True once after an interrupt/close request; the loop must exit cleanly.
bool consumeQuitFlag();

// Waits up to timeoutMs for raw input bytes (keys and mouse reports encoded as
// ANSI sequences). Returns an empty string on timeout.
std::string readInput(int timeoutMs);

}  // namespace truco
