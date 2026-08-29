# Fractory — model tick performance

Status: branch `perf/parallel-sweep`. All numbers from a headless `fractory --tick N`
benchmark on an 11.5M-node auto-board (16-core Apple Silicon, macOS).

## The original problem
Frame time was ~195 ms @ ~5 fps even though rendering was only ~13 ms. The gap was
`GameModel::tick()`:

```
tick: sweep=47.2ms nodes=11,578,672 items=6,157,252 ...
```

`tick()` swept *every* node serially and called `std::rand()` on ~6 M item cells each
tick. Because the frame loop ran 4–5 ticks per frame, the sweep alone ate ~190 ms.

## Fix 1 — parallel two-phase sweep  (merged: 8–9.5×)
The tick is split so the expensive part is embarrassingly parallel and the cheap
part stays serial:

- **Phase 1 (multi-core, strict read-only):** each thread scans a contiguous,
  disjoint node range and *plans* actions — no writes at all, so no locks/atomics.
  - ITEM cells: compute a **stateless per-cell PRNG** `hash(idx, salt, seed, tick)`
    instead of the global `std::rand` (shared RNG state would be a data race).
    Record a mutation (GROW/SHRINK/DESTROY) or a spill intent.
  - STATION cells: record their indices (processed serially in phase 2).
- **Phase 2 (single-thread, tiny):** apply mutations, apply spills
  **first-wins-per-target**, run stations, emit events. Only touches changed cells.

### Spill-conflict handling
The user-anticipated "two cells spill to the same target" race is resolved with no
locks: phase 1 never writes the target (so two sources may both *see* it empty and
both *record* a spill); phase 2 applies them serially with an `unordered_set` of
targets already claimed — the first request wins, the second is dropped (its source
keeps count 5 and retries next tick, mirroring the serial behaviour where the
already-spilled cell stops being a candidate).

### Measured
| threads | ms/tick |
|---|---|
| 1 | 35.9 |
| 2 | 19.1 |
| 4 | 10.4 |
| 8 | 5.7 |
| 16 | **4.8** |

vs **46.3 ms/tick** serial (old `std::rand`). Interactive FPS: **5 → ~50**.
Tunable at runtime: `FRACTORY_TICK_THREADS=N`. Default = `hardware_concurrency()`
capped at 16; boards < 16k cells stay single-threaded (thread-spawn overhead).

### Determinism note
The tick is now a *pure function* of (idx, salt, seed, tick) → reproducible given a
seed and a fixed thread count (spill first-wins ordering is thread-ordered). It is
deliberately NOT the old `std::rand` sequence, so the seeded CSV tests that check
exact tick-grown state now mismatch (expected). Board *generation* is unchanged
(still `std::rand`).

## Remaining avenues (explored, not merged)

### GPU compute shader for the sweep
Phase 1 (planning) is GPU-embarrassingly-parallel in principle: one thread per cell,
`prng()` classifier, `findSpillTarget` neighbor reads, and a compacted scratch
buffer via `atomicAdd` for mutation/spill records; CPU read-back of the records then
applies phase 2 serially.
**Verdict: not worth it yet.** Round-trip upload + compute + readback is ~1–2 ms a
tick vs the current CPU ~4.8 ms — a modest saving that disappears once the renderer
(now ~17 ms, the real floor) is the bottleneck. Adding a Metal compute pass to the
existing Skia/Metal present adds large complexity for little gain until the render
path is also optimized. Revisit only if (a) even more cores/threads don't help and
(b) the render cost is fixed.

### Item-index list (skip the full-node scan)
Phase 1 still scans all 11.5 M nodes and type-checks each. Maintaining a dense
`std::vector<int>` of item (and station) indices — updated only on the cells phase 2
mutates (~60 k/tick) — would cut the scan to ~6 M entries and skip the per-node type
branch. Moderate refactor; the biggest lever to push tick below ~4 ms on top of the
parallel sweep.

### Persistent thread pool
Spawning 16 threads per tick is bounded (~20 ticks/s), so saving is small, but a
reusable pool removes it entirely and stabilises smaller iterations. Low priority.

### The render is now the bottleneck
At ~50 fps, `run ≈ 17 ms` of the frame is the Skia/Metal render (tiles + SVG icons +
drag glow + particles). The next performance target is the render path, not the
model tick.

## How to reproduce
```
./build-native/fractory --tick 60                 # avg tick ms, default threads
FRACTORY_TICK_THREADS=16 ./build-native/fractory --tick 60
./build-native/fractory                          # interactive [prof] beacon each 1s
```

## Render parallelization — explored, NOT a win
Because the cells never overlap, we prototyped parallel software rasterisation of
the cell layer: flatten the visible cells to a display list (single-thread collect
pass), then rasterize contiguous slices on separate threads — each into its own
full-frame transparent surface via a `thread_local` render canvas — and composite
in list order (parent-before-child preserved). Code lives on `perf/parallel-render`,
gated behind `FRACTORY_RENDER_THREADS` (off on main).

Pixel-correct (fixed seed + frozen `animSeconds()`: only AA-level rounding,
`|Δ| <= 3`). But it is **not faster** (offscreen CPU bench @1572x1003, ~526 SVG
pieces):

| threads | render | geometry | SVG per-inst | fps |
|---:|---:|---:|---:|---:|
| 1 | 5.60 | 2.51 | 2.38 | 135 |
| 2 | 5.73 | 1.42 | 3.21 | 134 |
| 4 | 5.49 | 0.78 | 3.50 | 137 |
| 8 | 6.77 | 1.45 | 3.94 | 118 |
| 16 | 10.92 | 5.02 | 4.30 | 79 |

Diagnosis:
- Geometry/cells parallelize well (2.5 -> 0.78 ms).
- The single most expensive stage, per-instance SVG (`SkSVGDOM::render` on the
  ~526 piece sprites), shares the same 5 DOM objects across threads; they are
  not thread-safe, so that time *grows* with thread count instead of shrinking.
- Compositing N full-frame transparent surfaces per frame adds memory traffic
  that outweighs the geometry win at >4 threads.

Verdict: not worth pursuing until the SVG stage is either pre-rasterized (which the
user rejected) or given per-thread DOM copies AND the composite moves to Y-tiles.
The next real render target is that per-instance SVG+Sprite cost itself.