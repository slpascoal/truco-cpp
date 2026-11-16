# Truco Mineiro

Jogo completo de **truco mineiro** para o terminal, em C++17, sem nenhuma dependência externa.
Baralho de 40 cartas, manilha fixa, mão de 11, mão de ferro, truco/seis/nove/doze e uma CPU
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
- **Battle Royale online (LAN):** de 3 a 13 jogadores na mesma rede, todos com 12 pontos de vida;
  quem chega a 0 é eliminado e vence quem sobrar. Detalhes abaixo.

## Battle Royale online (LAN)

1. No host: escolha **Hospedar Battle Royale**, defina o teto de jogadores (3 a 13) e a porta,
   depois **Iniciar** com pelo menos 3 conectados. A partida começa para todos os conectados.
2. Nos demais computadores: **Conectar Battle Royale**, informe IP e porta.
3. Cada jogador recebe 3 cartas; a mesa joga uma carta por vaza na ordem dos assentos e a maior
   carta leva a vaza (manilhas fixas do truco mineiro).
4. **Pontos:** todos começam com 12. Correr de um pedido custa o valor pedido; perder a mão custa o
   valor final da mão. Ao chegar a 0 ou menos, o jogador é eliminado.
5. **Truco:** quem pede desafia todos os adversários em ordem. Quem aceita continua e passa a poder
   aumentar; quem corre paga e sai da mão. O pedinte não aumenta de novo.
6. **Fim da mão:** vence quem ganhar mais vazas; empate é desempatado pela maior carta jogada e,
   se persistir, pela mão. Todos os demais na mão pagam o valor final.
7. **Desconexão:** elimina o jogador sem descontar pontos; os demais seguem. Sem reconexão.
8. O host é a autoridade: clientes só enviam intenções. Cada jogador vê apenas a própria mão; as
   demais são reveladas ao correr, ao ser eliminado ou no fim da mão.

## Jogar na rede local (LAN)

1. No computador que vai hospedar: escolha **LAN → Hospedar 2 jogadores**, confirme a porta (padrão
   `4000`) e leia na tela o **IP local** e a porta.
2. No outro computador, na mesma rede: escolha **LAN → Conectar 2 jogadores**, digite o IP do host,
   use `Tab` para editar a porta e confirme com `Enter`.
3. O host é o **Jogador 1** e é a autoridade da partida; o convidado é o **Jogador 2**.
4. Cada lado recebe **apenas a própria mão**. A mão do adversário só é revelada no fim da mão ou em
   corrida de truco.
5. Se a conexão cair, os dois lados mostram **Conexao encerrada** e voltam ao menu. Não há reconexão.

Observações: rede local confiável, **sem criptografia**; libere a porta no firewall do host se
necessário. A partida começa assim que o convidado conecta e o handshake de versão é validado.

## Requisitos

- **Linux/POSIX:** terminal ANSI 256 cores (xterm, kitty, alacritty, GNOME Terminal, tmux...),
  `g++` com suporte a C++17 e `make`.
- **Windows 10 (1809+) ou 11:** Windows Terminal ou o console padrão, com `g++` do MinGW-w64 e
  `make` — por exemplo [MSYS2](https://www.msys2.org/) com
  `pacman -S mingw-w64-ucrt-x86_64-gcc make`, compilando pelo shell "UCRT64".
- Nenhuma biblioteca extra em nenhuma das plataformas.

## Build e execução

```bash
make          # compila o binário ./truco (truco.exe no Windows)
make run      # compila e executa
make test     # compila e roda a suíte de testes
make clean    # remove build/ e o binário
```

No Windows o `truco.exe` é ligado estaticamente: pode ser copiado e executado em outra máquina
sem instalar o MSYS2. Também dá para gerar o `.exe` a partir do Linux com o cross-compilador:

```bash
make clean && make CXX=x86_64-w64-mingw32-g++-posix OS=Windows_NT
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
| Menu | botões | `1` CPU, `2` hotseat, `3` LAN, `4` sair; `Enter` na opção; `Esc` volta da subpágina LAN |
| Editar IP/porta | botões | dígitos/`.`/`:`, `Backspace`, `Tab` alterna campo, `Enter` confirma, `Esc` volta |
| Confirmar troca de jogador | botão **Continuar** | `Enter` ou `1` |
| Sair | — | `Q` ou `Ctrl+C` |

A interface usa mouse tracking SGR. Se o terminal não reportar mouse, o jogo avisa e continua
jogável só pelo teclado. Tamanho mínimo: **80x24**; abaixo disso o jogo pede para aumentar a janela.

## Regras implementadas

- **Baralho:** 40 cartas (`4,5,6,7,Q,J,K,A,2,3` × `♦ ♠ ♥ ♣`), sem 8, 9 e 10.
- **Manilha fixa:** são manilhas, da mais forte para a mais fraca, `4♣ > 7♥ > A♠ > 7♦`. Nenhuma
  carta é separada antes da distribuição: as seis cartas da mão vêm do baralho completo.
- **Força:** manilhas valem mais que qualquer carta comum, na ordem fixa acima. Cartas comuns
  seguem `4<5<6<7<Q<J<K<A<2<3`, e o naipe não desempata carta comum.
- **Mão:** 3 cartas cada, quem fizer duas vazas leva. Empate na primeira vaza é decidido pela
  segunda; empate na segunda vaza decide a favor de quem ganhou a primeira; três empates vão para
  quem é mão.
- **Apostas:** mão vale 1; truco → 3, seis → 6, nove → 9, doze → 12. Quem corre entrega o valor
  anterior ao adversário, que vê as cartas. Depois que um pedido é aceito, só o adversário de quem
  pediu pode aumentar; o direito alterna a cada aceite.
- **Partida:** até 12 pontos; o placar exibido trava em 12.
- **Mão de 11:** com placar exato em 11, o jogador recebe uma carta oculta e escolhe **jogar**
  (mão vale 3) ou **esconder** (mão vale 1); vence quem tiver a carta mais forte contra a melhor
  carta do adversário.
- **Mão de ferro:** ambos com 11, uma carta oculta para cada lado, vale 3 e não há truco.

A CPU só usa informação pública: as próprias cartas, as cartas jogadas e o placar. Ela
nunca acessa a mão do jogador — o tipo `PublicView` não tem campo para isso.

## Estrutura do projeto

```
src/
  core/     regras puras: cartas, baralho, vazas, apostas, partida (sem I/O)
  ai/       heurística da CPU sobre informação pública
  ui/       terminal (termios no POSIX, console VT no Windows), parser de mouse, layout e renderização
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
- No Windows é preciso um console com suporte a ANSI (Windows 10 1809+); `cmd` antigo e MSVC não são alvo.
- O jogo depende de terminal interativo; redirecionar stdin/saída padrão desativa o modo raw e o
  mouse.
