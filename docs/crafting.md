# Crafting — Design & Behaviour

Shaped, Minecraft-style recipes matched against a pattern of `ITEM` cells on the
board. Data lives in `assets/crafting.csv`; the model side is
`GameModel::tryCraftWithDrag()` (`src/game/model/game_model.cpp`).

## 1. The crafting element

Item id **3** (`assets/items.csv`, `CRAFT_TOOL_ITEM` in
`src/game/model/crafting.hpp`) is the crafting tool — a **wand**. It is
deliberately **not** produced by the random world (`Config::numItems` stays 3),
and just as deliberately **never** a crafting result: a recipe producing the tool
would cost the tool it makes, so the wand could never be obtained at all. Its
**only** source is the reaction `wood + wood -> 1 wand`
(`assets/reactions.csv`), which needs no tool held, so the tech tree always
bootstraps from two sticks of wood.

Hold it (drag state, picked up with the left button) and **right-click** a cell
that is part of a pattern. One tool unit is spent per craft.

> **The right button is a click/hold pair** (`Game::mouseDown`/`mouseUp`):
> a **click** (a tap — released within `TAP_MAX_MS` = 250 ms and `TAP_EPSILON_PX`
> = 8 px of travel) is the **interact** — with a **craft attempt first** while the
> wand is held and its pattern matches — and a **hold** past that window is the
> **heat pump** that drives quantum fluctuations (`docs/heat.md`) and never
> crafts. Within the click, the *position* picks the action: the middle of a cell
> is the cell action, the gap between cells is a reaction (`docs/reactions.md`).
> A click that matches no craft pattern is not a craft, it falls through to the
> interact.

## 2. Recipe format

```
resultId,resultCount,dr:dc:itemId,dr:dc:itemId,...
```

Each entry after the first two columns is one **(offset, item)** pair.
`dr` = rows down, `dc` = columns right, relative to the recipe's own origin, so
a recipe is authored the way it reads on the board:

```
# 1x3 — three wood in a row -> 3 stick
8,3,0:0:0,0:1:0,0:2:0
```

No recipe may produce the crafting element itself (item 3) — see §1.

Entries may be non-contiguous (L, T, ring, …). Extra fields can be appended
inside an entry after a further `:` (`dr:dc:itemId:count:…`) without a format
change — that is the extension point for per-entry requirements later.

Rules are tried **in file order**, so author specific shapes before general
ones. The bounding box is derived from the entries.

## 3. Matching

```mermaid
flowchart TB
    press["right press"] --> kind{"click (&#8804;250 ms, &#8804;8 px)<br/>or hold?"}
    kind -->|hold| heat["heat pump (never crafts)"]
    kind -->|click| tool{item 3 held?}
    tool -->|no| interact["plain interact<br/>(machine load / spawn)"]
    tool -->|yes| slide["for each rule, slide its bounding box<br/>so the clicked cell is covered<br/>(origin = anchor - dr,dc)"]
    slide --> cells["every entry must be an ITEM cell<br/>with the matching id"]
    cells -->|no match| next["try next rule / fall through to the interact"]
    cells -->|match| place{"clicked cell free after<br/>losing one unit, or holds<br/>the result item?"}
    place -->|no| next
    place -->|yes| commit["consume 1 unit per pattern cell<br/>result -> clicked cell<br/>-1 tool unit<br/>+CRAFT_HEAT, CRAFT event"]
```

* **Patterns must fit inside one grid** (`GameModel::cellAtOffset` walks the
  parent grid only): a recipe cannot span a subgrid boundary.
* **Nothing is silently destroyed.** Every pattern cell loses exactly one unit,
  and the result lands on the *clicked* cell. If that cell still holds a
  different item after its unit is consumed, the craft is **refused** instead of
  overwriting it — split the pile first (or click a different pattern cell).
* The result *merges* into the clicked cell when it already holds the result
  item, keeping its leftover count.

## 4. Feedback

`CellEventType::CRAFT` → `EffectKind::CRAFT` → smoke puff, and
`Heat::CRAFT_HEAT` warms the cell the result lands on (a craft site gets hot, so
items around it keep acting).

## 5. Tests

`tests/test_crafting.csv` (+ fixture `tests/crafting.csv`) covers: no tool held →
no-op, a successful craft of the 1x3 recipe (pattern consumed, result placed,
tool spent), and a repeat craft on a consumed pattern matching nothing. The
harness drives it with the `craft,row,col` step.
