#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/card.hpp"
#include "ui/layout.hpp"
#include "ui/size.hpp"
#include "ui/terminal.hpp"

namespace truco {

struct ViewModel {
    enum class Screen { Menu, Handoff, Game, MatchResult, LanConfig, LanWaiting, LanError };

    Screen screen = Screen::Menu;
    std::string title = "TRUCO MINEIRO";
    std::string subtitle = "Voce contra o computador";
    std::string humanLabel = "VOCE";
    std::string cpuLabel = "COMPUTADOR";
    std::string handoffTitle;
    std::string handoffText;
    std::string humanTrickLabel = "VOCE";
    std::string cpuTrickLabel = "CPU";

    std::string lanTitle;
    std::string lanFieldLabel[2];
    std::string lanFieldValue[2];
    int lanFieldFocus = -1;
    std::string lanHint;
    std::string lanAddressText;
    std::string lanErrorText;

    std::array<Card, 3> humanCards{};
    std::array<bool, 3> humanPlayed{};
    std::array<Card, 3> cpuCards{};
    std::array<bool, 3> cpuPlayed{};

    std::optional<Card> vira;
    std::optional<Card> humanTrickCard;
    std::optional<Card> cpuTrickCard;

    int humanScore = 0;
    int cpuScore = 0;
    std::string stakeText;
    std::string manilhaText;
    std::string trickText;
    std::string turnText;
    std::string message;

    std::vector<std::string> actions;
    int selectedAction = -1;
    int selectedCard = -1;

    bool revealCpu = false;
    bool mouseWarning = false;
    bool ensureSize = false;
    bool showEleven = false;
    std::string elevenText;
    std::string matchResultText;
};

struct HitTarget {
    Rect rect;
    int actionId;
};

class Renderer {
public:
    explicit Renderer(Terminal& terminal) : terminal_(terminal) {}

    void draw(const ViewModel& vm, Size size) { terminal_.write(render(vm, size)); }

    std::string render(const ViewModel& vm, Size size);

    const std::vector<HitTarget>& hitTargets() const { return hitTargets_; }

    static std::string renderCard(Card card, bool faceDown = false);
    static std::string renderCardAt(Card card, bool faceDown, bool highlight);

private:
    Terminal& terminal_;
    std::vector<HitTarget> hitTargets_;
};

}  // namespace truco
