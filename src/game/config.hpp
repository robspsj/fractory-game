#pragma once

struct Config {
    const unsigned int seed;
    const int gridChance;

    Config() : seed(0), gridChance(50) {}
    Config(unsigned int seed_, int gridChance_)
        : seed(seed_), gridChance(gridChance_) {}
};
