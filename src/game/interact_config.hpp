#pragma once
#include <string>
#include <vector>
#include "model/crafting.hpp"

enum class ReserveRole : uint8_t { NONE, INPUT, OUTPUT, BUFFER };

// The most cells one gap can touch (a corner joins a 2x2 block), so also the
// largest ingredient list a reaction may carry.
inline constexpr int kMaxReactionCells = 4;

// One ingredient of a reaction: an item id and the EXACT stack count its cell
// must hold. A rule lists 2..4 of them (the number of board cells it consumes).
struct ReactionIngredient {
  int itemId = 0;
  int count = 1;
};

// Reactions are ingredient SETS, not shapes: the arrangement of the cells is
// irrelevant, their exact contents are not. A rule matches when the multiset of
// (itemId, count) over the ITEM cells of the clicked gap equals its ingredient
// multiset — same ids, same counts, nothing extra. GameModel::tryReaction does
// the matching and the commit (see docs/reactions.md).
struct ReactionRule {
  int result = 0;
  int resultCount = 1;
  std::vector<ReactionIngredient> ingredients;
};

struct SpawnRule {
  int heldItemId;
  int spawnItemId;
  int spawnCount;
  int weight;
};

struct StationRecipe {
  int id;
  int sizeR, sizeC;
  std::vector<ReserveRole> layout; // row-major, sizeR*sizeC entries
  int inputA = -1;
  int inputB = -1;
  int output = -1;
  int outputCount = 1;
  int duration = 0; // ticks to complete (0 = instant)
};

struct InteractConfig {
  std::vector<ReactionRule> reactions;
  std::vector<SpawnRule> spawnRules;
  std::vector<StationRecipe> stationRecipes;
  std::vector<CraftingRule> crafting;

  // Exact-set lookup: the arrays are the contents of the gap's ITEM cells, in
  // any order. Returns the first rule in file order whose ingredient multiset
  // matches (same ids, same counts, same size), or nullptr.
  const ReactionRule *findReaction(const int *itemIds, const int *counts,
                                   int n) const;
  const SpawnRule *pickSpawn(int heldItemId) const;
  const StationRecipe *findStationRecipe(int id) const;
};

void initInteractConfig(InteractConfig &cfg,
                        const std::string &reactionsPath,
                        const std::string &spawnPath = "",
                        const std::string &stationRecipesPath = "",
                        const std::string &stationRecipesCsv = "",
                        // Inline (filesystem-less) variants: take precedence
                        // over the matching path when non-empty.
                        const std::string &reactionsCsv = "",
                        const std::string &spawnCsv = "",
                        const std::string &craftingPath = "",
                        const std::string &craftingCsv = "");
