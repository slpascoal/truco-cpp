# Spec: Multiplayer local hotseat (dois humanos no mesmo terminal)

Slug: `multiplayer-local-hotseat`
Escala: SMALL
Status: validado pelo usuário em 2026-11-13

## 1. Intenção e Visão Geral

Permitir que **duas pessoas joguem truco no mesmo terminal**, alternando turnos, sem rede e sem segundo processo. O modo CPU atual continua idêntico. O ponto crítico é **privacidade das mãos**: antes de cada jogador agir, uma tela de troca ("passe o controle") oculta tudo e exige confirmação do próximo jogador, para que ninguém veja a mão do outro.

## 2. Premissas e Ambiguidades

- Hotseat significa mesma máquina e mesmo terminal; dois processos/sockets ficam para a feature de LAN.
- O motor existente é reaproveitado: Jogador 1 = lado `Human`, Jogador 2 = lado `Cpu`; a IA não é consultada em partidas hotseat.
- Cartas do adversário continuam ocultas durante a mão; ambas são reveladas apenas no fim da mão ou em corrida de truco.

## 3. Requisitos Funcionais (EARS/GEARS)

- **[Req-01]** **When** o jogo inicia, o sistema **shall** exibir no menu as opções **Jogar vs CPU**, **Jogar 2 jogadores** e **Sair**.
- **[Req-02]** **When** o jogador escolhe 2 jogadores, o sistema **shall** iniciar uma partida hotseat com os nomes **Jogador 1** e **Jogador 2**.
- **[Req-03]** **When** uma partida hotseat inicia ou um turno muda de dono, o sistema **shall** exibir uma tela de troca com o nome do próximo jogador e ocultar todas as cartas até a confirmação.
- **[Req-04]** **When** o jogador da vez confirma a tela de troca, o sistema **shall** revelar apenas a mão dele e habilitar suas ações.
- **[Req-05]** **While** for a vez do Jogador 1 ou do Jogador 2, o sistema **shall** aceitar clique/teclas para selecionar e confirmar carta, pedir truco, aceitar ou correr, exatamente como no modo CPU.
- **[Req-06]** **When** um jogador humano pede truco, o sistema **shall** exibir a tela de troca para o adversário antes de mostrar o diálogo de aceitar/correr.
- **[Req-07]** **When** um dos lados estiver na mão de 11, o sistema **shall** mostrar a tela de troca para o dono da decisão e oferecer **Jogar (3)** ou **Esconder (1)**.
- **[Req-08]** **When** ambos estiverem com 11, o sistema **shall** executar mão de ferro sem escolha, revelando as cartas apenas na resolução.
- **[Req-09]** **When** a mão termina, o sistema **shall** exibir o vencedor pelo nome (Jogador 1/Jogador 2), o valor creditado e o placar.
- **[Req-10]** **When** a partida termina, o sistema **shall** declarar o vencedor pelo nome e oferecer **Jogar novamente** e **Sair**.
- **[Req-11]** **While** uma partida hotseat estiver em andamento, o sistema **shall not** consultar a IA nem exibir textos de "computador".
- **[Req-12]** **When** o jogador escolhe Jogar vs CPU, o sistema **shall** manter o comportamento atual, incluindo a IA, sem telas de troca.

## 4. Critérios de Aceite

- [ ] Menu oferece CPU, 2 jogadores e Sair, acionáveis por mouse e teclado.
- [ ] Partida hotseat completa do início ao fim, com alternância de turnos e vencedor nomeado.
- [ ] Tela de troca aparece antes de cada turno do jogador que ainda não viu a própria mão e bloqueia ações até confirmação.
- [ ] Nenhuma carta da mão do adversário aparece antes da confirmação/tela de troca.
- [ ] Truco, aceitar/correr e mão de 11 funcionam entre os dois humanos.
- [ ] Modo CPU continua passando na suíte existente, sem telas de troca.
- [ ] `make test` cobre alternância, handoff, truco entre humanos, mão de 11 e partida completa hotseat.
- [ ] `make` sem warnings e `make test` exit 0.
