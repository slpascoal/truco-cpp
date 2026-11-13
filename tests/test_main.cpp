#include "test_framework.hpp"

#include <cstdio>

int g_checks = 0;
int g_failures = 0;

namespace {
struct Entry {
    const char* name;
    void (*fn)();
};
std::vector<Entry>& registry() {
    static std::vector<Entry> entries;
    return entries;
}
}  // namespace

void registerTest(const char* name, void (*fn)()) {
    registry().push_back({name, fn});
}

int main() {
    for (const Entry& entry : registry()) {
        const int before = g_failures;
        entry.fn();
        std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", entry.name);
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
