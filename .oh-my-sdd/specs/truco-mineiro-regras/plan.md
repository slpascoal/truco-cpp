# Plan — Regras do truco mineiro: manilha fixa e alternância de truco

## 1. Decisões arquiteturais

- **Força de carta como função pura de `Card`.** A manilha deixa de depender da vira; `cardStrength(Card)`
  passa a ser o único caminho de comparação. Remover o parâmetro `Rank vira` de todas as assinaturas
  evita caminho duplicado e impede que uma vira esquecida continue influenciando regra.
- **Baralho sem vira.** `Deal` perde o campo `vira`; o sorteio usa as seis primeiras posições do
  baralho embaralhado (0..2 humano, 3..5 CPU). Nenhuma carta é reservada.
- **Alternância de aumento no núcleo.** `Game` guarda quem fez o último pedido (`raiser_`) e se já
  houve pedido (`hasRaiser_`). `canCallTruco` é a única autoridade: nega o lado do último pedido.
  Sessão, IA e convidado LAN consultam essa função, sem duplicar regra.
- **Rede propaga permissão, não regra.** `SyncState` perde `vira` e ganha `canRaise`, calculado pelo
  host com `canCallTruco(viewer)`. Convidado apenas exibe a ação.
- **UI remove vira.** `ViewModel` perde `vira` e `manilhaText`; layout perde o retângulo da vira;
  render perde o bloco "VIRA"/"MANILHA".
- **Versão de protocolo 2.** O formato do `SYNC` muda; o handshake existente passa a exigir "2",
  bloqueando pareamento entre builds incompatíveis.

## 2. Esquema de dados e contratos

### Núcleo (`src/core`)

```cpp
// card.hpp — remove manilhaRank(Rank) e isManilha(Card, Rank)
bool isManilha(Card c);
int cardStrength(Card c); // 7♦=10, A♠=11, 7♥=12, 4♣=13; comuns 0..9

// deck.hpp
struct Deal { std::array<Card, 3> human; std::array<Card, 3> cpu; };
Deal deal(std::mt19937& rng);

// hand.hpp
TrickOutcome trickWinner(Card leader, Card follower);

// game.hpp — assinaturas públicas inalteradas
bool canCallTruco(Side side) const;
void callTruco(Side side);
void respondTruco(Side responder, bool accept);
```

### IA (`src/ai`)

```cpp
// PublicView perde `Card vira`
bool shouldPlayHandOfEleven(Card hidden, std::mt19937& rng); // sem Rank vira
```

### Rede (`src/net`)

```cpp
struct SyncState {
    // ... campos atuais, sem `Card vira`
    bool canRaise = false;  // ocupa a posição que era da vira no token SYNC
};
```
- `SYNC` continua com 23 campos: `f[10]` passa de carta da vira para `"0"`/`"1"` de `canRaise`.
- `HELLO`/`WELCOME` passam a exigir versão `"2"`.

### UI (`src/ui`)

```cpp
// render.hpp: ViewModel sem `std::optional<Card> vira` e sem `std::string manilhaText`
// layout.hpp: Layout sem `Rect vira`
```

## 3. Global Constraints

Herdadas literalmente do spec e da constitution:

- C++17, `g++`, `Makefile`; **nenhuma** dependência externa.
- `core/` sem I/O; UI sem regra de negócio; IA só com `PublicView`.
- Textos visíveis em pt-BR; identificadores, comentários e commits em inglês.
- Manilhas: `4♣ > 7♥ > A♠ > 7♦`, acima de qualquer comum.
- Comuns: `4 < 5 < 6 < 7 < Q < J < K < A < 2 < 3`; naipe não desempata comum.
- Apostas: truco → 3, seis → 6, nove → 9, doze → 12; doze é teto.
- Direito de aumento: só o adversário de quem pediu por último.
- Gate final: `make` e `make test` verdes com saída fresca.

## 4. Interfaces por tarefa

| Tarefa | Consome | Produz |
|---|---|---|
| T1 — `card` manilha fixa | — | `isManilha(Card)`, `cardStrength(Card)`; remove funções com vira |
| T2 — `deck` sem vira | T1 | `Deal` sem `vira`; `deal()` com 6 cartas |
| T3 — `hand`/`game` sem vira | T1, T2 | `trickWinner(Card, Card)`; `Game` compilando sem vira |
| T4 — alternância de truco | T3 | `Game` com `raiser_`/`hasRaiser_`; `canCallTruco` negando o último pedinte |
| T5 — IA sem vira | T1, T3 | `PublicView` sem `vira`; `shouldPlayHandOfEleven(Card, std::mt19937&)` |
| T6 — UI sem vira | T3 | `ViewModel`/`Layout` sem vira; render sem "VIRA"/"MANILHA" |
| T7 — LAN com permissão | T4, T5, T6 | `SyncState::canRaise`; protocolo versão 2; ação "Truco!" guiada por `canRaise` |
| T8 — testes e README | todas | suíte atualizada e documentação coerente |

## 5. Review Focus (riscos sem cobertura natural)

- Comparação `4♣` contra `7♥` e contra comuns `3` — testar explicitamente.
- Empate entre cartas comuns de naipes diferentes deve continuar empate.
- Após aceite, `canCallTruco` do pedinte falso, do adversário verdadeiro — teste reproduz o bug.
- Alternância após vários aceites (3 → 6 → 9 → 12) com lados alternando.
- `canCallTruco` falso por não ser o turno não pode ser confundido com falso por alternância.
- Convidado LAN vê "Truco!" apenas quando `canRaise`; host calcula por `viewer`.
- Índices do `SYNC` e contagem de 23 campos após troca de vira por `canRaise`.
- Handshake com versão "1" deve ser rejeitado.
- Distribuição: 6 cartas distintas, sem carta reservada; determinismo por seed preservado.
- Mão de 11/ferro usando `cardStrength(Card)` sem vira.

## 6. Alternativas consideradas

- **Manter `Rank vira` nas assinaturas e ignorá-lo** — descartada: parâmetro morto reintroduz
  ambiguidade e chamadas erradas compilam.
- **Alternância controlada só na `Session`** — descartada: LAN é autoridade do host e os testes de
  domínio precisam da regra em `Game`; sessão duplicaria estado.
- **Exigir carta jogada entre pedidos** — descartada: regra não pedida no spec.
- **Manter vira só como elemento visual** — descartada: spec exige remover vira da interface e do
  estado.

## 7. Justificativa de complexidade

- Dois campos em `Game` (`raiser_`, `hasRaiser_`) resolvem a alternância para quatro consumidores
  atuais (CPU, hotseat, host LAN, convidado via flag sincronizada).
- Nenhuma abstração nova: apenas remoção de parâmetros e um campo booleano no estado de rede.
- `canRaise` é necessário porque o convidado LAN não tem acesso ao `Game`; sem ele a UI mostraria
  ação inválida.
