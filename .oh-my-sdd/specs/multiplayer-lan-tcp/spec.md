# Spec: Multiplayer LAN via TCP

Slug: `multiplayer-lan-tcp`
Escala: MEDIUM
Status: validado pelo usuário em 2026-11-13

## 1. Intenção e Visão Geral

Permitir que **duas pessoas joguem truco em máquinas diferentes na mesma rede local**, sem servidor externo: um jogador **hospeda** a partida, o outro **conecta pelo IP**. O host é a autoridade do jogo (Jogador 1); o convidado (Jogador 2) envia apenas suas intenções. Cada lado vê somente a própria mão; a mão alheia **nunca** trafega pela rede.

Fora de escopo: internet/WAN, NAT traversal, reconexão, espectadores, criptografia, autenticação, chat e mais de dois jogadores.

## 2. Premissas e Ambiguidades

- Decisões confirmadas pelo usuário: host joga como Jogador 1 e é autoritativo; **IP e porta editáveis** no terminal (texto digitado); queda de conexão **encerra a partida com aviso** e volta ao menu.
- Protocolo próprio, texto, uma mensagem por linha, sem dependências externas. Versão de protocolo obrigatória no handshake; versões diferentes são rejeitadas.
- Sem reconexão, sem turnos por tempo, sem espectadores. Rede local é considerada confiável; não há TLS.
- Em caso de empate técnico de resolução (mão de 11, ferro), valem as regras do motor local já implementado.
- Interpretação concorrente registrada: "porta configurável" poderia significar apenas host; aqui **host escolhe a porta ao escutar e o convidado informa IP e porta**.

## 3. Requisitos Funcionais (EARS/GEARS)

### 3.1 Menu e configuração

- **[Req-01]** **When** o jogo inicia, o sistema **shall** exibir as opções **Jogar vs CPU**, **Jogar 2 jogadores**, **Hospedar partida (LAN)**, **Conectar em partida (LAN)** e **Sair**.
- **[Req-02]** **When** o jogador escolhe hospedar, o sistema **shall** permitir editar a porta (padrão `4000`), iniciar a escuta em `0.0.0.0` e exibir o endereço IP local da máquina e a porta em uso enquanto aguarda.
- **[Req-03]** **When** o jogador escolhe conectar, o sistema **shall** permitir editar o IP do host e a porta (padrão `4000`) e tentar a conexão ao confirmar.
- **[Req-04]** **If** a porta estiver em uso, o IP for inválido ou a conexão falhar, o sistema **shall** exibir mensagem de erro clara e permanecer na tela de configuração, sem travar.
- **[Req-05]** **While** o host aguarda conexão, o sistema **shall** manter a interface responsiva, exibindo o endereço e permitindo cancelar com `Esc`/`Q`.
- **[Req-06]** **When** um cliente conecta ao host, o sistema **shall** trocar um handshake com identificador e versão de protocolo e iniciar a partida somente se a versão for compatível.
- **[Req-07]** **If** um segundo cliente tentar conectar enquanto a partida estiver ativa, o sistema **shall** recusar a conexão e encerrá-la imediatamente.

### 3.2 Partida em rede

- **[Req-08]** **When** a partida em rede começa, o host **shall** atuar como Jogador 1 e o convidado como Jogador 2, com as mesmas regras do modo hotseat, sem IA.
- **[Req-09]** **While** a partida estiver em andamento, o sistema **shall** enviar a cada lado **apenas** sua própria mão, a vira, o placar, as cartas jogadas, o valor da mão e o estado público do jogo.
- **[Req-10]** **The system shall not** transmitir ao convidado a mão do host nem ao host a mão do convidado antes da revelação de fim de mão ou corrida.
- **[Req-11]** **When** o jogador da vez seleciona e confirma uma carta, **or** pede aumento, **or** aceita/corre, **or** decide a mão de 11, o cliente **shall** enviar apenas a intenção correspondente ao host.
- **[Req-12]** **When** o host recebe uma ação, o host **shall** validar turno, posse da carta e estado da partida antes de aplicá-la; ações inválidas **shall** ser ignoradas com erro registrado e sem alterar o estado.
- **[Req-13]** **When** o estado muda, o host **shall** difundir uma atualização que permita a ambos os lados renderizar a mesma partida.
- **[Req-14]** **While** for a vez do jogador local, o sistema **shall** habilitar suas ações; **while** for a vez do adversário, **shall** desabilitar seleção e exibir indicação de espera.
- **[Req-15]** **When** uma mão termina, o sistema **shall** revelar as cartas de ambos os jogados na mesma mão, exatamente como nos modos locais, e seguir para a mão seguinte.
- **[Req-16]** **When** a partida termina, ambos os lados **shall** exibir o vencedor pelo nome (**Jogador 1** host / **Jogador 2** convidado) e permitir **Jogar novamente** (nova partida) ou **Sair**.
- **[Req-17]** **If** a conexão cair ou o adversário desconectar, o sistema **shall** exibir aviso de desconexão e encerrar a partida, retornando ao menu; o lado restante **shall not** ficar preso em estado de espera.
- **[Req-18]** **If** uma mensagem malformada, desconhecida, com campos fora de faixa ou maior que o limite de linha for recebida, o sistema **shall** descartá-la sem afetar o estado e sem encerrar o processo por erro.
- **[Req-19]** **The system shall not** aceitar ações de carta fora do turno, de carta já jogada, nem índices de carta fora de `1..3` vindos da rede.

### 3.3 Interface

- **[Req-20]** **When** o sistema estiver em uma tela de configuração de rede, o terminal **shall** exibir campo de texto editável com cursor, aceitando dígitos, ponto (IP) e dois-pontos apenas onde aplicável, além de `Backspace`, `Enter` e `Esc`.
- **[Req-21]** **When** o host estiver aguardando, o sistema **shall** exibir todos os endereços IPv4 locais não-loopback detectáveis e a porta, para o convidado digitar.
- **[Req-22]** **While** em partida de rede, a tela de jogo **shall** manter placar, vira, manilha, valor da mão, vazas, vez e a mão local, ocultando a mão remota até a revelação.
- **[Req-23]** **If** a conexão com o adversário for perdida, o sistema **shall** mostrar o motivo na tela e oferecer **Voltar ao menu**.

## 4. Critérios de Aceite

- [ ] Menu oferece CPU, hotseat, hospedar LAN, conectar LAN e sair, por mouse e teclado.
- [ ] Host escuta na porta escolhida e mostra IP(s) local(is) e porta.
- [ ] Convidado conecta por IP:porta e a partida inicia só após handshake válido.
- [ ] Versão de protocolo incompatível é rejeitada com mensagem clara.
- [ ] Porta ocupada, IP inválido e host inacessível geram mensagem de erro sem travar o jogo.
- [ ] Partida completa J1 (host) vs J2 (convidado) até 12 pontos, com truco, corre, mão de 11 e mão de ferro.
- [ ] Cada lado só recebe a própria mão; nenhuma carta alheia trafega antes da revelação (teste de protocolo).
- [ ] Ações fora do turno e cartas repetidas são rejeitadas pelo host.
- [ ] Queda de conexão encerra a partida nos dois lados e mostra aviso; nenhum lado fica preso.
- [ ] Mensagens malformadas/truncadas não derrubam o processo.
- [ ] `make test` cobre parser/serializer do protocolo, autoridade do host, ocultação de mãos, validação de ações, desconexão e partida em loopback.
- [ ] `make` sem warnings e `make test` exit 0.

## 5. Requisitos Não-Funcionais e Contratos

- **Stack:** C++17, `g++`, `Makefile`, apenas biblioteca padrão e POSIX (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `fcntl.h`, `poll.h`, `unistd.h`). Proibido OpenSSL, Boost.Asio e qualquer dependência externa.
- **Transporte:** TCP IPv4, sockets **não bloqueantes** integrados ao loop de `poll` existente; sem threads obrigatórias.
- **Protocolo:** UTF-8/ASCII, uma mensagem por linha (`\n`), campos separados por espaço, limite rígido de 512 bytes por linha; mensagens desconhecidas descartadas.
- **Autoridade:** somente o host aplica regras; o convidado nunca executa o motor de jogo como fonte de verdade.
- **Privacidade:** a serialização de estado é por lado; o campo de mão remota só é preenchido após `revealed`.
- **Robustez:** nenhum `recv`/`send` pode bloquear indefinidamente; desconexão detectada por `recv()==0` ou erro; buffers com validação de índices e sem `strcpy`.
- **Segurança:** rede local confiável, sem TLS; nenhuma execução de shell, nenhum acesso a arquivo, nenhuma credencial; IP/porta digitados são validados antes de `inet_pton`/`htons`.
- **Testes:** protocolo com parser/serializer puros; integração de rede usando dois sockets em `127.0.0.1` dentro do mesmo binário de teste; sem depender de rede externa.
- **Idioma:** UI pt-BR; identificadores e mensagens de protocolo em inglês.

## 6. Fora de Escopo

- Internet/WAN, NAT, descoberta automática, reconexão, espectadores, chat, mais de 2 jogadores.
- Criptografia, autenticação, anti-cheat além da validação de turno/estado.
- Salvamento de partidas, replays, métricas.
