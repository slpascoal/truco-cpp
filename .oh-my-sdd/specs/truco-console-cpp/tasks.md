# Tasks: Jogo de Truco Mineiro em console (jogador vs CPU)

Slug: `truco-console-cpp`
Base: `.oh-my-sdd/specs/truco-console-cpp/plan.md` (MEDIUM)
Convenções: cada tarefa termina em `make test` verde e commit próprio. `altera:` é contrato de escopo — tocar arquivo fora dele exige parada e aviso.

## Tasks

- [x] 1. Build skeleton e harness de testes
      altera: `Makefile`, `src/main.cpp`, `tests/test_main.cpp`, `.gitignore`
      consome: —
      produz: alvos `all/run/test/clean`; macros `CHECK(cond)`, `CHECK_EQ(a,b)`, `TEST(name)`; `int test_main()`
      verifica: `make && ./truco` imprime placeholder e sai com 0; `make test` compila e retorna 0
      passos:
        1. escrever `tests/test_main.cpp` com um teste trivial que falha de propósito (RED)
        2. rodar `make test` e confirmar exit ≠ 0 com mensagem do teste
        3. implementar Makefile, harness e `main.cpp` mínimo (GREEN)
        4. rodar `make test` e confirmar exit 0; rodar `make clean && make`
      rastreia: Critérios de Aceite (build sem warnings), constitution §2

- [x] 2. Tipos de carta, força, manilha e rótulos
      altera: `src/core/card.hpp`, `src/core/card.cpp`, `tests/test_card.cpp`, `tests/test_main.cpp`
      consome: harness da tarefa 1
      produz: `enum class Suit`, `enum class Rank`, `struct Card`, `commonStrength`, `suitOrder`, `manilhaRank`, `isManilha`, `cardStrength`, `rankLabel`, `suitLabel`, `suitName` (assinaturas do plan §2)
      verifica: `make test` com testes de ordem comum, ordem de manilha por naipe, ciclo `3→4`, força total monotônica e rótulos
      passos:
        1. escrever testes de todas as funções (RED)
        2. rodar e confirmar falhas de link/símbolo
        3. implementar `card.cpp` mínimo (GREEN)
        4. `make test` verde
      rastreia: Req-01, Req-03, Req-04, Req-05

- [x] 3. Baralho, embaralhamento e distribuição
      altera: `src/core/deck.hpp`, `src/core/deck.cpp`, `tests/test_deck.cpp`
      consome: `Card`, `Rank`, `Suit`, `std::mt19937`
      produz: `buildDeck()`, `shuffleDeck()`, `struct Deal`, `deal(std::mt19937&)`
      verifica: `make test` com 40 cartas únicas, sem 8/9/10, vira fora das mãos, mesma seed ⇒ mesmo `Deal`, seeds diferentes ⇒ deals diferentes em amostra
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-01, Req-02, Req-06

- [x] 4. Resolução de vazas e da mão com empates
      altera: `src/core/hand.hpp`, `src/core/hand.cpp`, `tests/test_hand.cpp`
      consome: `cardStrength`, `Card`, `Rank`
      produz: `enum class Side`, `enum class TrickOutcome`, `trickWinner()`, `class HandProgress`, `resolveHandWinner()`
      verifica: `make test` cobrindo matriz de vazas, empate igual-valor-igual-naipe, 1.ª empata → 2.ª decide, 2.ª empata → vencedor da 1.ª, três empates → dealer
      passos:
        1. testes de casos e bordas (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-07, Req-08, Req-09

- [x] 5. Escala de aposta
      altera: `src/core/hand_value.hpp`, `src/core/hand_value.cpp`, `tests/test_hand_value.cpp`
      consome: —
      produz: `enum class Stake`, `nextStake`, `canRaise`, `valueAfterAccept`, `callLabel`, `valuePoints`
      verifica: `make test` com escala 1→3→6→9→12, sem próximo de 12, rótulos "Truco/Seis/Nove/Doze"
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-12, Req-15

- [x] 6. Motor de partida: mãos, truco, corre, mão de 11 e ferro
      altera: `src/core/game.hpp`, `src/core/game.cpp`, `tests/test_game.cpp`
      consome: tarefas 2–5
      produz: `HandKind`, `GameScore`, `class Game` com API do plan §2
      verifica: `make test` cobrindo início de partida, alternância de dealer, truco→aceite→próximo aumento, truco→corre (crédito anterior), limite 12, mão de 11 (jogar/esconder), mão de ferro sem truco, disparo só com placar exato 11 (limites 10/11/12), fim de partida e trava em 12
      passos:
        1. testes de transições e placar (RED)
        2. confirmar falhas
        3. implementar máquina de estados (GREEN)
        4. `make test` verde
      rastreia: Req-08, Req-10, Req-11, Req-12–Req-20

- [x] 7. IA da CPU
      altera: `src/ai/cpu_player.hpp`, `src/ai/cpu_player.cpp`, `tests/test_ai.cpp`
      consome: `core` + `struct PublicView`
      produz: `PublicView`, `chooseCardIndex`, `shouldCallTruco`, `shouldAcceptTruco`, `shouldPlayHandOfEleven`
      verifica: `make test` com escolha sempre legal, correlação entre mão forte e aceite/pedido, blefe presente em amostra, `PublicView` sem cartas humanas (garantia de tipo/compilação), determinismo com seed fixa
      passos:
        1. testes de propriedades e estatísticos com seed (RED)
        2. confirmar falha
        3. implementar heurística (GREEN)
        4. `make test` verde
      rastreia: Req-32, Req-33, Req-34, Req-35, Req-36

- [x] 8. Terminal RAII, parser de entrada e mouse SGR
      altera: `src/ui/terminal.hpp`, `src/ui/terminal.cpp`, `src/ui/input.hpp`, `src/ui/input.cpp`, `tests/test_input.cpp`
      consome: —
      produz: `class Terminal`, `struct InputEvent`, `parseInput`, `pollEvent`, `struct Size`
      verifica: `make test` com SGR press/release, X10, teclas `1/2/3/Enter/Espaço/Esc/q`, bytes lixo, sequência truncada, UTF-8; teste de restauração de `termios` sob exceção e em saída normal
      passos:
        1. testes do parser e de RAII (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde; rodar `make run` e sair confirmando terminal intacto (`stty -a` antes/depois)
      rastreia: Req-24, Req-26, Req-31

- [x] 9. Layout e renderização de cartas e tela
      altera: `src/ui/layout.hpp`, `src/ui/layout.cpp`, `src/ui/render.hpp`, `src/ui/render.cpp`, `tests/test_render.cpp`
      consome: `core` types, `Size`, `InputEvent`
      produz: `Rect`, `Layout::compute`, `HitTarget`, `ViewModel`, `Renderer::draw`, `Renderer::renderCard`, `hitTargets`
      verifica: `make test` com `renderCard` estável (9×7), nenhuma linha > largura em 80×24/120×40, hit-testing nas bordas dos retângulos, layout de min-size 40×10
      passos:
        1. testes de layout e strings (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde; inspeção visual em `make run`
      rastreia: Req-22, Req-25, Req-29, Req-30, Critérios de Aceite (cartas desenhadas)

- [x] 10. Sessão, loop de eventos e integração com main
      altera: `src/app/session.hpp`, `src/app/session.cpp`, `src/main.cpp`, `tests/test_session.cpp`
      consome: tarefas 1–9
      produz: `SessionState`, `class Session`, `Session::run()`
      verifica: `make test` com script de eventos levando Menu→Jogar→mão→resultado; clique com confirmação; cancelamento com `Esc`; pacing da CPU sem bloquear input; partida completa jogável por teclado em modo headless (eventos injetados)
      passos:
        1. testes de fluxo com eventos injetados (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde; `make run` partida manual curta
      rastreia: Req-21, Req-23, Req-27, transições do plan §1

- [x] 11. Polimento de UX: animações, menus, ajuda, min-size e resize
      altera: `src/app/session.cpp`, `src/ui/render.cpp`, `src/ui/layout.cpp`, `src/ui/terminal.cpp`
      consome: tarefas 8–10
      produz: telas Menu/Resultado completas, animação de distribuição/revelação, aviso de min-size, redesenho em resize
      verifica: `make test` de snapshots de tela em 80×24, 120×40 e 40×10; `SIGWINCH` redesenha sem perder estado (teste injetando evento Resize); partida manual completa no terminal
      passos:
        1. testes de snapshot e resize (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde; `make run` partida completa com mouse e com teclado
      rastreia: Req-21, Req-28, Req-29, Req-30, Critérios de Aceite (UI)

- [x] 12. README e verificação final de aceite
      altera: `README.md`
      consome: binário e alvos do Makefile
      produz: instruções de build/execução, controles, regras implementadas
      verifica: comandos do README executados a partir de diretório limpo (`make clean && make && make test && make run`); checklist de critérios de aceite do spec revisado item a item com evidência
      passos: reduzido (documentação) — justificativa: sem lógica testável; verificação é executar os comandos documentados
      rastreia: Critérios de Aceite (todos), constitution §7

- [ ] 13. Auditoria de diff
      altera: —
      consome: histórico de commits das tarefas 1–12
      produz: relatório de conformidade escopo × diff
      verifica: `git diff --stat` da implementação comparado com a união dos `altera:`; arquivos fora do previsto listados e justificados, nunca revertidos em silêncio; `make clean && make && make test` final
      passos: reduzido (auditoria) — justificativa: comando de inspeção, sem código novo
      rastreia: constitution §6 (diff cirúrgico)

## Cobertura de requisitos

| Requisito | Tarefas |
|---|---|
| Req-01, Req-03, Req-04, Req-05 | 2 |
| Req-02, Req-06 | 3 |
| Req-07, Req-08, Req-09 | 4 |
| Req-10, Req-11 | 6 |
| Req-12–Req-15 | 5, 6 |
| Req-16 | 6, 10 |
| Req-17–Req-20 | 6, 10 |
| Req-21, Req-23 | 10, 11 |
| Req-22, Req-25 | 9 |
| Req-24, Req-26, Req-31 | 8 |
| Req-27 | 10 |
| Req-28–Req-30 | 11 |
| Req-32–Req-36 | 7 |
| NFR (build, deps, RNG, testes) | 1, 2, 3, 8 |
| Critérios de Aceite | 1, 9, 11, 12, 13 |
