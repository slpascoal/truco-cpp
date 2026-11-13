# Truco Mineiro — console C++ (você vs computador)

Jogo completo de **truco mineiro** para o terminal, em C++17, sem nenhuma dependência externa.
Baralho de 40 cartas, manilha pela vira, mão de 11, mão de ferro, truco/seis/nove/doze e uma CPU
que pede truco, corre e blefa.

```
+-------+  +-------+  +-------+
|3      |  |7      |  |A      |
|       |  |       |  |       |
|   ♣   |  |   ♥   |  |   ♠   |
|       |  |       |  |       |
|      3|  |      7|  |      A|
+-------+  +-------+  +-------+
```

## Modos de jogo

- **Jogar vs CPU:** partida contra a heurística do computador, com truco, corre e blefe.
- **Jogar 2 jogadores (hotseat):** dois humanos no mesmo terminal, alternando turnos. Antes de
  cada turno (e antes de responder truco ou decidir a mão de 11) aparece a tela
  **"Passe o controle para Jogador N"**; nenhuma carta é desenhada até a confirmação. Ao final da
  mão as cartas são reveladas, como em uma mesa real.
- **Jogar na rede local (LAN):** um jogador **hospeda** e o outro **conecta pelo IP**. Detalhes abaixo.

## Jogar na rede local (LAN)

1. No computador que vai hospedar: escolha **Hospedar partida (LAN)**, confirme a porta (padrão `4000`) e
   leia na tela o **IP local** e a porta.
2. No outro computador, na mesma rede: escolha **Conectar em partida (LAN)**, digite o IP do host,
   use `Tab` para editar a porta e confirme com `Enter`.
3. O host é o **Jogador 1** e é a autoridade da partida; o convidado é o **Jogador 2**.
4. Cada lado recebe **apenas a própria mão**. A mão do adversário só é revelada no fim da mão ou em
   corrida de truco.
5. Se a conexão cair, os dois lados mostram **Conexao encerrada** e voltam ao menu. Não há reconexão.

Observações: rede local confiável, **sem criptografia**; libere a porta no firewall do host se
necessário. A partida começa assim que o convidado conecta e o handshake de versão é validado.

## Requisitos

- Linux/POSIX com terminal ANSI 256 cores (xterm, kitty, alacritty, GNOME Terminal, tmux...).
- `g++` com suporte a C++17 e `make`. Nenhuma biblioteca extra.

## Build e execução

```bash
make          # compila o binário ./truco
make run      # compila e executa
make test     # compila e roda a suíte de testes
make clean    # remove build/ e o binário
```

Seed fixa (útil para reproduzir uma partida e para depuração):

```bash
TRUCO_SEED=42 ./truco
```

## Controles

| Ação | Mouse | Teclado |
|---|---|---|
| Escolher carta | 1.º clique seleciona, 2.º clique na mesma carta confirma | `1` `2` `3` selecionam, `Enter` ou `Espaço` confirma |
| Cancelar seleção | — | `Esc` |
| Pedir truco / seis / nove / doze | botão **Truco!** | `2` na tela de vez (quando disponível) |
| Aceitar / correr | botões do diálogo | `1` aceita, `2` corre |
| Menu | botões | `1` CPU, `2` hotseat, `3` hospedar LAN, `4` conectar LAN, `5` sair; `Enter` na opção |
| Editar IP/porta | botões | dígitos/`.`/`:`, `Backspace`, `Tab` alterna campo, `Enter` confirma, `Esc` volta |
| Confirmar troca de jogador | botão **Continuar** | `Enter` ou `1` |
| Sair | — | `Q` ou `Ctrl+C` |

A interface usa mouse tracking SGR. Se o terminal não reportar mouse, o jogo avisa e continua
jogável só pelo teclado. Tamanho mínimo: **80x24**; abaixo disso o jogo pede para aumentar a janela.

## Regras implementadas

- **Baralho:** 40 cartas (`4,5,6,7,Q,J,K,A,2,3` × `♦ ♠ ♥ ♣`), sem 8, 9 e 10.
- **Vira e manilha:** a manilha é o valor imediatamente acima da vira, em ciclo
  (`4→5→6→7→Q→J→K→A→2→3→4`). A vira nunca é distribuída.
- **Força:** manilhas valem mais que qualquer carta comum, ordenadas por naipe
  `paus > copas > espadas > ouros`. Cartas comuns seguem `4<5<6<7<Q<J<K<A<2<3`, e o naipe não
  desempata carta comum.
- **Mão:** 3 cartas cada, quem fizer duas vazas leva. Empate na primeira vaza é decidido pela
  segunda; empate na segunda vaza decide a favor de quem ganhou a primeira; três empates vão para
  quem é mão.
- **Apostas:** mão vale 1; truco → 3, seis → 6, nove → 9, doze → 12. Quem corre entrega o valor
  anterior ao adversário, que vê as cartas.
- **Partida:** até 12 pontos; o placar exibido trava em 12.
- **Mão de 11:** com placar exato em 11, o jogador recebe uma carta oculta e escolhe **jogar**
  (mão vale 3) ou **esconder** (mão vale 1); vence quem tiver a carta mais forte contra a melhor
  carta do adversário.
- **Mão de ferro:** ambos com 11, uma carta oculta para cada lado, vale 3 e não há truco.

A CPU só usa informação pública: as próprias cartas, a vira, as cartas jogadas e o placar. Ela
nunca acessa a mão do jogador — o tipo `PublicView` não tem campo para isso.

## Estrutura do projeto

```
src/
  core/     regras puras: cartas, baralho, vazas, apostas, partida (sem I/O)
  ai/       heurística da CPU sobre informação pública
  ui/       terminal (termios/ANSI), parser de mouse, layout e renderização
  app/      máquina de estados e loop de eventos
  main.cpp  composição do processo
tests/      suíte de testes com asserts, sem framework
.oh-my-sdd/ spec, plan, tasks e constitution da feature
```

## Testes

`make test` roda cerca de 1300 verificações cobrindo força das cartas, manilha, distribuição, vazas,
empates, escala de truco, corre, mão de 11, mão de ferro, IA, parser de mouse SGR/X10, layout,
renderização, resize e uma partida completa simulada até 12 pontos.

## Limitações

- A LAN é local (mesma rede), sem internet/WAN, TLS, reconexão, espectadores ou chat.
- Não há persistência de partidas, áudio ou configuração de dificuldade.
- Windows não é suportado.
- O jogo depende de terminal interativo; redirecionar stdin/saída padrão desativa o modo raw e o
  mouse.
