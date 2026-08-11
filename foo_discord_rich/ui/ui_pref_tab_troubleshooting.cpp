#include <stdafx.h>

#include "ui_pref_tab_troubleshooting.h"

namespace drp::ui
{

HWND PreferenceTabTroubleshooting::CreateTab( HWND hParent )
{
    return Create( hParent );
}

CDialogImplBase& PreferenceTabTroubleshooting::Dialog()
{
    return *this;
}

const wchar_t* PreferenceTabTroubleshooting::Name() const
{
    return L"Troubleshooting";
}

void PreferenceTabTroubleshooting::OnUiChangeRequest( int, bool )
{
}

t_uint32 PreferenceTabTroubleshooting::GetState()
{
    return preferences_state::dark_mode_supported;
}

void PreferenceTabTroubleshooting::Apply()
{
}

void PreferenceTabTroubleshooting::Reset()
{
}

BOOL PreferenceTabTroubleshooting::OnInitDialog( HWND, LPARAM )
{
    darkModeHooks_.AddDialogWithControls( m_hWnd );

    return TRUE;
}

void PreferenceTabTroubleshooting::OnHelpUrlClick( UINT, int, CWindow )
{
    const auto result = ShellExecute( nullptr,
        L"open",
        L"" DRP_HOMEPAGE "/blob/master/docs/CONFIGURATION.md#discord-shows-another-application-instead-of-foobar2000",
        nullptr,
        nullptr,
        SW_SHOW );

    if ( reinterpret_cast<INT_PTR>( result ) <= 32 )
    {
        const auto message = fmt::format(
            "Windows could not open the troubleshooting guide (ShellExecute error {}).",
            reinterpret_cast<INT_PTR>( result ) );
        popup_message::g_show( message.c_str(), "Opening troubleshooting guide" );
    }
}

} // namespace drp::ui
