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
`grep -rn 'PLB-LOCAL(revive-outcome)' src` collects the whole thing.

## Prefer a local file over an upstream edit

The cheapest merge is the one that never conflicts. When a change can live in a local-only file
with a single call site in the upstream file, do that. `revive-outcome` is the worked example: the
classifier and its enum live in `src/Bot/Recovery/PlayerbotRecoveryPolicy.h`, which upstream has
never heard of, and the upstream actions carry one marked line each.

## On merging upstream

Resolve each conflict against the marker, not against the diff alone. A `PLB-LOCAL` region that
upstream has since rewritten needs a decision, not a reflex: keep the local behaviour, adopt
upstream's, or reconcile. Record which, by updating the marker.

`grep -rn 'PLB-LOCAL' src tests` lists every local region under the marker convention.

## Coverage as of the backfill (2026-08-25)

`tools/plb_local_markers.py --apply` backfilled the convention across the whole fork:

| | |
|---|---|
| Local only files bannered | 38 |
| Upstream files carrying a file delta header | 62 |
| Regions found in those files | 358 |
| Marked inline with the commit that introduced them | 351 |
| Unmarkable, listed in the inventory for hand checking | 5 |

The five unmarkable regions begin mid expression, so a comment could not be inserted above them
without splitting an argument list or an initialiser. The tool reports them rather than guessing,
and `docs/local-changes-inventory.md` names them. Those are the regions to read by hand during a
merge.

Coverage is measured against the merge base, not against `upstream/master`. This matters: a plain
`git diff upstream/master..HEAD` also contains upstream's own commits, so it overstates what this
fork changed. Use the base:

```bash
MB=$(git merge-base upstream/master HEAD)
git diff --name-only $MB..HEAD
```

At the time of the backfill that is 104 files, where `upstream/master..HEAD` reported 194.

## Before the next upstream merge

Read this section first. It records decisions taken while the convention was built, so the merge
does not have to rediscover them.

1. **Regenerate first.** `python3 tools/plb_local_markers.py --check` reports drift, and `--apply`
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
4. **The root AzerothCore checkout is a separate question.** Its `upstream` remote is
   `mod-playerbots/azerothcore-wotlk`, which as of 2026-08-25 is zero commits ahead of the local
   `Playerbot` branch, so there is nothing to merge from it. The local branch is about 1323 commits
   and 1734 files ahead of the shared base, which is a different scale of job from this module and
   has NOT been marked. Merging real upstream AzerothCore would mean adding a different remote,
   which is a deliberate act rather than an assumption.
