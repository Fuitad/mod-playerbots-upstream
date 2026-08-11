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
CLASSIFICATIONS = {
    "extracted_implementation",
    "irreducible_compatibility",
    "retained_seam",
    "retired_vanilla",
}


class ManifestError(ValueError):
    pass


def parse_manifest(path: Path) -> dict[tuple[str, str], str]:
    """Load and validate the extraction manifest."""
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or tuple(lines[0].split("\t")) != HEADER:
        raise ManifestError(f"invalid header in {path}")

    inventory: dict[tuple[str, str], str] = {}
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split("\t")
        if len(fields) != len(HEADER) or any(not field for field in fields):
            raise ManifestError(f"line {line_number} must contain five nonempty fields")

        source_status, source_path, classification, _, _ = fields
        if classification not in CLASSIFICATIONS:
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


def source_inventory(repository: Path, base: str, source: str) -> set[tuple[str, str]]:
    """Return the exact path inventory changed between the preserved commits."""
    command = [
        "git",
        "-C",
        str(repository),
        "diff",
        "--name-status",
        f"{base}...{source}",
    ]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    inventory: set[tuple[str, str]] = set()
    for line in result.stdout.splitlines():
        fields = line.split("\t")
        if len(fields) != 2:
            raise ManifestError(f"unsupported git name status row: {line}")
        inventory.add((fields[0], fields[1]))
    return inventory


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
    parser.add_argument("--base", default=DEFAULT_BASE)
    parser.add_argument("--source", default=DEFAULT_SOURCE)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    try:
        manifest = parse_manifest(arguments.manifest)
        source = source_inventory(
            arguments.source_repository,
            arguments.base,
            arguments.source,
        )
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
    if missing or extra:
        return 1

    counts = Counter(manifest.values())
    summary = ", ".join(f"{name}={counts[name]}" for name in sorted(counts))
    print(f"{len(manifest)} source paths classified exactly ({summary})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
