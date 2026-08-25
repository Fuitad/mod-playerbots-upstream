# PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
# Prefer adding here over editing an upstream file. See docs/local-changes.md.

#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from collections import Counter
from pathlib import Path, PurePosixPath


DEFAULT_BASE = "a7b885d27134466dbc1c91d39b8241ea725a1bbb"
DEFAULT_SOURCE = "9b17934e875e65e408a7122f4ea9d0fb59a65da0"
HEADER = ("source_status", "path", "classification", "destination", "evidence")
FINAL_HEADER = ("status", "path", "classification", "evidence")
CLASSIFICATIONS = {
    "extracted_implementation",
    "irreducible_compatibility",
    "retained_seam",
    "retired_vanilla",
}
FINAL_CLASSIFICATIONS = {
    "irreducible_compatibility",
    "repository_contract",
    "retained_seam",
}


class ManifestError(ValueError):
    pass


def parse_inventory(
    path: Path,
    header: tuple[str, ...],
    classifications: set[str],
) -> dict[tuple[str, str], str]:
    """Load and validate a path inventory."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or tuple(lines[0].split("\t")) != header:
        raise ManifestError(f"invalid header in {path}")

    inventory: dict[tuple[str, str], str] = {}
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split("\t")
        if len(fields) != len(header) or any(not field for field in fields):
            raise ManifestError(
                f"line {line_number} must contain {len(header)} nonempty fields"
            )

        source_status, source_path, classification = fields[:3]
        if classification not in classifications:
            raise ManifestError(f"line {line_number} has unknown classification: {classification}")
        if source_status not in {"A", "M", "D"}:
            raise ManifestError(f"line {line_number} has unsupported status: {source_status}")

        normalized_path = PurePosixPath(source_path)
        if normalized_path.is_absolute() or ".." in normalized_path.parts:
            raise ManifestError(f"line {line_number} has unsafe path: {source_path}")

        key = (source_status, source_path)
        if key in inventory:
            raise ManifestError(f"line {line_number} duplicates: {source_status}\t{source_path}")
        inventory[key] = classification

    return inventory


def parse_manifest(path: Path) -> dict[tuple[str, str], str]:
    """Load and validate the preserved source extraction manifest."""
    return parse_inventory(path, HEADER, CLASSIFICATIONS)


def parse_final_inventory(path: Path) -> dict[tuple[str, str], str]:
    """Load and validate the final fork inventory."""
    return parse_inventory(path, FINAL_HEADER, FINAL_CLASSIFICATIONS)


def git_inventory(repository: Path, revision: str) -> set[tuple[str, str]]:
    """Return the exact status and path inventory for a Git diff."""
    command = ["git", "-C", str(repository), "diff", "--name-status", revision]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    inventory: set[tuple[str, str]] = set()
    for line in result.stdout.splitlines():
        fields = line.split("\t")
        if len(fields) != 2:
            raise ManifestError(f"unsupported git name status row: {line}")
        inventory.add((fields[0], fields[1]))
    return inventory


def source_inventory(repository: Path, base: str, source: str) -> set[tuple[str, str]]:
    """Return the exact path inventory changed between the preserved commits."""
    return git_inventory(repository, f"{base}...{source}")


def final_inventory(repository: Path, base: str) -> set[tuple[str, str]]:
    """Return the exact working fork inventory changed from upstream."""
    return git_inventory(repository, base)


def parse_arguments() -> argparse.Namespace:
    repository_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Verify the exact extraction path inventory")
    parser.add_argument(
        "--source-repository",
        type=Path,
        default=repository_root.parent / "mod-playerbots",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=repository_root / "docs" / "extraction-manifest.tsv",
    )
    parser.add_argument("--final-repository", type=Path, default=repository_root)
    parser.add_argument(
        "--final-manifest",
        type=Path,
        default=repository_root / "docs" / "final-fork-inventory.tsv",
    )
    parser.add_argument("--final-base", default=DEFAULT_BASE)
    parser.add_argument("--base", default=DEFAULT_BASE)
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        manifest = parse_manifest(arguments.manifest)
        final_manifest = parse_final_inventory(arguments.final_manifest)
        source = source_inventory(
            arguments.source_repository,
            arguments.base,
            arguments.source,
        )
        final = final_inventory(arguments.final_repository, arguments.final_base)
    except (ManifestError, OSError, subprocess.CalledProcessError) as error:
        print(f"extraction manifest check failed: {error}", file=sys.stderr)
        return 1

    declared = set(manifest)
    missing = sorted(source - declared)
    extra = sorted(declared - source)
    for source_status, source_path in missing:
        print(f"missing from manifest: {source_status}\t{source_path}", file=sys.stderr)
    for source_status, source_path in extra:
        print(f"not present in source diff: {source_status}\t{source_path}", file=sys.stderr)

    final_declared = set(final_manifest)
    final_missing = sorted(final - final_declared)
    final_extra = sorted(final_declared - final)
    for source_status, source_path in final_missing:
        print(f"missing from final inventory: {source_status}\t{source_path}", file=sys.stderr)
    for source_status, source_path in final_extra:
        print(f"not present in final diff: {source_status}\t{source_path}", file=sys.stderr)
    if missing or extra or final_missing or final_extra:
        return 1

    counts = Counter(manifest.values())
    summary = ", ".join(f"{name}={counts[name]}" for name in sorted(counts))
    print(f"{len(manifest)} source paths classified exactly ({summary})")
    final_counts = Counter(final_manifest.values())
    final_summary = ", ".join(
        f"{name}={final_counts[name]}" for name in sorted(final_counts)
    )
    print(f"{len(final_manifest)} final paths classified exactly ({final_summary})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
