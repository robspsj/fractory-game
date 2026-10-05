#include "interact_config.hpp"
#include <cstdio>
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

const StationRecipe *InteractConfig::findStationRecipe(int id) const {
  for (const auto &r : stationRecipes) {
    if (r.id == id) return &r;
  }
  return nullptr;
}

// Loaders take a stream so the same parser serves the file path (native, reads
// assets/*.csv) and the inline string (wasm, no filesystem) — mirroring
// loadStationRecipes below.
static void loadReactions(std::istream &in, std::vector<ReactionRule> &out) {
  std::string line;
  while (std::getline(in, line)) {
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

static void loadSpawnRules(std::istream &in, std::vector<SpawnRule> &out) {
  std::string line;
  while (std::getline(in, line)) {
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

// Crafting table (assets/crafting.csv):
//   resultId,resultCount,dr:dc:itemId,dr:dc:itemId,...
// Each entry after the first two columns is one (offset, item) pair — offsets
// are relative to the recipe's own origin, so shapes are authored as they read
// on the board. Extra fields inside an entry are ignored, leaving room for
// per-entry requirements (count, consumed flag, …) later.
static void loadCrafting(std::istream &in, std::vector<CraftingRule> &out) {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string tok;
    CraftingRule r;
    if (!std::getline(ss, tok, ',')) continue;
    r.result = std::stoi(tok);
    if (!std::getline(ss, tok, ',')) continue;
    r.resultCount = std::stoi(tok);
    bool first = true;
    while (std::getline(ss, tok, ',')) {
      if (tok.empty()) continue;
      std::stringstream es(tok);
      std::string dr, dc, id;
      if (!std::getline(es, dr, ':')) continue;
      if (!std::getline(es, dc, ':')) continue;
      if (!std::getline(es, id, ':')) continue;
      CraftEntry e;
      e.dr = std::stoi(dr);
      e.dc = std::stoi(dc);
      e.itemId = std::stoi(id);
      if (first) {
        r.minDr = r.maxDr = e.dr;
        r.minDc = r.maxDc = e.dc;
        first = false;
      } else {
        if (e.dr < r.minDr) r.minDr = e.dr;
        if (e.dr > r.maxDr) r.maxDr = e.dr;
        if (e.dc < r.minDc) r.minDc = e.dc;
        if (e.dc > r.maxDc) r.maxDc = e.dc;
      }
      r.entries.push_back(e);
    }
    // A recipe with no entries would match anywhere — refuse to load it.
    if (!r.entries.empty()) out.push_back(std::move(r));
  }
}

static void loadStationRecipes(std::istream &in,
                               std::vector<StationRecipe> &out) {
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string tok;
    StationRecipe r;
    std::getline(ss, tok, ','); r.id = std::stoi(tok);
    std::getline(ss, tok, ','); r.sizeR = std::stoi(tok);
    std::getline(ss, tok, ','); r.sizeC = std::stoi(tok);
    std::getline(ss, tok, ','); r.inputA = std::stoi(tok);
    std::getline(ss, tok, ','); r.inputB = std::stoi(tok);
    std::getline(ss, tok, ','); r.output = std::stoi(tok);
    std::getline(ss, tok, ','); r.outputCount = std::stoi(tok);
    std::getline(ss, tok, ','); r.duration = std::stoi(tok);
    for (int i = 0; i < r.sizeR * r.sizeC; i++) {
      std::string roleStr;
      if (std::getline(ss, roleStr, ',')) {
        if (roleStr == "INPUT") r.layout.push_back(ReserveRole::INPUT);
        else if (roleStr == "OUTPUT") r.layout.push_back(ReserveRole::OUTPUT);
        else if (roleStr == "BUFFER") r.layout.push_back(ReserveRole::BUFFER);
        else r.layout.push_back(ReserveRole::NONE);
      } else {
        r.layout.push_back(ReserveRole::NONE);
      }
    }
    out.push_back(r);
  }
}

void initInteractConfig(InteractConfig &cfg,
                        const std::string &reactionsPath,
                        const std::string &spawnPath,
                        const std::string &stationRecipesPath,
                        const std::string &stationRecipesCsv,
                        const std::string &reactionsCsv,
                        const std::string &spawnCsv,
                        const std::string &craftingPath,
                        const std::string &craftingCsv) {
  cfg.reactions.clear();
  cfg.spawnRules.clear();
  cfg.stationRecipes.clear();
  cfg.crafting.clear();
  // Inline CSV wins over the path for every table: it is how the wasm build
  // feeds data in (no filesystem to read assets/ from). Both go through the
  // same loader, so native and web parse identical bytes.
  if (!reactionsCsv.empty()) {
    std::istringstream in(reactionsCsv);
    loadReactions(in, cfg.reactions);
  } else if (!reactionsPath.empty()) {
    std::ifstream file(reactionsPath);
    if (file)
      loadReactions(file, cfg.reactions);
  }
  if (!spawnCsv.empty()) {
    std::istringstream in(spawnCsv);
    loadSpawnRules(in, cfg.spawnRules);
  } else if (!spawnPath.empty()) {
    std::ifstream file(spawnPath);
    if (file)
      loadSpawnRules(file, cfg.spawnRules);
  }
  if (!stationRecipesCsv.empty()) {
    std::istringstream in(stationRecipesCsv);
    loadStationRecipes(in, cfg.stationRecipes);
  } else if (!stationRecipesPath.empty()) {
    std::ifstream file(stationRecipesPath);
    if (file)
      loadStationRecipes(file, cfg.stationRecipes);
  }
  if (!craftingCsv.empty()) {
    std::istringstream in(craftingCsv);
    loadCrafting(in, cfg.crafting);
  } else if (!craftingPath.empty()) {
    std::ifstream file(craftingPath);
    if (file)
      loadCrafting(file, cfg.crafting);
  }
  // One-line summary: an empty table is otherwise silent (a native run from a
  // directory without assets/, a typo in a path, a wasm build without the
  // embedded copy) — and 0 reactions means a right click falls back to the
  // id-cycle instead of reacting, which reads as "reactions are broken".
  fprintf(stderr, "[GameModel] interact config: %zu reactions, %zu spawn rules, %zu station recipes, %zu crafting recipes\n",
          cfg.reactions.size(), cfg.spawnRules.size(), cfg.stationRecipes.size(),
          cfg.crafting.size());
}
