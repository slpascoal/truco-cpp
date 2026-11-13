#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

extern int g_checks;
extern int g_failures;

void registerTest(const char* name, void (*fn)());

struct TestReg {
    TestReg(const char* name, void (*fn)()) { registerTest(name, fn); }
};

#define TEST(name)                                        \
    static void name();                                   \
    static TestReg reg_##name(#name, name);               \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        const auto _a = (a);                                                     \
        const auto _b = (b);                                                    \
        if (!(_a == _b)) {                                                       \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);   \
        }                                                                        \
    } while (0)

#define CHECK_NE(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        const auto _a = (a);                                                     \
        const auto _b = (b);                                                    \
        if (_a == _b) {                                                          \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);   \
        }                                                                        \
    } while (0)

#define CHECK_MSG(cond, msg)                                                     \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, msg);\
        }                                                                        \
    } while (0)
