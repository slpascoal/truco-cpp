# Tasks — Regras do truco mineiro: manilha fixa e alternância de truco

Ordem de execução. Cada tarefa é um commit verificado. `altera:` é contrato de escopo: tocar
arquivo fora dele exige parar e avisar.

## Tasks

- [x] 1. Manilha fixa no núcleo e na IA (sem remover a vira do baralho/UI ainda)
      rastreia: R1, R2, R3, R4, R8
      altera: src/core/card.hpp, src/core/card.cpp, src/core/hand.hpp, src/core/hand.cpp,
              src/core/game.cpp, src/ai/cpu_player.hpp, src/ai/cpu_player.cpp,
              src/app/session.cpp (apenas chamada de mão de 11), tests/test_card.cpp,
              tests/test_hand.cpp, tests/test_game.cpp, tests/test_ai.cpp
      consome: — (estado atual)
      produz: `bool isManilha(Card)`; `int cardStrength(Card)`; `TrickOutcome trickWinner(Card, Card)`;
              `bool shouldPlayHandOfEleven(Card hidden, std::mt19937& rng)`; `PublicView` sem `vira`
      verifica: `make test` verde com manilhas `4♣ > 7♥ > A♠ > 7♦` e comuns sem desempate por naipe
      passos:
        1. escrever testes que falham: ordem fixa das quatro manilhas, manilha vence comum,
           comum de naipes diferentes empata, mão de 11 usa força fixa (RED)
        2. rodar e confirmar a falha esperada (funções ainda recebem vira / vira define manilha)
        3. trocar as assinaturas no núcleo e ajustar chamadas em `game`, `session` e `cpu_player`
        4. rodar `make test` e confirmar verde

- [x] 2. Alternância de truco no `Game` (corrige pedido repetido após aceite)
      rastreia: R9, R10, R12, R13, R14, R15
      altera: src/core/game.hpp, src/core/game.cpp, tests/test_game.cpp
      consome: `canCallTruco(Side)`, `callTruco(Side)`, `respondTruco(Side, bool)` de T1
      produz: mesma API pública; invariante interna `raiser_`/`hasRaiser_`
      verifica: `make test` Verde com teste que reproduz o bug: A pede, B aceita, A não pode pedir,
                B pode; após B pedir e A aceitar, A volta a poder; nova mão libera os dois
      passos:
        1. escrever teste que falha reproduzindo o bug (A pede → B aceita → A ainda pode pedir) (RED)
        2. rodar e confirmar a falha
        3. adicionar estado do último pedinte em `Game` e negar em `canCallTruco`; resetar em
           `resetHand`; atualizar `game_escalates_to_twelve_then_blocks` para alternar os lados
        4. rodar `make test` e confirmar verde

- [x] 3. Remover vira do baralho, da UI e da rede; sincronizar permissão de truco
      rastreia: R5, R6, R7, R11
      altera: src/core/deck.hpp, src/core/deck.cpp, src/ui/render.hpp, src/ui/render.cpp,
              src/ui/layout.hpp, src/ui/layout.cpp, src/net/protocol.hpp, src/net/protocol.cpp,
              src/app/session.cpp, tests/test_deck.cpp, tests/test_protocol.cpp,
              tests/test_render.cpp, tests/test_session.cpp, tests/test_session_lan.cpp,
              tests/test_net_guest.cpp
      consome: T1 (`cardStrength(Card)`), T2 (`canCallTruco` com alternância)
      produz: `Deal` sem `vira`; `SyncState` sem `vira` e com `bool canRaise`;
              `ViewModel` sem `vira`/`manilhaText`; `Layout` sem `Rect vira`; protocolo versão `"2"`
      verifica: `make test` verde: 6 cartas distintas sem carta reservada; frame não contém
                "VIRA"/"MANILHA"; `SYNC` ida-e-volta preserva `canRaise` com 23 campos;
                handshake com versão "1" é rejeitado e com "2" aceito
      passos:
        1. escrever testes que falham: `Deal` sem vira, render sem "VIRA", `SYNC` com `canRaise`,
           handshake "1" rejeitado (RED)
        2. rodar e confirmar a falha
        3. remover vira de `Deal`, `ViewModel`, `Layout` e `SyncState`; calcular
           `sync.canRaise = game_.canCallTruco(viewer)` no host; subir HELLO/WELCOME para "2";
           atualizar textos do menu
        4. rodar `make test` e confirmar verde

- [x] 4. Convidado LAN exibe "Truco!" só quando tem direito
      rastreia: R11, R14
      altera: src/app/session.cpp, tests/test_session_lan.cpp
      consome: `SyncState::canRaise` de T3
      produz: ação "Truco!" guiada por `canRaise` em `ViewModel::actions`
      verifica: `make test` verde com teste de sessão LAN: após aceitar truco do host, o
                convidado vê "Truco!"; após pedir truco aceito pelo host, não vê
      passos:
        1. escrever teste que falha com o convidado vendo "Truco!" mesmo sem direito (RED)
        2. rodar e confirmar a falha
        3. trocar `guestView_.stake < 12` por `guestView_.canRaise` na montagem da ação
        4. rodar `make test` e confirmar verde

- [x] 5. Atualizar README para as regras do truco mineiro
      rastreia: R1, R3, R5, R6, R9
      altera: README.md
      consome: comportamento de T1–T4
      produz: —
      verifica: `grep -n -i "vira\|manilha pela" README.md` sem resultado; `make test` continua verde
      passos: reduzido (documentação; sem teste de comportamento próprio)

- [x] 6. Auditoria de diff final
      rastreia: todos os requisitos do spec
      altera: — (somente leitura)
      consome: commits de T1–T5
      produz: relatório de arquivos tocados fora do previsto
      verifica: `git diff --stat` desde o início da feature comparado com a união dos `altera:`
                de T1–T5; reportar qualquer arquivo extra, nunca reverter em silêncio
      passos: reduzido (auditoria somente leitura)
