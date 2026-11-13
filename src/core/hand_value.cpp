#include "core/hand_value.hpp"

namespace truco {

Stake nextStake(Stake current) {
    switch (current) {
        case Stake::One:
            return Stake::Three;
        case Stake::Three:
            return Stake::Six;
        case Stake::Six:
            return Stake::Nine;
        case Stake::Nine:
            return Stake::Twelve;
        case Stake::Twelve:
            return Stake::Twelve;
    }
    return Stake::One;
}

bool canRaise(Stake current) {
    return current != Stake::Twelve;
}

Stake valueAfterAccept(Stake current) {
    return nextStake(current);
}

const char* callLabel(Stake current) {
    switch (current) {
        case Stake::One:
            return "Truco";
        case Stake::Three:
            return "Seis";
        case Stake::Six:
            return "Nove";
        case Stake::Nine:
            return "Doze";
        case Stake::Twelve:
            return "Doze";
    }
    return "Truco";
}

int valuePoints(Stake stake) {
    return static_cast<int>(stake);
}

}  // namespace truco
