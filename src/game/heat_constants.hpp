#pragma once

// Central balancing knobs for the heat mechanic.
// All magic numbers from model + view live here so tuning is one-file.

namespace Heat {

// ── Storage ──────────────────────────────────────────────────────────
// 1 byte per non-GRID cell (unsigned char 0..255). GRID has no heat.

 // ── Seeding ──────────────────────────────────────────────────────────
 inline constexpr int INIT_WARM_CHANCE_PCT = 2;      // % of non-GRID cells start warm
 inline constexpr int INIT_WARM_MIN        = 80;
 inline constexpr int INIT_WARM_MAX        = 200;    // inclusive: 80 + rand%120

 // ── ITEM reactiveness (src/game/model/item_cell.cpp) ─────────────────
 inline constexpr int ITEM_FREQ_BASE      = 100;     // cold denominator
 inline constexpr int ITEM_FREQ_RANGE     = 80;      // 100→20 as heat 0→255
 inline constexpr int ITEM_FREQ_MIN       = 20;
 inline constexpr int ITEM_FREQ_MAX       = 100;
 // heat added on GROW/SHRINK/DESTROY mut. The bonus is INVERTED in heat
 // (cold cells heat up fast, hot cells add little): with constant gain the
 // activity↔heat loop had no fixed point (gain ≥ 0.2/tick always, decay
 // −0.03) and the whole board ratcheted to 255 in minutes — that multiplied
 // plan/apply/event work per tick and is what made fps decay over time.
 inline constexpr int HEAT_MAX            = 255;
 inline constexpr int ITEM_MUT_BASE       = 12;
 inline constexpr int ITEM_MUT_SCALE_SHIFT = 4;      // + ((255-heat)>>4) => 27..12

 // ── Decay ──────────────────────────────────────────────────────────────
 // Every DECAY_PERIOD_MASK+1 ticks a hot cell loses (heat >> SHIFT) + 1.
 // Proportional (≈12.5%/pass) so loss scales with activity and meets the
 // gain curve: steady heat under sustained activity settles ~60-80
 // (≈1.3× cold action rate) instead of saturating at 255.
 inline constexpr int DECAY_PERIOD_MASK         = 31;
 inline constexpr int DECAY_PROPORTIONAL_SHIFT  = 3;

 // ── Spill ────────────────────────────────────────────────────────────
 inline constexpr int SPILL_HEAT_SRC = 18;
 inline constexpr int SPILL_HEAT_TGT = 18;

 // ── Interactions ─────────────────────────────────────────────────────
 inline constexpr int INTERACT_REACTION_A = 18; // actor cell
 inline constexpr int INTERACT_REACTION_B = 12; // neighbor
 inline constexpr int INTERACT_CYCLE      = 6;  // fallback id cycle
 inline constexpr int EMPTY_SPAWN_HEAT    = 10;

 // ── Station / Reserved I/O ───────────────────────────────────────────
 inline constexpr int STATION_INPUT_HEAT  = 8;
 inline constexpr int STATION_ANCHOR_HEAT = 4;
 inline constexpr int STATION_TICK_SELF_HEAT = 1;
 inline constexpr int STATION_STEP_DIV    = 64; // steps = 1 + heat/64  → 1..4
 inline constexpr int RESERVED_BUFFER_HEAT_IN  = 5;
 inline constexpr int RESERVED_BUFFER_HEAT_OUT = 2;

 // ── Crafting (src/game/model/game_model.cpp tryCraftWithDrag) ────────
 // Heat fed to the cell the crafted result lands on, so a craft site warms up
 // like every other interaction (and hot items keep acting around it).
 inline constexpr int CRAFT_HEAT = 8;

 // ── Item growth & spill (src/game/model/item_cell.cpp) ───────────────
 // Stacks have NO hard cap: below ITEM_SPILL_COMFORT a grown stack simply
 // grows. Past it, the grow action instead rolls a split whose chance rises
 // with the stack (BASE + PER per unit over the comfort line, capped at MAX).
 // The split moves HALF the stack to an empty neighbour (src keeps
 // count-amount) — proportional, so a tall pile drains in ~log2 steps
 // instead of being clamped to a fixed transfer (the old code forced
 // src=tgt=3, which destroyed 44 units out of a 50-stack).
 // ITEM_COUNT_MAX is the uint16 storage ceiling: a stack that reaches it
 // must split (it cannot grow), and growth is clamped there as a fallback
 // when no empty neighbour exists.
 inline constexpr int ITEM_COUNT_MAX        = 65535;
 inline constexpr int ITEM_SPILL_COMFORT    = 5;   // stacks above this may split
 inline constexpr int ITEM_SPILL_CHANCE_BASE = 20; // % at COMFORT+1
 inline constexpr int ITEM_SPILL_CHANCE_PER  = 10; // +% per extra unit above it
 inline constexpr int ITEM_SPILL_CHANCE_MAX  = 90; // ceiling

 // ── Quantum fluctuation — superheated vacuum births particles ────────
 // Hold RMB on a cell and Game::step pumps QUANTUM_HOLD_HEAT_PER_TICK of
 // heat into it per sim tick. EMPTY cells at/above QUANTUM_MIN_HEAT then
 // roll QUANTUM_CHANCE_PCT per heat-decay pass (~1.6s at 20 tps — the pass
 // is the amortized decay slice, so the roll costs nothing extra). On a
 // hit the cell materializes a random item (uniform over _numItems), born
 // at heat 0: the fluctuation absorbs the local energy, which also acts
 // as a cooldown. Roll uses GameModel::prng → deterministic under
 // test_rng and per-cell reproducible.
 inline constexpr int QUANTUM_HOLD_HEAT_PER_TICK = 6;   // 120/s at 20 tps → ~2s to saturate
 inline constexpr int QUANTUM_MIN_HEAT           = 200; // same ceiling as INIT_WARM_MAX
 inline constexpr int QUANTUM_CHANCE_PCT         = 25;  // per decay pass → ~6s of hold expected
 inline constexpr int QUANTUM_PRNG_SALT          = 91;  // +1 salt picks the item id

 // ── Decay & diffusion (src/game/model/game_model.cpp) ───────────────
 // Power-of-two masks: interval = mask+1. Decay uses DECAY_PERIOD_MASK /
 // DECAY_PROPORTIONAL_SHIFT (defined above with reactiveness).
 inline constexpr int DIFFUSION_PERIOD_MASK  = 63; // every 64 ticks
 inline constexpr int DIFFUSION_SPARSE_MASK  = 127; // ~1/128 hot cells per diffusion pass

 // ── Visuals ──────────────────────────────────────────────────────────
 namespace Visual {
   inline constexpr float ALPHA_BASE = 0.14f;
   inline constexpr float ALPHA_SCALE = 0.45f;
   inline constexpr float ALPHA_CAP   = 0.55f;
   inline constexpr float WARM_R = 1.0f;
   inline constexpr float WARM_G = 0.22f;
   inline constexpr float WARM_B = 0.05f;

   // Sway modulation (multipliers on top of base amp/freq, t=heat/255)
   inline constexpr float SWAY_AMP_SCALE   = 0.7f; // amp *= 1+0.7*t
   inline constexpr float SWAY_FREQ_SCALE  = 0.9f; // freq *=1+0.9*t
   inline constexpr float TRANS_AMP_SCALE  = 0.6f;
   inline constexpr float TRANS_FREQ_SCALE = 0.6f;
 }

} // namespace Heat
