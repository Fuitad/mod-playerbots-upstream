from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CHECKER = Path(__file__).parents[2] / "tools" / "check_extraction_manifest.py"


class CheckExtractionManifestTest(unittest.TestCase):
    def test_checker_requires_an_exact_inventory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            repository = Path(temporary_directory) / "source"
            repository.mkdir()
            self.run_git(repository, "init", "-b", "main")
            (repository / "retained.txt").write_text("base\n", encoding="utf-8")
            self.run_git(repository, "add", "retained.txt")
            self.commit(repository, "base")
            base = self.run_git(repository, "rev-parse", "HEAD").stdout.strip()

            (repository / "retained.txt").write_text("source\n", encoding="utf-8")
            self.run_git(repository, "add", "retained.txt")
            self.commit(repository, "source")
            source = self.run_git(repository, "rev-parse", "HEAD").stdout.strip()

            manifest = Path(temporary_directory) / "manifest.tsv"
            manifest.write_text(
                "source_status\tpath\tclassification\tdestination\tevidence\n"
                "M\tretained.txt\tretained_seam\tmod-playerbots\tneutral_hook\n",
                encoding="utf-8",
            )
            final_manifest = Path(temporary_directory) / "final-inventory.tsv"
            final_manifest.write_text(
                "status\tpath\tclassification\tevidence\n"
                "M\tretained.txt\tretained_seam\tneutral_hook\n",
                encoding="utf-8",
            )

            exact = self.run_checker(repository, manifest, final_manifest, base, source)
            self.assertEqual(exact.returncode, 0, exact.stderr)
            self.assertIn("1 source paths classified exactly", exact.stdout)
            self.assertIn("1 final paths classified exactly", exact.stdout)

            (repository / "final-only.txt").write_text("unclassified\n", encoding="utf-8")
            self.run_git(repository, "add", "final-only.txt")
            self.commit(repository, "unclassified")
            source_with_extra_path = self.run_git(repository, "rev-parse", "HEAD").stdout.strip()

            final_drift = self.run_checker(repository, manifest, final_manifest, base, source)
            self.assertNotEqual(final_drift.returncode, 0)
            self.assertIn("missing from final inventory: A\tfinal-only.txt", final_drift.stderr)

            final_manifest.write_text(
                "status\tpath\tclassification\tevidence\n"
                "M\tretained.txt\tretained_seam\tneutral_hook\n"
                "A\tfinal-only.txt\trepository_contract\tchecker_fixture\n",
                encoding="utf-8",
            )
            incomplete = self.run_checker(repository, manifest, final_manifest, base, source_with_extra_path)
            self.assertNotEqual(incomplete.returncode, 0)
            self.assertIn("missing from manifest: A\tfinal-only.txt", incomplete.stderr)

    def run_checker(
        self,
        repository: Path,
        manifest: Path,
        final_manifest: Path,
        base: str,
        source: str,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(CHECKER),
                "--source-repository",
                str(repository),
                "--manifest",
                str(manifest),
                "--final-repository",
                str(repository),
                "--final-manifest",
                str(final_manifest),
                "--final-base",
                base,
                "--base",
                base,
                "--source",
                source,
            ],
            check=False,
            capture_output=True,
            text=True,
        )

    @staticmethod
    def run_git(repository: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", "-C", str(repository), *arguments],
            check=True,
            capture_output=True,
            text=True,
        )

    @staticmethod
    def commit(repository: Path, message: str) -> None:
        subprocess.run(
            [
                "git",
                "-C",
                str(repository),
                "-c",
                "user.name=Manifest Test",
                "-c",
                "user.email=manifest-test@example.invalid",
                "commit",
                "-m",
                message,
            ],
            check=True,
            capture_output=True,
            text=True,
        )


if __name__ == "__main__":
    unittest.main()
