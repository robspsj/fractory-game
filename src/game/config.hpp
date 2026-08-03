#pragma once
#include <string>

constexpr float MIN_ZOOM = 0.0001f;
constexpr float MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD = 1.15f/10;
constexpr float MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT = 0.7f/10;

struct Config {
    const unsigned int seed;
    const int gridChance;
    const int gridLimit;
    const bool showAnchor;
    const int ticksPerSecond;
    std::string reactionsCsvPath;
    std::string spawnCsvPath;
    std::string stationRecipesCsvPath;

    Config() : seed(0), gridChance(20), gridLimit(500000), showAnchor(false), ticksPerSecond(20) {}
    Config(unsigned int seed_, int gridChance_, int gridLimit_ = 500000,
           bool showAnchor_ = false, int ticksPerSecond_ = 20)
        : seed(seed_), gridChance(gridChance_), gridLimit(gridLimit_), showAnchor(showAnchor_), ticksPerSecond(ticksPerSecond_) {}
};
