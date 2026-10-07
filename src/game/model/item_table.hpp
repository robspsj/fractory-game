#pragma once
#include <string>

// Model-side slice of assets/items.csv — the item identity table the whole
// content layer indexes into. The model needs exactly one thing from it: how
// many of the LEADING item ids the random world is allowed to grow. That count
// is `Config::numItems` -> `GameModel::_numItems`, and it drives random item
// generation and the quantum-birth roll. Names, colours and icons belong to the
// view, which reads the same file (src/game/view/asset_manager.cpp).
//
// Format (assets/items.csv): id,name,R,G,B,icon,svgPath,sway[,natural]
//   natural=1 marks an id the world generator may spawn; everything else has to
//   be made in-game. The natural ids must be CONTIGUOUS from 0, because
//   generation rolls `id % numItems`: a gap (say ids 0,1,3 marked natural) would
//   silently make a made-in-game item spawnable and leave id 2 unreachable.
//
// Both functions return the length of the contiguous natural prefix, or
// `fallback` when the table is unreadable / declares nothing.
int naturalItemCountFromCsv(const std::string &csvText, int fallback = 0);
int naturalItemCountFromFile(const std::string &path, int fallback = 0);
