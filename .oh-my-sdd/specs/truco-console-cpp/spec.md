# Spec: Jogo de Truco Mineiro em console (jogador vs CPU)

Slug: `truco-console-cpp`
Escala: MEDIUM
Status: validado pelo usuário em 2026-11-13

## 1. Intenção e Visão Geral

Entregar um jogo de **truco mineiro completo, jogável do início ao fim**, em um único binário C++ executado no terminal, em partida de **humano contra computador**. O jogador deve poder dar as cartas com o **mouse** (clique) ou pelo teclado, ver suas cartas desenhadas como cartas de baralho no console, acompanhar placar, vira, manilhas e as apostas de truco, e sentir que está jogando uma partida real — não um protótipo.

Prioridades, nesta ordem: (1) regras corretas e auditáveis; (2) interface clara e bonita, legível em qualquer terminal ANSI; (3) IA adversária decente, que pede truco, corre e blefa; (4) código testável, com o núcleo de regras independente do terminal.

Não fazem parte desta entrega: rede/multiplayer, persistência de partidas, áudio, suporte a Windows e qualquer dependência externa.

## 2. Premissas e Ambiguidades

Premissas adotadas, todas confirmadas ou declaradas explicitamente:

- **Variante:** truco mineiro com baralho de **40 cartas** (sem 8, 9 e 10) e manilha definida pela **vira** — escolha explícita do usuário.
- **Partida até 12 pontos**, com escala de aposta 1 → 3 (truco) → 6 (seis) → 9 (nove) → 12 (doze) — declarada no pedido original.
- **Mão de 11 completa** — escolha explícita do usuário. Como "completa" varia por região, este spec fixa a variante abaixo e pede confirmação no checkpoint:
  - Jogador que atinge **exatamente 11 pontos** no início de uma mão recebe **1 carta oculta** e escolhe: **Jogar** (a mão vale **3 pontos**) ou **Esconder** (a mão vale **1 ponto**). A mão é resolvida em confronto direto de uma carta por lado; quem tiver a carta maior leva o valor apostado.
  - Se **ambos** estiverem com 11 pontos, ocorre **mão de ferro**: ambos recebem 1 carta sem vê-la, não há escolha, a mão vale **3 pontos** e **não é permitido pedir truco**.
  - Não existe opção de "passar a mão" nesta versão.
  - Se um jogador atinge 11 ou mais por qualquer valor de mão (inclusive 3), a mão de 11 não se aplica — ela só ocorre quando o placar está exatamente em 11 no começo da mão.
- **Empates de vaza ("empachou")** adotam a regra mineira usual: a primeira vaza empatada é decidida pela segunda; se a segunda também empatar, vence quem ganhou a primeira; se as três empatarem, vence quem deu as cartas (mão de quem "é mão"). Premissa explícita — o spec aceita ajuste no checkpoint.
- **Saída** do jogo só por ação explícita do jogador (tecla de saída no menu ou confirmação), nunca por fim abrupto sem restaurar o terminal.
- Nenhuma ambiguidade de implementação será resolvida em silêncio durante a execução; divergências viram pergunta.

## 3. Requisitos Funcionais (EARS/GEARS)

### 3.1 Baralho, distribuição e manilha

- **[Req-01]** O sistema **shall** usar um baralho de 40 cartas composto pelos valores `4, 5, 6, 7, Q, J, K, A, 2, 3` nos naipes `ouros, espadas, copas, paus`.
- **[Req-02]** **When** uma mão começa, o sistema **shall** embaralhar o baralho e distribuir 3 cartas para o jogador e 3 para a CPU, e sortear 1 carta como **vira**.
- **[Req-03]** **When** a vira é sorteada, o sistema **shall** definir como manilha o valor imediatamente acima da vira na sequência circular `4 → 5 → 6 → 7 → Q → J → K → A → 2 → 3 → 4`.
- **[Req-04]** **While** houver manilhas em jogo, o sistema **shall** ordená-las por naipe: `paus > copas > espadas > ouros`, e considerá-las sempre superiores a qualquer carta comum.
- **[Req-05]** O sistema **shall** ordenar cartas comuns por `4 < 5 < 6 < 7 < Q < J < K < A < 2 < 3`, independentemente do naipe.
- **[Req-06]** The system **shall not** permitir que a vira seja distribuída a um jogador.

### 3.2 Vazas, mão e placar

- **[Req-07]** **When** os dois jogadores jogam uma carta, o sistema **shall** declarar vencedor da vaza pela ordem de força e, em caso de igualdade de valor e naipe, declarar a vaza empatada.
- **[Req-08]** **When** um jogador vence duas vazas, o sistema **shall** encerrar a mão e somar ao vencedor o valor apostado.
- **[Req-09]** **If** a primeira vaza empatar, o sistema **shall** decidir a mão pela segunda vaza; **if** a segunda também empatar, o sistema **shall** dar a mão a quem venceu a primeira; **if** as três vazas empatarem, o sistema **shall** dar a mão a quem é mão (quem recebeu as cartas primeiro).
- **[Req-10]** **When** uma mão termina, o sistema **shall** exibir o resultado da mão, atualizar o placar e iniciar a mão seguinte com o "é mão" alternando entre os jogadores.
- **[Req-11]** **When** um jogador atinge ou ultrapassa 12 pontos, o sistema **shall** encerrar a partida e declarar o vencedor (placar é travado em 12 para exibição).

### 3.3 Apostas de truco

- **[Req-12]** **While** a mão estiver em andamento e o valor corrente for 1, 3, 6 ou 9, o jogador da vez **may** pedir truco (ou seis, nove, doze, conforme o valor corrente).
- **[Req-13]** **When** um jogador pede aumento de aposta, o adversário **shall** escolher entre **aceitar** (valor sobe para a escala seguinte) ou **correr** (a mão termina imediatamente e o valor corrente anterior é creditado a quem pediu).
- **[Req-14]** **When** um jogador aceita um aumento, o sistema **shall** permitir que o adversário, na sua vez, peça o próximo aumento da escala.
- **[Req-15]** **While** o valor da mão for 12, o sistema **shall not** permitir novos pedidos de aumento.
- **[Req-16]** **If** um jogador corre de um truco, o sistema **shall** mostrar as cartas de ambos os jogadores na mesma mão antes de encerrá-la.

### 3.4 Mão de 11 e mão de ferro

- **[Req-17]** **When** uma mão começa com o jogador humano exatamente em 11 pontos e a CPU abaixo de 11, o sistema **shall** dar a ele 1 carta oculta e oferecer as opções **Jogar** ou **Esconder**, exibindo o valor da mão (3 ou 1) em cada opção.
- **[Req-18]** **When** uma mão começa com a CPU exatamente em 11 pontos e o humano abaixo de 11, a CPU **shall** decidir entre Jogar e Esconder, e o sistema **shall** informar a decisão e o valor da mão ao humano.
- **[Req-19]** **When** ambos os jogadores estiverem com 11 pontos no início da mão, o sistema **shall** iniciar **mão de ferro**, com 1 carta oculta para cada lado, sem diálogo de escolha, valendo 3 pontos e sem truco.
- **[Req-20]** **When** uma mão de 11 for resolvida, o sistema **shall** comparar a carta do jogador de 11 com a melhor carta do adversário e creditar o valor apostado a quem vencer, revelando as cartas.

### 3.5 Interface e entrada

- **[Req-21]** **When** o jogo inicia, o sistema **shall** exibir uma tela de abertura com título, instruções de input e opções **Jogar** e **Sair**, acionáveis por mouse ou teclado.
- **[Req-22]** **While** a partida estiver em andamento, o sistema **shall** exibir em tela: cartas do jogador em tamanho grande e legíveis, cartas da CPU viradas para baixo, vira e manilha em destaque, placar dos dois lados, valor corrente da mão, vazas ganhas e indicador de vez.
- **[Req-23]** **When** o jogador clica sobre uma de suas cartas, o sistema **shall** jogar aquela carta, exigindo confirmação via segundo clique ou tecla de confirmação para evitar jogada acidental.
- **[Req-24]** **If** o terminal não suportar relatório de mouse, o sistema **shall** exibir aviso e continuar jogável via teclado, sem abortar.
- **[Req-25]** **While** o jogador puder agir, o sistema **shall** destacar visualmente as cartas jogáveis e o botão/ação disponível (Truco, Aceitar, Correr, Jogar, Esconder).
- **[Req-26]** **When** o jogador pressiona `1`, `2` ou `3`, o sistema **shall** selecionar a carta correspondente; **when** pressiona `Enter` ou `Espaço`, **shall** confirmar a ação selecionada; `Esc` **shall** cancelar a seleção.
- **[Req-27]** **When** a CPU joga ou decide, o sistema **shall** exibir a ação da CPU com uma pausa perceptível (250–800 ms) e mensagem em português, para que a jogada seja legível.
- **[Req-28]** **When** a partida termina, o sistema **shall** exibir tela de resultado com placar final e opções **Jogar novamente** e **Sair**.
- **[Req-29]** **While** a janela do terminal for menor que 80 colunas por 24 linhas, o sistema **shall** exibir uma mensagem pedindo redimensionamento e **shall not** renderizar a partida corrompida.
- **[Req-30]** **When** o terminal é redimensionado, o sistema **shall** redesenhar a interface no novo tamanho sem perder o estado do jogo.
- **[Req-31]** **When** o jogo encerra por qualquer caminho, inclusive `Ctrl+C`, o sistema **shall** restaurar o terminal ao estado original (cursor, modo canônico, mouse, tela alternativa).

### 3.6 Inteligência artificial

- **[Req-32]** **When** for a vez da CPU, o sistema **shall** escolher a carta com base na força das cartas, no placar parcial das vazas e na informação pública disponível (cartas já jogadas, vira, manilhas).
- **[Req-33]** **When** a CPU puder pedir truco, o sistema **shall** decidir pedir ou não com base na força da mão, podendo blefar com probabilidade limitada.
- **[Req-34]** **When** o humano pedir truco, a CPU **shall** decidir aceitar ou correr com base na força da mão e no placar, podendo aceitar mão fraca ocasionalmente para não ficar previsível.
- **[Req-35]** **When** a CPU estiver em situação de mão de 11, o sistema **shall** escolher Jogar ou Esconder pela força da carta recebida.
- **[Req-36]** The system **shall** usar apenas informação acessível à CPU (suas cartas, vira, cartas jogadas e placar), nunca as cartas do humano.

## 4. Critérios de Aceite

- [ ] `make` compila o binário sem erros e sem warnings com `-Wall -Wextra`.
- [ ] `make test` executa a suíte e retorna exit 0; suíte cobre força de cartas, manilha, resolução de vazas, empates, escala de truco, corre, mão de 11, mão de ferro e fim de partida.
- [ ] Uma partida completa do início ao fim pode ser jogada apenas com o mouse, incluindo truco, aceitar, correr e escolhas de mão de 11.
- [ ] Uma partida completa pode ser jogada apenas com teclado quando o mouse não é reportado.
- [ ] As cartas do jogador são desenhadas como cartas (moldura, valor e naipe), com naipes distinguíveis por cor e forma, não apenas texto solto.
- [ ] Vira e manilha são visíveis durante toda a mão.
- [ ] Placar, valor corrente da mão, vazas ganhas e indicador de vez estão sempre visíveis e corretos.
- [ ] Pedido de truco pela CPU e pelo humano abre diálogo com as opções corretas e valor da escala correto.
- [ ] Correr de um truco mostra as cartas e credita o valor correto.
- [ ] Ao atingir 12 pontos, a partida encerra e a tela final permite jogar novamente.
- [ ] Sair por qualquer caminho deixa o terminal utilizável, sem cursor invisível, sem mouse tracking ativo e sem modo raw.
- [ ] Em terminal 80x24 a interface cabe sem cortes; abaixo disso, mensagem de redimensionamento aparece.
- [ ] A CPU nunca joga carta que não tem e nunca acessa cartas do humano (verificado por teste de estado).
- [ ] Nenhuma dependência externa é necessária para compilar e rodar.

## 5. Requisitos Não-Funcionais e Contratos

- **Stack (constitution):** C++17, `g++`, `Makefile` com alvos `all`, `run`, `test`, `clean`. Proibido `cmake` e bibliotecas de terceiros.
- **Terminal:** ANSI escape sequences para tela alternativa, posicionamento, cores 256/16, cursor; `termios` para modo raw; `ioctl(TIOCGWINSZ)` para dimensões; mouse tracking **SGR 1006** ligado/desligado simetricamente; `SIGINT`/`SIGTERM`/`SIGWINCH` tratados.
- **Determinismo:** todo embaralhamento usa `std::mt19937` com seed explícita; a seed pode ser fixada por variável de ambiente para testes. Proibido `rand()`, `time()` como única fonte de seed em testes.
- **Arquitetura contratual:** `core/` não inclui headers de terminal nem faz I/O; `ai/` recebe apenas visão pública do estado; `ui/` não contém regra de jogo. Regras de negócio devem ser verificáveis sem TTY.
- **Estado:** jogo é uma máquina de estados explícita (menu, mão em andamento, diálogo de truco, mão de 11, resultado de mão, fim de partida). Nenhuma transição implícita.
- **Desempenho:** renderização de quadro completo abaixo de 16 ms; jogo não deve ocupar CPU parado em espera (espera por input de forma bloqueante ou com timeout curto).
- **Idioma:** UI em português do Brasil; identificadores e commits em inglês.
- **Testes:** binário próprio em `tests/`, sem framework; exit code ≠ 0 em falha; casos com seed fixa e asserções sobre estados e transições.
- **Dependências externas:** proibidas. Nenhum download em tempo de build ou execução.
- **Segurança:** nenhuma leitura/escrita de arquivo arbitrário, nenhuma execução de shell, nenhum segredo; todo índice de carta vindo do mouse/teclado validado antes de acessar vetores.

## 6. Fora de Escopo (não implementar)

- Multiplayer local ou em rede, espectadores, chat.
- Persistência de partidas, ranking, histórico em arquivo.
- Níveis de dificuldade configuráveis, temas de cor selecionáveis.
- Áudio, notificações desktop.
- Suporte a Windows, macOS sem ANSI, ou terminais sem 256 cores.
- Truco paulista, flor, ou variações de 2/4 jogadores.
