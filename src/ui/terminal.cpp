// POSIX terminal backend (termios + signals). Windows lives in terminal_win.cpp.
#ifndef _WIN32

#include "ui/terminal.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace truco {
namespace {

volatile sig_atomic_t g_resize = 0;
volatile sig_atomic_t g_quit = 0;

extern "C" void onResize(int) {
    g_resize = 1;
}

extern "C" void onQuit(int) {
    g_quit = 1;
}

}  // namespace

bool consumeResizeFlag() {
    if (g_resize != 0) {
        g_resize = 0;
        return true;
    }
    return false;
}

bool consumeQuitFlag() {
    if (g_quit != 0) {
        g_quit = 0;
        return true;
    }
    return false;
}

Terminal::Terminal() {
    enter();
}

Terminal::~Terminal() {
    leave();
}

void Terminal::enter() {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &original_) != 0) {
        return;
    }

    termios raw = original_;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~OPOST;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return;
    }
    active_ = true;

    struct sigaction resizeAction {};
    resizeAction.sa_handler = onResize;
    sigemptyset(&resizeAction.sa_mask);
    resizeAction.sa_flags = 0;
    sigaction(SIGWINCH, &resizeAction, nullptr);

    struct sigaction quitAction {};
    quitAction.sa_handler = onQuit;
    sigemptyset(&quitAction.sa_mask);
    quitAction.sa_flags = 0;
    sigaction(SIGINT, &quitAction, nullptr);
    sigaction(SIGTERM, &quitAction, nullptr);
    handlerInstalled_ = true;

    const char* term = std::getenv("TERM");
    mouseSupported_ = term != nullptr && std::strcmp(term, "dumb") != 0;
    mouseEnabled_ = mouseSupported_;

    std::string sequence = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";
    if (mouseEnabled_) {
        sequence += "\x1b[?1000h\x1b[?1006h";
    }
    write(sequence);
}

void Terminal::leave() {
    if (!active_) {
        return;
    }
    std::string sequence;
    if (mouseEnabled_) {
        sequence += "\x1b[?1006l\x1b[?1000l";
    }
    sequence += "\x1b[?25h\x1b[?1049l";
    write(sequence);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
    if (handlerInstalled_) {
        std::signal(SIGWINCH, SIG_DFL);
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
    }
    active_ = false;
}

Size Terminal::size() const {
    Size result;
    struct winsize window {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0) {
        if (window.ws_col > 0) {
            result.cols = static_cast<int>(window.ws_col);
        }
        if (window.ws_row > 0) {
            result.rows = static_cast<int>(window.ws_row);
        }
    }
    return result;
}

void Terminal::write(const std::string& text) {
    size_t offset = 0;
    while (offset < text.size()) {
        const ssize_t written =
            ::write(STDOUT_FILENO, text.data() + offset, text.size() - offset);
        if (written <= 0) {
            break;
        }
        offset += static_cast<size_t>(written);
    }
}

std::string readInput(int timeoutMs) {
    std::string bytes;
    struct pollfd descriptor {};
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    const int ready = poll(&descriptor, 1, timeoutMs);
    if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
        char buffer[256];
        const ssize_t got = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (got > 0) {
            bytes.assign(buffer, static_cast<size_t>(got));
        }
    }
    return bytes;
}

}  // namespace truco

#endif  // !_WIN32
