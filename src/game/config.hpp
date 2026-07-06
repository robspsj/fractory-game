#pragma once

constexpr float MIN_ZOOM = 0.0001f;
constexpr float MIN_CELL_ON_SCREEN_PROPORTION_THRESHOLD = 1.15f;
constexpr float MAX_CELL_ON_SCREEN_PROPORTION_THRESHOLD_PARENT = 0.7f;

struct Config {
    const unsigned int seed;
    const int gridChance;
    const int gridLimit;

    Config() : seed(0), gridChance(20), gridLimit(500000) {}
    Config(unsigned int seed_, int gridChance_, int gridLimit_ = 500000)
        : seed(seed_), gridChance(gridChance_), gridLimit(gridLimit_) {}
};
