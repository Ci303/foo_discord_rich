#include <stdafx.h>

#include "ui_pref_tab_providers.h"

#include <artwork/fetcher.h>
#include <artwork/theaudiodb_fetcher.h>
#include <artwork/uploader.h>
#include <ui/ui_pref_tab_manager.h>
#include <utils/credential_store.h>
#include <utils/validation.h>
#include <utils/theaudiodb.h>

#include <qwr/fb2k_config_ui_option.h>

namespace
{

qwr::u8string EvaluateTrackField( const metadb_handle_ptr& handle, const char* query )
{
    titleformat_object::ptr format;
    titleformat_compiler::get()->compile_safe( format, query );
    if ( handle.is_empty() || format.is_empty() )
    {
        return {};
    }

    pfc::string8_fast result;
    handle->format_title( nullptr, result, format, nullptr );
    return result.c_str();
}

} // namespace

namespace drp::ui
{

PreferenceTabProviders::PreferenceTabProviders( PreferenceTabManager* pParent )
    : pParent_( pParent )
    , enableAlbumArtFetch_( config::enableAlbumArtFetch )
    , enableTheAudioDbFetch_( config::enableTheAudioDbFetch )
    , enableArtUpload_( config::enableArtUpload )
    , artUploadCmd_( config::artUploadCmd )
    , artUploadPinQuery_( config::artUploadPinQuery )
    , ddxOptions_( {
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_CheckBox>( enableAlbumArtFetch_, IDC_CHECK_FETCH_ALBUM_ART ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_CheckBox>( enableTheAudioDbFetch_, IDC_CHECK_FETCH_THEAUDIODB ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_CheckBox>( enableArtUpload_, IDC_CHECK_UPLOAD_ART ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( artUploadCmd_, IDC_EDIT_UPLOAD_COMMAND ),
          qwr::ui::CreateUiDdxOption<qwr::ui::UiDdx_TextEdit>( artUploadPinQuery_, IDC_EDIT_UPLOAD_ART_PIN_QUERY ),
      } )
{
}

PreferenceTabProviders::~PreferenceTabProviders()
{
    theAudioDbTestState_->dialogHwnd.store( nullptr );
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Revert();
    }
    CancelPendingTheAudioDbCredentialChange();
}

HWND PreferenceTabProviders::CreateTab( HWND hParent )
{
    return Create( hParent );
}

CDialogImplBase& PreferenceTabProviders::Dialog()
{
    return *this;
}

const wchar_t* PreferenceTabProviders::Name() const
{
    return L"Providers";
}

void PreferenceTabProviders::OnUiChangeRequest( int nID, bool enable )
{
}

t_uint32 PreferenceTabProviders::GetState()
{
    const bool hasChanged = ddxOptions_.cend() != std::find_if(
                                                    ddxOptions_.cbegin(),
                                                    ddxOptions_.cend(),
                                                    []( const auto& ddxOpt ) { return ddxOpt->Option().HasChanged(); } );
    return preferences_state::resettable
           | preferences_state::dark_mode_supported
           | ( hasChanged || !pendingTheAudioDbApiKey_.empty() || isTheAudioDbKeyDeletionPending_ ? preferences_state::changed : 0 );
}

void PreferenceTabProviders::Apply()
{
    const bool theAudioDbKeyChanged = !pendingTheAudioDbApiKey_.empty();
    const bool theAudioDbKeyDeletionRequested = isTheAudioDbKeyDeletionPending_ && !theAudioDbKeyChanged;
    const bool theAudioDbEnableRequested = enableTheAudioDbFetch_.HasChanged()
                                           && enableTheAudioDbFetch_.GetCurrentValue();
    const bool theAudioDbCredentialNeedsValidation = theAudioDbKeyChanged || theAudioDbEnableRequested;
    const bool uploaderCommandChanged = artUploadCmd_.HasChanged();
    std::optional<qwr::u8string> apiKey;
    if ( theAudioDbCredentialNeedsValidation )
    {
        try
        {
            apiKey = GetEffectiveTheAudioDbApiKey();
        }
        catch ( const std::exception& e )
        {
            popup_message::g_show( e.what(), "TheAudioDB API key" );
            return;
        }
    }
    const auto uploaderCommand = artUploadCmd_.GetCurrentValue();
    const auto uploaderPinQuery = artUploadPinQuery_.GetCurrentValue();
    const bool theAudioDbSettingsInvalid = ( theAudioDbKeyDeletionRequested && enableTheAudioDbFetch_.GetCurrentValue() )
                                           || ( theAudioDbCredentialNeedsValidation
                                                && ( ( enableTheAudioDbFetch_.GetCurrentValue() && !apiKey )
                                                     || ( apiKey && !artwork::IsEligibleTheAudioDbSupporterKey( *apiKey ) ) ) );
    const bool uploaderSettingsInvalid = enableArtUpload_.GetCurrentValue()
                                         && ( uploaderCommand.empty() || uploaderPinQuery.empty() );
    if ( theAudioDbSettingsInvalid )
    {
        popup_message::g_show(
            "TheAudioDB settings were not saved. Enter your own supporter API key containing only letters, numbers, hyphens, and underscores. The shared development key 123 is not accepted.",
            "TheAudioDB API key" );
        enableTheAudioDbFetch_.Revert();
        CancelPendingTheAudioDbCredentialChange();
    }
    if ( uploaderSettingsInvalid )
    {
        popup_message::g_show(
            "Local artwork uploader settings were not saved. Enabling this provider requires both an upload command and a cache pin query.",
            "Local artwork uploader" );
        enableArtUpload_.Revert();
        artUploadCmd_.Revert();
        artUploadPinQuery_.Revert();
    }

    if ( theAudioDbKeyChanged && !theAudioDbSettingsInvalid )
    {
        try
        {
            credentials::WriteTheAudioDbApiKey( *apiKey );
            theaudiodb::ResetRejectedApiKeyState( *apiKey );
            hasStoredTheAudioDbApiKey_ = true;
            ClearPendingTheAudioDbApiKey();
            isTheAudioDbKeyDeletionPending_ = false;
        }
        catch ( const std::exception& e )
        {
            popup_message::g_show( e.what(), "TheAudioDB API key" );
            return;
        }
    }
    else if ( theAudioDbKeyDeletionRequested && !theAudioDbSettingsInvalid )
    {
        try
        {
            credentials::ClearTheAudioDbApiKey();
            hasStoredTheAudioDbApiKey_ = false;
            isTheAudioDbKeyDeletionPending_ = false;
        }
        catch ( const std::exception& e )
        {
            popup_message::g_show( e.what(), "TheAudioDB API key" );
            return;
        }
    }

    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().Apply();
    }

    if ( ( theAudioDbKeyChanged || theAudioDbKeyDeletionRequested ) && !theAudioDbSettingsInvalid )
    {
        ArtworkFetcher::Get().InvalidateProviderCache( ArtworkFetcher::ProviderCache::TheAudioDb );
    }
    if ( uploaderCommandChanged && !uploaderSettingsInvalid )
    {
        ArtworkFetcher::Get().InvalidateProviderCache( ArtworkFetcher::ProviderCache::Uploader );
    }

    if ( theAudioDbSettingsInvalid || uploaderSettingsInvalid )
    {
        DoFullDdxToUi();
        UpdateControlState();
    }
}

void PreferenceTabProviders::Reset()
{
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Option().ResetToDefault();
    }
    CancelPendingTheAudioDbCredentialChange();
    DoFullDdxToUi();
    UpdateControlState();
}

bool PreferenceTabProviders::HasPendingArtworkSettings() const
{
    return !pendingTheAudioDbApiKey_.empty() || isTheAudioDbKeyDeletionPending_ || std::ranges::any_of( ddxOptions_, []( const auto& ddxOpt ) {
        return ddxOpt->Option().HasChanged();
    } );
}

BOOL PreferenceTabProviders::OnInitDialog( HWND hwndFocus, LPARAM lParam )
{
    darkModeHooks_.AddDialogWithControls( m_hWnd );
    theAudioDbTestState_->dialogHwnd.store( m_hWnd );

    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Ddx().SetHwnd( m_hWnd );
    }
    DoFullDdxToUi();
    try
    {
        hasStoredTheAudioDbApiKey_ = credentials::ReadTheAudioDbApiKey().has_value();
    }
    catch ( const std::exception& e )
    {
        popup_message::g_show( e.what(), "TheAudioDB API key" );
        hasStoredTheAudioDbApiKey_ = false;
    }
    auto apiKeyEdit = CEdit( GetDlgItem( IDC_EDIT_THEAUDIODB_API_KEY ) );
    apiKeyEdit.LimitText( 128 );
    apiKeyEdit.SetPasswordChar( L'\x25CF' );
    CButton( GetDlgItem( IDC_CHECK_SHOW_THEAUDIODB_KEY ) ).SetCheck( BST_UNCHECKED );
    if ( !pendingTheAudioDbApiKey_.empty() )
    {
        auto pendingApiKey = qwr::unicode::ToWide( pendingTheAudioDbApiKey_ );
        isRestoringTheAudioDbCredentialUi_ = true;
        apiKeyEdit.SetWindowTextW( pendingApiKey.c_str() );
        isRestoringTheAudioDbCredentialUi_ = false;
        SecureZeroMemory( pendingApiKey.data(), pendingApiKey.size() * sizeof( wchar_t ) );
    }
    UpdateTheAudioDbCredentialUi();
    UpdateControlState();

    return TRUE;
}

void PreferenceTabProviders::OnDestroy()
{
    auto expectedHwnd = m_hWnd;
    theAudioDbTestState_->dialogHwnd.compare_exchange_strong( expectedHwnd, nullptr );
}

void PreferenceTabProviders::OnDdxUiChange( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    if ( nID == IDC_EDIT_THEAUDIODB_API_KEY )
    {
        if ( isRestoringTheAudioDbCredentialUi_ )
        {
            return;
        }
        pendingTheAudioDbApiKey_ = qwr::pfc_x::uGetDlgItemText<char>( m_hWnd, nID );
        if ( !pendingTheAudioDbApiKey_.empty() && isTheAudioDbKeyDeletionPending_ )
        {
            isTheAudioDbKeyDeletionPending_ = false;
            UpdateTheAudioDbCredentialUi();
        }
        UpdateControlState();
        OnChanged();
        return;
    }

    const auto it = std::find_if( ddxOptions_.begin(), ddxOptions_.end(), [nID]( const auto& value ) {
        return value->Ddx().IsMatchingId( nID );
    } );
    if ( it != ddxOptions_.end() )
    {
        ( *it )->Ddx().ReadFromUi();
    }

    if ( nID == IDC_CHECK_FETCH_THEAUDIODB || nID == IDC_CHECK_UPLOAD_ART )
    {
        UpdateControlState();
    }
    OnChanged();
}

void PreferenceTabProviders::OnShowApiKeyClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    const bool showKey = CButton( GetDlgItem( IDC_CHECK_SHOW_THEAUDIODB_KEY ) ).GetCheck() == BST_CHECKED;
    auto apiKeyEdit = CEdit( GetDlgItem( IDC_EDIT_THEAUDIODB_API_KEY ) );
    apiKeyEdit.SetPasswordChar( showKey ? 0 : L'\x25CF' );
    apiKeyEdit.Invalidate();
}

void PreferenceTabProviders::OnTestTheAudioDbClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    if ( theAudioDbTestState_->isRunning.load() )
    {
        return;
    }

    std::optional<qwr::u8string> apiKey;
    try
    {
        apiKey = GetEffectiveTheAudioDbApiKey();
    }
    catch ( const std::exception& e )
    {
        popup_message::g_show( e.what(), "TheAudioDB test" );
        return;
    }
    if ( !apiKey || !artwork::IsEligibleTheAudioDbSupporterKey( *apiKey ) )
    {
        popup_message::g_show( "Enter a valid TheAudioDB supporter API key first.", "TheAudioDB test" );
        return;
    }

    metadb_handle_ptr handle;
    if ( !playback_control::get()->get_now_playing( handle ) )
    {
        popup_message::g_show( "Start playing a track before testing TheAudioDB.", "TheAudioDB test" );
        return;
    }
    const auto artist = EvaluateTrackField( handle, "$if3(%album artist%,%artist%,%composer%)" );
    const auto album = EvaluateTrackField( handle, "%album%" );
    if ( artist.empty() || album.empty() )
    {
        popup_message::g_show( "The current track needs artist and album metadata.", "TheAudioDB test" );
        return;
    }

    const auto testState = theAudioDbTestState_;
    auto message = std::make_shared<qwr::u8string>();
    const auto callback = threaded_process_callback_lambda::create(
        {},
        [artist, album, apiKey = *apiKey, message]( threaded_process_status&, abort_callback& aborter ) {
            try
            {
                const auto result = theaudiodb::FetchArt( artist, album, apiKey, aborter );
                if ( !result )
                {
                    *message = "TheAudioDB returned no exact album match with artwork.";
                }
                else if ( !validation::IsSecureImageUrl( *result ) )
                {
                    *message = "TheAudioDB returned an image URL Discord cannot use.";
                }
                else
                {
                    *message = fmt::format( "TheAudioDB found artwork.\n\n{}", *result );
                }
            }
            catch ( const exception_aborted& )
            {
                throw;
            }
            catch ( const std::exception& e )
            {
                *message = fmt::format( "TheAudioDB test failed.\n\n{}", e.what() );
            }
        },
        [message, testState]( fb2k::hwnd_t, bool wasAborted ) {
            testState->isRunning.store( false );
            popup_message::g_show(
                wasAborted ? "TheAudioDB test was cancelled." : message->c_str(),
                "TheAudioDB test" );
            const auto dialogHwnd = testState->dialogHwnd.load();
            if ( dialogHwnd && ::IsWindow( dialogHwnd ) )
            {
                ::PostMessageW( dialogHwnd, PreferenceTabProviders::kTheAudioDbTestFinishedMessage, 0, 0 );
            }
        } );
    try
    {
        theaudiodb::ResetRejectedApiKeyState( *apiKey );
        testState->isRunning.store( true );
        UpdateControlState();
        threaded_process::g_run_modeless(
            callback,
            threaded_process::flag_show_delayed,
            m_hWnd,
            "Testing TheAudioDB" );
    }
    catch ( const std::exception& e )
    {
        theAudioDbTestState_->isRunning.store( false );
        UpdateControlState();
        popup_message::g_show( fmt::format( "Failed to start TheAudioDB test.\n\n{}", e.what() ).c_str(), "TheAudioDB test" );
    }
    catch ( ... )
    {
        theAudioDbTestState_->isRunning.store( false );
        UpdateControlState();
        popup_message::g_show( "Failed to start TheAudioDB test.", "TheAudioDB test" );
    }
}

void PreferenceTabProviders::OnClearTheAudioDbKeyClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    if ( ::MessageBoxW(
             m_hWnd,
             L"Clear the stored TheAudioDB API key when these settings are applied? The provider will also be disabled.",
             L"Clear TheAudioDB API key",
             MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2 )
         != IDYES )
    {
        return;
    }

    isTheAudioDbKeyDeletionPending_ = true;
    enableTheAudioDbFetch_ = false;
    ClearPendingTheAudioDbApiKey();
    DoFullDdxToUi();
    UpdateControlState();
    OnChanged();
}

void PreferenceTabProviders::OnTheAudioDbHelpClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    ShellExecute( nullptr, L"open", L"https://www.theaudiodb.com/api_apply.php", nullptr, nullptr, SW_SHOW );
}

void PreferenceTabProviders::OnMusicBrainzHelpClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    ShellExecute( nullptr, L"open", L"https://musicbrainz.org/doc/Cover_Art_Archive/API", nullptr, nullptr, SW_SHOW );
}

void PreferenceTabProviders::OnTestUploaderClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    const auto command = artUploadCmd_.GetCurrentValue();
    if ( command.empty() )
    {
        popup_message::g_show( "Enter an upload command first.", "Local artwork uploader test" );
        return;
    }

    metadb_handle_ptr handle;
    if ( !playback_control::get()->get_now_playing( handle ) )
    {
        popup_message::g_show( "Start playing a track before testing the uploader.", "Local artwork uploader test" );
        return;
    }

    auto message = std::make_shared<qwr::u8string>();
    const auto callback = threaded_process_callback_lambda::create(
        {},
        [handle, command, message]( threaded_process_status&, abort_callback& aborter ) {
            try
            {
                const auto result = UploadArt( handle, command, aborter );
                if ( !result )
                {
                    *message = "No local or embedded front-cover artwork was available for the current track.";
                }
                else if ( !validation::IsSecureImageUrl( *result ) )
                {
                    *message = "Uploader output was not a valid HTTPS image URL.";
                }
                else
                {
                    *message = fmt::format( "Uploader succeeded.\n\n{}", *result );
                }
            }
            catch ( const exception_aborted& )
            {
                throw;
            }
            catch ( const std::exception& e )
            {
                *message = fmt::format( "Uploader failed.\n\n{}", e.what() );
            }
        },
        [message]( fb2k::hwnd_t, bool wasAborted ) {
            popup_message::g_show(
                wasAborted ? "Uploader test was cancelled." : message->c_str(),
                "Local artwork uploader test" );
        } );
    threaded_process::g_run_modeless(
        callback,
        threaded_process::flag_show_delayed,
        m_hWnd,
        "Testing local artwork uploader" );
}

void PreferenceTabProviders::OnUploaderHelpClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    ShellExecute( nullptr, L"open", L"" DRP_HOMEPAGE "/blob/master/docs/CONFIGURATION.md#local-and-embedded-artwork", nullptr, nullptr, SW_SHOW );
}

void PreferenceTabProviders::OnRequirementsClick( UINT uNotifyCode, int nID, CWindow wndCtl )
{
    popup_message::g_show(
        "Discord: only a public Application ID is used. Never enter a bot token or client secret.\n\n"
        "MusicBrainz / Cover Art Archive: no account or key. A release MUSICBRAINZ_ALBUMID is used when available; otherwise artist and album metadata are required.\n\n"
        "TheAudioDB: requires your own supporter API key, available from TheAudioDB after upgrading. It is stored for the current Windows user in Credential Manager and is never redisplayed after saving.\n\n"
        "Local artwork: Discord cannot read local files, so a trusted uploader command must return a public HTTPS image URL.\n\n"
        "Discogs is not offered because its image-transfer, caching and linked-attribution requirements cannot be met reliably inside a Discord activity card.",
        "Artwork provider requirements" );
}

LRESULT PreferenceTabProviders::OnTheAudioDbTestFinished( UINT message, WPARAM wParam, LPARAM lParam, BOOL& wasHandled )
{
    theAudioDbTestState_->isRunning.store( false );
    UpdateControlState();
    return 0;
}

void PreferenceTabProviders::OnChanged()
{
    pParent_->OnDataChanged();
}

void PreferenceTabProviders::DoFullDdxToUi()
{
    if ( !m_hWnd )
    {
        return;
    }
    for ( auto& ddxOpt: ddxOptions_ )
    {
        ddxOpt->Ddx().WriteToUi();
    }
    UpdateTheAudioDbCredentialUi();
}

std::optional<qwr::u8string> PreferenceTabProviders::GetEffectiveTheAudioDbApiKey()
{
    if ( !pendingTheAudioDbApiKey_.empty() )
    {
        return pendingTheAudioDbApiKey_;
    }
    if ( isTheAudioDbKeyDeletionPending_ )
    {
        return std::nullopt;
    }
    return credentials::ReadTheAudioDbApiKey();
}

void PreferenceTabProviders::ClearPendingTheAudioDbApiKey()
{
    if ( !pendingTheAudioDbApiKey_.empty() )
    {
        SecureZeroMemory( pendingTheAudioDbApiKey_.data(), pendingTheAudioDbApiKey_.size() );
        pendingTheAudioDbApiKey_.clear();
    }
    if ( m_hWnd )
    {
        ::SetDlgItemTextW( m_hWnd, IDC_EDIT_THEAUDIODB_API_KEY, L"" );
        CButton( GetDlgItem( IDC_CHECK_SHOW_THEAUDIODB_KEY ) ).SetCheck( BST_UNCHECKED );
        CEdit( GetDlgItem( IDC_EDIT_THEAUDIODB_API_KEY ) ).SetPasswordChar( L'\x25CF' );
    }
}

void PreferenceTabProviders::CancelPendingTheAudioDbCredentialChange()
{
    ClearPendingTheAudioDbApiKey();
    isTheAudioDbKeyDeletionPending_ = false;
}

void PreferenceTabProviders::UpdateTheAudioDbCredentialUi()
{
    if ( !m_hWnd )
    {
        return;
    }
    const auto cueText = isTheAudioDbKeyDeletionPending_
                             ? L"Will be cleared on Apply"
                         : hasStoredTheAudioDbApiKey_ ? L"Stored - type to replace"
                                                     : L"Enter supporter API key";
    GetDlgItem( IDC_EDIT_THEAUDIODB_API_KEY ).SendMessageW( EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>( cueText ) );
}

void PreferenceTabProviders::UpdateControlState()
{
    if ( !m_hWnd )
    {
        return;
    }

    const bool enableTheAudioDbControls = enableTheAudioDbFetch_.GetCurrentValue();
    const bool isTheAudioDbTestRunning = theAudioDbTestState_->isRunning.load();
    GetDlgItem( IDC_EDIT_THEAUDIODB_API_KEY ).EnableWindow( enableTheAudioDbControls && !isTheAudioDbTestRunning );
    GetDlgItem( IDC_CHECK_SHOW_THEAUDIODB_KEY ).EnableWindow( enableTheAudioDbControls && !pendingTheAudioDbApiKey_.empty() && !isTheAudioDbTestRunning );
    const bool hasEffectiveTheAudioDbApiKey = !pendingTheAudioDbApiKey_.empty()
                                               || ( hasStoredTheAudioDbApiKey_ && !isTheAudioDbKeyDeletionPending_ );
    GetDlgItem( IDC_BUTTON_TEST_THEAUDIODB ).EnableWindow( enableTheAudioDbControls && hasEffectiveTheAudioDbApiKey && !isTheAudioDbTestRunning );
    GetDlgItem( IDC_BUTTON_CLEAR_THEAUDIODB_KEY ).EnableWindow( hasStoredTheAudioDbApiKey_ && !isTheAudioDbKeyDeletionPending_ && !isTheAudioDbTestRunning );

    const bool enableUploaderControls = enableArtUpload_.GetCurrentValue();
    GetDlgItem( IDC_EDIT_UPLOAD_COMMAND ).EnableWindow( enableUploaderControls );
    GetDlgItem( IDC_EDIT_UPLOAD_ART_PIN_QUERY ).EnableWindow( enableUploaderControls );
    GetDlgItem( IDC_BUTTON_TEST_UPLOADER ).EnableWindow( enableUploaderControls );
}

} // namespace drp::ui
