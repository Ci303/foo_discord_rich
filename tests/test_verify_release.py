import tempfile
import unittest
from pathlib import Path
from sys import path

path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
from verify_release import read_version, verify
from download_submodules import configured_submodule_paths


class ReleaseVerificationTests(unittest.TestCase):
    def make_root(self, version="1.2.3-test.1"):
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        (root / "VERSION").write_text(version + "\n", encoding="utf-8")
        (root / "RELEASE_NOTES.md").write_text(
            f"# Discord Rich Presence Integration v{version}\n", encoding="utf-8"
        )
        return temporary, root

    def test_matching_metadata(self):
        temporary, root = self.make_root()
        with temporary:
            verify(root, "v1.2.3-test.1")

    def test_tag_mismatch_is_rejected(self):
        temporary, root = self.make_root()
        with temporary, self.assertRaises(ValueError):
            verify(root, "v1.2.3-test.2")

    def test_invalid_version_is_rejected(self):
        temporary, root = self.make_root("not a version")
        with temporary, self.assertRaises(ValueError):
            read_version(root)

    def test_submodule_discovery_ignores_generated_directories(self):
        repository_root = Path(__file__).resolve().parent.parent
        paths = configured_submodule_paths(repository_root)
        self.assertIn(Path("submodules/fmt"), paths)
        self.assertNotIn(Path("submodules/_result"), paths)


if __name__ == "__main__":
    unittest.main()
