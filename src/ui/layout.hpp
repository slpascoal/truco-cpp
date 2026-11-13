#pragma once

#include "ui/size.hpp"

namespace truco {

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    bool empty() const { return w <= 0 || h <= 0; }
};

struct Layout {
    bool fits = false;
    Rect banner;
    Rect scoreHuman;
    Rect scoreCpu;
    Rect stake;
    Rect cpuCards[3];
    Rect humanCards[3];
    Rect trickArea;
    Rect actions[4];
    int actionCount = 0;
    Rect status;

    static Layout compute(Size size, int actionCount);
};

}  // namespace truco
