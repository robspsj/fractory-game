#pragma once
#include <cstdint>
#include <vector>

// Recipe-book progress — which transformations the player has actually
// performed. The book itself always lists EVERY entry (it doubles as the
// manual); these flags only add the "done" mark, so the player can see at a
// glance what is left to discover.
//
// Flags are set from the model at the exact place the transformation happens
// (craft commit, reaction, spill, merge, quantum birth, …), so the book can
// never drift from the simulation.
enum class BookCategory : int {
  Recipes = 0,   // crafting shapes       (assets/crafting.csv)
  Machines,      // station footprints    (assets/station_recipes.csv)
  Reactions,     // right-click pairs     (assets/reactions.csv)
  Stacking,      // grow / shrink / destroy / merge / split
  Quantum,       // heat-driven fluctuation
  COUNT
};

// Stacking and Quantum entries are code-defined rather than data-table rows, so
// they get fixed ids. Keep in sync with RecipeBook's renderers.
enum class BookPhysics : int {
  Grow = 0,   // a stack gained a unit
  Shrink,     // a stack lost a unit
  Destroy,    // a lone unit vanished
  Merge,      // a dragged stack merged into a same-id stack
  Split,      // a tall stack split in half toward an empty neighbour
  Quantum,    // a superheated vacuum birthed a particle
  COUNT
};

struct BookState {
  // One flag per row of the matching data table (index == row order).
  std::vector<uint8_t> recipes, machines, reactions;
  // Bit per BookPhysics entry, shared by the Stacking and Quantum categories.
  uint32_t physics = 0;

  void resize(int nRecipes, int nMachines, int nReactions) {
    recipes.assign((size_t)(nRecipes > 0 ? nRecipes : 0), 0);
    machines.assign((size_t)(nMachines > 0 ? nMachines : 0), 0);
    reactions.assign((size_t)(nReactions > 0 ? nReactions : 0), 0);
    physics = 0;
  }

  bool has(BookCategory c, int id) const {
    switch (c) {
      case BookCategory::Recipes:
        return id >= 0 && id < (int)recipes.size() && recipes[id] != 0;
      case BookCategory::Machines:
        return id >= 0 && id < (int)machines.size() && machines[id] != 0;
      case BookCategory::Reactions:
        return id >= 0 && id < (int)reactions.size() && reactions[id] != 0;
      case BookCategory::Stacking:
      case BookCategory::Quantum:
        return id >= 0 && id < (int)BookPhysics::COUNT && (physics & (1u << id));
      default:
        return false;
    }
  }

  void mark(BookCategory c, int id) {
    switch (c) {
      case BookCategory::Recipes:
        if (id >= 0 && id < (int)recipes.size()) recipes[id] = 1;
        break;
      case BookCategory::Machines:
        if (id >= 0 && id < (int)machines.size()) machines[id] = 1;
        break;
      case BookCategory::Reactions:
        if (id >= 0 && id < (int)reactions.size()) reactions[id] = 1;
        break;
      case BookCategory::Stacking:
      case BookCategory::Quantum:
        if (id >= 0 && id < (int)BookPhysics::COUNT) physics |= (1u << id);
        break;
      default:
        break;
    }
  }
};
