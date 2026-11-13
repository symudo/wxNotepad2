#pragma once

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/font.h>
#include <wx/fontpicker.h>
#include <wx/gdicmn.h>
#include <wx/propdlg.h>
#include <wx/slider.h>
#include <wx/textctrl.h>

class MainFrame;
class SettingDialog : public wxPropertySheetDialog {
public:
    SettingDialog(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title =
        wxT("设置"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = wxDialogNameStr) {
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title =
        wxT("设置"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = wxDialogNameStr);
    //从配置文件中获取准确值，然后
//1.写入到配置文件;2.修改内存中变量;3.调用相关方法设置变化
    struct SettingsData {
        bool dm_ClipboardGetMoreInfo;
        bool dm_CloseToTray;
        bool dm_MinimizeToTray;
        int dm_themeMode;
        bool dm_darkmode;
        bool dm_hideNavbar;
        int dm_link[5];
        int dm_port;
        wxString dm_token;
        wxString dm_linkName[5];
        wxString dm_linkContent[5];
        wxColour dm_DefaultThemeColor;
        wxColour dm_DarkModeThemeColor;
        wxColour dm_stcBGcolor;
        wxColour dm_stcFGcolor;
        wxFont dm_stcFont;
        int dm_toolbarIconSize;
        int dm_treeIconSize;
        wxString dm_ClipboardBlackList;
        int dm_ImagePreviewMaxSize;
        int dm_RecentItemMaxValue;
        int dm_autoSaveDelayMs;
        wxString dm_hotkey;

        bool operator==(const SettingsData& other) const {
            return dm_ClipboardGetMoreInfo == other.dm_ClipboardGetMoreInfo &&
                dm_CloseToTray == other.dm_CloseToTray &&
                dm_MinimizeToTray == other.dm_MinimizeToTray &&
                dm_themeMode == other.dm_themeMode &&
                dm_darkmode == other.dm_darkmode &&
                dm_hideNavbar == other.dm_hideNavbar &&
                dm_link[0] == other.dm_link[0] &&
                dm_link[1] == other.dm_link[1] &&
                dm_link[2] == other.dm_link[2] &&
                dm_link[3] == other.dm_link[3] &&
                dm_link[4] == other.dm_link[4] &&
                dm_port == other.dm_port &&
                dm_token == other.dm_token &&
                dm_linkName[0] == other.dm_linkName[0] &&
                dm_linkName[1] == other.dm_linkName[1] &&
                dm_linkName[2] == other.dm_linkName[2] &&
                dm_linkName[3] == other.dm_linkName[3] &&
                dm_linkName[4] == other.dm_linkName[4] &&
                dm_linkContent[0] == other.dm_linkContent[0] &&
                dm_linkContent[1] == other.dm_linkContent[1] &&
                dm_linkContent[2] == other.dm_linkContent[2] &&
                dm_linkContent[3] == other.dm_linkContent[3] &&
                dm_linkContent[4] == other.dm_linkContent[4] &&
                dm_DefaultThemeColor == other.dm_DefaultThemeColor &&
                dm_DarkModeThemeColor == other.dm_DarkModeThemeColor &&
                dm_stcBGcolor == other.dm_stcBGcolor &&
                dm_stcFGcolor == other.dm_stcFGcolor &&
                dm_stcFont == other.dm_stcFont &&
                dm_toolbarIconSize == other.dm_toolbarIconSize &&
                dm_treeIconSize == other.dm_treeIconSize &&
                dm_ClipboardBlackList == other.dm_ClipboardBlackList &&
                dm_ImagePreviewMaxSize == other.dm_ImagePreviewMaxSize &&
                dm_RecentItemMaxValue == other.dm_RecentItemMaxValue &&
                dm_autoSaveDelayMs == other.dm_autoSaveDelayMs &&
                dm_hotkey == other.dm_hotkey;
        }
        bool operator!=(const SettingsData& other) {
            return !(*this == other);
        }
    };
    SettingsData originData = { 0 };
    SettingsData changedData = { 0 };
private:
    void LoadConfig();
    void SaveConfig() const;
    void SetControlState();
    void GetControlState();
    void OnOK(wxCommandEvent& event);
    void OnTextCtrlKillFocus(wxFocusEvent& event);
    void OnHotkeyPressed(wxKeyEvent& event);
    void OnNetApply(wxCommandEvent& event);
    // Validator variables

    unsigned int ImagePreviewMaxSize;
    unsigned int RecentItemMaxValue;

    // Class member variables
    wxButton* m_netApply;
    wxCheckBox* m_ClipboardGetMoreInfo;
    wxCheckBox* m_CloseToTray;
    wxCheckBox* m_MinimizeToTray;
    wxChoice* m_themeModeChoice;
    wxCheckBox* m_hideNavbar;
    wxChoice* m_link1;
    wxChoice* m_link2;
    wxChoice* m_link3;
    wxChoice* m_link4;
    wxChoice* m_link5;
    wxColourPickerCtrl* m_DefaultThemeColor;
    wxColourPickerCtrl* m_DarkModeThemeColor;
    wxColourPickerCtrl* m_stcBGcolor;
    wxColourPickerCtrl* m_stcFGcolor;
    wxFontPickerCtrl* m_stcFont;
    wxSlider* m_toolbarIconSize;
    wxSlider* m_treeIconSize;
    wxSpinCtrl* m_AutoSaveDelaySpin;
    wxSpinCtrl* m_portSpin;
    wxTextCtrl* m_ClipboardBlackList;
    wxTextCtrl* m_ImagePreviewMaxSize;
    wxTextCtrl* m_RecentItemMaxValue;
    wxTextCtrl* m_hotkey;
    wxTextCtrl* m_link1Content;
    wxTextCtrl* m_link1name;
    wxTextCtrl* m_link2Content;
    wxTextCtrl* m_link2name;
    wxTextCtrl* m_link3Content;
    wxTextCtrl* m_link3name;
    wxTextCtrl* m_link4Content;
    wxTextCtrl* m_link4name;
    wxTextCtrl* m_link5Content;
    wxTextCtrl* m_link5name;
    wxTextCtrl* m_token;
    
    MainFrame* m_parent;
};
