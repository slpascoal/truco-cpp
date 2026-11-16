// Windows console backend. The console is switched to virtual terminal mode so
// the renderer and the input parser keep speaking ANSI, exactly as on POSIX.
#ifdef _WIN32

#include "ui/terminal.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace truco {
namespace {

volatile LONG g_quit = 0;
volatile LONG g_active = 0;

bool g_sizeKnown = false;
int g_lastCols = 0;
int g_lastRows = 0;
bool g_leftButtonDown = false;

BOOL WINAPI onConsoleControl(DWORD type) {
    InterlockedExchange(&g_quit, 1);
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        // The process is killed as soon as this handler returns: give the event
        // loop a moment to leave and restore the console first.
        for (int waited = 0; waited < 2000 && g_active != 0; waited += 10) {
            Sleep(10);
        }
    }
    return TRUE;
}

bool querySize(int& cols, int& rows) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return false;
    }
    cols = info.srWindow.Right - info.srWindow.Left + 1;
    rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return cols > 0 && rows > 0;
}

void appendUtf8(std::string& bytes, unsigned int code) {
    if (code < 0x80) {
        bytes.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        bytes.push_back(static_cast<char>(0xC0 | (code >> 6)));
        bytes.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        bytes.push_back(static_cast<char>(0xE0 | (code >> 12)));
        bytes.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        bytes.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

void appendMouseReport(std::string& bytes, int button, const COORD& position) {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    int left = 0;
    int top = 0;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        left = info.srWindow.Left;
        top = info.srWindow.Top;
    }
    bytes += "\x1b[<" + std::to_string(button) + ";" + std::to_string(position.X - left + 1) +
             ";" + std::to_string(position.Y - top + 1) + "M";
}

// Consoles that do not translate the mouse into SGR sequences deliver native
// mouse records instead; re-encode them so the parser sees a single format.
void appendMouse(std::string& bytes, const MOUSE_EVENT_RECORD& mouse) {
    const bool leftDown = (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
    if (mouse.dwEventFlags == MOUSE_MOVED) {
        appendMouseReport(bytes, 35, mouse.dwMousePosition);
    } else if ((mouse.dwEventFlags == 0 || mouse.dwEventFlags == DOUBLE_CLICK) && leftDown &&
               !g_leftButtonDown) {
        appendMouseReport(bytes, 0, mouse.dwMousePosition);
    }
    g_leftButtonDown = leftDown;
}

}  // namespace

bool consumeResizeFlag() {
    int cols = 0;
    int rows = 0;
    if (!querySize(cols, rows)) {
        return false;
    }
    const bool changed = g_sizeKnown && (cols != g_lastCols || rows != g_lastRows);
    g_sizeKnown = true;
    g_lastCols = cols;
    g_lastRows = rows;
    return changed;
}

bool consumeQuitFlag() {
    return InterlockedExchange(&g_quit, 0) != 0;
}

Terminal::Terminal() {
    enter();
}

Terminal::~Terminal() {
    leave();
}

void Terminal::enter() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD inputMode = 0;
    DWORD outputMode = 0;
    if (!GetConsoleMode(input, &inputMode) || !GetConsoleMode(output, &outputMode)) {
        return;  // redirected stdin/stdout
    }
    originalInputMode_ = inputMode;
    originalOutputMode_ = outputMode;

    if (!SetConsoleMode(output, outputMode | ENABLE_PROCESSED_OUTPUT |
                                    ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                    DISABLE_NEWLINE_AUTO_RETURN)) {
        return;  // console without ANSI support (before Windows 10 1607)
    }
    // Raw input: no line buffering, echo, Ctrl+C processing or quick-edit
    // selection (which would swallow the clicks).
    if (!SetConsoleMode(input, ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_MOUSE_INPUT |
                                   ENABLE_EXTENDED_FLAGS)) {
        SetConsoleMode(output, originalOutputMode_);
        return;
    }
    originalOutputCodePage_ = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    active_ = true;
    InterlockedExchange(&g_active, 1);

    SetConsoleCtrlHandler(onConsoleControl, TRUE);
    handlerInstalled_ = true;

    mouseSupported_ = true;
    mouseEnabled_ = true;
    g_sizeKnown = false;
    g_leftButtonDown = false;

    write("\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H\x1b[?1000h\x1b[?1006h");
}

void Terminal::leave() {
    if (!active_) {
        return;
    }
    write("\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[?1049l");
    SetConsoleOutputCP(originalOutputCodePage_);
    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), originalOutputMode_);
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), originalInputMode_);
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    if (handlerInstalled_) {
        SetConsoleCtrlHandler(onConsoleControl, FALSE);
    }
    active_ = false;
    InterlockedExchange(&g_active, 0);
}

Size Terminal::size() const {
    Size result;
    int cols = 0;
    int rows = 0;
    if (querySize(cols, rows)) {
        result.cols = cols;
        result.rows = rows;
    }
    return result;
}

void Terminal::write(const std::string& text) {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    size_t offset = 0;
    while (offset < text.size()) {
        DWORD written = 0;
        if (!WriteFile(output, text.data() + offset, static_cast<DWORD>(text.size() - offset),
                       &written, nullptr) ||
            written == 0) {
            break;
        }
        offset += written;
    }
}

std::string readInput(int timeoutMs) {
    std::string bytes;
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    const DWORD timeout = timeoutMs > 0 ? static_cast<DWORD>(timeoutMs) : 0;
    DWORD mode = 0;
    if (!GetConsoleMode(input, &mode)) {
        // Redirected stdin: only pipes can be polled without blocking.
        DWORD available = 0;
        if (PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char buffer[256];
            DWORD got = 0;
            if (ReadFile(input, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
                bytes.assign(buffer, got);
            }
        } else {
            Sleep(timeout);
        }
        return bytes;
    }

    if (WaitForSingleObject(input, timeout) != WAIT_OBJECT_0) {
        return bytes;
    }
    DWORD queued = 0;
    if (!GetNumberOfConsoleInputEvents(input, &queued)) {
        return bytes;
    }
    while (queued > 0) {
        INPUT_RECORD records[64];
        DWORD got = 0;
        const DWORD wanted = queued < 64 ? queued : 64;
        if (!ReadConsoleInputW(input, records, wanted, &got) || got == 0) {
            break;
        }
        for (DWORD index = 0; index < got; ++index) {
            const INPUT_RECORD& record = records[index];
            if (record.EventType == KEY_EVENT) {
                const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
                if (key.bKeyDown && key.uChar.UnicodeChar != 0) {
                    for (WORD repeat = 0; repeat < (key.wRepeatCount > 0 ? key.wRepeatCount : 1);
                         ++repeat) {
                        appendUtf8(bytes, static_cast<unsigned int>(key.uChar.UnicodeChar));
                    }
                }
            } else if (record.EventType == MOUSE_EVENT) {
                appendMouse(bytes, record.Event.MouseEvent);
            }
        }
        queued -= got < queued ? got : queued;
    }
    return bytes;
}

}  // namespace truco

#endif  // _WIN32
