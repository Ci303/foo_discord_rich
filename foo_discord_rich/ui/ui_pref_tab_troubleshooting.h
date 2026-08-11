#pragma once

#include <ui/ui_itab.h>

#include <resource.h>

#include <foobar2000/SDK/coreDarkMode.h>

namespace drp::ui
{

class PreferenceTabTroubleshooting
    : public CDialogImpl<PreferenceTabTroubleshooting>
    , public ITab
{
public:
    enum
    {
        IDD = IDD_PREFS_TROUBLESHOOTING_TAB
    };

    BEGIN_MSG_MAP( PreferenceTabTroubleshooting )
        MSG_WM_INITDIALOG( OnInitDialog )
        COMMAND_HANDLER_EX( IDC_BUTTON_DISCORD_ACTIVITY_HELP, BN_CLICKED, OnHelpUrlClick )
    END_MSG_MAP()

public:
    HWND CreateTab( HWND hParent ) override;
    CDialogImplBase& Dialog() override;
    const wchar_t* Name() const override;
    void OnUiChangeRequest( int nID, bool enable ) override;
    t_uint32 GetState() override;
    void Apply() override;
    void Reset() override;

private:
    BOOL OnInitDialog( HWND hwndFocus, LPARAM lParam );
    void OnHelpUrlClick( UINT uNotifyCode, int nID, CWindow wndCtl );

private:
    fb2k::CCoreDarkModeHooks darkModeHooks_;
};

} // namespace drp::ui
