#pragma once
#include <string>
#include <vector>
#include "model/crafting.hpp"

enum class ReserveRole : uint8_t { NONE, INPUT, OUTPUT, BUFFER };

struct ReactionRule {
  int itemA;
  int itemB;
  int result;
  int resultCount;
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

  const ReactionRule *findReaction(int itemA, int itemB) const;
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
