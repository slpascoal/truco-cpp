#include "ui/render.hpp"

#include <algorithm>

namespace truco {
namespace {

constexpr int kDefaultColor = 0;
constexpr int kBorder = 7;
constexpr int kSelected = 9;

int suitColor(Suit suit) {
    switch (suit) {
        case Suit::Ouros:
            return 3;  // yellow
        case Suit::Espadas:
            return 6;  // cyan
        case Suit::Copas:
            return 1;  // red
        case Suit::Paus:
            return 2;  // green
    }
    return kDefaultColor;
}

size_t glyphLength(unsigned char lead) {
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

std::string colorSequence(int color) {
    switch (color) {
        case 0:
            return "\x1b[0m";
        case 1:
            return "\x1b[1;31m";
        case 2:
            return "\x1b[1;32m";
        case 3:
            return "\x1b[1;33m";
        case 4:
            return "\x1b[1;34m";
        case 5:
            return "\x1b[1;35m";
        case 6:
            return "\x1b[1;36m";
        case 7:
            return "\x1b[1;37m";
        case 8:
            return "\x1b[7m";
        case 9:
            return "\x1b[1;30;103m";
        default:
            return "\x1b[0m";
    }
}

class Canvas {
public:
    Canvas(int cols, int rows) : cols_(cols), rows_(rows) {
        cells_.assign(static_cast<size_t>(rows), std::vector<std::string>(static_cast<size_t>(cols), " "));
        colors_.assign(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(cols), kDefaultColor));
    }

    void put(int x, int y, const std::string& glyph, int color) {
        if (x < 0 || y < 0 || x >= cols_ || y >= rows_) return;
        cells_[static_cast<size_t>(y)][static_cast<size_t>(x)] = glyph;
        colors_[static_cast<size_t>(y)][static_cast<size_t>(x)] = color;
    }

    void text(int x, int y, const std::string& value, int color) {
        size_t i = 0;
        int cursor = x;
        while (i < value.size()) {
            const size_t length = std::min(glyphLength(static_cast<unsigned char>(value[i])),
                                           value.size() - i);
            put(cursor, y, value.substr(i, length), color);
            i += length;
            ++cursor;
        }
    }

    void fill(const Rect& rect, const std::string& glyph, int color) {
        for (int row = rect.y; row < rect.y + rect.h; ++row) {
            for (int col = rect.x; col < rect.x + rect.w; ++col) {
                put(col, row, glyph, color);
            }
        }
    }

    std::string toString() const {
        std::string output = "\x1b[H";
        for (int row = 0; row < rows_; ++row) {
            int currentColor = -1;
            for (int col = 0; col < cols_; ++col) {
                const int color = colors_[static_cast<size_t>(row)][static_cast<size_t>(col)];
                if (color != currentColor) {
                    output += colorSequence(color);
                    currentColor = color;
                }
                output += cells_[static_cast<size_t>(row)][static_cast<size_t>(col)];
            }
            output += "\x1b[0m";
            if (row + 1 < rows_) output += "\r\n";
        }
        return output;
    }

private:
    int cols_;
    int rows_;
    std::vector<std::vector<std::string>> cells_;
    std::vector<std::vector<int>> colors_;
};

std::vector<std::string> cardLines(Card card, bool faceDown, bool highlight) {
    std::vector<std::string> lines;
    if (faceDown) {
        lines = {"+-------+", "|       |", "|  ???  |", "|       |",
                 "|       |", "|       |", "+-------+"};
        return lines;
    }
    const std::string rank = rankLabel(card.rank);
    const std::string suit = suitLabel(card.suit);
    const std::string leftPad(6, ' ');
    const std::string rightPad(6, ' ');
    lines = {
        "+-------+",
        "|" + rank + leftPad.substr(0, 7 - rank.size()) + "|",
        "|       |",
        "|   " + suit + "   |",
        "|       |",
        "|" + rightPad.substr(0, 7 - rank.size()) + rank + "|",
        "+-------+"};
    (void)highlight;
    return lines;
}

void drawCard(Canvas& canvas, const Rect& rect, Card card, bool faceDown, bool highlight,
              int frameColor) {
    const std::vector<std::string> lines = cardLines(card, faceDown, highlight);
    for (size_t row = 0; row < lines.size(); ++row) {
        int column = 0;
        size_t i = 0;
        while (i < lines[row].size()) {
            const size_t length =
                std::min(glyphLength(static_cast<unsigned char>(lines[row][i])),
                         lines[row].size() - i);
            const std::string glyph = lines[row].substr(i, length);
            int color = frameColor;
            if (glyph == "+" || glyph == "-" || glyph == "|") {
                color = highlight ? kSelected : kBorder;
            } else if (!faceDown && glyph != " ") {
                color = suitColor(card.suit);
            }
            canvas.put(rect.x + column, rect.y + static_cast<int>(row), glyph, color);
            i += length;
            ++column;
        }
    }
}

void drawButton(Canvas& canvas, const Rect& rect, const std::string& label, bool selected) {
    const std::string inner = " " + label + " ";
    const int width = rect.w;
    const int textX = rect.x + (width - static_cast<int>(inner.size())) / 2;
    canvas.fill(rect, " ", kDefaultColor);
    const std::string border(static_cast<size_t>(width), '-');
    canvas.text(rect.x, rect.y, "+" + border.substr(1, border.size() - 2) + "+",
                selected ? kSelected : kBorder);
    canvas.text(rect.x, rect.y + 1, "|" + std::string(static_cast<size_t>(width - 2), ' ') + "|",
                selected ? kSelected : kBorder);
    canvas.text(textX, rect.y + 1, inner, selected ? kSelected : 7);
}

void centeredText(Canvas& canvas, int row, int cols, const std::string& text, int color) {
    const int x = (cols - static_cast<int>(text.size())) / 2;
    canvas.text(std::max(0, x), row, text, color);
}

}  // namespace

std::string Renderer::renderCard(Card card, bool faceDown) {
    return renderCardAt(card, faceDown, false);
}

std::string Renderer::renderCardAt(Card card, bool faceDown, bool highlight) {
    (void)highlight;
    std::string output;
    const std::vector<std::string> lines = cardLines(card, faceDown, highlight);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) output += "\n";
        output += lines[i];
    }
    return output;
}

std::string Renderer::render(const ViewModel& vm, Size size) {
    hitTargets_.clear();
    Canvas canvas(size.cols, size.rows);

    if (size.cols < 80 || size.rows < 24) {
        centeredText(canvas, size.rows / 2 - 1, size.cols,
                     "Terminal pequeno demais.", 1);
        centeredText(canvas, size.rows / 2 + 1, size.cols,
                     "Aumente para pelo menos 80x24.", 3);
        return canvas.toString();
    }

    const Layout layout = Layout::compute(size, static_cast<int>(vm.actions.size()));

    if (vm.screen == ViewModel::Screen::LanConfig) {
        centeredText(canvas, 3, size.cols, vm.lanTitle, 3);
        for (int i = 0; i < 2; ++i) {
            if (vm.lanFieldLabel[i].empty()) continue;
            const int row = 7 + i * 3;
            canvas.text(20, row, vm.lanFieldLabel[i] + ":", 7);
            const std::string value = vm.lanFieldValue[i];
            const std::string boxed = "[" + value + "]";
            canvas.text(34, row, boxed, vm.lanFieldFocus == i ? 3 : 7);
            if (vm.lanFieldFocus == i) {
                const int cursorX = 35 + static_cast<int>(value.size());
                canvas.text(cursorX, row, "_", vm.lanFieldFocus == i ? 3 : 7);
            }
        }
        if (!vm.lanHint.empty()) {
            centeredText(canvas, 15, size.cols, vm.lanHint, 6);
        }
        if (!vm.message.empty()) {
            centeredText(canvas, 17, size.cols, vm.message, 1);
        }
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::LanWaiting) {
        centeredText(canvas, 4, size.cols, vm.lanTitle, 3);
        centeredText(canvas, 7, size.cols, "Aguardando conexao do convidado...", 7);
        if (!vm.lanAddressText.empty()) {
            centeredText(canvas, 10, size.cols, vm.lanAddressText, 5);
        }
        centeredText(canvas, 12, size.cols, "Informe este IP e a porta no outro computador", 7);
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::LanError) {
        centeredText(canvas, 4, size.cols, "Conexao encerrada", 1);
        centeredText(canvas, 7, size.cols, vm.lanErrorText, 7);
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::Handoff) {
        centeredText(canvas, 5, size.cols, vm.title, 3);
        centeredText(canvas, 8, size.cols, vm.handoffTitle, 5);
        centeredText(canvas, 10, size.cols, vm.handoffText, 7);
        centeredText(canvas, 13, size.cols, "As cartas ficam ocultas ate voce confirmar.", 7);
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::Menu) {
        centeredText(canvas, 3, size.cols, vm.title, 3);
        centeredText(canvas, 5, size.cols, vm.subtitle, 7);
        centeredText(canvas, 8, size.cols, "Baralho de 40 cartas - manilha fixa", 6);
        centeredText(canvas, 9, size.cols, "Partida ate 12 pontos - truco, seis, nove e doze", 6);
        centeredText(canvas, 12, size.cols, "Mouse: clique nas cartas | Teclado: 1 2 3 + Enter", 7);
        centeredText(canvas, 13, size.cols, "ESC cancela a selecao | Q sai do jogo", 7);
        if (vm.mouseWarning) {
            centeredText(canvas, 15, size.cols, "Mouse nao detectado - use o teclado", 3);
        }
        if (vm.message.empty()) {
            centeredText(canvas, 17, size.cols, "Pressione uma opcao para comecar", 7);
        } else {
            centeredText(canvas, 17, size.cols, vm.message, 7);
        }
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::MatchResult) {        centeredText(canvas, 4, size.cols, "FIM DE JOGO", 3);
        centeredText(canvas, 6, size.cols, vm.matchResultText, 7);
        centeredText(canvas, 8, size.cols,
                     "Voce " + std::to_string(vm.humanScore) + " x " +
                         std::to_string(vm.cpuScore) + " Computador",
                     7);
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    if (vm.screen == ViewModel::Screen::Royale) {
        canvas.text(1, 0, "BATTLE ROYALE", 7);
        if (!vm.royaleTitle.empty()) {
            centeredText(canvas, 1, size.cols, vm.royaleTitle, 6);
        }
        if (!vm.stakeText.empty()) {
            centeredText(canvas, 2, size.cols, vm.stakeText, 5);
        }
        const int half = size.cols / 2;
        const int perColumn = (static_cast<int>(vm.royaleRows.size()) + 1) / 2;
        for (size_t i = 0; i < vm.royaleRows.size(); ++i) {
            const RoyaleRow& row = vm.royaleRows[i];
            const int column = static_cast<int>(i) / (perColumn > 0 ? perColumn : 1);
            const int lineInColumn = static_cast<int>(i) % (perColumn > 0 ? perColumn : 1);
            const int y = 3 + lineInColumn;
            if (y >= layout.humanCards[0].y - 1) {
                break;
            }
            const int x = column == 0 ? 2 : half + 1;
            const std::string status =
                row.eliminated ? "eliminado" : (row.inHand ? "em jogo" : "fora");
            std::string line = "J" + std::to_string(row.seat + 1);
            line += row.seat == vm.royaleYou ? "* " : "  ";
            line += std::to_string(row.points) + "pts " + status;
            line += " " + row.card + " vazas " + std::to_string(row.tricks);
            canvas.text(x, y, line, row.seat == vm.royaleYou ? 3 : 7);
        }
        if (!vm.royaleInfo.empty()) {
            canvas.text(2, layout.humanCards[0].y - 2, vm.royaleInfo, 6);
        }
        if (!vm.message.empty()) {
            centeredText(canvas, layout.trickArea.y + layout.trickArea.h - 1, size.cols,
                         vm.message, 3);
        }
        for (int i = 0; i < 3; ++i) {
            const bool played = vm.humanPlayed[static_cast<size_t>(i)];
            const bool selected = vm.selectedCard == i;
            drawCard(canvas, layout.humanCards[i], vm.humanCards[static_cast<size_t>(i)],
                     played, selected, played ? 4 : kBorder);
            hitTargets_.push_back({layout.humanCards[i], i});
        }
        for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
            drawButton(canvas, layout.actions[i], vm.actions[i],
                       vm.selectedAction == static_cast<int>(i));
            hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
        }
        return canvas.toString();
    }

    // Game screen.
    canvas.text(1, 0, vm.humanLabel, 7);
    const int cpuLabelX = size.cols - 2 - static_cast<int>(vm.cpuLabel.size());
    canvas.text(cpuLabelX, 0, vm.cpuLabel, 7);
    canvas.text(2 + static_cast<int>(vm.humanLabel.size()), 0,
                std::to_string(vm.humanScore), 3);
    canvas.text(cpuLabelX - 3, 0, std::to_string(vm.cpuScore), 3);
    if (!vm.stakeText.empty()) {
        centeredText(canvas, 1, size.cols, vm.stakeText, 5);
    }
    if (!vm.turnText.empty()) {
        canvas.text(1, 2, vm.turnText, 6);
    }
    if (!vm.trickText.empty()) {
        canvas.text(1, 3, vm.trickText, 7);
    }

    for (int i = 0; i < 3; ++i) {
        const bool played = vm.cpuPlayed[static_cast<size_t>(i)];
        drawCard(canvas, layout.cpuCards[i], vm.cpuCards[static_cast<size_t>(i)],
                 !vm.revealCpu, false, played ? 4 : kBorder);
        if (vm.revealCpu) {
            hitTargets_.push_back({layout.cpuCards[i], 200 + i});
        }
    }

    {
        const Rect area = layout.trickArea;
        const int chipRow = area.y + std::max(0, area.h / 2 - 1);
        if (vm.cpuTrickCard.has_value()) {
            canvas.text(2, chipRow, vm.cpuTrickLabel + ": " +
                                        rankLabel(vm.cpuTrickCard->rank) +
                                        suitLabel(vm.cpuTrickCard->suit),
                        suitColor(vm.cpuTrickCard->suit));
        }
        if (vm.humanTrickCard.has_value()) {
            const std::string chip = vm.humanTrickLabel + ": " +
                                     rankLabel(vm.humanTrickCard->rank) +
                                     suitLabel(vm.humanTrickCard->suit);
            canvas.text(size.cols - static_cast<int>(chip.size()) - 3, chipRow, chip,
                        suitColor(vm.humanTrickCard->suit));
        }
    }
    if (!vm.message.empty()) {
        centeredText(canvas, layout.trickArea.y + layout.trickArea.h - 1, size.cols,
                     vm.message, 3);
    }

    for (int i = 0; i < 3; ++i) {
        const bool played = vm.humanPlayed[static_cast<size_t>(i)];
        const bool selected = vm.selectedCard == i;
        drawCard(canvas, layout.humanCards[i], vm.humanCards[static_cast<size_t>(i)], played,
                 selected, played ? 4 : kBorder);
        hitTargets_.push_back({layout.humanCards[i], i});
    }

    if (vm.showEleven) {
        centeredText(canvas, layout.trickArea.y + 2, size.cols, vm.elevenText, 5);
    }

    for (size_t i = 0; i < vm.actions.size() && i < 4; ++i) {
        drawButton(canvas, layout.actions[i], vm.actions[i],
                   vm.selectedAction == static_cast<int>(i));
        hitTargets_.push_back({layout.actions[i], 100 + static_cast<int>(i)});
    }

    const std::string status =
        vm.mouseWarning ? "Mouse nao detectado - use o teclado | Q sai"
                        : "Clique na carta ou tecle 1-3 + Enter | ESC cancela | Q sai";
    canvas.text(0, layout.status.y, status, 7);

    return canvas.toString();
}

}  // namespace truco
