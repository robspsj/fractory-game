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
 // heat added on GROW/SHRINK/DESTROY mut
 inline constexpr int ITEM_MUT_BASE       = 12;
 inline constexpr int ITEM_MUT_SCALE_SHIFT = 4;      // + (heat>>4)  => 12..27

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

 // ── Decay & diffusion (src/game/model/game_model.cpp) ───────────────
 // Power-of-two masks: interval = mask+1
 inline constexpr int DECAY_PERIOD_MASK      = 31; // every 32 ticks: -1 to all hot
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
