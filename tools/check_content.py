#!/usr/bin/env python3
"""Check the item content tables for the mistakes that do not crash anything.

`assets/items.csv` is the identity table every other table indexes into, so a
typo there is silent: the game loads, the item simply can never be made, or two
items quietly share artwork, or two reaction rules have the same ingredient set
and one of them is unreachable. This tool reads the four CSVs and reports:

  * ids referenced by reactions / crafting / stations that no item defines;
  * items that are neither natural (id < Config::numItems) nor produced by
    anything — i.e. impossible to obtain;
  * duplicate reaction ingredient sets (the first rule in file order wins, so
    the later one can never fire);
  * the crafting tool being produced by a crafting recipe or a station (it would
    cost the tool it makes), or by nothing at all;
  * two items pointing at the same SVG;
  * reaction rules outside the 2..4 ingredient range a gap can supply, and
    non-positive counts.

Usage: python3 tools/check_content.py [repo_root]
Exit code 0 = clean, 1 = problems (printed).
"""
import os
import sys

NUM_NATURAL_FALLBACK = 3   # only when the table has no `natural` column at all
CRAFT_TOOL = 3             # CRAFT_TOOL_ITEM (src/game/model/crafting.hpp)


def rows(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#")[0].strip()
            if line:
                out.append([t.strip() for t in line.split(",")])
    return out


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
    a = lambda *p: os.path.join(root, "assets", *p)

    items = {int(r[0]): {"name": r[1], "svg": r[6]} for r in rows(a("items.csv"))}
    ids = set(items)

    # `natural` (9th column) = ids the random world may grow. Only the contiguous
    # prefix counts, because generation rolls `id % numItems` — this is exactly
    # what src/game/model/item_table.cpp computes for Config::numItems.
    natural_rows = [r for r in rows(a("items.csv"))
                    if len(r) > 8 and r[8].lower() in ("1", "true", "yes")]
    natural_ids = {int(r[0]) for r in natural_rows}
    num_natural = 0
    while num_natural in natural_ids:
        num_natural += 1
    if not natural_rows:
        num_natural = NUM_NATURAL_FALLBACK
        print(f"note: items.csv has no `natural` column — assuming ids 0.."
              f"{num_natural - 1} are the world's")

    reactions = []
    for r in rows(a("reactions.csv")):
        ings = sorted((int(x.split(":")[0]), int(x.split(":")[1]))
                      for x in r[2:] if ":" in x)
        reactions.append((int(r[0]), int(r[1]), tuple(ings)))
    crafts = []
    for r in rows(a("crafting.csv")):
        crafts.append((int(r[0]), int(r[1]),
                       [int(x.split(":")[2]) for x in r[2:] if x.count(":") >= 2]))
    stations = [dict(id=int(r[0]), a=int(r[3]), b=int(r[4]), out=int(r[5]),
                     dur=int(r[7])) for r in rows(a("station_recipes.csv"))]

    errs = []

    for res, cnt, ings in reactions:
        if res not in ids:
            errs.append(f"reaction result {res} is not an item")
        for i, c in ings:
            if i not in ids:
                errs.append(f"reaction ->{res}: ingredient {i} is not an item")
            if c < 1:
                errs.append(f"reaction ->{res}: item {i} has count {c}")
        if not 2 <= len(ings) <= 4:
            errs.append(f"reaction ->{res}: {len(ings)} ingredients (a gap gives 2..4)")
    for res, cnt, pat in crafts:
        if res not in ids:
            errs.append(f"crafting result {res} is not an item")
        for i in pat:
            if i not in ids:
                errs.append(f"crafting ->{res}: pattern item {i} is not an item")
    for s in stations:
        for k in ("a", "b", "out"):
            if s[k] not in ids:
                errs.append(f"station {s['id']}: {k}={s[k]} is not an item")
        if s["dur"] < 0:
            errs.append(f"station {s['id']}: negative duration")

    seen = {}
    for res, cnt, ings in reactions:
        if ings in seen:
            errs.append(f"reaction ingredient set {ings} is claimed by ->{seen[ings]} "
                        f"first, so ->{res} can never fire")
        seen[ings] = res

    sources = {}
    for res, cnt, _ in reactions:
        sources.setdefault(res, []).append("reaction")
    for res, cnt, _ in crafts:
        sources.setdefault(res, []).append("craft")
    for s in stations:
        sources.setdefault(s["out"], []).append("station")
    for i in sorted(items):
        if i >= num_natural and i not in sources:
            errs.append(f"item {i} ({items[i]['name']}) is not natural and nothing "
                        f"produces it")
    for i in range(num_natural):
        if i not in ids:
            errs.append(f"natural id {i} is not defined in items.csv")
    # A natural id outside the contiguous prefix never spawns, and the ids inside
    # it that are not marked natural spawn anyway — both are silent content bugs.
    for i in sorted(natural_ids):
        if i >= num_natural:
            errs.append(f"item {i} ({items[i]['name']}) is marked natural but sits "
                        f"after the contiguous prefix 0..{num_natural - 1}, so it "
                        f"will never be grown — mark the ids before it too")

    if CRAFT_TOOL in [c[0] for c in crafts]:
        errs.append(f"a crafting recipe produces the crafting tool (item {CRAFT_TOOL})")
    if CRAFT_TOOL in [s["out"] for s in stations]:
        errs.append(f"a station produces the crafting tool (item {CRAFT_TOOL})")
    if CRAFT_TOOL not in sources:
        errs.append(f"nothing produces the crafting tool (item {CRAFT_TOOL})")

    by_svg = {}
    for i, it in items.items():
        by_svg.setdefault(it["svg"], []).append(i)
    for svg, l in by_svg.items():
        if len(l) > 1:
            errs.append(f"items {l} share artwork {svg} — they will look identical")

    print(f"items={len(items)} reactions={len(reactions)} crafting={len(crafts)} "
          f"stations={len(stations)} world-grows={num_natural} "
          f"({', '.join(items[i]['name'] for i in range(num_natural))})")
    if errs:
        print(f"\n{len(errs)} problem(s):")
        for e in errs:
            print("  - " + e)
        return 1
    print("clean: every item is reachable, every reference resolves, no rule shadows another")
    return 0


if __name__ == "__main__":
    sys.exit(main())
