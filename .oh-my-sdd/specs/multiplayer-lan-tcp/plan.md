# Plano Técnico: Multiplayer LAN via TCP

Slug: `multiplayer-lan-tcp`
Base: `.oh-my-sdd/specs/multiplayer-lan-tcp/spec.md` (validado)
Restrição vinculante: `.oh-my-sdd/constitution.md`

## 1. Decisões Arquiteturais

Uma nova camada `src/net/` concentra transporte e protocolo; o jogo continua sendo o `core::Game` já testado. O host é a única autoridade e serializa um **snapshot por lado** (`SYNC`), já filtrado para nunca incluir a mão alheia antes de `revealed`.

```
app/Session ──> net/HostServer ──┐
     │            net/GuestClient ├─> net/protocol (texto, linha)
     │            net/socket       ┘
     └──> core/Game (host autoritativo, local J1)
```

- **Host (LanHost):** `Session` roda o `Game` normalmente como Jogador 1. Ao aceitar a conexão e validar o handshake, inicia a partida. Cada transição gera um `SYNC` para o convidado (e mantém a tela local). Intenções recebidas são validadas com as mesmas guardas usadas localmente (`canCallTruco`, `cardPlayed`, `currentTurn`).
- **Convidado (LanGuest):** não roda regras. Mantém um `SyncState` espelhado, renderiza a partir dele e envia apenas intenções (`PLAY`, `TRUCO`, `ACCEPT`, `RUN`, `ELEVEN`, `REMATCH`).
- **Protocolo:** texto ASCII, uma mensagem por linha, campos separados por espaço, limite rígido de 512 bytes. Handshake `HELLO TRUCO 1` → `WELCOME 1`; versão diferente recebe `ERROR VERSION` e a conexão é fechada. Mensagens desconhecidas são descartadas.
- **Privacidade estrutural:** `SyncState` possui só a mão do ponto de vista de quem recebe; o campo da mão adversária é preenchido com `-` enquanto `revealed == 0`. O encoder não tem caminho para serializar a mão remota oculta.
- **Não bloqueio:** listener e clientes usam `SOCK_NONBLOCK`; `accept`/`recv`/`send` rodam dentro do `tick()` já existente, com `poll(0)` para leitura. `send` enfileira em buffer interno quando o kernel não aceita tudo.
- **Configuração:** novas telas `LanConfig` (campos de texto), `LanWaiting` (endereços + porta) e `LanError` (motivo + voltar). Edição de IP/porta usa `ui::TextInput` (cursor, backspace, caracteres permitidos).
- **Queda:** `recv()==0`/erro marca desconexão; a sessão entra em `LanError` nos dois lados e oferece voltar ao menu. Sem reconexão.
- **Modos existentes:** `GameMode` passa a `{ Cpu, Hotseat, LanHost, LanGuest }`; CPU e hotseat não mudam e continuam cobertos pelos testes atuais.

## 2. Protocolo (contratos de dados)

### `net/protocol.hpp`

```cpp
enum class MessageType {
    Hello, Welcome, Error, Sync, Msg, Play, Truco, Accept, Run, Eleven, Rematch, Unknown
};

struct Message {
    MessageType type = MessageType::Unknown;
    std::vector<std::string> fields;   // tokens após o tipo
    std::string text;                  // resto da linha para Msg/Error
};

Message parseLine(const std::string& line);          // tolerante; Unknown se inválida
std::string encodeLine(MessageType, const std::vector<std::string>& fields = {},
                       const std::string& text = {});

std::string sideToken(Side side);                    // "J1" / "J2"
bool sideFromToken(const std::string&, Side& out);

std::string cardToken(Card card);                    // "3C" (rank + suit inicial)
bool cardFromToken(const std::string&, Card& out);

enum class NetPhase { Play, TrucoAnswer, Eleven, Wait, HandEnd, MatchEnd };

struct SyncState {
    uint32_t seq = 0;
    Side you = Side::Human;
    Side turn = Side::Human;
    NetPhase phase = NetPhase::Wait;
    int stake = 1;
    int scoreHuman = 0;      // J1
    int scoreCpu = 0;        // J2
    HandKind handKind = HandKind::Normal;
    Card vira{};
    std::array<Card, 3> own{};
    std::array<bool, 3> playedOwn{};
    std::array<bool, 3> playedOpponent{};
    std::optional<Card> trickOwn;
    std::optional<Card> trickOpponent;
    bool revealed = false;
    Side elevenSide = Side::Human;
    std::array<Card, 3> opponent{};   // só preenchido quando revealed
};

std::string encodeSync(const SyncState&);
bool decodeSync(const std::string& line, SyncState& out);
```

Formato de `SYNC` (todos os campos obrigatórios, ordem fixa):

```
SYNC <seq> <you:J1|J2> <turn:J1|J2> <phase:PLAY|TRUCO_ANSWER|ELEVEN|WAIT|HANDEND|MATCHEND>
     <stake> <scoreJ1> <scoreJ2> <kind:NORMAL|ELEVEN|IRON>
     <vira> <own1> <own2> <own3> <playedOwn:bitmask> <playedOpp:bitmask>
     <trickOwn|-'> <trickOpp|-> <revealed:0|1> <eleven:J1|J2>
     <opp1-> <opp2-> <opp3->      # "-" quando revealed=0
```

Cartas em token curto: rank (`4,5,6,7,Q,J,K,A,2,3`) + inicial do naipe (`O,E,C,P`).

## 3. Transporte

### `net/socket.hpp` (interno à camada)

```cpp
class Socket {
public:
    static int  listenTcp(uint16_t port, std::string& error);
    static int  connectTcp(const std::string& ip, uint16_t port, int timeoutMs, std::string& error);
    static void closeFd(int& fd);
    static std::vector<std::string> readLines(int fd, std::string& buffer, bool& closed);
    static bool sendAll(int fd, const std::string& data, std::string& outBuffer);
};
```

### `net/host_server.hpp` / `net/guest_client.hpp`

```cpp
class HostServer {
public:
    ~HostServer();
    bool listen(uint16_t port, std::string& error);
    bool waiting() const;                 // escutando sem convidado
    bool acceptPending();                  // aceita 1 conexão; true se conectou
    bool connected() const;
    uint16_t port() const;
    void sendLine(const std::string& line);
    std::vector<std::string> readLines();
    bool disconnected();                   // latches em close/erro
    void dropClient();
    void shutdown();
    std::vector<std::string> localAddresses() const;   // IPv4 não loopback
};

class GuestClient {
public:
    bool connectTo(const std::string& ip, uint16_t port, int timeoutMs, std::string& error);
    bool connected() const;
    void sendLine(const std::string& line);
    std::vector<std::string> readLines();
    bool disconnected();
    void close();
};
```

Regras de robustez: buffer de leitura por conexão; linha acima de 512 bytes é descartada até o próximo `\n`; `recv` com `EAGAIN` não é erro; `MSG_NOSIGNAL` em `send` para não morrer com `SIGPIPE`.

## 4. Integração com a Session

### `app/session.hpp` (adições)

```cpp
enum class GameMode { Cpu, Hotseat, LanHost, LanGuest };
enum class SessionState { Menu, Handoff, Dealing, HumanTurn, CpuTurn, TrucoDialog,
                          ElevenChoice, TrickReveal, HandResult, MatchResult,
                          LanConfig, LanWaiting, LanError };

// telas de rede
void openLanConfig(bool hostMode);
void confirmLanConfig();
void cancelLan();
void enterLanError(const std::string& reason);

// host
void startMatchAndBroadcast();
void broadcastSync();
void broadcastMessage(const std::string& text);
void pumpHost();          // accept, handshake, intents, disconnect
void applyGuestIntent(const Message& msg);
SyncState makeSync(Side viewer) const;
NetPhase currentPhase(Side viewer) const;

// guest
void pumpGuest();          // read lines, apply SYNC/MSG/ERROR, disconnect
void sendIntent(MessageType type, const std::vector<std::string>& fields = {});

// campos
GameMode mode_;
SessionState netScreenState_;
std::unique_ptr<HostServer> host_;
std::unique_ptr<GuestClient> guest_;
bool lanHandshakeDone_ = false;
uint32_t netSeq_ = 0;
SyncState guestView_;
std::string guestMessage_;
ui::TextInput ipInput_, portInput_;
int configFocus_ = 0;
```

Regras de fluxo:

- Menu (5 ações): `Jogar vs CPU`, `Jogar 2 jogadores`, `Hospedar partida (LAN)`, `Conectar em partida (LAN)`, `Sair`.
- `LanConfig`: host edita só porta; convidado edita IP e porta; `Tab`/clique alterna foco; `Enter` confirma; `Esc` volta ao menu; erro de validação permanece na tela com mensagem.
- `LanWaiting` (host): mostra `localAddresses()` + porta; `pumpHost()` aceita conexão, lê `HELLO`, valida versão, responde `WELCOME`, inicia `startMatch` como Jogador 1 e envia `SYNC`.
- Em partida `LanHost`, `activeSide_` é sempre `Human` (J1). Estados `HumanTurn`/`CpuTurn` significam "vez do host"/"vez do convidado"; `CpuTurn` não chama IA — `pumpHost` lê intenções.
- Em partida `LanGuest`, a `Session` **não** chama `Game`; `refreshView()` constrói `ViewModel` a partir de `guestView_`.
- Fase enviada em `SYNC` é derivada do estado da Session no host: `PLAY` (turno de carta), `TRUCO_ANSWER` (aguardando resposta), `ELEVEN`, `HANDEND`, `MATCHEND`, `WAIT`.
- Desconexão: `disconnected()` → `enterLanError("Conexao perdida com o adversario")`, fecha sockets, ação **Voltar ao menu**.
- Após cada mutação relevante do estado no host, chamar `broadcastSync()` e `broadcastMessage(texto)`.

## 5. Text input e UI

### `ui/text_input.hpp`

```cpp
class TextInput {
public:
    explicit TextInput(std::string allowed, std::string initial = {});
    bool handleKey(char key);          // 127/8 backspace, imprimível permitido
    void set(std::string value);
    const std::string& value() const;
    size_t cursor() const;
    bool empty() const;
private:
    std::string allowed_;
    std::string value_;
    size_t cursor_ = 0;
};
```

`ui/input.cpp` passa a repassar qualquer ASCII imprimível (0x20–0x7E) e `\t`, além de 127 (backspace).

### `ui/render.hpp` (adições ao `ViewModel`)

```cpp
enum class Screen { Menu, Handoff, Game, MatchResult, LanConfig, LanWaiting, LanError };
std::string lanTitle;                    // "Hospedar partida" / "Conectar"
std::string lanFieldLabel[2];            // rótulos ("IP", "Porta")
std::string lanFieldValue[2];
int lanFieldFocus = -1;
std::string lanHint;                     // ajuda de teclas
std::string lanAddressText;              // IPs + porta do host
std::string lanErrorText;
```

Renderização:
- `LanConfig`: título, até 2 campos com cursor no foco, hint, botões (`Iniciar`/`Conectar`, `Voltar`).
- `LanWaiting`: `Aguardando conexao`, endereços e porta, botão `Cancelar`.
- `LanError`: motivo, botão `Voltar ao menu` (id 100+0).
- Jogo em rede usa a mesma tela e o mesmo `ViewModel` do hotseat; a mão remota fica virada para baixo até `revealed`.

## 6. Global Constraints (herdadas literalmente do spec)

- C++17, `g++`, `Makefile`; apenas biblioteca padrão e POSIX (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `fcntl.h`, `poll.h`, `unistd.h`). Proibido OpenSSL, Boost.Asio e dependências externas.
- TCP IPv4, sockets não bloqueantes integrados ao `poll` existente; sem threads obrigatórias.
- Uma mensagem por linha, campos separados por espaço, limite de 512 bytes; mensagens desconhecidas descartadas.
- Somente o host aplica regras; o convidado nunca é fonte de verdade.
- A serialização de estado é por lado; mão remota só aparece após `revealed`.
- Sem TLS; rede local confiável. Nenhum shell, arquivo ou credencial.
- `recv`/`send` não podem bloquear indefinidamente; validação de índices e limites; sem `strcpy`.
- Testes de rede usam `127.0.0.1` dentro do binário de teste, sem rede externa.
- UI pt-BR; identificadores e mensagens de protocolo em inglês.

## 7. Review Focus

| Risco | Teste que cobre |
|---|---|
| Mão remota serializada antes do fim da mão | `test_protocol`: card slots `-` com `revealed=0`; `encodeSync` nunca preenche opponent oculto |
| Cliente consegue agir fora do turno | `test_net`: `PLAY` fora do turno é rejeitado e estado não muda |
| Jogar carta repetida pela rede | `test_net`: segundo `PLAY` do mesmo índice rejeitado |
| Linha gigante ou sem `\n` derruba o jogo | `test_net`: >512 bytes é descartada, conexão permanece e próxima linha válida funciona |
| Mensagem desconhecida altera estado | `test_protocol`/`test_net`: `FOO bar` é ignorado |
| Versão de protocolo incompatível | `test_net`: handshake com versão 999 recebe `ERROR VERSION` e fecha |
| Queda do peer deixa o outro travado | `test_net`: fechar socket do convidado leva host a `disconnected()`; sessão vai para `LanError` |
| Dois convidados | `test_net`: segundo `connect` é recusado enquanto há cliente |
| IP/porta inválidos | `test_text_input` + `test_net`: porta 0/`70000`, IP `999.999.999.999` geram erro sem exceção |
| Backspace/cursor do campo de texto | `test_text_input` |
| Porta ocupada | `test_net`: segundo `listen` na mesma porta falha com erro claro |
| Fase errada habilita ação | `test_net_session`: Truco só quando `phase==PLAY` e turno do lado |
| Vazamento de cartas via `MSG` | `test_net`: mensagens de texto não contêm tokens de carta da mão oculta |

## 8. Interfaces por Tarefa

1. **Text input + parser imprimível** — produz `ui::TextInput`; `parseInput` repassa 0x20–0x7E, `\t`, 127. Consome nada. Verifica `test_text_input` + casos de parser.
2. **Protocolo** — produz `net/protocol.{hpp,cpp}` conforme §2. Consome `core/card`, `core/game` (`HandKind`). Verifica round-trip de todas as mensagens, `SYNC` com/sem revelação, tokens inválidos, linha >512.
3. **Transporte de escuta/aceite** — produz `net/socket.{hpp,cpp}`, `net/host_server.{hpp,cpp}`. Verifica loopback: escutar porta efêmera, aceitar, ler linha, recusar segundo cliente, porta ocupada.
4. **Transporte cliente + handshake** — produz `net/guest_client.{hpp,cpp}`. Verifica conexão, envio/recebimento, timeout de conexão, handshake válido e versão inválida entre host e guest reais em `127.0.0.1`.
5. **Host autoritativo (adapter)** — produz funções de `Session` para o host: aplicar intenções com validação, montar `SyncState` por lado, broadcast. Verifica via testes de `Session` com host e convidado reais: turno, carta repetida, truco, onze, hand end, match end.
6. **Telas de rede** — produz `ViewModel`/render das telas `LanConfig`, `LanWaiting`, `LanError` e mapeamento por modo. Verifica snapshots 80x24 e hit targets.
7. **Sessão LanHost** — menu, config, escuta, handshake, partida local como J1, `pumpHost`, desconexão. Verifica testes de sessão com sockets reais.
8. **Sessão LanGuest** — conexão, espelho de `SYNC`, envio de intenções, render, desconexão, rematch. Verifica testes de sessão.
9. **Partida completa em loopback** — teste de integração host+guest até `MATCHEND`, com truco/aceite e mão de 11 quando ocorrer. Verifica asserções de placar idêntico nos dois lados.
10. **README + auditoria** — documenta host/conectar, porta, privacidade e limitações; auditoria de diff.

## 9. Alternativas Consideradas

- **JSON ou biblioteca de serialização:** descartado (dependência externa proibida). Protocolo de linha com campos fixos é suficiente e testável.
- **Eventos incrementais em vez de snapshot:** descartado. Snapshot simplifica o espelhamento no convidado e torna a privacidade verificável num único ponto (`encodeSync`).
- **Threads separadas para servidor/cliente:** descartado. Integração no `poll` existente evita concorrência e mantém o desenho atual.
- **Binário servidor separado:** descartado. Host precisa da mesma UI e do mesmo motor; um binário em dois papéis reduz duplicação.
- **Reconexão:** descartada pelo escopo validado; queda volta ao menu.

## 10. Justificativa de Complexidade

- `net/protocol`: consumido por host, cliente e testes de protocolo (3 consumidores) e é a fronteira de privacidade.
- `net/socket`: isola POSIX e permite testar loopback sem duplicar código em host/cliente.
- `HostServer`/`GuestClient`: dois papéis concretos, cada um com um consumidor (Session) e testes de integração próprios; separados porque as operações de socket e o ciclo de vida diferem.
- `ui::TextInput`: dois campos em duas telas; evita lógica de edição duplicada e é testável sem TTY.
- `SyncState`: contrato único entre rede e renderização do convidado; sem ele, o convidado precisaria reimplementar o motor.
