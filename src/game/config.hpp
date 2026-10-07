#pragma once
#include <string>

constexpr float MIN_ZOOM = 0.0001f;
constexpr float MAX_ZOOM = 1e6f;  // cap to avoid float-overflow in culling at extreme zoom
constexpr float MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD = 1.15f/10;
constexpr float MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT = 0.7f/10;

struct Config {
    const unsigned int seed;
    const int gridChance;
    int gridLimit;  // non-const: overridable via --grid-limit for board-size studies
    const bool showAnchor;
    const int ticksPerSecond;
    bool emptyGrid = false;
    bool vsync = false;  // sync present to display refresh; off = unlocked FPS
    int initialGridSize = 3;
    // Random generator: percent of cells whose roll lands in the board's
    // "machine" share — they stay empty at that point, and the station pass then
    // anchors a recipe footprint on a random subset of the cells left empty (see
    // GameModel::initRandomGrid). Net effect at this default: roughly a third of
    // the subgrids hold a machine. 0 disables stations entirely.
    // Footprints come from assets/station_recipes.csv; this only sets density.
    int stationChance = 20;
    // Number of distinct item types the simulation grows at random — the world
    // spawns ids 0..numItems-1 and the quantum-birth roll uses `id % numItems`,
    // so the natural ids in assets/items.csv must be contiguous from 0.
    // 0 (the default) = take it from that table's `natural` column, which is what
    // the game does (src/main.cpp -> naturalItemCountFromFile). Tests and tools
    // that build a Config by hand leave it 0 and get the model's built-in
    // fallback; --num-items sets it explicitly.
    int numItems = 0;
    // ── Feature flags: automatic item dynamics ──────────────────────────
    // The tick engine's own item behaviour — a stack growing on its own, a
    // stack shrinking/vanishing, and a tall stack splitting toward a neighbour.
    // All three are OFF by default: the board only changes when the player does
    // something (craft, react, merge, drop, heat). The code paths stay intact;
    // flip these on (--item-dynamics, or individually --item-grow/--item-shrink/
    // --item-split) to get the self-running simulation back.
    // Manual transformations are never affected by these flags.
    bool itemGrow = false;
    bool itemShrink = false;
    bool itemSplit = false;
    std::string reactionsCsvPath;
    std::string spawnCsvPath;
    std::string craftingCsvPath;
    std::string stationRecipesCsvPath;
    // Inline interaction CSVs, used instead of the path when there is no
    // filesystem to read them from (wasm bakes the assets/*.csv copies into the
    // binary — see tools/gen_wasm_interact_data.py). Take precedence if set.
    std::string reactionsCsv;
    std::string spawnCsv;
    std::string craftingCsv;
    std::string stationRecipesCsv;

    Config() : seed(0), gridChance(20), gridLimit(500000), showAnchor(false), ticksPerSecond(20) {}
    Config(unsigned int seed_, int gridChance_, int gridLimit_ = 500000,
           bool showAnchor_ = false, int ticksPerSecond_ = 20, int numItems_ = 0)
        : seed(seed_), gridChance(gridChance_), gridLimit(gridLimit_), showAnchor(showAnchor_), ticksPerSecond(ticksPerSecond_), numItems(numItems_) {}
};
