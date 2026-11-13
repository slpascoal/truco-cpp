# Tasks: Multiplayer LAN via TCP

Slug: `multiplayer-lan-tcp`
Base: `.oh-my-sdd/specs/multiplayer-lan-tcp/plan.md` (MEDIUM)
Convenções: `make test` verde e commit por tarefa; `altera:` é contrato de escopo.

## Tasks

- [x] 1. Campo de texto e parser de teclas imprimíveis
      altera: `src/ui/text_input.{hpp,cpp}`, `src/ui/input.cpp`, `tests/test_text_input.cpp`, `tests/test_input.cpp`
      consome: `parseInput`
      produz: `ui::TextInput` (value/cursor/handleKey/set/empty); `parseInput` repassa 0x20–0x7E, `\t`, 127
      verifica: `make test` com edição de IP/porta (dígitos, `.`, `:`, backspace no meio, cursor), teclas fora do conjunto descartadas, e casos de parser para `.`, `:`, Tab e backspace
      passos:
        1. escrever testes (RED)
        2. rodar e confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-20, Req-03

- [x] 2. Protocolo de linha e snapshot SYNC
      altera: `src/net/protocol.{hpp,cpp}`, `tests/test_protocol.cpp`
      consome: `core::Card`, `core::HandKind`, `Side`
      produz: `MessageType`, `Message`, `parseLine`, `encodeLine`, `cardToken/cardFromToken`, `sideToken/sideFromToken`, `NetPhase`, `SyncState`, `encodeSync`, `decodeSync`
      verifica: `make test` com round-trip de HELLO/WELCOME/PLAY/TRUCO/ACCEPT/RUN/ELEVEN/REMATCH/MSG/ERROR; `SYNC` com `revealed=0` produz `-` nos slots adversários e `decodeSync` não materializa mão oculta; linha >512 rejeitada; tokens de carta inválidos rejeitados
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-06, Req-09, Req-10, Req-18, Req-19

- [x] 3. Transporte do host: escuta, aceite e leitura de linhas
      altera: `src/net/socket.{hpp,cpp}`, `src/net/host_server.{hpp,cpp}`, `tests/test_net_host.cpp`
      consome: tarefa 2 (`readLines` devolve linhas cruas)
      produz: `Socket::listenTcp/closeFd/readLines/sendAll`; `HostServer::listen/acceptPending/connected/readLines/sendLine/disconnected/dropClient/shutdown/localAddresses/port`
      verifica: `make test` loopback em porta efêmera: escutar, aceitar, trocar linha, recusar segundo cliente, porta ocupada retorna erro, peer fechando marca `disconnected()`, linha acima de 512 é descartada sem derrubar a conexão
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-02, Req-04, Req-07, Req-17, Req-18

- [x] 4. Transporte do convidado e handshake entre host e guest
      altera: `src/net/guest_client.{hpp,cpp}`, `tests/test_net_guest.cpp`
      consome: tarefas 2–3
      produz: `GuestClient::connectTo/connected/sendLine/readLines/disconnected/close`
      verifica: `make test` com host e guest reais em `127.0.0.1`: conexão, `HELLO TRUCO 1` → `WELCOME 1`; versão 999 recebe `ERROR VERSION`; timeout de conexão resolve com erro sem travar; IP inválido retorna erro
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-03, Req-04, Req-06

- [x] 5. Telas de rede no renderer
      altera: `src/ui/render.{hpp,cpp}`, `src/app/session.cpp`, `tests/test_render.cpp`
      consome: tarefas 1–2
      produz: `ViewModel::Screen::{LanConfig,LanWaiting,LanError}`, campos `lan*`, desenho com campos/cursor, endereços, erro e botões
      verifica: `make test` com snapshots 80x24 das três telas: largura ≤ 80, campos e valores visíveis, hit targets de botões, nenhuma carta desenhada
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-01, Req-05, Req-20, Req-21, Req-23

- [x] 6. Sessão LanHost: menu, configuração, espera, handshake e início
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session_lan_host.cpp`
      consome: tarefas 1–5
      produz: `GameMode::{LanHost,LanGuest}`, `SessionState::{LanConfig,LanWaiting,LanError}`, menu de 5 ações, `openLanConfig`, `confirmLanConfig`, `cancelLan`, `startMatchAndBroadcast`, `broadcastSync`, `broadcastMessage`, `makeSync`, `pumpHost`
      verifica: `make test` com host real em porta efêmera: menu escolhe hospedar, porta inválida mostra erro, escuta e exibe endereços, aceita convidado, valida handshake, inicia partida como J1 e envia `SYNC` com apenas a mão do host
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-01, Req-02, Req-05, Req-06, Req-08

- [x] 7. Sessão LanHost: intenções do convidado e fluxo completo
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session_lan_host.cpp`
      consome: tarefa 6
      produz: `applyGuestIntent`, fases `PLAY/TRUCO_ANSWER/ELEVEN/HANDEND/MATCHEND`, broadcasts após cada mutação, tratamento de desconexão e `REMATCH`
      verifica: `make test` com host e guest reais: carta fora do turno e repetida rejeitadas sem mudar estado; `TRUCO` do guest abre resposta do host; `ACCEPT`/`RUN` corretos; mão de 11 do guest aplicada; `REMATCH` só após fim; desconexão leva a `LanError`
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-11, Req-12, Req-13, Req-14, Req-15, Req-16, Req-17, Req-19

- [x] 8. Sessão LanGuest: conexão, espelho e intenções
      altera: `src/app/session.{hpp,cpp}`, `tests/test_session_lan_guest.cpp`
      consome: tarefas 6–7
      produz: conexão por IP/porta, aplicação de `SYNC`/`MSG`/`ERROR`, `sendIntent`, render a partir de `SyncState`, `LanError` em queda
      verifica: `make test` com guest real contra host real: SYNC atualiza placar/vira/mão própria; mão adversária só aparece com `revealed=1`; ações enviadas apenas no turno/fase corretos; queda mostra `LanError`; rematch envia `REMATCH`
      passos:
        1. testes (RED)
        2. confirmar falha
        3. implementar (GREEN)
        4. `make test` verde
      rastreia: Req-09, Req-10, Req-11, Req-14, Req-16, Req-17, Req-22

- [x] 9. Partida completa em loopback
      altera: `tests/test_lan_match.cpp`
      consome: tarefas 6–8
      produz: teste de integração de host e guest no mesmo processo, dirigidos por `tick`, até `MATCHEND`
      verifica: `make test` com partida completa: ambos chegam a `MatchResult`, placares idênticos dos dois lados, pelo menos uma mão resolvida e ações de rematch disponíveis
      passos:
        1. escrever teste de integração (RED se algo faltar)
        2. rodar e confirmar
        3. corrigir o que faltar (GREEN)
        4. `make test` verde
      rastreia: Critérios de Aceite (partida completa, privacidade, desconexão indireta)

- [x] 10. README de LAN
      altera: `README.md`
      consome: tarefas 1–9
      produz: seção "Jogar na rede local" (host, IP, porta, firewall, privacidade, limitações)
      verifica: comandos e passos do README executados em duas instâncias `make run`; `make clean && make && make test` exit 0
      passos: reduzido (documentação) — justificativa: sem lógica testável
      rastreia: Req-01–Req-05

- [x] 11. Auditoria de diff
      altera: —
      consome: commits das tarefas 1–10
      produz: relatório de conformidade escopo × diff
      verifica: `git diff --stat` do intervalo comparado com a união dos `altera:`; arquivos fora do previsto listados e justificados; `make clean && make && make test` final
      passos: reduzido (auditoria) — justificativa: inspeção, sem código novo
      rastreia: constitution §6

## Cobertura

| Requisito | Tarefas |
|---|---|
| Req-01 | 5, 6, 10 |
| Req-02, Req-04 | 3, 6 |
| Req-03 | 1, 4, 8 |
| Req-05 | 5, 6 |
| Req-06 | 2, 4, 6 |
| Req-07 | 3 |
| Req-08 | 6 |
| Req-09, Req-10 | 2, 8 |
| Req-11 | 7, 8 |
| Req-12, Req-13 | 7 |
| Req-14 | 7, 8 |
| Req-15, Req-16 | 7, 8 |
| Req-17 | 3, 7, 8 |
| Req-18, Req-19 | 2, 3, 7 |
| Req-20–Req-23 | 5, 8 |
| Critérios de Aceite | 9, 10, 11 |
