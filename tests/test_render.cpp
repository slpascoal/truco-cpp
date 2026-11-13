#include <string>
#include <vector>

#include "test_framework.hpp"
#include "ui/layout.hpp"
#include "ui/render.hpp"
#include "ui/terminal.hpp"

using namespace truco;

namespace {

std::string stripAnsi(const std::string& input) {
    std::string output;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '[') {
            i += 2;
            while (i < input.size() &&
                   !(input[i] >= 'A' && input[i] <= 'Z') &&
                   !(input[i] >= 'a' && input[i] <= 'z')) {
                ++i;
            }
            ++i;
            continue;
        }
        output.push_back(input[i]);
        ++i;
    }
    return output;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
            lines.push_back(current);
            current.clear();
            ++i;
        } else if (text[i] == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(text[i]);
        }
    }
    lines.push_back(current);
    return lines;
}

int displayWidth(const std::string& line) {
    int width = 0;
    for (unsigned char byte : line) {
        if ((byte & 0xC0) != 0x80) {
            ++width;
        }
    }
    return width;
}

ViewModel gameView() {
    ViewModel vm;
    vm.screen = ViewModel::Screen::Game;
    vm.humanCards = {Card{Rank::Three, Suit::Paus}, Card{Rank::Seven, Suit::Copas},
                     Card{Rank::Ace, Suit::Espadas}};
    vm.cpuCards = {Card{Rank::Two, Suit::Ouros}, Card{Rank::Six, Suit::Paus},
                   Card{Rank::Queen, Suit::Copas}};
    vm.stakeText = "MAO VALE 3";
    vm.turnText = "Sua vez";
    vm.actions = {"Jogar carta", "Truco!"};
    return vm;
}

}  // namespace

TEST(render_card_has_nine_columns_and_seven_rows) {
    const std::string card = Renderer::renderCard(Card{Rank::Three, Suit::Copas});
    const std::vector<std::string> lines = splitLines(card);
    CHECK_EQ(lines.size(), size_t{7});
    for (const std::string& line : lines) {
        CHECK_EQ(displayWidth(line), 9);
    }
}

TEST(render_face_down_card_has_same_size) {
    const std::string card = Renderer::renderCard(Card{Rank::Three, Suit::Copas}, true);
    for (const std::string& line : splitLines(card)) {
        CHECK_EQ(displayWidth(line), 9);
    }
}

TEST(render_game_screen_fits_eighty_columns) {
    Terminal terminal;
    Renderer renderer(terminal);
    const std::string frame = stripAnsi(renderer.render(gameView(), Size{80, 24}));
    CHECK(frame.find("MAO VALE 3") != std::string::npos);
    CHECK(frame.find("Sua vez") != std::string::npos);
    const std::vector<std::string> lines = splitLines(frame);
    CHECK_EQ(lines.size(), size_t{24});
    for (const std::string& line : lines) {
        CHECK(displayWidth(line) <= 80);
    }
}

TEST(render_game_screen_fits_wide_terminal) {
    Terminal terminal;
    Renderer renderer(terminal);
    const std::string frame = stripAnsi(renderer.render(gameView(), Size{120, 40}));
    const std::vector<std::string> lines = splitLines(frame);
    CHECK_EQ(lines.size(), size_t{40});
    for (const std::string& line : lines) {
        CHECK(displayWidth(line) <= 120);
    }
}

TEST(render_menu_shows_title) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.actions = {"Jogar", "Sair"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("TRUCO MINEIRO") != std::string::npos);
    CHECK(frame.find("Jogar") != std::string::npos);
}

TEST(render_small_terminal_shows_warning) {
    Terminal terminal;
    Renderer renderer(terminal);
    const std::string frame = stripAnsi(renderer.render(gameView(), Size{40, 10}));
    CHECK(frame.find("pequeno demais") != std::string::npos);
}

TEST(layout_rejects_small_sizes) {
    CHECK(!Layout::compute(Size{79, 24}, 2).fits);
    CHECK(!Layout::compute(Size{80, 23}, 2).fits);
    CHECK(Layout::compute(Size{80, 24}, 2).fits);
    CHECK(Layout::compute(Size{120, 40}, 2).fits);
}

TEST(layout_hit_targets_match_cards) {
    Terminal terminal;
    Renderer renderer(terminal);
    renderer.render(gameView(), Size{80, 24});
    const auto& targets = renderer.hitTargets();
    CHECK(targets.size() >= 5);
    int cardTargets = 0;
    int actionTargets = 0;
    for (const HitTarget& target : targets) {
        if (target.actionId >= 0 && target.actionId < 3) {
            ++cardTargets;
            const Rect& rect = target.rect;
            CHECK(rect.contains(rect.x, rect.y));
            CHECK(rect.contains(rect.x + rect.w - 1, rect.y + rect.h - 1));
            CHECK(!rect.contains(rect.x + rect.w, rect.y));
            CHECK(!rect.contains(rect.x, rect.y + rect.h));
        }
        if (target.actionId >= 100) {
            ++actionTargets;
        }
    }
    CHECK_EQ(cardTargets, 3);
    CHECK_EQ(actionTargets, 2);
}

TEST(layout_card_rects_do_not_overlap) {
    const Layout layout = Layout::compute(Size{80, 24}, 2);
    for (int i = 0; i < 3; ++i) {
        CHECK(!layout.humanCards[i].empty());
        for (int j = i + 1; j < 3; ++j) {
            const Rect& a = layout.humanCards[i];
            const Rect& b = layout.humanCards[j];
            const bool overlap = a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
                                 b.y < a.y + a.h;
            CHECK(!overlap);
        }
    }
}

TEST(render_match_result_screen) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::MatchResult;
    vm.matchResultText = "Voce venceu a partida!";
    vm.humanScore = 12;
    vm.cpuScore = 9;
    vm.actions = {"Jogar novamente", "Sair"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("FIM DE JOGO") != std::string::npos);
    CHECK(frame.find("Voce venceu a partida!") != std::string::npos);
    CHECK(renderer.hitTargets().size() >= 2);
}

TEST(render_selected_card_and_revealed_cpu) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm = gameView();
    vm.selectedCard = 1;
    vm.revealCpu = true;
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    const std::vector<std::string> lines = splitLines(frame);
    for (const std::string& line : lines) {
        CHECK(displayWidth(line) <= 80);
    }
    CHECK(frame.find("VIRA") == std::string::npos);
}

TEST(render_shows_trick_count_without_vira_or_manilha) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm = gameView();
    vm.trickText = "Vazas: voce 1 x 0 CPU";
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("Vazas: voce 1 x 0 CPU") != std::string::npos);
    CHECK(frame.find("VIRA") == std::string::npos);
    CHECK(frame.find("MANILHA") == std::string::npos);
}

TEST(render_handoff_screen_hides_cards) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::Handoff;
    vm.handoffTitle = "Passe o controle para Jogador 2";
    vm.handoffText = "Passe o controle para Jogador 2";
    vm.actions = {"Continuar"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("Passe o controle para Jogador 2") != std::string::npos);
    CHECK(frame.find("Continuar") != std::string::npos);
    CHECK(frame.find("VIRA") == std::string::npos);
    CHECK(frame.find("+-------+") == std::string::npos);
    for (const std::string& line : splitLines(frame)) {
        CHECK(displayWidth(line) <= 80);
    }
}

TEST(render_lan_config_screen) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::LanConfig;
    vm.lanTitle = "Conectar em partida";
    vm.lanFieldLabel[0] = "IP do host";
    vm.lanFieldValue[0] = "192.168.0.10";
    vm.lanFieldLabel[1] = "Porta";
    vm.lanFieldValue[1] = "4000";
    vm.lanFieldFocus = 0;
    vm.lanHint = "Tab alterna campo";
    vm.actions = {"Conectar", "Voltar"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("Conectar em partida") != std::string::npos);
    CHECK(frame.find("192.168.0.10") != std::string::npos);
    CHECK(frame.find("4000") != std::string::npos);
    CHECK(frame.find("Tab alterna campo") != std::string::npos);
    CHECK(frame.find("+-------+") == std::string::npos);
    CHECK_EQ(renderer.hitTargets().size(), size_t{2});
}

TEST(render_lan_waiting_screen) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::LanWaiting;
    vm.lanTitle = "Hospedando partida";
    vm.lanAddressText = "IP: 192.168.0.10  Porta: 4000";
    vm.actions = {"Cancelar"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("Hospedando partida") != std::string::npos);
    CHECK(frame.find("192.168.0.10") != std::string::npos);
    CHECK(frame.find("4000") != std::string::npos);
    CHECK(frame.find("Aguardando conexao") != std::string::npos);
}

TEST(render_lan_error_screen) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::LanError;
    vm.lanErrorText = "Conexao perdida com o adversario";
    vm.actions = {"Voltar ao menu"};
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("Conexao encerrada") != std::string::npos);
    CHECK(frame.find("Conexao perdida com o adversario") != std::string::npos);
    CHECK_EQ(renderer.hitTargets().size(), size_t{1});
}

TEST(render_royale_lists_thirteen_players_without_overflow) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::Royale;
    vm.royaleYou = 0;
    vm.royaleTitle = "Jogadores vivos: 13/13";
    vm.royaleInfo = "Vazas jogadas: 1";
    vm.humanCards = {Card{Rank::Three, Suit::Paus}, Card{Rank::Seven, Suit::Copas},
                     Card{Rank::Ace, Suit::Espadas}};
    vm.actions = {"Jogar", "Truco!"};
    for (int i = 0; i < 13; ++i) {
        RoyaleRow row;
        row.seat = i;
        row.points = 12 - (i % 3);
        row.eliminated = i == 12;
        row.inHand = i != 12;
        row.tricks = i % 2;
        row.card = i == 0 ? "7O" : "-";
        vm.royaleRows.push_back(row);
    }
    const std::string frame = stripAnsi(renderer.render(vm, Size{80, 24}));
    CHECK(frame.find("BATTLE ROYALE") != std::string::npos);
    CHECK(frame.find("J13") != std::string::npos);
    CHECK(frame.find("Jogadores vivos: 13/13") != std::string::npos);
    const std::vector<std::string> lines = splitLines(frame);
    CHECK_EQ(lines.size(), size_t{24});
    for (const std::string& line : lines) {
        CHECK(displayWidth(line) <= 80);
    }
}

TEST(render_royale_fits_wide_terminal) {
    Terminal terminal;
    Renderer renderer(terminal);
    ViewModel vm;
    vm.screen = ViewModel::Screen::Royale;
    vm.humanCards = {Card{Rank::Two, Suit::Ouros}, Card{Rank::Six, Suit::Paus},
                     Card{Rank::Queen, Suit::Copas}};
    for (int i = 0; i < 13; ++i) {
        RoyaleRow row;
        row.seat = i;
        row.points = 12;
        row.inHand = true;
        vm.royaleRows.push_back(row);
    }
    const std::string frame = stripAnsi(renderer.render(vm, Size{120, 40}));
    for (const std::string& line : splitLines(frame)) {
        CHECK(displayWidth(line) <= 120);
    }
}
