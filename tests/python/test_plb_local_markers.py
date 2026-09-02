# PLB-LOCAL FILE. Local marker checker regression tests.
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

CHECKER = Path(__file__).parents[2] / "tools" / "plb_local_markers.py"
INVENTORY = "docs/local-changes-inventory.md"


def git(repo: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(repo), *args], check=True, text=True, capture_output=True
    ).stdout.strip()


def commit(repo: Path) -> None:
    _ = git(repo, "add", ".")
    _ = git(
        repo,
        "-c",
        "user.name=Marker Test",
        "-c",
        "user.email=marker@example.invalid",
        "commit",
        "-qm",
        "fixture",
    )


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    _ = git(tmp_path, "init", "-b", "main")
    _ = (tmp_path / "sample.cpp").write_text(
        "int base = 1;\nint kept = 2;\n", encoding="utf-8"
    )
    commit(tmp_path)
    _ = git(tmp_path, "update-ref", "refs/remotes/upstream/master", "HEAD")
    return tmp_path


def run(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(CHECKER), "--repo", str(repo), *args],
        text=True,
        capture_output=True,
        check=False,
    )


def apply_and_check(repo: Path, *args: str) -> str:
    result = run(repo, "--apply", *args)
    assert result.returncode == 0, result.stdout + result.stderr
    checked = run(repo, "--check", *args)
    assert checked.returncode == 0, checked.stdout + checked.stderr
    snapshot = {
        path: path.read_bytes()
        for path in repo.rglob("*")
        if path.is_file() and ".git" not in path.parts
    }
    repeated = run(repo, "--apply", *args)
    assert repeated.returncode == 0, repeated.stdout + repeated.stderr
    assert snapshot == {path: path.read_bytes() for path in snapshot}
    return (repo / INVENTORY).read_text(encoding="utf-8")


def test_check_is_read_only_and_apply_covers_dirty_staged_and_new_files(
    repo: Path,
) -> None:
    sample = repo / "sample.cpp"
    _ = sample.write_text("int base = 9;\nint kept = 2;\n", encoding="utf-8")
    _ = git(repo, "add", "sample.cpp")
    _ = sample.write_text(
        "// shifted in worktree\nint base = 9;\nint kept = 2;\n", encoding="utf-8"
    )
    _ = (repo / "new.py").write_text(
        "#!/usr/bin/env python3\n# coding: utf-8\nprint('hello')\n", encoding="utf-8"
    )
    before = sample.read_bytes()
    index = git(repo, "diff", "--cached")
    checked = run(repo, "--check")
    assert checked.returncode == 1, checked.stderr
    assert sample.read_bytes() == before
    assert not (repo / INVENTORY).exists()
    inventory = apply_and_check(repo)
    assert "new.py" in inventory and "sample.cpp" in inventory
    assert git(repo, "diff", "--cached") == index
    assert (
        (repo / "new.py")
        .read_text()
        .startswith("#!/usr/bin/env python3\n# coding: utf-8\n# PLB-LOCAL FILE")
    )
    assert "missing markers       : 0" in run(repo, "--check").stdout


def test_existing_marker_inside_long_hunk_and_banner_are_not_duplicated(
    repo: Path,
) -> None:
    source = "\n".join(f"int local_{i} = {i};" for i in range(10)) + "\n"
    source = source.replace(
        "int local_7",
        "// PLB-LOCAL(hand-tag): retain the hand-written reason.\nint local_7",
    )
    _ = (repo / "sample.cpp").write_text(source, encoding="utf-8")
    local = "#!/bin/sh\n# PLB-LOCAL FILE. Existing banner.\necho done\n"
    _ = (repo / "local.sh").write_text(local, encoding="utf-8")
    _ = apply_and_check(repo)
    output = (repo / "sample.cpp").read_text()
    assert output.count("PLB-LOCAL(hand-tag)") == 1
    assert "PLB-LOCAL BEGIN(" not in output
    assert (repo / "local.sh").read_text() == local


def test_deleted_files_deletion_only_hunks_and_unsupported_formats_are_accounted(
    repo: Path,
) -> None:
    _ = (repo / "gone.cpp").write_text("int gone = 1;\n", encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    (repo / "gone.cpp").unlink()
    _ = (repo / "sample.cpp").write_text("int kept = 2;\n", encoding="utf-8")
    contents = {
        "data.json": '{"a": 1}\n',
        "rows.tsv": "a\tb\n",
        "fix.patch": "+line\n",
        ".nvmrc": "24\n",
        "update.sql": "SELECT 1;\n",
    }
    for path, content in contents.items():
        _ = (repo / path).write_text(content, encoding="utf-8")
    commit(repo)
    inventory = apply_and_check(repo)
    assert "deletion" in inventory.lower()
    assert "migration checksum preserved" in inventory
    for path, content in contents.items():
        assert path in inventory
        assert (repo / path).read_text() == content
    assert "gone.cpp" in inventory
    _ = (repo / "data.json").write_text('{"a": 2}\n', encoding="utf-8")
    assert run(repo, "--check").returncode == 1


@pytest.mark.parametrize(
    ("name", "before", "after"),
    [
        (
            "raw.cpp",
            'auto text = R"tag(first\nold\nlast)tag";\n',
            'auto text = R"tag(first\nnew\nlast)tag";\n',
        ),
        (
            "block.cpp",
            "/* first\nold\nlast */\nint n = 1;\n",
            "/* first\nnew\nlast */\nint n = 1;\n",
        ),
        (
            "macro.h",
            "#define VALUE \\\n    old \\\n    + 1\n",
            "#define VALUE \\\n    newer \\\n    + 1\n",
        ),
        (
            "triple.py",
            'text = """first\nold\nlast"""\n',
            'text = """first\nnew\nlast"""\n',
        ),
        (
            "heredoc.sh",
            "#!/bin/sh\ncat <<'EOF'\nold\nEOF\n",
            "#!/bin/sh\ncat <<'EOF'\nnew\nEOF\n",
        ),
        (
            "bracket.cmake",
            "set(TEXT [[first\nold\nlast]])\n",
            "set(TEXT [[first\nnew\nlast]])\n",
        ),
    ],
)
def test_unsafe_regions_preserve_payload_and_are_inventoried(
    repo: Path, name: str, before: str, after: str
) -> None:
    path = repo / name
    _ = path.write_text(before, encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = path.write_text(after, encoding="utf-8")
    inventory = apply_and_check(repo)
    without_header = "".join(
        line
        for line in path.read_text().splitlines(keepends=True)
        if "PLB-LOCAL UPSTREAM-FILE" not in line
    )
    assert without_header == after
    assert "unsafe" in inventory.lower()
    assert name in inventory


@pytest.mark.parametrize(
    "name",
    [
        "settings.dist",
        "CMakeLists.txt",
        "checks.bats",
        "guide.md",
        "config.toml",
        ".gitignore",
    ],
)
def test_local_comment_formats(repo: Path, name: str) -> None:
    _ = (repo / name).write_text("plain\n", encoding="utf-8")
    _ = apply_and_check(repo)
    source = (repo / name).read_text()
    assert source.startswith(
        "<!-- PLB-LOCAL FILE" if name.endswith(".md") else "# PLB-LOCAL FILE"
    )


def test_current_metadata_and_core_upstream_flag(repo: Path) -> None:
    _ = git(repo, "update-ref", "refs/remotes/upstream/Playerbot", "HEAD")
    _ = (repo / "sample.cpp").write_text(
        "int base = 3;\nint kept = 2;\n", encoding="utf-8"
    )
    inventory = apply_and_check(repo, "--upstream", "upstream/Playerbot")
    assert "upstream/Playerbot" in inventory
    path = repo / "sample.cpp"
    _ = path.write_text(
        path.read_text().replace("1 region(s)", "999 region(s)"), encoding="utf-8"
    )
    assert run(repo, "--check", "--upstream", "upstream/Playerbot").returncode == 1
    _ = apply_and_check(repo, "--upstream", "upstream/Playerbot")


def test_upstream_only_commits_are_not_local_changes(repo: Path) -> None:
    _ = git(repo, "checkout", "-b", "incoming")
    _ = (repo / "upstream_only.cpp").write_text("int incoming = 1;\n", encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = git(repo, "checkout", "main")
    _ = (repo / "local.cpp").write_text("int local = 1;\n", encoding="utf-8")
    inventory = apply_and_check(repo)
    assert "local.cpp" in inventory
    assert "upstream_only.cpp" not in inventory


def test_bad_repository_git_ref_and_conflicting_flags_fail_as_invocations(
    repo: Path, tmp_path: Path
) -> None:
    assert run(repo, "--check", "--upstream", "missing/ref").returncode == 2
    assert run(repo, "--check", "--apply").returncode == 2
    assert run(tmp_path / "missing", "--check").returncode == 2


def test_new_long_region_gets_matching_begin_end_markers(repo: Path) -> None:
    source = "\n".join(f"int field_{i} = {i};" for i in range(8)) + "\n"
    _ = (repo / "sample.cpp").write_text(source, encoding="utf-8")
    _ = apply_and_check(repo)
    marked = (repo / "sample.cpp").read_text()
    assert marked.count("PLB-LOCAL BEGIN(") == 1
    assert marked.count("PLB-LOCAL END(") == 1


def test_generated_markers_include_local_commit_provenance_and_upstream_text(
    repo: Path,
) -> None:
    _ = (repo / "sample.cpp").write_text(
        "int base = 5;\nint kept = 2;\n", encoding="utf-8"
    )
    commit(repo)
    sha = git(repo, "rev-parse", "HEAD")[:12]
    _ = apply_and_check(repo)
    source = (repo / "sample.cpp").read_text()
    assert f"PLB-LOCAL({sha})" in source
    assert "fixture" in source
    assert "Upstream: int base = 1;" in source


def test_marker_text_in_multiline_string_does_not_count_as_coverage(repo: Path) -> None:
    source = 'auto text = R"(\n// PLB-LOCAL(fake): string content, not a comment\n)";\n'
    _ = (repo / "sample.cpp").write_text(source, encoding="utf-8")
    assert run(repo, "--check").returncode == 1
    _ = apply_and_check(repo)
    assert (repo / "sample.cpp").read_text().count("PLB-LOCAL(") == 2


def test_header_never_lands_inside_string_opened_on_license_closing_line(
    repo: Path,
) -> None:
    source = '/* copyright\n*/ auto text = R"(\nvalue\n)";\n'
    _ = (repo / "sample.cpp").write_text(source, encoding="utf-8")
    _ = apply_and_check(repo)
    assert source in (repo / "sample.cpp").read_text()


def test_added_markers_do_not_merge_separate_payload_regions(repo: Path) -> None:
    source = "int base = 1;\nint kept = 2;\nint other = 3;\nint tail = 4;\n"
    _ = (repo / "sample.cpp").write_text(source, encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = (repo / "sample.cpp").write_text(
        source.replace("base = 1", "base = 8").replace("other = 3", "other = 9")
    )
    inventory = apply_and_check(repo)
    assert "2 region(s)" in inventory


def test_check_reports_git_failure_instead_of_empty_success(repo: Path) -> None:
    result = subprocess.run(
        [sys.executable, str(CHECKER), "--repo", str(repo)],
        env={"PATH": "/nonexistent"},
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode == 2
    assert "Marker check failed" in result.stderr


def test_independent_upstream_addition_never_gets_local_only_banner(repo: Path) -> None:
    _ = git(repo, "checkout", "-b", "incoming")
    _ = (repo / "collision.cpp").write_text("int incoming = 1;\n", encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = git(repo, "checkout", "main")
    _ = (repo / "collision.cpp").write_text("int local = 2;\n", encoding="utf-8")
    inventory = apply_and_check(repo)
    assert "PLB-LOCAL FILE" not in (repo / "collision.cpp").read_text()
    assert inventory.index("collision.cpp") < inventory.index("## Local only files")


def test_changed_shebang_and_encoding_headers_stay_in_their_required_positions(
    repo: Path,
) -> None:
    path = repo / "program.py"
    _ = path.write_text(
        "#!/usr/bin/python3\n# coding: latin-1\nprint(1)\n", encoding="utf-8"
    )
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = path.write_text(
        "#!/usr/bin/env python3\n# coding: utf-8\nprint(1)\n", encoding="utf-8"
    )
    _ = apply_and_check(repo)
    assert path.read_text().startswith("#!/usr/bin/env python3\n# coding: utf-8\n")


def test_symlink_is_never_written(repo: Path, tmp_path: Path) -> None:
    outside = tmp_path.parent / f"outside-{tmp_path.name}.cpp"
    _ = outside.write_text("int external = 1;\n", encoding="utf-8")
    (repo / "link.cpp").symlink_to(outside)
    _ = apply_and_check(repo)
    assert outside.read_text() == "int external = 1;\n"
    assert "symlink" in (repo / INVENTORY).read_text()


def test_check_does_not_change_index_bytes(repo: Path) -> None:
    _ = (repo / "sample.cpp").write_text("int base = 9;\n", encoding="utf-8")
    index = repo / ".git" / "index"
    before = index.read_bytes()
    _ = run(repo, "--check")
    assert index.read_bytes() == before


def test_generated_comment_normalizes_decoration_but_preserves_accents(
    repo: Path,
) -> None:
    original = (
        "// caf\u200bé \u2014 old \u2192 \u201cvalue\u201d\u2026\ufeff\nint base = 1;\n"
    )
    _ = (repo / "sample.cpp").write_text(original, encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = (repo / "sample.cpp").write_text("int base = 2;\n", encoding="utf-8")
    _ = apply_and_check(repo)
    source = (repo / "sample.cpp").read_text()
    assert 'Upstream: // café , old -> "value"...' in source
    assert not any(
        character in source
        for character in "\u2014\u2013\u2192\u201c\u201d\u2026\u200b\ufeff"
    )


def test_generated_comments_fit_120_columns_without_truncating_marker_tag(
    repo: Path,
) -> None:
    indent = " " * 88
    original = f'{indent}const char* text = "' + "a" * 240 + '";\n'
    _ = (repo / "sample.cpp").write_text(original, encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    _ = (repo / "sample.cpp").write_text(
        original.replace("aaa", "bbb"), encoding="utf-8"
    )
    commit(repo)
    sha = git(repo, "rev-parse", "HEAD")[:12]
    _ = apply_and_check(repo)
    generated = [
        line
        for line in (repo / "sample.cpp").read_text().splitlines()
        if line.lstrip().startswith("//")
    ]
    assert all(len(line) <= 120 for line in generated)
    assert any(f"PLB-LOCAL({sha})" in line for line in generated)
    assert any("Upstream:" in line and line.endswith("...") for line in generated)


@pytest.mark.parametrize("legacy", [True, False])
def test_legacy_header_separator_is_metadata_and_first_apply_converges(
    repo: Path, legacy: bool
) -> None:
    licence = (
        "/*\n"
        " * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright\n"
        " * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,\n"
        " * or (at your option) any later version.\n"
        " */\n"
    )
    base = licence + '\n#include "AcceptQuestAction.h"\nint base = 1;\n'
    _ = (repo / "sample.cpp").write_text(base, encoding="utf-8")
    commit(repo)
    _ = git(repo, "update-ref", "refs/remotes/upstream/master", "HEAD")
    header = "// PLB-LOCAL UPSTREAM-FILE: this fork changes 999 region(s) of this upstream file.\n"
    if legacy:
        header += (
            "// Each is tagged PLB-LOCAL(<sha>) where a marker could be placed safely; run\n"
            "// tools/plb_local_markers.py --check for the authoritative list. docs/local-changes.md.\n"
        )
    current = (
        licence + "\n" + header + '\n#include "AcceptQuestAction.h"\nint base = 2;\n'
    )
    _ = (repo / "sample.cpp").write_text(current, encoding="utf-8")
    checked = run(repo, "--check")
    assert checked.returncode == 1
    assert "missing markers       : 1" in checked.stdout
    result = apply_and_check(repo)
    assert "1 region(s)" in result
    assert ": missing" not in result
    assert "PLB-LOCAL(<sha>)" not in (repo / "sample.cpp").read_text()


def test_placeholder_marker_never_counts_as_real_coverage(repo: Path) -> None:
    _ = (repo / "sample.cpp").write_text(
        "// PLB-LOCAL(<tag>): an example, not an actual marker.\nint base = 3;\nint kept = 2;\n",
        encoding="utf-8",
    )
    checked = run(repo, "--check")
    assert checked.returncode == 1
    assert "covered inline        : 0" in checked.stdout
    assert "missing markers       : 1" in checked.stdout
    _ = apply_and_check(repo)
