<!-- PLB-LOCAL FILE. Local only; see docs/local-changes.md. -->
# Marking local changes in this fork

This checkout is a fork. `origin` is `Fuitad/mod-playerbots-upstream` and `upstream` is
`mod-playerbots/mod-playerbots`, and upstream is pulled in periodically. Every local change is
therefore a future merge conflict, or worse, a change that gets silently reverted because nobody
recognised it as ours.

The rule: **anything this fork changes must say so, in the code, at the point of the change.**
A commit message does not survive a merge conflict resolution. A comment does.

## Two markers, chosen by whether upstream owns the file

Check before editing:

```bash
git cat-file -e upstream/master:<path> && echo UPSTREAM || echo LOCAL-ONLY
```

### Local-only files: one banner, no inline markers

A file that does not exist upstream cannot conflict, so it needs nothing per hunk. Put this after
the licence header and leave the body clean:

```cpp
/*
 * PLB-LOCAL FILE. This file does not exist upstream and never conflicts on a merge.
 * ...
 */
```

### Upstream files: one marker per hunk

These are the conflict sites, so each edited region is tagged. Say what upstream does, so a
resolver can compare without fetching the original:

```cpp
// PLB-LOCAL(<tag>): what this fork changed and why, in a line or two.
// Upstream: what the original did here.
```

Use a BEGIN and END pair when the change spans more than a few lines:

```cpp
// PLB-LOCAL BEGIN(<tag>): ...
// PLB-LOCAL END(<tag>)
```

The `<tag>` groups every hunk of one logical change, across every file it touches, so
`rg -n 'PLB-LOCAL\(revive-outcome\)' src` collects the whole thing.

## Prefer a local file over an upstream edit

The cheapest merge is the one that never conflicts. When a change can live in a local-only file
with a single call site in the upstream file, do that. `revive-outcome` is the worked example: the
classifier and its enum live in `src/Bot/Recovery/PlayerbotRecoveryPolicy.h`, which upstream has
never heard of, and the upstream actions carry one marked line each.

## On merging upstream

Resolve each conflict against the marker, not against the diff alone. A `PLB-LOCAL` region that
upstream has since rewritten needs a decision, not a reflex: keep the local behaviour, adopt
upstream's, or reconcile. Record which, by updating the marker.

`rg -n 'PLB-LOCAL' src tests` lists every local region under the marker convention.

## Coverage and verification

The current counts and exact locations live in `docs/local-changes-inventory.md`. Regenerate
that inventory rather than relying on historical counts from the original 2026-08-25 backfill.
Run from this module's root:

```bash
uv run --no-project python tools/plb_local_markers.py --check
uv run --no-project python tools/plb_local_markers.py --apply
```

`--check` is read-only. Exit `0` means coverage and inventory are current, including explicitly
listed manual exceptions. Exit `1` means missing markers or stale metadata. Exit `2` means the
check could not run. `--apply` inserts safe markers and refreshes the inventory. Review the diff
and rerun `--check`; a second `--apply` must not change files. Existing tags anywhere in a local
region count as markers, not as proposed insertions.

The check includes the current working tree, staged changes, and non-ignored new files. It also
accounts for deletion-only regions and formats that cannot carry a safe comment. The generated
inventory excludes its own diff so refreshing it does not manufacture another coverage change.

### Explicit manual exceptions

Keep code and data behavior unchanged. Use the documented manual-inventory mechanism whenever
a marker cannot be inserted safely. An exception must name the current location and its reason.

1. Tracked SQL migration contents remain unchanged because the updater hashes their exact bytes,
   including comments. Record their local ownership in the inventory instead of adding a banner.
2. JSON, TSV, patch artifacts, and Node version pins remain unchanged. Their formats or consumers
   do not permit inserting ordinary comments safely.
3. Never insert inside a string, continued macro, or unsafe expression boundary. List such regions
   for manual review during a merge. Prefer a safe enclosing boundary when one is clear.
4. Include deletion-only regions in the inventory. No replacement line does not mean no change.

Inventoried exceptions are reported separately from inline coverage. A successful marker check
does not prove that an upstream integration preserves runtime behavior.

Coverage is measured against the merge base, not against `upstream/master`. This matters: a plain
`git diff upstream/master..HEAD` also contains upstream's own commits, so it overstates what this
fork changed. Use the base:

```bash
MB=$(git merge-base upstream/master HEAD)
git diff --name-only $MB..HEAD
```

The commands above illustrate committed changes. The checker additionally includes working-tree
changes, so its inventory is current before a commit as well as after one.

## Testing the checker

The checker itself uses only Python's standard library. Run its tests and type check in an
isolated dependency environment, without changing the project's Python configuration:

```bash
uv run --no-project --with pytest pytest -q tests/python
uv run --no-project --with basedpyright --with pytest basedpyright \
  tools/plb_local_markers.py tests/python/test_plb_local_markers.py
ruff check --ignore EXE001 tools/plb_local_markers.py tests/python/test_plb_local_markers.py
ruff format --check tools/plb_local_markers.py tests/python/test_plb_local_markers.py
```

`EXE001` is excluded because the existing script has a shebang but is intentionally invoked
through Python with its existing non-executable Git mode. Marker backfills do not change that mode.

## Before the next upstream merge

Read this section first. It records decisions taken while the convention was built, so the merge
does not have to rediscover them.

1. **Regenerate first.** `uv run --no-project python tools/plb_local_markers.py --check` reports drift, and `--apply`
   refreshes markers and the inventory. Do this before starting the merge and again after, so the
   region counts in the file headers describe the tree you actually have.
2. **Resolve against the marker, not the diff alone.** A `PLB-LOCAL` region upstream has since
   rewritten needs a decision: keep local behaviour, adopt upstream's, or reconcile. Record which
   by updating the marker. Git's own conflict markers tell you which side is yours; `PLB-LOCAL`
   tells you why yours is that way, and it is also there in the regions that do NOT conflict,
   which is where an upstream change silently undoes local intent.
3. **Do not reformat upstream files before a merge.** 951 of this module's files violate the
   `.clang-format` it ships, so non conformance is upstream wide rather than a local lapse.
   Reformatting adds conflict surface wherever upstream also touched those lines, and the same
   reformat costs nothing once the merge has landed. Five files were reformatted deliberately on
   2026-08-25; anything broader waits.
4. **Check AzerothCore independently.** Its upstream is `mod-playerbots/azerothcore-wotlk:Playerbot`,
   not official AzerothCore `master`. Follow the root `docs/local-changes.md` and its separate
   inventory. The same checker supports `--repo` and `--upstream upstream/Playerbot`. Do not use
   historical ahead/behind counts as evidence of current merge readiness.
