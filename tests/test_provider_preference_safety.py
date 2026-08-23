import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def function_body(source: str, function_name: str, next_function_name: str) -> str:
    start = source.index(function_name)
    end = source.index(next_function_name, start)
    return source[start:end]


class ProviderPreferenceSafetyTests(unittest.TestCase):
    def test_clear_key_is_deferred_until_apply(self):
        source = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        clear_handler = function_body(
            source,
            "PreferenceTabProviders::OnClearTheAudioDbKeyClick",
            "PreferenceTabProviders::OnTheAudioDbHelpClick",
        )
        apply_handler = function_body(
            source,
            "PreferenceTabProviders::Apply",
            "PreferenceTabProviders::Reset",
        )

        self.assertIn("isTheAudioDbKeyDeletionPending_ = true", clear_handler)
        self.assertNotIn("credentials::ClearTheAudioDbApiKey", clear_handler)
        self.assertIn("credentials::ClearTheAudioDbApiKey", apply_handler)
        self.assertIn("CancelPendingTheAudioDbCredentialChange", source)

    def test_typing_a_replacement_cancels_a_staged_key_deletion(self):
        source = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        change_handler = function_body(
            source,
            "PreferenceTabProviders::OnDdxUiChange",
            "PreferenceTabProviders::OnShowApiKeyClick",
        )

        self.assertIn("!pendingTheAudioDbApiKey_.empty()", change_handler)
        self.assertIn("isTheAudioDbKeyDeletionPending_ = false", change_handler)
        self.assertIn("UpdateTheAudioDbCredentialUi()", change_handler)

    def test_pending_key_is_restored_when_the_tab_window_is_recreated(self):
        source = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        init_handler = function_body(
            source,
            "PreferenceTabProviders::OnInitDialog",
            "PreferenceTabProviders::OnDestroy",
        )

        self.assertIn("if ( !pendingTheAudioDbApiKey_.empty() )", init_handler)
        self.assertIn("qwr::unicode::ToWide( pendingTheAudioDbApiKey_ )", init_handler)
        self.assertIn("apiKeyEdit.SetWindowTextW", init_handler)
        self.assertIn("apiKeyEdit.SetPasswordChar", init_handler)
        self.assertIn("SetCheck( BST_UNCHECKED )", init_handler)
        self.assertIn("isRestoringTheAudioDbCredentialUi_", init_handler)

    def test_theaudiodb_test_state_survives_tab_window_recreation(self):
        source = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        header = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.h"
        ).read_text(encoding="utf-8")
        test_handler = function_body(
            source,
            "PreferenceTabProviders::OnTestTheAudioDbClick",
            "PreferenceTabProviders::OnClearTheAudioDbKeyClick",
        )

        self.assertIn("std::shared_ptr<TheAudioDbTestState>", header)
        self.assertIn("theAudioDbTestState_->dialogHwnd.store( m_hWnd )", source)
        self.assertIn("dialogHwnd.compare_exchange_strong", source)
        self.assertIn("[message, testState]", test_handler)
        self.assertIn("testState->isRunning.store( false )", test_handler)
        self.assertIn("testState->dialogHwnd.load()", test_handler)
        self.assertNotIn("const auto dialogHwnd = m_hWnd", test_handler)

    def test_apply_does_not_read_an_untouched_provider_credential(self):
        source = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        apply_handler = function_body(
            source,
            "PreferenceTabProviders::Apply",
            "PreferenceTabProviders::Reset",
        )
        compact_handler = " ".join(apply_handler.split())

        self.assertIn(
            "theAudioDbEnableRequested = enableTheAudioDbFetch_.HasChanged() "
            "&& enableTheAudioDbFetch_.GetCurrentValue();",
            compact_handler,
        )
        self.assertIn(
            "theAudioDbCredentialNeedsValidation = theAudioDbKeyChanged || "
            "theAudioDbEnableRequested;",
            compact_handler,
        )
        validation_guard = apply_handler.index(
            "if ( theAudioDbCredentialNeedsValidation )"
        )
        credential_read = apply_handler.index("GetEffectiveTheAudioDbApiKey()")
        self.assertLess(validation_guard, credential_read)
        self.assertIn(
            "theAudioDbKeyDeletionRequested && "
            "enableTheAudioDbFetch_.GetCurrentValue()",
            compact_handler,
        )

    def test_uploader_test_passes_its_abort_callback_to_the_process_wait(self):
        providers = (
            ROOT / "foo_discord_rich" / "ui" / "ui_pref_tab_providers.cpp"
        ).read_text(encoding="utf-8")
        uploader = (
            ROOT / "foo_discord_rich" / "artwork" / "uploader.cpp"
        ).read_text(encoding="utf-8")
        subprocess = (
            ROOT / "foo_discord_rich" / "utils" / "subprocess_executor.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn("UploadArt( handle, command, aborter )", providers)
        self.assertIn("WaitUntilCompleted( kMaxWaitTime, aborter )", uploader)
        self.assertIn("aborter.get_handle()", subprocess)
        self.assertIn("TerminateJobObject", subprocess)

    def test_uploader_secret_warning_is_documented(self):
        documentation = (ROOT / "docs" / "CONFIGURATION.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("Never put an API key, access", documentation)
        self.assertIn("visible in the spawned process's", documentation)


if __name__ == "__main__":
    unittest.main()
