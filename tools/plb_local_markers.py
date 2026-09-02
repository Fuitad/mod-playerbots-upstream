#!/usr/bin/env python3
# PLB-LOCAL FILE. Local fork marker coverage and inventory maintenance.
"""Check or annotate local changes without changing executable or data payloads.

Compare the merge base with current files, including staged, unstaged and nonignored
new files. Check mode is read only. Exit 0 means covered, 1 means coverage or metadata
drift, and 2 means an invocation or Git failure. See docs/local-changes.md.
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import os
import re
import subprocess
import sys
import unicodedata
from dataclasses import dataclass, field
from pathlib import Path

FILE_TAG = "PLB-LOCAL FILE"
DELTA_TAG = "PLB-LOCAL UPSTREAM-FILE"
INVENTORY = "docs/local-changes-inventory.md"
MARKER = re.compile(r"PLB-LOCAL(?: (BEGIN|END))?\(([^)]+)\)")
COMMENT = {
    ".cpp": "//",
    ".cc": "//",
    ".c": "//",
    ".h": "//",
    ".hpp": "//",
    ".py": "#",
    ".sh": "#",
    ".bats": "#",
    ".cmake": "#",
    ".dist": "#",
    ".toml": "#",
    ".md": "<!--",
}


class InvocationError(Exception):
    """The repository cannot be audited reliably."""


@dataclass
class Arguments(argparse.Namespace):
    apply: bool = False
    check: bool = False
    repo: Path = Path(__file__).resolve().parent.parent
    upstream: str = "upstream/master"


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=repo,
        capture_output=True,
        check=False,
        env={**os.environ, "GIT_OPTIONAL_LOCKS": "0"},
    )
    if result.returncode:
        raise InvocationError(
            f"git {' '.join(args)}: {result.stderr.decode(errors='replace').strip()}"
        )
    return result.stdout.decode("utf-8", errors="surrogateescape")


def comment_style(path: str) -> str | None:
    if Path(path).name in {"CMakeLists.txt", ".gitignore"}:
        return "#"
    return COMMENT.get(Path(path).suffix)


def comment(style: str, text: str, indent: str = "") -> str:
    text = "".join(char for char in text if unicodedata.category(char) != "Cf")
    text = text.translate(
        str.maketrans(
            {
                "\u2014": ",",
                "\u2013": ",",
                "\u2018": "'",
                "\u2019": "'",
                "\u201c": '"',
                "\u201d": '"',
                "\u2026": "...",
                "\u2192": "->",
                "\u2190": "<-",
                "\u2194": "<->",
                "\u2022": "*",
                "\u2713": "ok",
                "\u2717": "failed",
            }
        )
    )
    if style == "<!--":
        text = text.replace("--", "- -")
    suffix = " -->" if style == "<!--" else ""
    marker = MARKER.match(text)
    minimum = marker.end() + 5 if marker else 12
    indent = indent[: max(0, 120 - len(style) - 1 - len(suffix) - minimum)]
    available = 120 - len(indent) - len(style) - 1 - len(suffix)
    if len(text) > available:
        text = text[: available - 3].rstrip() + "..."
    return f"{indent}{style} {text}{suffix}\n"


def lexical_boundaries(lines: list[str], path: str) -> tuple[list[bool], set[int]]:
    """Find safe whole-line comment boundaries, conservatively, without parsing code."""
    suffix = Path(path).suffix
    cpp = comment_style(path) == "//"
    shell = suffix in {".sh", ".bats"}
    cmake = suffix == ".cmake" or Path(path).name == "CMakeLists.txt"
    markdown = suffix == ".md"
    state = ""
    end = ""
    continued = False
    heredocs: list[str] = []
    boundaries: list[bool] = []
    comments: set[int] = set()
    fence = ""
    for number, line in enumerate(lines):
        boundaries.append(not state and not continued and not heredocs and not fence)
        if heredocs:
            if line.strip() == heredocs[0]:
                _ = heredocs.pop(0)
            continue
        if markdown:
            match = re.match(r"\s*(`{3,}|~{3,})", line)
            if match:
                if not fence:
                    fence = match[1][0]
                elif match[1][0] == fence:
                    fence = ""
                continue
            if fence:
                continue
        i = 0
        while i < len(line):
            if state:
                if state == "comment":
                    comments.add(number)
                close = line.find(end, i)
                if close == -1:
                    break
                if state == "quote" and close > 0:
                    escapes = len(line[:close]) - len(line[:close].rstrip("\\"))
                    if escapes % 2:
                        i = close + 1
                        continue
                i = close + len(end)
                state = ""
                continue
            if cpp and line.startswith("//", i):
                comments.add(number)
                break
            if cpp and line.startswith("/*", i):
                state, end = "comment", "*/"
                comments.add(number)
                i += 2
                continue
            if markdown and line.startswith("<!--", i):
                state, end = "comment", "-->"
                comments.add(number)
                i += 4
                continue
            if not cpp and not markdown and line[i] == "#":
                comments.add(number)
                if not (cmake and re.match(r"#\[=*\[", line[i:])):
                    break
            if cpp and (raw := re.match(r'R"([^\s()\\]{0,16})\(', line[i:])):
                state, end = "raw", f'){raw[1]}"'
                i += len(raw[0])
                continue
            if cmake and (bracket := re.match(r"#?\[(=*)\[", line[i:])):
                state, end = "bracket", f"]{bracket[1]}]"
                i += len(bracket[0])
                continue
            if shell and line.startswith("<<", i):
                here = re.match(r"<<-?\s*(['\"]?)([A-Za-z_][A-Za-z_0-9]*)\1", line[i:])
                if here:
                    heredocs.append(here[2])
                    i += len(here[0])
                    continue
            if not cpp and not shell and line[i : i + 3] in {'"""', "'''"}:
                state, end = "raw", line[i : i + 3]
                i += 3
                continue
            if not markdown and line[i] in "\"'`":
                if cpp and line[i] == "'" and i and line[i - 1].isdigit():
                    i += 1
                    continue
                state, end = "quote", line[i]
                i += 1
                continue
            i += 2 if line[i] == "\\" else 1
        continued = line.rstrip("\r\n").endswith("\\")
    boundaries.append(not state and not continued and not heredocs and not fence)
    return boundaries, comments


def header_indices(lines: list[str], comments: set[int]) -> set[int]:
    indices: set[int] = set()
    for i, line in enumerate(lines):
        if (
            i not in comments
            or DELTA_TAG not in line
            or not line.lstrip().startswith(("//", "#", "<!--"))
        ):
            continue
        indices.add(i)
        for j in range(i + 1, min(i + 3, len(lines))):
            if j in comments and (
                "Each is tagged PLB-LOCAL" in lines[j]
                or "authoritative list" in lines[j]
            ):
                indices.add(j)
    return indices


def coverage_marker(line: str) -> re.Match[str] | None:
    match = MARKER.search(line)
    return match if match and not any(char in match[2] for char in "<>") else None


def payload(lines: list[str], comments: set[int]) -> tuple[list[str], list[int]]:
    excluded = header_indices(lines, comments)
    # The legacy generator inserted a separator before its header in addition to
    # the upstream separator after it. Keep the bytes but do not count that
    # doubled metadata separator as a local source region, even after refresh.
    for start in sorted(excluded):
        if start - 1 in excluded:
            continue
        end = start
        while end + 1 in excluded:
            end += 1
        if (
            start > 0
            and end + 1 < len(lines)
            and not lines[start - 1].strip()
            and not lines[end + 1].strip()
        ):
            excluded.add(start - 1)
    for i in comments:
        stripped = lines[i].lstrip()
        standalone = stripped.startswith(("//", "#", "/*", "*", "<!--"))
        if standalone and (coverage_marker(lines[i]) or FILE_TAG in lines[i]):
            excluded.add(i)
            j = i + 1
            while j in comments and lines[j].lstrip().startswith(
                ("// Upstream:", "# Upstream:", "<!-- Upstream:")
            ):
                excluded.add(j)
                j += 1
    positions = [i for i in range(len(lines)) if i not in excluded]
    return [lines[i] for i in positions], positions


def protected_prefix(lines: list[str], path: str) -> int:
    at = 1 if lines and lines[0].startswith("#!") else 0
    if Path(path).suffix == ".py":
        for i in range(min(2, len(lines))):
            if re.match(r"\s*#.*coding[:=]\s*[-\w.]+", lines[i]):
                at = max(at, i + 1)
    return at


def banner_position(lines: list[str], path: str) -> int:
    at = protected_prefix(lines, path)
    if at < len(lines) and lines[at].lstrip().startswith("/*"):
        for i in range(at, len(lines)):
            if "*/" in lines[i]:
                return i + 1
    return at


def safe_header_position(lines: list[str], path: str, safe: list[bool]) -> int:
    at = banner_position(lines, path)
    return at if safe[at] else protected_prefix(lines, path)


def local_provenance(repo: Path, path: str) -> dict[int, tuple[str, str]]:
    provenance: dict[int, tuple[str, str]] = {}
    sha = ""
    number = 0
    subject = ""
    for line in git(repo, "blame", "--line-porcelain", "--", path).splitlines():
        if match := re.match(r"^([0-9a-f]{40}) \d+ (\d+)", line):
            sha, number = match[1], int(match[2])
            subject = ""
        elif line.startswith("summary "):
            subject = line.removeprefix("summary ")
        elif line.startswith("\t"):
            provenance[number - 1] = (sha, subject)
    return provenance


@dataclass
class Region:
    start: int
    end: int
    base_start: int
    base_end: int
    status: str = ""
    reason: str = ""

    def location(self) -> str:
        current = (
            f"current {self.start + 1}-{self.end}"
            if self.end > self.start
            else f"current boundary after line {self.start}"
        )
        old = (
            f"base {self.base_start + 1}-{self.base_end}"
            if self.base_end > self.base_start
            else f"base boundary after line {self.base_start}"
        )
        return f"{current}; {old}"


@dataclass
class FileReport:
    path: str
    local_only: bool
    digest: str
    regions: list[Region] = field(default_factory=list)
    manual: str = ""
    banner_missing: bool = False
    header_stale: bool = False
    proposed: str | None = None


def marker_covers(lines: list[str], comments: set[int], start: int, end: int) -> bool:
    comments = comments - header_indices(lines, comments)
    active: set[str] = set()
    for i in range(start):
        if i not in comments or not (match := coverage_marker(lines[i])):
            continue
        if match[1] == "BEGIN":
            active.add(match[2])
        elif match[1] == "END":
            active.discard(match[2])
    if active:
        return True
    if any(i in comments and coverage_marker(lines[i]) for i in range(start, end)):
        return True
    i = start - 1
    while i >= 0 and (i in comments or not lines[i].strip()):
        if i in comments and (match := coverage_marker(lines[i])) and match[1] != "END":
            return True
        i -= 1
    return False


def audit_file(
    repo: Path,
    base: str,
    path: str,
    base_paths: set[str],
    upstream_paths: set[str],
    local_shas: set[str],
) -> FileReport:
    full = repo / path
    local_only = path not in base_paths and path not in upstream_paths
    if full.is_symlink() or full.resolve() != full or not full.is_file():
        reason = "symlink or submodule preserved" if full.exists() else "file deletion"
        return FileReport(path, local_only, "absent", manual=reason)
    data = full.read_bytes()
    report = FileReport(path, local_only, hashlib.sha256(data).hexdigest()[:16])
    if Path(path).suffix == ".sql":
        report.manual = "migration checksum preserved"
        return report
    style = comment_style(path)
    if style is None:
        report.manual = "format exception (no safe comment syntax; contents preserved)"
        return report
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        report.manual = "non UTF-8 contents preserved"
        return report
    if b"\x00" in data or b"\r" in data:
        report.manual = "binary or non-LF contents preserved"
        return report
    lines = text.splitlines(keepends=True)
    safe, comments = lexical_boundaries(lines, path)
    if local_only:
        report.banner_missing = not any(FILE_TAG in lines[i] for i in comments)
        if report.banner_missing:
            at = safe_header_position(lines, path, safe)
            lines.insert(
                at,
                comment(style, f"{FILE_TAG}. Local only; see docs/local-changes.md."),
            )
            report.proposed = "".join(lines)
        return report
    original = (
        git(repo, "show", f"{base}:{path}").splitlines(keepends=True)
        if path in base_paths
        else []
    )
    _, original_comments = lexical_boundaries(original, path)
    old, _ = payload(original, original_comments)
    new, positions = payload(lines, comments)
    edits: dict[int, list[str]] = {}
    provenance: dict[int, tuple[str, str]] | None = None
    for operation, old_start, old_end, new_start, new_end in difflib.SequenceMatcher(
        None, old, new, autojunk=False
    ).get_opcodes():
        if operation == "equal":
            continue
        start = positions[new_start] if new_start < len(positions) else len(lines)
        end = positions[new_end - 1] + 1 if new_end > new_start else start
        region = Region(start, end, old_start, old_end)
        report.regions.append(region)
        if operation == "delete":
            region.status, region.reason = (
                "manual",
                "deletion-only region; inspect the base lines during a merge",
            )
        elif marker_covers(lines, comments, start, end):
            region.status = "covered"
        elif (
            start < protected_prefix(lines, path)
            or not safe[start]
            or not safe[end]
            or (end == len(lines) and lines and not lines[-1].endswith("\n"))
        ):
            region.status, region.reason = (
                "manual",
                "unsafe multiline string, comment, macro or continuation boundary",
            )
        else:
            region.status = "missing"
            indent = lines[start][: len(lines[start]) - len(lines[start].lstrip(" \t"))]
            if provenance is None:
                provenance = local_provenance(repo, path) if path in base_paths else {}
            sha, subject = provenance.get(start, ("", ""))
            tag = sha[:12] if sha in local_shas else "working-tree"
            label = " BEGIN" if new_end - new_start > 4 else ""
            description = (
                f"{subject}." if sha in local_shas else "Uncommitted local change."
            )
            edits.setdefault(start, []).append(
                comment(style, f"PLB-LOCAL{label}({tag}): {description}", indent)
            )
            excerpt = " ".join(line.strip() for line in old[old_start:old_end])
            excerpt = excerpt[:180] + ("..." if len(excerpt) > 180 else "")
            upstream = excerpt or "No corresponding block at the merge base."
            edits[start].append(
                comment(style, f"Upstream: {upstream} (base {base[:12]}).", indent)
            )
            if label:
                edits.setdefault(end, []).insert(
                    0, comment(style, f"PLB-LOCAL END({tag})", indent)
                )
    expected = comment(
        style,
        f"{DELTA_TAG}: this fork changes {len(report.regions)} region(s) of this upstream file.",
    )
    headers = header_indices(lines, comments)
    report.header_stale = "".join(lines[i] for i in sorted(headers)) != expected
    if edits or report.header_stale:
        updated: list[str] = []
        at = min(headers) if headers else safe_header_position(lines, path, safe)
        for i in range(len(lines) + 1):
            if i == at:
                updated.append(expected)
            updated.extend(edits.get(i, []))
            if i < len(lines) and i not in headers:
                updated.append(lines[i])
        report.proposed = "".join(updated)
    return report


def audit(repo: Path, base: str, upstream: str) -> list[FileReport]:
    base_paths = set(git(repo, "ls-tree", "-rz", "--name-only", base).split("\x00"))
    upstream_paths = set(
        git(repo, "ls-tree", "-rz", "--name-only", upstream).split("\x00")
    )
    paths = set(
        git(repo, "diff", "--name-only", "-z", "--no-renames", base, "--").split("\x00")
    )
    paths.update(
        git(repo, "ls-files", "--others", "--exclude-standard", "-z").split("\x00")
    )
    local_shas = set(git(repo, "rev-list", f"{base}..HEAD").split())
    return [
        audit_file(repo, base, path, base_paths, upstream_paths, local_shas)
        for path in sorted(paths - {"", INVENTORY})
    ]


def inventory(reports: list[FileReport], base: str, upstream: str) -> str:
    regions = [region for report in reports for region in report.regions]
    out = [
        "<!-- PLB-LOCAL FILE. Generated local fork coverage inventory. -->",
        "# Local change inventory",
        "",
        f"Generated by `tools/plb_local_markers.py` against merge base `{base}` with `{upstream}`.",
        "The comparison includes the current index, working tree and nonignored new files.",
        "The inventory excludes its own contents. File fingerprints detect stale manual exceptions.",
        "Manual exceptions are accounted for, not automatically modified or proof of merge compatibility.",
        "",
        f"Local only files: **{sum(report.local_only for report in reports)}**.",
        f"Upstream files changed: **{sum(not report.local_only for report in reports)}**.",
        (
            f"Regions: **{len(regions)}**. Covered inline: **{sum(r.status == 'covered' for r in regions)}**. "
            f"Missing markers: **{sum(r.status == 'missing' for r in regions)}**."
        ),
        "",
    ]
    for heading, local_only in [("Upstream files", False), ("Local only files", True)]:
        out.extend([f"## {heading}", ""])
        for report in reports:
            if report.local_only != local_only:
                continue
            out.extend(
                [f"### `{report.path}`", f"Content fingerprint: `{report.digest}`."]
            )
            if report.manual:
                out.append(f"Manual exception: {report.manual}.")
            elif report.local_only:
                out.append(
                    "Banner: missing." if report.banner_missing else "Banner: covered."
                )
            else:
                out.append(
                    f"{len(report.regions)} region(s). Header: {'stale' if report.header_stale else 'current'}."
                )
                for region in report.regions:
                    detail = f". {region.reason}" if region.reason else ""
                    out.append(f"1. {region.location()}: {region.status}{detail}.")
            out.append("")
    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    _ = modes.add_argument(
        "--apply",
        action="store_true",
        help="insert safe missing markers and refresh the inventory",
    )
    _ = modes.add_argument(
        "--check", action="store_true", help="read-only check (the default)"
    )
    _ = parser.add_argument(
        "--repo", type=Path, default=Path(__file__).resolve().parent.parent
    )
    _ = parser.add_argument(
        "--upstream",
        default="upstream/master",
        help="upstream ref, e.g. upstream/Playerbot for core",
    )
    args = parser.parse_args(namespace=Arguments())
    try:
        repo = args.repo.resolve()
        if (
            not repo.is_dir()
            or Path(git(repo, "rev-parse", "--show-toplevel").strip()).resolve() != repo
        ):
            raise InvocationError("--repo must name an existing Git repository root")
        if git(repo, "ls-files", "-u"):
            raise InvocationError(
                "resolve the existing Git conflicts before checking marker coverage"
            )
        base = git(repo, "merge-base", args.upstream, "HEAD").strip()
        target = repo / INVENTORY
        if target.resolve() != target:
            raise InvocationError("the inventory path must not contain symlinks")
        reports = audit(repo, base, args.upstream)
        if args.apply:
            for report in reports:
                if report.proposed is not None:
                    _ = (repo / report.path).write_text(
                        report.proposed, encoding="utf-8"
                    )
            reports = audit(repo, base, args.upstream)
        expected = inventory(reports, base, args.upstream)
        stale_inventory = (
            not target.is_file() or target.read_text(encoding="utf-8") != expected
        )
        if args.apply and stale_inventory:
            target.parent.mkdir(parents=True, exist_ok=True)
            _ = target.write_text(expected, encoding="utf-8")
            stale_inventory = False
        regions = [region for report in reports for region in report.regions]
        missing = sum(report.banner_missing for report in reports) + sum(
            region.status == "missing" for region in regions
        )
        stale_headers = sum(report.header_stale for report in reports)
        print(f"local-only files      : {sum(r.local_only for r in reports)}")
        print(f"upstream files changed: {sum(not r.local_only for r in reports)}")
        print(f"regions               : {len(regions)}")
        print(f"covered inline        : {sum(r.status == 'covered' for r in regions)}")
        print(f"missing markers       : {missing}")
        print(
            f"manual exceptions     : {sum(bool(r.manual) for r in reports) + sum(r.status == 'manual' for r in regions)}"
        )
        print(f"stale file headers    : {stale_headers}")
        print(f"inventory             : {'stale' if stale_inventory else 'current'}")
        return int(bool(missing or stale_headers or stale_inventory))
    except (InvocationError, OSError, UnicodeError) as error:
        print(f"Marker check failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
