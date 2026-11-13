# Tasks — Modo online Battle Royale de truco mineiro

Ordem de execução. Cada tarefa é um commit verificado. `altera:` é contrato de escopo: tocar
arquivo fora dele exige parar e avisar.

## Tasks

- [x] 1. Motor `RoyaleMatch`: assentos, distribuição e vazas
      rastreia: R6, R7, R8, R9, R25
      altera: src/core/royale.hpp, src/core/royale.cpp, tests/test_royale.cpp,
              tests/test_main.cpp (registro do arquivo de teste, se necessário)
      consome: `cardStrength(Card)`, `Stake` de `core/hand_value.hpp`
      produz: `RoyaleMatch(int, std::mt19937&)`, `RoyaleSeat`, `seat(int)`, `startHand()`,
              `currentTurn()`, `canPlay(int)`, `playCard(int,int)`, `tricksPlayed()`, `mao()`,
              `RoyalePhase`
      verifica: `make test` verde com 3 e 13 jogadores: 3 cartas cada, 39 cartas no máximo, ordem
                de assentos, maior carta vence, vaza com força máxima repetida sem vencedor
      passos:
        1. escrever testes que falham para criação, deal, ordem de turno e vazas (RED)
        2. rodar e confirmar falha de compilação/assertivas
        3. implementar `royale.hpp/cpp` com estado por assento e resolução da vaza
        4. rodar `make test` e confirmar verde

- [x] 2. `RoyaleMatch`: fim de mão, pontos, eliminação e fim de partida
      rastreia: R10, R11, R12, R20
      altera: src/core/royale.hpp, src/core/royale.cpp, tests/test_royale.cpp
      consome: T1 (`playCard`, `tricksWon`, assentos)
      produz: `handWinner()`, `matchWinner()`, `handValue()`, `aliveCount()`, `removePlayer(int)`,
              `RoyaleMatch::startHand()` rotativo
      verifica: `make test` verde: vencedor por vazas, desempate por maior carta e depois pela mão,
                perdedores pagam o valor final, 0 pontos elimina, um sobrevivente encerra, mão
                termina com um único jogador e a mão gira
      passos:
        1. testes que falham para desempate, cobrança, eliminação e match end (RED)
        2. rodar e confirmar falha
        3. implementar `finishHand`, cobrança, eliminação e checagem de fim de partida
        4. rodar `make test` e confirmar verde

- [x] 3. `RoyaleMatch`: truco multi-adversário, corrida e direitos de aumento
      rastreia: R13, R14, R15, R16, R17, R18, R19
      altera: src/core/royale.hpp, src/core/royale.cpp, tests/test_royale.cpp
      consome: T1, T2
      produz: `callTruco(int)`, `respondTruco(bool)`, `trucoPending()`, `trucoCaller()`,
              `trucoResponder()`, `pendingValue()`, `raiseAllowed(int)`
      verifica: `make test` verde: respostas em ordem, correr desconta e sai da mão, aceitar mantém e
                libera aumento, pedinte nunca aumenta, todos correndo encerram a mão para o pedinte,
                respostas bloqueiam jogadas
      passos:
        1. testes que falham para fila de respostas, custos e direitos (RED)
        2. rodar e confirmar falha
        3. implementar fila de respondentes, cobrança e conjunto de direitos
        4. rodar `make test` e confirmar verde

- [x] 4. `RoomServer` multi-cliente em loopback
      rastreia: R2, R4, R23, R24
      altera: src/net/room_server.hpp, src/net/room_server.cpp, tests/test_room_server.cpp
      consome: helpers de `net/socket.hpp`
      produz: `RoomServer` completo conforme `plan.md`
      verifica: `make test` verde: 3 clientes conectam, assentos estáveis, leitura/escrita por
                cliente, broadcast, desconexão detectada e novo cliente recusado quando cheio
      passos:
        1. teste que falha com 3 clientes e troca de linhas (RED)
        2. rodar e confirmar falha
        3. implementar vetor de conexões, aceitação e multiplexação
        4. rodar `make test` e confirmar verde

- [x] 5. Protocolo Royale: `JOIN`, `LOBBY`, `START`, `RSTATE`
      rastreia: R21, R22, R23
      altera: src/net/protocol.hpp, src/net/protocol.cpp, tests/test_royale_protocol.cpp
      consome: `RoyalePhase`, `RoyaleSeat` de T1–T3; `Message`/`parseLine`
      produz: `RoyaleView`, `RoyaleSeatView`, `encodeRoyaleState`, `decodeRoyaleState`,
              `encodeLobby`, `decodeLobby`, `MessageType::{Join,Lobby,Start,RState}`
      verifica: `make test` verde: roundtrip de 3 e 13 assentos, cartas ocultas como `-`,
                `revealed` só quando permitido, decode rejeita contagem inválida e token corrompido,
                linha ≤ `kMaxLine`
      passos:
        1. testes de roundtrip e rejeição que falham (RED)
        2. rodar e confirmar falha
        3. implementar tokens, codificação e validação
        4. rodar `make test` e confirmar verde

- [x] 6. Sessão host Royale: configuração, lobby, início e intenções
      rastreia: R1, R2, R3, R4, R23
      altera: src/app/session.hpp, src/app/session.cpp, src/ui/render.hpp (estados, se necessário),
              tests/test_royale_session.cpp
      consome: T3 (`RoyaleMatch`), T4 (`RoomServer`), T5 (`RoyaleView`)
      produz: `GameMode::{RoyaleHost}`, `SessionState::{RoyaleConfig,RoyaleLobby,RoyaleWaiting,
              RoyalePlay,RoyaleResult}`, `setRoyalePlayersForTesting(int)`,
              `openRoyaleHostForTesting(uint16_t,int)`, `royaleMatch()`
      verifica: `make test` verde: host configura teto 3–13, inicia com ≥3, recusa entrada após
                início e acima do teto, aplica PLAY/TRUCO/ACCEPT/RUN de clientes no motor
      passos:
        1. testes que falham para lobby e start (RED)
        2. rodar e confirmar falha
        3. implementar estados, wiring do motor e leitura de intenções
        4. rodar `make test` e confirmar verde

- [x] 7. Sessão guest Royale: entrada, estado e jogadas
      rastreia: R5, R21, R22, R23
      altera: src/app/session.hpp, src/app/session.cpp, tests/test_royale_session.cpp
      consome: T6
      produz: `GameMode::RoyaleGuest`, `royaleView()`, `openRoyaleJoinForTesting(const
              std::string&, uint16_t)`
      verifica: `make test` verde: guest recebe assento, vê só a própria mão, envia intenções e
                reflete jogadas/truco/game over; desconexão elimina no host
      passos:
        1. testes que falham para handshake, aplicação de `RSTATE` e intenções (RED)
        2. rodar e confirmar falha
        3. implementar conexão, decodificação e envio de intenções
        4. rodar `make test` e confirmar verde

- [x] 8. UI Royale: tabela de jogadores, mão local e submenu LAN
      rastreia: R21, R22, R1
      altera: src/ui/render.hpp, src/ui/render.cpp, src/ui/layout.hpp, src/ui/layout.cpp,
              src/app/session.cpp, tests/test_render.cpp, tests/test_session.cpp
      consome: `RoyaleView`/`RoyaleMatch` de T5–T7
      produz: `RoyaleRow`, `ViewModel::royaleRows/royaleTitle/royaleInfo`, `Screen::Royale`,
              submenu LAN dentro de `Menu`
      verifica: `make test` verde: frame Royale lista assentos/HP/status/carta, não excede 80
                colunas com 13 jogadores, mãos alheias ocultas, menu LAN abre 4 opções e volta
      passos:
        1. testes de render/menu que falham (RED)
        2. rodar e confirmar falha
        3. implementar linhas Royale, tela e submenu
        4. rodar `make test` e confirmar verde

- [x] 9. Integração Royale com 3 clientes e partida curta
      rastreia: critérios de aceite de partida completa e desconexão
      altera: tests/test_royale_integration.cpp, src/app/session.cpp (correções de fluxo
              encontradas)
      consome: T4–T8
      produz: testes de ponta a ponta
      verifica: `make test` verde: 3 sessões em loopback jogam até restar um vencedor; correr de
                truco desconta; desconexão de um cliente elimina e a partida continua
      passos:
        1. teste de integração que falha (RED)
        2. rodar e confirmar falha
        3. corrigir fluxo de sessão/rede sem alterar regra do motor
        4. rodar `make test` e confirmar verde

- [x] 10. README do Battle Royale
      rastreia: R1–R24 (documentação)
      altera: README.md
      consome: comportamento de T1–T9
      produz: —
      verifica: `make test` continua verde; README descreve regras, lobby, limites (3–13) e
                desconexão
      passos: reduzido (documentação, sem teste de comportamento próprio)

- [x] 11. Auditoria de diff final
      rastreia: todos os requisitos do spec
      altera: — (somente leitura)
      consome: commits de T1–T10
      produz: relatório de arquivos fora do escopo
      verifica: `git diff --stat` desde o início da feature comparado com a união dos `altera:`;
                reportar arquivos extras, nunca reverter em silêncio
      passos: reduzido (auditoria somente leitura)
