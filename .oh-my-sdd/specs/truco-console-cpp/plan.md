# Plano Técnico: Jogo de Truco Mineiro em console (jogador vs CPU)

Slug: `truco-console-cpp`
Base: `.oh-my-sdd/specs/truco-console-cpp/spec.md` (validado)
Restrição vinculante: `.oh-my-sdd/constitution.md`

## 1. Decisões Arquiteturais

Aplicação single-thread, loop de eventos bloqueante em `app/`, com quatro camadas de dependência estritamente unidirecional:

```
main.cpp ──> app/ ──> ui/ ──> core/ (somente tipos)
                 └──> ai/ ──> core/ (somente tipos)
```

- `core/`: regras puras, nenhum include de terminal, nenhum I/O, determinístico por seed. Testável sem TTY.
- `ai/`: recebe apenas `PublicView` (visão pública). Impossibilidade estrutural de ver a mão do humano: a função de decisão não recebe as cartas humanas.
- `ui/`: terminal, entrada, desenho. Não contém regra de jogo; consome um `ViewModel` já resolvido pelo `app/`.
- `app/`: máquina de estados explícita, orquestra `core` + `ai` + `ui`. Único lugar que conhece todos.
- `main.cpp`: composição, tratamento de exceções, restauração final do terminal.

**Máquina de estados** (`app::SessionState`): `Menu → Dealing → PlayerTurn → CpuTurn → TrucoDialog → HandOfElevenChoice → TrickReveal → HandResult → MatchResult`.

Toda transição é função explícita; nenhuma mutação de estado fora do `app/`. `core::Game` é um objeto de estado serializável e sem callbacks.

**Renderização:** full redraw por quadro em um buffer de células próprio, emitido de uma vez (`\x1b[H` + linhas). Sem diff de buffer: o quadro cabe em ~2.5 KB e o orçamento é <16 ms; diff adicionaria complexidade sem consumidor real. Todo desenho parte de coordenadas de `Layout`, recalculado a cada resize.

**Entrada:** modo raw via `termios`, mouse tracking SGR 1006 (`\x1b[?1000h` + `\x1b[?1006h`), leitura com `poll()` e timeout curto (16–50 ms) para permitir animação e `SIGWINCH`. Parser de entrada isolado de `read()` para ser testável por string.

**Restauração:** `ui::Terminal` é RAII e restaura tudo no destrutor; `main` instala handlers para `SIGINT`/`SIGTERM` que apenas marcam flag atômica, e o loop encerra de forma limpa. `std::set_terminate`/`std::atexit` não substituem o RAII.

**Animação:** distribuição, revelação de carta e jogada da CPU são sequências de quadros com `sleep` curto no `app/`, sem threads.

## 2. Esquema de Dados e Contratos (interfaces)

### `core/card.hpp`

```cpp
enum class Suit { Ouros, Espadas, Copas, Paus };
enum class Rank { Four, Five, Six, Seven, Queen, Jack, King, Ace, Two, Three };
struct Card { Rank rank; Suit suit; };
bool operator==(Card a, Card b);
int  commonStrength(Rank r);        // 0..9  (4..3)
int  suitOrder(Suit s);             // 0..3  (Ouros..Paus)
Rank manilhaRank(Rank vira);        // próxima na sequência circular
bool isManilha(Card c, Rank vira);
int  cardStrength(Card c, Rank vira);// 0..29; manilha sempre > comum
std::string rankLabel(Rank r);      // "4".."3", "Q","J","K","A"
std::string suitLabel(Suit s);      // "♦","♠","♥","♣"
std::string suitName(Suit s);       // "ouros","espadas","copas","paus"
```

### `core/deck.hpp`

```cpp
std::array<Card, 40> buildDeck();
void shuffleDeck(std::array<Card, 40>& deck, std::mt19937& rng);
struct Deal { std::array<Card, 3> human; std::array<Card, 3> cpu; Card vira; };
Deal deal(std::mt19937& rng);       // vira = baralho[0]; human = [1..3]; cpu = [4..6]
```

### `core/hand.hpp`

```cpp
enum class Side { Human, Cpu };
enum class TrickOutcome { Human, Cpu, Tie };
TrickOutcome trickWinner(Card a, Card b, Rank vira);   // a = first player's card

class HandProgress {
public:
    void playTrick(TrickOutcome o);
    int  tricksWon(Side s) const;
    bool finished() const;
    Side winner() const;            // válido quando finished()
    int  trickIndex() const;        // 0,1,2
    TrickOutcome outcomeAt(int i) const;
private:
    std::vector<TrickOutcome> outcomes_;
};
Side resolveHandWinner(const std::vector<TrickOutcome>& outcomes, Side dealer);
```

Regra de empate implementada em `resolveHandWinner` exatamente como no spec (2.º vaza decide; 2.º empate devolve ao vencedor da 1.ª; três empates → mão/dealer).

### `core/hand_value.hpp`

```cpp
enum class Stake { One = 1, Three = 3, Six = 6, Nine = 9, Twelve = 12 };
Stake   nextStake(Stake current);
bool    canRaise(Stake current);             // current != Twelve
Stake   valueAfterAccept(Stake current);
const char* callLabel(Stake current);        // "Truco" | "Seis" | "Nove" | "Doze"
int     valuePoints(Stake s);
```

### `core/game.hpp`

```cpp
enum class HandKind { Normal, HandOfEleven, IronHand };

struct GameScore { int human = 0; int cpu = 0; };

class Game {
public:
    explicit Game(std::mt19937& rng);
    void startMatch();
    void startHand();                         // decide HandKind pelo placar exato em 11
    const Deal& deal() const;
    HandKind handKind() const;
    Stake stake() const;
    Side dealer() const;                      // alterna a cada mão
    GameScore score() const;
    HandProgress& progress();
    // ações
    void humanPlaysCard(int index);           // valida índice e turno
    void cpuPlaysCard(int index);
    void callTruco(Side by);                  // valida canRaise e turno
    void acceptTruco(Side by);
    void runFromTruco(Side by);               // credita valor anterior ao que pediu
    void chooseHandOfEleven(Side s, bool play);// vale 3 se play, 1 se hide
    void finishHand(Side winner);             // credita stake, alterna dealer
    bool matchOver() const;
    Side matchWinner() const;
    Side currentTurn() const;
    void revealAll();                         // para correr/exibição
private:
    std::mt19937& rng_;
    GameScore score_;
    Side dealer_ = Side::Human;
    ...
};
```

### `ai/cpu_player.hpp`

```cpp
struct PublicView {
    std::array<Card, 3> hand;
    std::array<bool, 3> played;      // cartas já jogadas da própria mão
    Card vira;
    Stake stake;
    HandProgress progress;
    GameScore score;
    Side turn;
    bool trucoPending;
};

int  chooseCardIndex(const PublicView& view, std::mt19937& rng);
bool shouldCallTruco(const PublicView& view, std::mt19937& rng);
bool shouldAcceptTruco(const PublicView& view, std::mt19937& rng);
bool shouldPlayHandOfEleven(Card hidden, std::mt19937& rng);
```

`PublicView` nunca contém cartas do humano — garantia estrutural do Req-36.

### `ui/terminal.hpp`, `ui/input.hpp`

```cpp
class Terminal {                       // RAII
public:
    Terminal();                        // raw, alt screen, cursor off, mouse on
    ~Terminal();                       // restaura tudo
    bool mouseSupported() const;       // false se usuário/terminal recusar
    Size size() const;                 // ioctl TIOCGWINSZ
    void write(const std::string&);
};

struct InputEvent {
    enum class Kind { None, Key, MouseClick, MouseMove, Resize, Quit };
    Kind kind = Kind::None;
    char key = 0;                      // '1'..'3', '\r', ' ', 27 (Esc), 'q'
    int x = 0, y = 0;                  // 0-based
};
std::vector<InputEvent> parseInput(const std::string& buffer); // testável
InputEvent pollEvent(int timeoutMs);  // usa parseInput
```

Parser reconhece: SGR `<ESC>[<b;x;yM` e `m`, X10 `<ESC>[M` + 3 bytes, teclas de controle e UTF-8 ignorado com segurança (consumo de bytes de continuação).

### `ui/layout.hpp`, `ui/render.hpp`

```cpp
struct Rect { int x, y, w, h; bool contains(int px, int py) const; };
struct Layout {
    Rect humanCards[3], cpuCards[3], vira, scoreHuman, scoreCpu, stake,
         trickArea, actions[4], status;
    static Layout compute(Size size, int nActions);
};

struct ViewModel {
    // tudo já resolvido pelo app: texto de placar, cartas visíveis, diálogo, etc.
};

struct HitTarget { Rect rect; int actionId; };
class Renderer {
public:
    void draw(const ViewModel& vm, Size size);            // emite quadro completo
    std::vector<HitTarget> hitTargets() const;            // p/ clique
    static std::string renderCard(const Card& c);         // usado em testes
};
```

Cartas são desenhadas em 9×7 células, com moldura Unicode, valor no canto e naipe no centro; cores: ♦ amarelo, ♥ vermelho, ♠ ciano, ♣ verde; selecionada com moldura inversa.

### `app/session.hpp`

```cpp
class Session {
public:
    int run();                         // loop principal; retorna exit code
private:
    void handleEvent(const InputEvent&);
    void updateCpu();                  // avança decisões da CPU com pacing
    void render();
    SessionState state_ = SessionState::Menu;
};
```

## 3. Bibliotecas e Dependências

Nenhuma dependência nova. Apenas biblioteca padrão e POSIX (`termios.h`, `unistd.h`, `poll.h`, `sys/ioctl.h`, `signal.h`). `ncurses` (headers ausentes no ambiente) e `cmake` permanecem proibidos pela constitution. `Makefile` manual com detecção implícita de headers por dependências `-MMD -MP`.

## 4. Global Constraints (herdadas literalmente do spec)

- C++17, `g++`, `Makefile` com alvos `all`, `run`, `test`, `clean`.
- 40 cartas: `4,5,6,7,Q,J,K,A,2,3` × `ouros,espadas,copas,paus`.
- Manilha = valor seguinte circular à vira: `4→5→6→7→Q→J→K→A→2→3→4`.
- Manilhas: `paus > copas > espadas > ouros`; sempre acima das comuns.
- Comuns: `4<5<6<7<Q<J<K<A<2<3`, naipe não desempata carta comum.
- Escala: 1 → 3 → 6 → 9 → 12; sem aumento em 12. Partida até 12, placar exibido travado em 12.
- Mão de 11: só com placar exatamente 11; Jogar=3, Esconder=1; ambos em 11 = mão de ferro (1 carta oculta cada, vale 3, sem truco).
- Empates: 1.ª empata → 2.ª decide; 2.ª empata → vence quem ganhou a 1.ª; três empates → vence quem é mão (dealer).
- Mouse SGR 1006 ligado e desligado simetricamente; terminal restaurado em qualquer saída.
- Tamanho mínimo 80×24; abaixo disso, aviso.
- UI em português do Brasil; identificadores e commits em inglês.
- RNG `std::mt19937` com seed explícita; seed fixável por env `TRUCO_SEED`.
- Testes sem framework em `tests/`; exit code ≠ 0 em falha.
- Proibido: `cmake`, `ncurses`, `rand()`, I/O em `core/`, acesso da IA à mão humana.

## 5. Review Focus (riscos que teste precisa exercitar)

| Risco | Onde o teste cobre |
|---|---|
| Restauração do terminal em `Ctrl+C`/exceção | teste de RAII com PTY simulado (fork + verify termios) |
| Parser de mouse erra coordenadas 1-based do SGR | `parseInput` com strings reais SGR/X10 |
| Clique cai na carta errada por off-by-one | `Layout` + `Rect::contains` com bordas |
| Empate de vazas resolvido errado | matriz completa de 3 vazas em `resolveHandWinner` |
| Dealer alternando errado após correr | teste de sequência de mãos |
| Correr credita valor errado | teste de truco → corre → placar |
| Mão de 11 disparando com 12+ pontos | teste nos limites 10/11/12 |
| Mão de ferro sem truco | teste que `callTruco` falha em `IronHand` |
| CPU vendo carta do humano | `PublicView` sem campo; teste que decisão só depende dele |
| Resize durante diálogo | teste de `Layout::compute` em 80×24, 120×40, 40×10 |
| UTF-8 dos naipes quebrando largura | teste de comprimento de linha renderizada |
| Loop a 100% de CPU parado | `poll` com timeout; verificação manual + revisão |
| Input desconhecido travando o jogo | `parseInput` com bytes lixo e sequências truncadas |

## 6. Interfaces por Tarefa

1. **Build skeleton** — produz: `Makefile` (`all/run/test/clean`, flags `-std=c++17 -Wall -Wextra -Wpedantic -O2 -MMD -MP`), `src/main.cpp` mínimo, `tests/test_main.cpp` com harness `CHECK`/`CHECK_EQ` e exit code. Consome: nada.
2. **Cards** — produz `core/card.{hpp,cpp}` conforme §2. Consome: nada. Verifica: `make test` com testes de ordem, manilha e naipe.
3. **Deck** — produz `core/deck.{hpp,cpp}`. Consome `Card`, `Rank`, `Suit`. Verifica: 40 cartas únicas, vira fora das mãos, distribuição com seed fixa reproduzível.
4. **Trick and hand resolution** — produz `core/hand.{hpp,cpp}`. Consome `Card`/`cardStrength`. Verifica: matriz de vazas, empates e `resolveHandWinner`.
5. **Stake ladder** — produz `core/hand_value.{hpp,cpp}`. Consome nada. Verifica: escala, nomes, limite em 12.
6. **Game engine** — produz `core/game.{hpp,cpp}`. Consome tarefas 2–5. Verifica: fluxo de mão, corre, aceite, mão de 11/ferro, placar, alternância de dealer, fim de partida.
7. **CPU AI** — produz `ai/cpu_player.{hpp,cpp}`. Consome `core` + `PublicView`. Verifica: escolha legal, pedido de truco correlacionado com força, aceite/corre, mão de 11, ausência de cartas humanas no `PublicView`.
8. **Terminal and input** — produz `ui/terminal.{hpp,cpp}`, `ui/input.{hpp,cpp}`. Consome nada. Verifica: `parseInput` com SGR/X10/teclas/lixo/truncado; RAII restaura termios sob exceção.
9. **Layout and renderer** — produz `ui/layout.{hpp,cpp}`, `ui/render.{hpp,cpp}`. Consome `core` types. Verifica: `renderCard` estável, largura 80 sem overflow, hit-testing correto em 3 tamanhos.
10. **App session** — produz `app/session.{hpp,cpp}` e integra `main.cpp`. Consome tudo. Verifica: script de input simulado (sequência de eventos) levando de Menu a MatchResult; confirmação de clique; pacing da CPU não bloqueante.
11. **UX polish** — animações de distribuição/revelação, telas de menu/resultado, help, min-size warning, resize. Verifica: renderização nos tamanhos-alvo e partida manual completa.
12. **README** — instruções de build, execução, regras implementadas e controles. Verifica: comandos do README executados literalmente a partir de clone limpo.

## 7. Alternativas Consideradas

- **ncurses/FTXUI:** descartado por constitution (headers ausentes, dependência externa proibida). Custo: implementar mouse e resize manualmente — pago uma vez em `ui/`.
- **Buffer com diff de células:** descartado. Full redraw cabe em ~2.5 KB/quadro e <1 ms; diff adiciona estado e bugs de cintilação sem consumidor.
- **Threads para CPU "pensar":** descartado. Pacing com `poll`/`sleep` no loop evita concorrência e mantém `core`/`ai` puros.
- **Biblioteca de teste (Catch2/GoogleTest):** descartada. Harness de asserts próprio atende e evita dependência.

## 8. Justificativa de Complexidade

- `core/` separado: consumido por `app/`, `ai/` e pela suíte de testes — 3 consumidores.
- `ai/` separado: único consumidor é `app/`, mas isola a fronteira de informação (Req-36) e permite testes diretos. Justificado pela regra de segurança do spec.
- `ui/layout` separado de `ui/render`: `layout` é a única fonte de hit-testing e é testável sem TTY; 2 consumidores (`render` e `session`).
- `app/session` separado de `main.cpp`: isola máquina de estados do bootstrap de processo; 1 consumidor, justificado por testabilidade do fluxo de input.
- Buffer de células em `render`: único consumidor, mas necessário para largura UTF-8 e moldura — substitui manipulação ad-hoc de strings.
