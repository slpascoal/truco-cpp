# Spec — Regras do truco mineiro: manilha fixa e alternância de truco

## 1. Intenção e Visão Geral

O jogo se apresenta como **truco mineiro**, mas hoje usa a manilha dinâmica do truco paulista
(uma carta virada define a manilha) e permite que o mesmo jogador aumente a aposta várias vezes
seguidas enquanto o adversário aceita. Esta feature alinha o jogo às regras do truco mineiro:

1. **Sem vira**: as manilhas são fixas (`4♣ > 7♥ > A♠ > 7♦`), o baralho é distribuído inteiro e
   nenhuma carta virada é exibida.
2. **Alternância de truco**: depois que um jogador pede truco (ou um aumento) e o adversário
   aceita, somente o adversário pode pedir o próximo aumento. Quem pediu não pode pedir de novo
   na mesma mão.

O resultado observável é uma partida com as cartas fortes corretas do truco mineiro e uma
sequência de apostas justa, sem "pedir truco em sequência contra quem aceitou".

## 2. Premissas e Ambiguidades

- **Manilhas fixas**: `4♣` (mais forte) > `7♥` > `A♠` > `7♦`. Decisão explícita do usuário.
- **Ordem do truco**: truco → seis → nove → doze permanece inalterada; o doze continua sendo o
  teto.
- **Turno ao aumentar**: não é exigido jogar uma carta entre o aceite e o próximo pedido; a
  restrição é apenas de **lado** (quem tem direito de pedir). O pedido continua só sendo possível
  na vez do jogador, como hoje.
- **LAN**: host e convidado devem rodar a mesma versão (handshake de versão já existente). O
  estado sincronizado deixa de carregar a vira e passa a carregar se o jogador local pode pedir
  truco.
- Nenhuma ambiguidade em aberto após as decisões acima.

## 3. Requisitos Funcionais (EARS/GEARS)

### Manilha fixa

- **R1** — O sistema **shall** considerar manilhas fixas, em ordem decrescente de força:
  `4♣`, `7♥`, `A♠`, `7♦`.
- **R2** — O sistema **shall** fazer toda manilha superar qualquer carta comum.
- **R3** — **When** duas manilhas se enfrentam, o sistema **shall** decidir pela ordem fixa
  `4♣ > 7♥ > A♠ > 7♦`.
- **R4** — O sistema **shall** manter a ordem das cartas comuns `4 < 5 < 6 < 7 < Q < J < K < A < 2 < 3`
  e **shall not** desempatar cartas comuns pelo naipe.
- **R5** — **When** distribuir uma mão, o sistema **shall** sortear 3 cartas para cada jogador a
  partir do baralho completo de 40 cartas, sem separar vira.
- **R6** — O sistema **shall not** exibir vira nem texto de manilha derivada de vira na interface
  do jogo (CPU, hotseat e LAN).
- **R7** — O sistema **shall not** enviar nem receber o campo de vira no estado sincronizado da
  partida LAN.
- **R8** — O sistema **shall** calcular a mão de 11 e a mão de ferro com a mesma força fixa de
  carta usada nas vazas.

### Alternância de truco

- **R9** — **When** um jogador pede truco/seis/nove/doze e o adversário aceita, o sistema
  **shall** permitir o próximo aumento somente ao adversário (quem aceitou).
- **R10** — **If** o jogador que fez o último pedido aceito tentar pedir novamente na mesma mão,
  o sistema **shall** recusar o pedido (`canCallTruco` falso e exceção no núcleo).
- **R11** — **While** um jogador não tiver o direito de aumentar, o sistema **shall** ocultar ou
  desabilitar a ação "Truco!" para ele em todos os modos.
- **R12** — **When** o adversário aumenta e o outro aceita, o sistema **shall** inverter o direito
  de aumento para o lado oposto, mantendo a alternância até o doze.
- **R13** — **When** uma nova mão começa, o sistema **shall** restaurar o direito de pedir truco
  para os dois lados.
- **R14** — O sistema **shall** aplicar a alternância também à CPU e ao convidado LAN, que não
  podem pedir fora do seu direito.
- **R15** — **When** o adversário corre, o sistema **shall** encerrar a mão e pontuar o autor do
  último pedido, sem alterar a alternância.

## 4. Critérios de Aceite

- [ ] `4♣` vence `7♥`, que vence `A♠`, que vence `7♦`, que vence qualquer carta comum.
- [ ] Nenhuma tela, mensagem ou sincronização LAN menciona ou desenha vira/manilha dinâmica.
- [ ] Mão é distribuída do baralho de 40 sem carta separada; 6 cartas distintas por mão.
- [ ] Após "truco" aceito por B, A não consegue pedir "seis"; B consegue.
- [ ] Após "seis" pedido por B e aceito por A, A consegue pedir "nove" e B não.
- [ ] A sequência continua alternando até "doze", que não pode ser aumentado.
- [ ] O botão "Truco!" não aparece para quem perdeu o direito em CPU, hotseat e LAN.
- [ ] CPU nunca pede truco quando não tem o direito.
- [ ] Nova mão libera o direito para ambos os lados.
- [ ] `make` e `make test` passam com a suíte atualizada.

## 5. Requisitos Não-Funcionais e Contratos

- **Stack**: C++17, `g++`, `Makefile`; nenhuma dependência externa (constitution §2).
- **Camadas**: regra de força e alternância vivem em `src/core` (sem I/O); UI apenas reflete o
  estado; IA decide apenas com `PublicView` (constitution §3).
- **Força de carta**: função pura de `Card`, sem parâmetro de vira; contratos públicos do núcleo
  atualizados sem manter sobrecargas antigas (evita caminho duplicado de regra).
- **Rede**: estado sincronizado perde o campo vira e ganha um indicador de permissão de truco para
  o jogador local; handshake de versão do protocolo deve impedir pareamento entre builds
  incompatíveis.
- **Testes**: toda regra nova/alterada coberta em `make test`, incluindo teste que reproduz o bug
  de pedido repetido antes da correção (constitution §7).
- **UI**: textos visíveis em pt-BR (constitution §3).
- **Persistência/migração**: não aplicável; não há dados salvos.
