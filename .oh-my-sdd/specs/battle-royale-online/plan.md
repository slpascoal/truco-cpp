# Plan — Modo online Battle Royale de truco mineiro

## 1. Decisões arquiteturais

- **Motor próprio em `src/core/royale.hpp/cpp`.** O `Game` atual é 2 jogadores com `Side`; forçar
  N jogadores nele quebraria contratos existentes. `RoyaleMatch` é puro (sem I/O), indexado por
  assento `int` 0..N-1, com assentos mortos representados por usuários eliminados.
- **Host autoritativo.** O host roda a única instância de `RoyaleMatch`; clientes enviam intenções
  (`PLAY`, `TRUCO`, `ACCEPT`, `RUN`) e recebem estado específico para o próprio assento. Nenhum
  cliente recebe a mão alheia.
- **Servidor multi-cliente novo.** `RoomServer` aceita N conexões e multiplexa leitura/escrita por
  assento. `HostServer` continua intocado para o modo LAN de 2 jogadores (contrato single-client).
- **Protocolo separado por modo.** Handshake `HELLO ROYALE 1`; mensagens novas `JOIN`, `LOBBY`,
  `START`, `RSTATE`. `SYNC` e o modo 2 jogadores permanecem como estão.
- **Estados Royale dentro de `Session`.** `Session` já é dona do loop de terminal, entrada e
  renderização; um `RoyaleSession` separado duplicaria esse loop. Novos estados:
  `RoyaleConfig`, `RoyaleLobby`, `RoyaleWaiting`, `RoyalePlay`, `RoyaleResult`.
- **Menu com subpágina LAN.** A UI de menu desenha no máximo 4 botões. O menu principal passa a
  `{CPU, 2 jogadores, LAN, Sair}` e a subpágina LAN expõe
  `{Hospedar 2p, Conectar 2p, Hospedar Battle Royale, Conectar Battle Royale}` com `ESC` voltando.
- **Renderização compacta.** Até 13 jogadores não cabem como cartas na mesa; o render Royale usa
  uma linha por assento (`J4  9pts  em jogo  [7♦]`) e mantém as 3 cartas do jogador local embaixo.

## 2. Esquema de dados e contratos

### Core — `src/core/royale.hpp`

```cpp
constexpr int kRoyaleMaxPlayers = 13;
constexpr int kRoyaleStartPoints = 12;

enum class RoyalePhase { Playing, TrucoAnswer, HandEnd, MatchEnd };

struct RoyaleSeat {
    int points = kRoyaleStartPoints;
    bool eliminated = false;
    bool inHand = false;
    std::array<Card, 3> hand{};
    std::array<bool, 3> played{};
    std::optional<Card> tableCard;  // carta da vaza atual
    int tricksWon = 0;
    int bestCard = -1;              // maior força jogada nesta mão
};

class RoyaleMatch {
public:
    RoyaleMatch(int playerCount, std::mt19937& rng);

    int playerCount() const;                 // 3..13
    RoyalePhase phase() const;
    bool matchOver() const;
    int matchWinner() const;                 // -1 enquanto em andamento
    int handWinner() const;                  // -1 fora de HandEnd/MatchEnd
    const RoyaleSeat& seat(int index) const;
    int aliveCount() const;
    int inHandCount() const;

    int handValue() const;                   // 1/3/6/9/12
    int pendingValue() const;
    bool trucoPending() const;
    int trucoCaller() const;                 // -1 se não pendente
    int trucoResponder() const;              // -1 se não pendente
    bool canCallTruco(int seat) const;
    void callTruco(int seat);
    void respondTruco(bool accept);          // responde pelo trucoResponder()
    bool raiseAllowed(int seat) const;

    int currentTurn() const;                 // -1 fora de Playing
    bool canPlay(int seat) const;
    void playCard(int seat, int index);
    int tricksPlayed() const;
    int mao() const;

    void startHand();
    void removePlayer(int seat);             // desconexão: elimina sem custo
};
```

### Rede — `src/net/room_server.hpp`

```cpp
class RoomServer {
public:
    bool listen(uint16_t port, std::string& error);
    int acceptPending();                       // retorna quantos entraram
    int clientCount() const;                   // assentos remotos (0..N-2)
    bool connected(int client) const;
    uint16_t port() const;
    std::vector<std::string> readLines(int client);
    void sendLine(int client, const std::string& line);
    void broadcast(const std::string& line);
    std::vector<int> connectedClients() const;
    void dropClient(int client);
    void shutdown();
    std::vector<std::string> localAddresses() const;
};
```

### Protocolo Royale — `src/net/protocol.hpp`

Novos `MessageType`: `Join`, `Lobby`, `Start`, `RState`. Novo contrato:

```cpp
struct RoyaleSeatView {
    int points = kRoyaleStartPoints;
    bool eliminated = false;
    bool inHand = false;
    std::optional<Card> tableCard;
    std::optional<std::array<Card, 3>> revealed;  // só correu/eliminado/fim de mão
};

struct RoyaleView {
    uint32_t seq = 0;
    int you = 0;
    RoyalePhase phase = RoyalePhase::Playing;
    int turn = -1;
    int stake = 1;
    int caller = -1;
    int responder = -1;
    int handWinner = -1;
    int matchWinner = -1;
    int tricksPlayed = 0;
    int playerCount = 0;
    std::array<RoyaleSeatView, kRoyaleMaxPlayers> seats{};
    std::array<Card, 3> own{};
    std::array<bool, 3> playedOwn{};
};

std::string encodeRoyaleState(const RoyaleView& view);
bool decodeRoyaleState(const std::string& line, RoyaleView& out);
std::string encodeLobby(const std::vector<std::string>& names);
bool decodeLobby(const std::string& line, std::vector<std::string>& names);
```

- `RSTATE` inclui apenas `playerCount` assentos, máscara `1/0` para eliminado/na mão, token de
  carta da vaza ou `-`, e 3 cartas reveladas ou `---` quando ocultas.
- Intenções reutilizam `PLAY <1..3>`, `TRUCO`, `ACCEPT`, `RUN`.

### Sessão — `src/app/session.hpp`

```cpp
enum class GameMode { Cpu, Hotseat, LanHost, LanGuest, RoyaleHost, RoyaleGuest };
enum class SessionState {
    // atuais...
    RoyaleConfig, RoyaleLobby, RoyaleWaiting, RoyalePlay, RoyaleResult
};

void setRoyalePlayersForTesting(int players);
bool openRoyaleHostForTesting(uint16_t port, int maxPlayers);
bool openRoyaleJoinForTesting(const std::string& ip, uint16_t port);
const RoyaleMatch* royaleMatch() const;     // host
const RoyaleView& royaleView() const;       // guest
```

- Host: assento 0 = host local; assentos 1..N-1 = clientes do `RoomServer` (ordem de conexão).
- Guest: assento recebido no `WELCOME`.
- `Session::activateAction` roteia por estado; `refreshView` monta `ViewModel` Royale.

### UI — `src/ui/render.hpp`

```cpp
struct RoyaleRow {
    int seat = 0;
    int points = 0;
    bool eliminated = false;
    bool inHand = false;
    std::string card;      // ex.: "7♦" ou "-"
};

struct ViewModel {
    // campos atuais...
    std::vector<RoyaleRow> royaleRows;
    std::string royaleTitle;
    std::string royaleInfo;
    int royaleYou = 0;
};
```

## 3. Global Constraints

- C++17, `g++`, `Makefile`; **nenhuma** dependência externa (constitution §2).
- `core/` sem I/O; UI sem regra; rede não decide regra (constitution §3).
- Manilhas fixas `4♣ > 7♥ > A♠ > 7♦`; comuns `4<5<6<7<Q<J<K<A<2<3`; naipe não desempata comum.
- Jogadores 3–13; 12 pontos iniciais; valores 1/3/6/9/12; host inicia com ≥3 conectados.
- Corrida paga o pedido; perdedor da mão paga o valor final; 0 pontos elimina.
- Vaza empatada não tem vencedor; mão desempata por maior carta e depois pela mão.
- Textos de UI em pt-BR; identificadores e commits em inglês.
- Gate: `make` e `make test` frescos.

## 4. Interfaces por tarefa

| Tarefa | Consome | Produz |
|---|---|---|
| T1 — `RoyaleMatch` base: assentos, deal, turno, vazas | `cardStrength` | `RoyaleMatch`, `RoyaleSeat`, `playCard`, `currentTurn`, `tricksPlayed` |
| T2 — fim de mão, pontos, eliminação, fim de partida | T1 | `handWinner`, `matchWinner`, `startHand`, `removePlayer` |
| T3 — truco multi-resposta e direitos | T1, T2 | `callTruco`, `respondTruco`, `trucoResponder`, `raiseAllowed`, `pendingValue` |
| T4 — `RoomServer` multi-cliente | `net/socket` | `RoomServer` completo |
| T5 — protocolo Royale | T1–T3 | `RoyaleView`, `encodeRoyaleState`, `decodeRoyaleState`, `encodeLobby`, `decodeLobby`, novos `MessageType` |
| T6 — sessão host: config, lobby, start, intenções | T3, T4, T5 | `GameMode::RoyaleHost`, `SessionState::Royale*`, `openRoyaleHostForTesting` |
| T7 — sessão guest: entrada, estado, jogadas | T5, T6 | `GameMode::RoyaleGuest`, `royaleView`, `openRoyaleJoinForTesting` |
| T8 — render/menu Royale | T5–T7 | `RoyaleRow`, `ViewModel::royaleRows`, submenu LAN |
| T9 — testes de integração e README | todas | suíte e documentação |

## 5. Review Focus (riscos e cobertura exigida)

- Ordem de assentos com jogadores eliminados/fora da mão: pular corretamente (T1/T2).
- Vaza com força máxima repetida: nenhum vencedor; próxima vaza abre pelo assento seguinte ao líder
  anterior (T1).
- Desempate de mão: vazas → maior carta → mão, nesta ordem (T2).
- Corrida que zera pontos elimina e remove da mão no meio da rodada de respostas (T3).
- Pedido com todos correndo encerra a mão sem cobrança extra do pedinte (T3).
- Direito de aumento: pedinte nunca; aceitantes do último pedido sim (T3).
- Fim antecipado com um único jogador na mão (T2/T3).
- Desconexão como respondente/caller/jogador comum e durante a vaza (T2/T6).
- Serialização: 3..13 assentos, cartas ocultas como `-`, linha ≤ `kMaxLine`, decode rejeita lixo (T5).
- Host recusa entrada após início e acima do teto (T6).
- Integração com 3 clientes reais em loopback e uma partida curta até vitória (T9).
- Regressão dos modos CPU/hotseat/LAN 2p (T9).

## 6. Alternativas consideradas

- **Generalizar `Game` para N jogadores** — descartada: `Side` binário permeia sessão, IA e rede 2p;
  risco alto de regressão sem ganho.
- **Reutilizar `HostServer` com vetor de clientes** — descartada: `waiting()/connected()` são
  single-client e usados pelo modo LAN 2p; manter compatibilidade é mais seguro.
- **Classe `RoyaleSession` separada** — descartada: duplicaria loop de terminal, polling e render;
  `Session` já centraliza isso.
- **Sempre serializar 13 assentos fixos** — descartada: desperdício e testes frágeis; usar
  `playerCount` variável.
- **Revelar todas as mãos no fim da mão** — adotada apenas onde o spec permite: corrida/eliminação
  e fim de mão.

## 7. Justificativa de complexidade

- `RoyaleMatch` separado: regra N-jogadores não cabe no `Game` binário; um único consumidor agora,
  mas é a regra central do modo — abstração exigida pelo domínio.
- `RoomServer`: terceiro consumidor de `socket` e único multi-cliente; requer estado por conexão.
- Campos `RoyaleRow` no `ViewModel`: permitem render por linha sem multiplicar cartas na tela.
- Nenhuma dependência nova; nenhum serviço externo.
