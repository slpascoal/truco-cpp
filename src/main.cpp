#include <cstdlib>
#include <random>

#include "app/session.hpp"

int main() {
    uint32_t seed = 0;
    const char* fromEnv = std::getenv("TRUCO_SEED");
    if (fromEnv != nullptr) {
        seed = static_cast<uint32_t>(std::strtoul(fromEnv, nullptr, 10));
    } else {
        std::random_device device;
        seed = device();
    }
    truco::Session session(seed);
    return session.run();
}
