#pragma once

namespace truco {

enum class Stake { One = 1, Three = 3, Six = 6, Nine = 9, Twelve = 12 };

Stake nextStake(Stake current);

bool canRaise(Stake current);

Stake valueAfterAccept(Stake current);

// Label of the call that raises from `current`: Truco, Seis, Nove or Doze.
const char* callLabel(Stake current);

int valuePoints(Stake stake);

}  // namespace truco
