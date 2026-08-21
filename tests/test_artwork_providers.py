import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ArtworkProviderIntegrationTests(unittest.TestCase):
    def test_new_sources_are_in_the_visual_studio_project(self):
        project = (ROOT / "foo_discord_rich" / "foo_discord_rich.vcxproj").read_text(
            encoding="utf-8"
        )
        for relative_path in (
            r"artwork\theaudiodb_fetcher.cpp",
            r"artwork\theaudiodb_fetcher.h",
            r"ui\ui_pref_tab_providers.cpp",
            r"ui\ui_pref_tab_providers.h",
            r"utils\theaudiodb.h",
            r"utils\credential_store.cpp",
            r"utils\credential_store.h",
        ):
            self.assertEqual(project.count(relative_path), 1, relative_path)

    def test_provider_options_have_one_ui_owner(self):
        ui_sources = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (ROOT / "foo_discord_rich" / "ui").glob("ui_pref_tab_*.cpp")
        )
        for config_name in (
            "enableAlbumArtFetch",
            "enableTheAudioDbFetch",
            "enableArtUpload",
            "artUploadCmd",
            "artUploadPinQuery",
        ):
            self.assertEqual(
                ui_sources.count(f"_( config::{config_name} )"),
                1,
                config_name,
            )

    def test_theaudiodb_key_is_not_stored_in_plain_foobar_config(self):
        config = "\n".join(
            (ROOT / "foo_discord_rich" / "fb2k" / name).read_text(
                encoding="utf-8"
            )
            for name in ("config.cpp", "config.h")
        )
        credentials = (
            ROOT / "foo_discord_rich" / "utils" / "credential_store.cpp"
        ).read_text(encoding="utf-8")
        providers = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")

        self.assertNotIn("theAudioDbApiKey", config)
        self.assertIn("CredWriteW", credentials)
        self.assertIn("CredReadW", credentials)
        self.assertIn("CredDeleteW", credentials)
        self.assertIn("IDC_BUTTON_CLEAR_THEAUDIODB_KEY", providers)

    def test_theaudiodb_request_logging_redacts_the_path_key(self):
        source = (
            ROOT / "foo_discord_rich" / "artwork" / "theaudiodb_fetcher.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("/json/<redacted>/searchalbum.php?<redacted>", source)
        self.assertNotIn("response.url", source)
        self.assertNotIn("resp.url", source)

    def test_providers_tab_is_registered(self):
        manager = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_manager.cpp"
        ).read_text(encoding="utf-8")
        resource = (ROOT / "foo_discord_rich" / "foo_discord_rich.rc").read_text(
            encoding="utf-8"
        )
        self.assertIn("PreferenceTabProviders", manager)
        self.assertIn("IDD_PREFS_PROVIDERS_TAB DIALOGEX", resource)


if __name__ == "__main__":
    unittest.main()
