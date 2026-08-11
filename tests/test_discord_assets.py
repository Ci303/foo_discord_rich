import re
import struct
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
APPLICATION_ID = "1536157545863847938"

EXPECTED_DIMENSIONS = {
    "playing.png": (512, 512),
    "playing-dark.png": (512, 512),
    "paused.png": (512, 512),
    "paused-dark.png": (512, 512),
}

CONFIGURED_ASSET_DEFAULTS = {
    "largeImageId_Light": "foobar2000",
    "largeImageId_Dark": "foobar2000-dark",
    "playingImageId_Light": "playing",
    "playingImageId_Dark": "playing-dark",
    "pausedImageId_Light": "paused",
    "pausedImageId_Dark": "paused-dark",
}

MANIFEST_FILES = {
    "foobar2000": "Not redistributed",
    "foobar2000-dark": "Not redistributed",
    "playing": "playing.png",
    "playing-dark": "playing-dark.png",
    "paused": "paused.png",
    "paused-dark": "paused-dark.png",
}


def read_png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if (
        len(header) != 24
        or header[:8] != b"\x89PNG\r\n\x1a\n"
        or header[12:16] != b"IHDR"
    ):
        raise ValueError(f"Invalid PNG header: {path}")
    return struct.unpack(">II", header[16:24])


class DiscordAssetTests(unittest.TestCase):
    def test_portal_asset_files_and_dimensions(self):
        image_root = REPOSITORY_ROOT / "images"
        actual_dimensions = {
            filename: read_png_dimensions(image_root / filename)
            for filename in EXPECTED_DIMENSIONS
        }
        self.assertEqual(EXPECTED_DIMENSIONS, actual_dimensions)
        self.assertFalse((image_root / "pause.png").exists())
        self.assertFalse((image_root / "pause-dark.png").exists())

    def test_default_application_and_asset_keys_match_manifest(self):
        config = (REPOSITORY_ROOT / "foo_discord_rich" / "fb2k" / "config.cpp").read_text(
            encoding="utf-8"
        )
        migration_header = (
            REPOSITORY_ROOT
            / "foo_discord_rich"
            / "utils"
            / "discord_application_id_migration.h"
        ).read_text(encoding="utf-8")
        manifest = (REPOSITORY_ROOT / "images" / "README.md").read_text(encoding="utf-8")

        self.assertIn(f'kDefaultDiscordApplicationId[] = "{APPLICATION_ID}"', migration_header)
        self.assertIn("discordAppToken( guid::conf_app_token, kDefaultDiscordApplicationId )", config)
        self.assertIn(f"Application ID: `{APPLICATION_ID}`", manifest)

        configured_defaults = dict(
            re.findall(
                r'ConfigString\s+(\w+)\([^,]+,\s*"([a-z0-9-]+)"\s*\)',
                config,
            )
        )
        self.assertEqual(CONFIGURED_ASSET_DEFAULTS, configured_defaults)

        manifest_files = {}
        for key, backticked_file, plain_file in re.findall(
            r"^\| `([^`]+)` \| (?:`([^`]+)`|([^|]+?)) \|",
            manifest,
            flags=re.MULTILINE,
        ):
            manifest_files[key] = backticked_file or plain_file.strip()
        self.assertEqual(MANIFEST_FILES, manifest_files)

    def test_legacy_application_migration_has_persisted_one_time_marker(self):
        config = (REPOSITORY_ROOT / "foo_discord_rich" / "fb2k" / "config.cpp").read_text(
            encoding="utf-8"
        )
        guids = (
            REPOSITORY_ROOT / "foo_discord_rich" / "component_guids.h"
        ).read_text(encoding="utf-8")

        self.assertIn("conf_discord_application_id_migration_complete", guids)
        self.assertRegex(
            config,
            r"ConfigBool\s+discordApplicationIdMigrationComplete\([^;]+false\s*\)",
        )

        migration = re.search(
            r"void\s+MigrateLegacyDiscordApplicationId\(\)\s*\{(?P<body>.*?)(?=\nvoid\s+SanitiseArtworkDisplayPolicy)",
            config,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(migration)
        body = migration.group("body")
        self.assertIn("discordApplicationIdMigrationComplete", body)
        self.assertIn("discordApplicationIdMigrationComplete = true", body)
        self.assertIn("discordAppToken = kDefaultDiscordApplicationId", body)

        dllmain = (REPOSITORY_ROOT / "foo_discord_rich" / "dllmain.cpp").read_text(
            encoding="utf-8"
        )
        self.assertLess(
            dllmain.index("MigrateLegacyDiscordApplicationId()"),
            dllmain.index("DiscordAdapter::GetInstance().Initialize()"),
        )


if __name__ == "__main__":
    unittest.main()
