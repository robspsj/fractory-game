#include "interact_config.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>

const ReactionRule *InteractConfig::findReaction(int itemA, int itemB) const {
  for (const auto &r : reactions) {
    if ((r.itemA == itemA && r.itemB == itemB) ||
        (r.itemA == itemB && r.itemB == itemA)) {
      return &r;
    }
  }
  return nullptr;
}

const SpawnRule *InteractConfig::pickSpawn(int heldItemId) const {
  std::vector<const SpawnRule *> candidates;
  int totalWeight = 0;
  for (const auto &s : spawnRules) {
    if (s.heldItemId == -1 || s.heldItemId == heldItemId) {
      candidates.push_back(&s);
      totalWeight += s.weight;
    }
  }
  if (candidates.empty()) return nullptr;
  int roll = std::rand() % totalWeight;
  for (const auto *s : candidates) {
    roll -= s->weight;
    if (roll < 0) return s;
  }
  return candidates.back();
}

static void loadReactions(const std::string &path,
                          std::vector<ReactionRule> &out) {
  std::ifstream file(path);
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string a, b, res, cnt;
    std::getline(ss, a, ',');
    std::getline(ss, b, ',');
    std::getline(ss, res, ',');
    std::getline(ss, cnt, ',');
    out.push_back({std::stoi(a), std::stoi(b), std::stoi(res),
                    std::stoi(cnt)});
  }
}

static void loadSpawnRules(const std::string &path,
                           std::vector<SpawnRule> &out) {
  std::ifstream file(path);
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string held, item, cnt, w;
    std::getline(ss, held, ',');
    std::getline(ss, item, ',');
    std::getline(ss, cnt, ',');
    std::getline(ss, w, ',');
    out.push_back({std::stoi(held), std::stoi(item), std::stoi(cnt),
                    std::stoi(w)});
  }
}

void initInteractConfig(InteractConfig &cfg,
                        const std::string &reactionsPath,
                        const std::string &spawnPath) {
  cfg.reactions.clear();
  cfg.spawnRules.clear();
  if (!reactionsPath.empty())
    loadReactions(reactionsPath, cfg.reactions);
  if (!spawnPath.empty())
    loadSpawnRules(spawnPath, cfg.spawnRules);
}
