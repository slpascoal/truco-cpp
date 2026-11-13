# Spec — Modo online Battle Royale de truco mineiro

## 1. Intenção e Visão Geral

Adicionar um modo **exclusivamente online (LAN)** de battle royale de truco mineiro para **3 a 13
jogadores**, hospedado por um jogador autoritativo. Diferente do truco normal (corrida até 12
pontos), cada jogador começa com **12 pontos de vida** e vai **perdendo** pontos até ser eliminado;
vence quem sobrar.

Cada jogador recebe 3 cartas. A mão é jogada em vazas com todos os jogadores ativos. Perder a mão
ou correr de um pedido de truco desconta pontos. Pedir truco desafia todos os adversários em ordem
de turno; quem corre paga o valor do pedido e sai da mão, quem aceita continua jogando. A partida
termina quando resta apenas um jogador com pontos.

## 2. Premissas e Ambiguidades

### Decisões já tomadas pelo usuário

- Jogadores: host define de **3 a 13** (máximo = 40 cartas / 3 por jogador = 13; sobra 1 carta).
- Truco: **todos os adversários respondem em ordem de turno**, um por vez.
- Pontos: **correr custa o valor do pedido** e **perder a mão custa o valor da mão**.
- Desconexão **elimina** o jogador da partida.

### Premissas necessárias (não fixadas no pedido)

- **Início:** o host define o **teto** N (3–13) e inicia manualmente quando houver **pelo menos 3**
  conectados (até N). Sem entrada após o início.
- **Assentos:** ordem de conexão; host é o assento 0. A "mão" (primeiro a jogar) gira a cada mão.
- **Vazas:** cada jogador ativo joga exatamente uma carta por vaza, na ordem de assentos. Vence a
  vaza a maior carta pela força fixa das manilhas (`4♣ > 7♥ > A♠ > 7♦` acima das comuns).
- **Empate de vaza:** se a maior força estiver repetida, a vaza não tem vencedor.
- **Vencedor da mão:** quem vencer mais vazas. Se o total de vazas empatar, vence quem tiver jogado
  a **carta de maior valor** durante a mão; se ainda assim empatar, vence a mão (primeiro a jogar).
- **Custo da mão:** ao fim da mão, todos os jogadores ainda na mão, exceto o vencedor, perdem o
  valor final da mão. Quem correu já pagou ao correr e não paga de novo.
- **Corrida:** paga o valor do pedido em disputa (3/6/9/12), sai da mão e continua na partida se
  ainda tiver pontos.
- **Direito de aumento:** depois de um pedido aceito, apenas adversários que aceitaram aquele
  pedido podem aumentar; quem pediu não pode. Novo pedido aceito reinicia o conjunto de quem pode
  aumentar.
- **Todos correm:** se nenhum adversário aceitar, a mão termina imediatamente e o pedinte vence a
  mão sem perdas adicionais.
- **Fim da mão antecipado:** se sobrar um único jogador na mão, ela termina e esse jogador vence.
- **Eliminação:** jogador chega a 0 ou menos pontos → eliminado; é ignorado nas vazas e pedidos.
- **Desconexão:** elimina sem descontar pontos; os demais seguem.
- **Fim da partida:** quando resta um jogador com pontos → vencedor. Sem timer, sem reconexão, sem
  espectadores e sem chat.

### Perguntas em aberto (resolvidas nesta validação)

- Empate de vazas: vence quem jogou a maior carta na mão; se persistir, vence a mão.
- Início: manual pelo host, com pelo menos 3 conectados e no máximo N.

## 3. Requisitos Funcionais (EARS/GEARS)

### Lobby e conexão

- **R1** — **When** o host criar uma sala battle royale, o sistema **shall** permitir escolher o
  teto N entre 3 e 13 e exibir IP e porta.
- **R2** — **When** um cliente se conectar, o sistema **shall** atribuir um assento na ordem de
  conexão e exibi-lo no lobby do host.
- **R3** — **When** o host iniciar a partida com pelo menos 3 jogadores conectados, o sistema
  **shall** começar a partida com todos os conectados (no máximo N).
- **R4** — **If** um cliente tentar entrar após o início, o sistema **shall** recusar a entrada.
- **R5** — **If** um jogador desconectar durante a partida, o sistema **shall** eliminá-lo sem
  descontar pontos e continuar a partida.

### Estado de partida

- **R6** — O sistema **shall** iniciar cada jogador com 12 pontos de vida.
- **R7** — **When** uma mão começar, o sistema **shall** distribuir 3 cartas para cada jogador
  ativo a partir do baralho de 40 e usar as manilhas fixas do truco mineiro.
- **R8** — **While** a mão estiver em andamento, o sistema **shall** fazer cada jogador ativo jogar
  uma carta por vaza, na ordem de assentos.
- **R9** — **When** todos os jogadores ativos jogarem na vaza, o sistema **shall** resolvê-la pela
  maior carta e registrar a vaza para o vencedor, ou sem vencedor em caso de empate.
- **R10** — **When** a mão terminar, o sistema **shall** eleger o vencedor por vazas ganhas
  (desempate pela maior carta jogada na mão e, se persistir, pela mão) e descontar o valor final da
  mão de todos os demais jogadores ainda na mão.
- **R11** — **If** um jogador atingir 0 pontos ou menos, o sistema **shall** eliminá-lo da partida.
- **R12** — **When** restar apenas um jogador com pontos, o sistema **shall** encerrar a partida e
  declará-lo vencedor.

### Truco

- **R13** — **When** for a vez de um jogador e ele ainda tiver direito de aumento, o sistema
  **shall** permitir pedir truco/seis/nove/doze, elevando o valor da mão para 3/6/9/12.
- **R14** — **When** um jogador pedir truco, o sistema **shall** perguntar a cada adversário ainda
  na mão, em ordem de turno, se aceita ou corre.
- **R15** — **When** um adversário correr, o sistema **shall** descontar o valor do pedido, retirá-lo
  da mão e mantê-lo na partida se ainda tiver pontos.
- **R16** — **When** um adversário aceitar, o sistema **shall** mantê-lo na mão e no conjunto de
  jogadores que podem aumentar depois.
- **R17** — **If** o jogador que pediu tentar aumentar novamente antes de um aceitante aumentar, o
  sistema **shall** recusar.
- **R18** — **If** todos os adversários correrem, o sistema **shall** encerrar a mão e declarar o
  pedinte vencedor sem cobrança adicional.
- **R19** — **While** um pedido estiver em disputa, o sistema **shall** bloquear jogadas até todas
  as respostas serem coletadas.
- **R20** — **If** restar um único jogador na mão após corridas, o sistema **shall** encerrá-la e
  declará-lo vencedor.

### Interface e rede

- **R21** — O sistema **shall** exibir, para cada jogador conectado, assento, pontos de vida,
  situação (ativo/eliminado) e carta jogada na vaza quando houver.
- **R22** — O sistema **shall** mostrar a cada jogador apenas a própria mão; cartas alheias só são
  reveladas ao fim da mão ou quando o jogador corre/é eliminado.
- **R23** — O sistema **shall** ser autoritativo no host: clientes apenas enviam intenções de
  jogar, pedir e responder.
- **R24** — O sistema **shall** operar apenas em LAN, sem internet/WAN, criptografia ou reconexão.
- **R25** — O sistema **shall** manter o modo 2 jogadores e a CPU existentes sem alteração de regra.

## 4. Critérios de Aceite

- [ ] Host cria sala com teto N de 3 a 13; lobby lista jogadores e assentos; partida inicia quando
      o host manda, com 3 a N conectados.
- [ ] Entrada após início é recusada.
- [ ] Cada jogador começa com 12 pontos e 3 cartas; 39 cartas no máximo são usadas.
- [ ] Todos jogam uma carta por vaza, na ordem de assentos; maior carta leva, manilhas fixas valem mais.
- [ ] Vaza com maior força repetida não tem vencedor.
- [ ] Fim da mão: vencedor por número de vazas; empate desempatado pela maior carta jogada e, se
      persistir, pela mão; todos os demais na mão perdem o valor final.
- [ ] Correr de um truco desconta o valor pedido, sai da mão e não elimina se ainda houver pontos.
- [ ] Quem pediu não aumenta de novo; apenas quem aceitou pode aumentar.
- [ ] Se todos correm, o pedinte vence a mão sem cobrança extra.
- [ ] Jogador com 0 ou menos é eliminado e ignorado nas vazas seguintes.
- [ ] Desconexão elimina sem descontar pontos e a partida continua.
- [ ] Partida termina com um sobrevivente, declarado vencedor.
- [ ] Cada cliente vê apenas a própria mão até a revelação.
- [ ] Modo LAN de 2 jogadores continua funcionando.
- [ ] `make` e `make test` passam com a suíte ampliada.

## 5. Requisitos Não-Funcionais e Contratos

- **Stack:** C++17, `g++`, `Makefile`, zero dependências externas (constitution §2).
- **Camadas:** novo motor battle royale em `src/core` sem I/O; host TCP autoritativo em `src/net`;
  UI em `src/ui`; máquina de estados em `src/app` (constitution §3).
- **Rede:** protocolo versionado e distinto do modo 2 jogadores; mensagens de linha limitadas a
  `kMaxLine`; validação de índices, assentos e fases vindos da rede.
- **Capacidade:** 3–13 jogadores; 40 cartas / 3 = 13 assentos e 1 carta não usada.
- **Segurança:** rede local confiável, sem TLS; nenhuma escrita de credencial; restaurar terminal
  em qualquer saída (constitution §4).
- **Testes:** motor de partida N jogadores, ordem de turno, empates, corridas, eliminações e fim de
  partida cobertos por testes unitários; integração host/convidado em socket local.
- **Compatibilidade:** modos CPU, hotseat e LAN 2 jogadores preservam comportamento e testes atuais.
