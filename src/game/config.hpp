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
    // Number of distinct item types the simulation uses (drives random item-id
    // generation). Keep this in sync with the rows defined in assets/items.csv
    // (id 0..numItems-1). Defaults to 3: wood, stone, water.
    int numItems = 3;
    std::string reactionsCsvPath;
    std::string spawnCsvPath;
    std::string stationRecipesCsvPath;
    // Inline station-recipe CSV, used instead of the path when there is no
    // filesystem to read it from (wasm bakes assets/station_recipes.csv into the
    // binary — see tools/gen_wasm_interact_data.py). Takes precedence if set.
    std::string stationRecipesCsv;

    Config() : seed(0), gridChance(20), gridLimit(500000), showAnchor(false), ticksPerSecond(20) {}
    Config(unsigned int seed_, int gridChance_, int gridLimit_ = 500000,
           bool showAnchor_ = false, int ticksPerSecond_ = 20, int numItems_ = 3)
        : seed(seed_), gridChance(gridChance_), gridLimit(gridLimit_), showAnchor(showAnchor_), ticksPerSecond(ticksPerSecond_), numItems(numItems_) {}
};
