#!/usr/bin/env python3
"""Mark this fork's local changes so an upstream merge can recognise them.

See docs/local-changes.md for the convention. This tool backfills it and keeps it true:

  python3 tools/plb_local_markers.py --check    report coverage, change nothing
  python3 tools/plb_local_markers.py --apply    insert missing markers, rewrite the inventory

Both modes are idempotent. A marker already present is never duplicated, and a hunk whose
insertion point cannot be proven safe is left alone and reported instead of guessed at.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

FILE_TAG = "PLB-LOCAL FILE"
DELTA_TAG = "PLB-LOCAL UPSTREAM-FILE"
HUNK_TAG = "PLB-LOCAL"

# A line ending in one of these is mid-expression, so the line after it is not a statement
# boundary and a comment inserted there could land inside an argument list or an initialiser.
CONTINUATION = re.compile(r"[,(\[{<]$|(&&|\|\||[+\-*/%=?:.]|->|::)$")
SAFE_PREV = re.compile(r"[;{}:]$|^\s*$|^\s*(//|/\*|\*)")

COMMENT = {".cpp": "//", ".h": "//", ".hpp": "//", ".py": "#", ".sh": "#", ".cmake": "#", ".sql": "--"}


def git(*args: str, cwd: Path) -> str:
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, check=False).stdout


@dataclass
class FileReport:
    path: str
    local_only: bool
    hunks: int = 0
    marked: int = 0
    skipped: list[str] = field(default_factory=list)
    commits: Counter = field(default_factory=Counter)


def merge_base(repo: Path) -> str:
    return git("merge-base", "upstream/master", "HEAD", cwd=repo).strip()


def changed_files(repo: Path, base: str) -> list[str]:
    return [f for f in git("diff", "--name-only", f"{base}..HEAD", cwd=repo).splitlines() if f]


def is_local_only(repo: Path, base: str, path: str) -> bool:
    r = subprocess.run(["git", "cat-file", "-e", f"{base}:{path}"], cwd=repo, capture_output=True)
    return r.returncode != 0


def added_line_ranges(repo: Path, base: str, path: str) -> list[tuple[int, int]]:
    """Line ranges in the CURRENT file that this fork added or changed."""
    ranges: list[tuple[int, int]] = []
    for line in git("diff", "-U0", f"{base}..HEAD", "--", path, cwd=repo).splitlines():
        if not line.startswith("@@"):
            continue
        m = re.search(r"\+(\d+)(?:,(\d+))?", line)
        if not m:
            continue
        start = int(m.group(1))
        count = int(m.group(2)) if m.group(2) is not None else 1
        if count == 0:  # pure deletion: nothing in the current file to mark
            continue
        ranges.append((start, start + count - 1))
    return ranges


def blame_commits(repo: Path, path: str, local_shas: set[str]) -> dict[int, tuple[str, str]]:
    """line number -> (short sha, subject) for lines authored by a local commit."""
    out = git("blame", "--line-porcelain", "HEAD", "--", path, cwd=repo)
    result: dict[int, tuple[str, str]] = {}
    sha = None
    summary = ""
    lineno = 0
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]{40}) \d+ (\d+)", line)
        if m:
            sha, lineno = m.group(1), int(m.group(2))
            summary = ""
        elif line.startswith("summary "):
            summary = line[len("summary "):]
        elif line.startswith("\t") and sha is not None:
            if sha in local_shas:
                result[lineno] = (sha[:12], summary)
    return result


def insert_marker_safe(lines: list[str], idx: int) -> bool:
    """True when a comment line may be inserted before lines[idx] without splitting an expression."""
    if idx <= 0:
        return False
    prev = lines[idx - 1].rstrip()
    if SAFE_PREV.search(prev):
        return True
    return not CONTINUATION.search(prev)


def licence_end(lines: list[str]) -> int:
    """Index just past the leading block comment, or 0."""
    if not lines or not lines[0].lstrip().startswith("/*"):
        return 0
    for i, line in enumerate(lines[:40]):
        if "*/" in line:
            return i + 1
    return 0


def apply_file(repo: Path, base: str, path: str, local_shas: set[str], apply: bool) -> FileReport:
    ext = Path(path).suffix
    local_only = is_local_only(repo, base, path)
    rep = FileReport(path=path, local_only=local_only)
    full = repo / path
    if ext not in COMMENT or not full.exists():
        return rep

    text = full.read_text()
    if FILE_TAG in text or DELTA_TAG in text:
        already_headed = True
    else:
        already_headed = False
    lines = text.splitlines(keepends=True)
    c = COMMENT[ext]

    ranges = added_line_ranges(repo, base, path)
    rep.hunks = len(ranges)
    blame = blame_commits(repo, path, local_shas) if (ranges and not local_only) else {}

    if local_only:
        if not already_headed and apply:
            at = licence_end(lines)
            banner = (f"{c} {FILE_TAG}. Not present upstream, so it can never conflict on a merge.\n"
                      f"{c} Prefer adding here over editing an upstream file. See docs/local-changes.md.\n\n")
            lines.insert(at, ("\n" if at else "") + banner.rstrip("\n") + "\n")
            full.write_text("".join(lines))
        rep.marked = 0 if already_headed else 1
        return rep

    # Upstream file: mark each hunk bottom up so earlier line numbers stay valid.
    inserted = 0
    for start, end in sorted(ranges, reverse=True):
        idx = start - 1
        if idx >= len(lines):
            rep.skipped.append(f"{start}-{end} (past EOF)")
            continue
        window = "".join(lines[max(0, idx - 3):idx + 1])
        if HUNK_TAG in window:
            continue
        sha, subject = blame.get(start, ("", ""))
        if sha:
            rep.commits[f"{sha} {subject}"] += 1
        if not insert_marker_safe(lines, idx):
            rep.skipped.append(f"{start}-{end} (not a statement boundary)")
            continue
        indent = re.match(r"[ \t]*", lines[idx]).group(0)
        why = f" {subject}" if subject else ""
        marker = f"{indent}{c} {HUNK_TAG}({sha or 'fork'}):{why}\n"
        if apply:
            lines.insert(idx, marker)
        inserted += 1
    rep.marked = inserted

    if apply:
        if not already_headed:
            at = licence_end(lines)
            hdr = (f"{c} {DELTA_TAG}: this fork changes {len(ranges)} region(s) of this upstream file.\n"
                   f"{c} Each is tagged {HUNK_TAG}(<sha>) where a marker could be placed safely; run\n"
                   f"{c} tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.\n\n")
            lines.insert(at, ("\n" if at else "") + hdr.rstrip("\n") + "\n")
        full.write_text("".join(lines))
    return rep


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    apply = args.apply

    repo = Path(__file__).resolve().parent.parent
    base = merge_base(repo)
    if not base:
        print("No merge base with upstream/master. Add the upstream remote and fetch.", file=sys.stderr)
        return 2
    local_shas = set(git("rev-list", f"{base}..HEAD", cwd=repo).split())

    reports = [apply_file(repo, base, f, local_shas, apply) for f in changed_files(repo, base)]
    reports = [r for r in reports if r.hunks or r.local_only]

    total_hunks = sum(r.hunks for r in reports if not r.local_only)
    total_marked = sum(r.marked for r in reports if not r.local_only)
    total_skipped = sum(len(r.skipped) for r in reports)
    banners = sum(1 for r in reports if r.local_only)

    inventory = ["# Local change inventory", "",
                 f"Generated by `tools/plb_local_markers.py` against merge base `{base[:12]}`.",
                 "Regenerate after every upstream merge and whenever local changes land.", "",
                 f"- Local only files: **{banners}**",
                 f"- Upstream files this fork changes: **{sum(1 for r in reports if not r.local_only)}**",
                 f"- Regions in those files: **{total_hunks}**, marked inline: **{total_marked}**, "
                 f"unmarkable: **{total_skipped}**", "",
                 "An unmarkable region is one whose first line is mid expression, so a comment could not",
                 "be inserted without splitting it. Those are listed below and are the ones to check by",
                 "hand during a merge.", "", "## Upstream files", ""]
    for r in sorted((r for r in reports if not r.local_only), key=lambda r: -r.hunks):
        inventory.append(f"### `{r.path}`")
        inventory.append(f"{r.hunks} region(s), {r.marked} marked.")
        if r.commits:
            inventory.append("")
            for entry, n in r.commits.most_common():
                inventory.append(f"- {entry} ({n} region(s))")
        if r.skipped:
            inventory.append("")
            inventory.append(f"Unmarkable, check by hand: {', '.join(r.skipped)}")
        inventory.append("")
    inventory.append("## Local only files")
    inventory.append("")
    for r in sorted((r for r in reports if r.local_only), key=lambda r: r.path):
        inventory.append(f"- `{r.path}`")
    inventory.append("")

    if apply:
        (repo / "docs" / "local-changes-inventory.md").write_text("\n".join(inventory))

    print(f"local-only files      : {banners}")
    print(f"upstream files changed: {sum(1 for r in reports if not r.local_only)}")
    print(f"regions               : {total_hunks}")
    print(f"marked inline         : {total_marked}")
    print(f"unmarkable (by hand)  : {total_skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
