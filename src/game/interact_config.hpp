#pragma once
#include <string>
#include <vector>

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

struct InteractConfig {
  std::vector<ReactionRule> reactions;
  std::vector<SpawnRule> spawnRules;

  const ReactionRule *findReaction(int itemA, int itemB) const;
  const SpawnRule *pickSpawn(int heldItemId) const;
};

void initInteractConfig(InteractConfig &cfg,
                        const std::string &reactionsPath,
                        const std::string &spawnPath = "");
