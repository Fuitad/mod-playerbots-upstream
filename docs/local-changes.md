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

## Known gap

The convention starts from `revive-outcome` (2026-08-25). The fork was already 39 commits and
about 194 files diverged from `upstream/master` at that point, and none of that is marked.
Backfilling is worth doing and has not been done. Until it is, absence of a marker does NOT mean
a region is untouched by this fork.
