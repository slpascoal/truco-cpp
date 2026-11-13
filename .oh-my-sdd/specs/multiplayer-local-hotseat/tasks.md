# Tasks: Multiplayer local hotseat

Slug: `multiplayer-local-hotseat`
Escala: SMALL (plan embutido)
Base: `.oh-my-sdd/specs/multiplayer-local-hotseat/spec.md` (validado)

## Decisões

- **Modo de jogo:** `enum class GameMode { Cpu, Hotseat }` em `app/session.hpp`, escolhido no menu (3 ações: `Jogar vs CPU`, `Jogar 2 jogadores`, `Sair`). `Cpu` preserva o comportamento atual.
- **Lados reaproveitados:** `Side::Human` = Jogador 1, `Side::Cpu` = Jogador 2. Em hotseat a IA nunca é chamada; a única diferença é quem controla o lado `Cpu`.
- **Novo estado:** `SessionState::Handoff`. `Session` passa a rastrear `activeSide_` (lado que está vendo a tela) e `pendingSide_` (lado que agirá após a troca). Toda entrada em turno, resposta de truco ou mão de 11 de um lado diferente de `activeSide_` passa por `Handoff`.
- **Privacidade:** `ViewModel::Screen::Handoff` renderiza apenas nome do próximo jogador e instrução; nenhuma carta é desenhada. `view_` continua sem expor cartas alheias durante a mão.
- **Ações por lado:** `Session::selectCard`, `confirmSelection`, `activateAction` operam sobre `activeSide_` quando o modo é Hotseat e o estado é o turno daquele lado. Em modo CPU, `CpuTurn` continua automático (tick + IA).
- **Truco:** em hotseat, qualquer lado pede aumento no próprio turno; a resposta do adversário sempre passa por `Handoff` antes de exibir `Aceitar`/`Correr`.
- **Mão de 11:** o dono da decisão passa por `Handoff` antes das opções; mão de ferro resolve como hoje, sem escolha.
- **Nomes:** helper de exibição por modo (`Voce`/`Computador` vs `Jogador 1`/`Jogador 2`); mensagens e resultado final usam o nome do vencedor.
- **Test seam:** `Session::setScoreForTesting(GameScore)` delega a `Game` para exercitar mão de 11/ferro em hotseat.
- **Arquivos:** `src/app/session.{hpp,cpp}`, `src/ui/render.{hpp,cpp}`, `tests/test_session.cpp`, `tests/test_render.cpp`, `README.md`. `layout`/`terminal`/`core`/`ai` não mudam.

## Tasks

- [ ] 1. Modo de jogo, menu de três opções e nomes por modo
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session.cpp`
      consome: `Session`, `ViewModel`
      produz: `enum class GameMode { Cpu, Hotseat }`; `void Session::setModeForTesting(GameMode)`; `std::string Session::sideName(Side) const`; menu com 3 ações
      verifica: `make test` com testes de menu (3 ações), início em modo CPU e nomes `Voce`/`Computador` vs `Jogador 1`/`Jogador 2`
      passos:
        1. escrever testes de menu e nomes (RED)
        2. rodar e confirmar falha
        3. implementar modo, nomes e menu (GREEN)
        4. `make test` verde
      rastreia: Req-01, Req-02, Req-12

- [ ] 2. Estado Handoff e alternância de turnos entre humanos
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session.cpp`
      consome: tarefa 1
      produz: `SessionState::Handoff`; `activeSide_`, `pendingSide_`, `confirmHandoff()`; ações genéricas por lado
      verifica: `make test` com teste de partida hotseat: Dealing → Handoff (sem cartas) → confirmação → turno do Jogador 1; jogada → Handoff → Jogador 2; tick não avança turno automático em hotseat
      passos:
        1. testes de handoff e alternância (RED)
        2. rodar e confirmar falha
        3. implementar estado e ações por lado (GREEN)
        4. `make test` verde
      rastreia: Req-03, Req-04, Req-05, Req-11

- [ ] 3. Truco e resposta com handoff entre humanos
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session.cpp`
      consome: tarefa 2
      produz: fluxo `TrucoDialog` precedido de `Handoff` quando o respondente é o outro lado; mensagens com nomes
      verifica: `make test` com teste: Jogador 1 pede truco → `Handoff` para Jogador 2 → confirmação → `TrucoDialog` com `Aceitar`/`Correr`; aceitar sobe a aposta; correr encerra e credita
      passos:
        1. testes de truco hotseat (RED)
        2. rodar e confirmar falha
        3. implementar handoff de truco (GREEN)
        4. `make test` verde
      rastreia: Req-06, Req-05

- [ ] 4. Mão de 11 e mão de ferro em hotseat
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session.cpp`
      consome: tarefas 1–3
      produz: `Session::setScoreForTesting(GameScore)`; fluxo ciente do lado em `ElevenChoice` com `Handoff` prévio; resultado nomeado
      verifica: `make test` com testes: placar 11/5 ⇒ Handoff do Jogador 1 e opções Jogar/Esconder; placar 5/11 ⇒ Handoff do Jogador 2; 11/11 ⇒ mão de ferro sem handoff de escolha; resultado da partida com nome
      passos:
        1. testes de mão de 11/ferro hotseat (RED)
        2. rodar e confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-07, Req-08, Req-09, Req-10

- [ ] 5. Tela de troca e privacidade visual
      altera: `src/ui/render.{hpp,cpp}`, `src/app/session.cpp`, `tests/test_render.cpp`
      consome: tarefa 2
      produz: `ViewModel::Screen::Handoff`; renderização com nome do próximo jogador, instrução de confirmação e sem cartas
      verifica: `make test` com render de Handoff em 80x24: contém nome do jogador e `Continuar`, não contém `VIRA` nem molduras de carta; larguras ≤ 80
      passos:
        1. testes de render (RED)
        2. rodar e confirmar falha
        3. implementar tela (GREEN)
        4. `make test` verde
      rastreia: Req-03, Req-04, Req-11

- [ ] 6. Partida hotseat completa e regressão do modo CPU
      altera: `tests/test_session.cpp`
      consome: tarefas 1–5
      produz: teste de integração de partida hotseat completa até `MatchResult`
      verifica: `make test` com partida hotseat simulada (confirma handoffs, joga dos dois lados, aceita truco, decide mão de 11 quando ocorrer) e com o teste existente de partida CPU ainda verde
      passos:
        1. escrever teste de integração (RED se algo faltar)
        2. rodar e confirmar
        3. corrigir o que faltar (GREEN)
        4. `make test` verde
      rastreia: Critérios de Aceite (partida completa, modo CPU intacto)

- [ ] 7. README do modo 2 jogadores
      altera: `README.md`
      consome: tarefas 1–6
      produz: seção de controles/menu do hotseat e nota de privacidade da tela de troca
      verifica: leitura do README contra o comportamento implementado; `make clean && make && make test` exit 0
      passos: reduzido (documentação) — justificativa: sem lógica testável; verificação é executar os comandos e conferir os controles
      rastreia: Req-01, Req-03

- [ ] 8. Auditoria de diff
      altera: —
      consome: commits das tarefas 1–7
      produz: relatório de conformidade escopo × diff
      verifica: `git diff --stat` do intervalo comparado com a união dos `altera:`; arquivos fora do previsto listados e justificados; `make clean && make && make test` final
      passos: reduzido (auditoria) — justificativa: inspeção, sem código novo
      rastreia: constitution §6

## Cobertura

| Requisito | Tarefas |
|---|---|
| Req-01, Req-02 | 1, 7 |
| Req-03, Req-04 | 2, 5 |
| Req-05 | 2, 3 |
| Req-06 | 3 |
| Req-07, Req-08 | 4 |
| Req-09, Req-10 | 4 |
| Req-11 | 2, 5 |
| Req-12 | 1, 6 |
