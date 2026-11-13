#include "ui/layout.hpp"

namespace truco {
namespace {
constexpr int kCardW = 9;
constexpr int kCardH = 7;
constexpr int kGap = 3;
constexpr int kGroupW = 3 * kCardW + 2 * kGap;

Rect centered(int cols, int y, int width, int height) {
    Rect rect;
    rect.x = (cols - width) / 2;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    return rect;
}
}  // namespace

Layout Layout::compute(Size size, int actionCount) {
    Layout layout;
    layout.fits = size.cols >= 80 && size.rows >= 24;
    if (!layout.fits) {
        return layout;
    }

    layout.banner = Rect{0, 0, size.cols, 2};
    layout.scoreHuman = Rect{1, 0, size.cols / 2 - 2, 1};
    layout.scoreCpu = Rect{size.cols / 2 + 1, 0, size.cols / 2 - 2, 1};
    layout.stake = centered(size.cols, 1, 16, 1);

    const int groupX = (size.cols - kGroupW) / 2;
    const int cpuY = 2;
    for (int i = 0; i < 3; ++i) {
        layout.cpuCards[i] = Rect{groupX + i * (kCardW + kGap), cpuY, kCardW, kCardH};
        layout.humanCards[i] =
            Rect{groupX + i * (kCardW + kGap), size.rows - 11, kCardW, kCardH};
    }

    layout.trickArea = Rect{0, cpuY + kCardH + 2, size.cols, size.rows - 11 - (cpuY + kCardH + 2)};
    layout.vira = Rect{size.cols - 14, cpuY + kCardH + 3, kCardW, kCardH};

    const int count = actionCount < 0 ? 0 : (actionCount > 4 ? 4 : actionCount);
    layout.actionCount = count;
    const int buttonW = 18;
    const int buttonGap = 2;
    const int totalW = count > 0 ? count * buttonW + (count - 1) * buttonGap : 0;
    int actionX = (size.cols - totalW) / 2;
    for (int i = 0; i < count; ++i) {
        layout.actions[i] = Rect{actionX, size.rows - 4, buttonW, 2};
        actionX += buttonW + buttonGap;
    }

    layout.status = Rect{0, size.rows - 1, size.cols, 1};
    return layout;
}

}  // namespace truco
