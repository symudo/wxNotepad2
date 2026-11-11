#include "pch.h"

#include "App.h"
#include "SettingDialog.h"
#include "Helper.h"
#include "wx/spinctrl.h"
#include "mainFrame.h"

bool SettingDialog::Create(wxWindow* parent, wxWindowID id, const wxString& title,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name) {
    m_parent = (MainFrame*)parent;
    SetSheetStyle(wxPROPSHEET_NOTEBOOK);
    if (!wxPropertySheetDialog::Create(parent, id, title, pos, size, style, name))
        return false;

    CreateButtons(wxOK | wxCANCEL);
    //page 1
    auto page1 = new wxPanel(GetBookCtrl(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    GetBookCtrl()->AddPage(page1, wxT("全局"));

    auto page_sizer_1 = new wxBoxSizer(wxVERTICAL);

    auto staticText2 = new wxStaticText(page1, wxID_ANY, wxT("全局热键"));
    page_sizer_1->Add(staticText2, wxSizerFlags().Border(wxALL));

    m_hotkey = new wxTextCtrl(page1, wxID_ANY, L"None", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxBORDER_SIMPLE);
    page_sizer_1->Add(m_hotkey, wxSizerFlags().Border(wxALL));

    page_sizer_1->AddSpacer(20);

    m_MinimizeToTray = new wxCheckBox(page1, wxID_ANY, wxT("最小化时隐藏到通知区域"));
    page_sizer_1->Add(m_MinimizeToTray, wxSizerFlags().Border(wxALL));

    m_CloseToTray = new wxCheckBox(page1, wxID_ANY, wxT("关闭时隐藏到通知区域"));
    page_sizer_1->Add(m_CloseToTray, wxSizerFlags().Border(wxALL));
    page1->SetSizerAndFit(page_sizer_1);
    //page 2
    auto page2 = new wxPanel(GetBookCtrl(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    GetBookCtrl()->AddPage(page2, wxT("外观"));

    auto flex_grid_sizer2 = new wxFlexGridSizer(2, 0, 10);

    m_darkmode = new wxCheckBox(page2, wxID_ANY, wxT("黑暗模式"));
    flex_grid_sizer2->Add(m_darkmode, wxSizerFlags().CenterVertical().Border(wxALL));

    flex_grid_sizer2->AddSpacer(0);

    auto staticText3 = new wxStaticText(page2, wxID_ANY, wxT("正常模式主题色"));
    flex_grid_sizer2->Add(staticText3, wxSizerFlags().CenterVertical().Border(wxALL));

    m_DefaultThemeColor = new wxColourPickerCtrl(page2, wxID_ANY, *wxBLACK, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(50, 10)));
    flex_grid_sizer2->Add(m_DefaultThemeColor, wxSizerFlags().Border(wxALL));

    auto staticText4 = new wxStaticText(page2, wxID_ANY, wxT("黑暗模式主题色"));
    flex_grid_sizer2->Add(staticText4, wxSizerFlags().CenterVertical().Border(wxALL));

    m_DarkModeThemeColor = new wxColourPickerCtrl(page2, wxID_ANY, *wxBLACK, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(50, 10)));
    flex_grid_sizer2->Add(m_DarkModeThemeColor, wxSizerFlags().Border(wxALL));

    auto staticText5 = new wxStaticText(page2, wxID_ANY, wxT("工具栏图标大小"));
    flex_grid_sizer2->Add(staticText5, wxSizerFlags().CenterVertical().Border(wxALL));

    m_toolbarIconSize = new wxSlider(page2, wxID_ANY, 32, 16, 256, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(100, -1)), wxSL_HORIZONTAL | wxSL_LABELS);
    m_toolbarIconSize->SetValue(32);
    m_toolbarIconSize->SetLineSize(16);
    m_toolbarIconSize->SetMinSize(ConvertDialogToPixels(wxSize(200, -1)));
    flex_grid_sizer2->Add(m_toolbarIconSize, wxSizerFlags().Border(wxALL, 0));

    auto staticText6 = new wxStaticText(page2, wxID_ANY, wxT("树节点图标大小"));
    flex_grid_sizer2->Add(staticText6, wxSizerFlags().CenterVertical().Border(wxALL));

    m_treeIconSize = new wxSlider(page2, wxID_ANY, 32, 16, 256, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(100, -1)), wxSL_HORIZONTAL | wxSL_LABELS);
    m_treeIconSize->SetValue(32);
    m_treeIconSize->SetLineSize(16);
    m_treeIconSize->SetMinSize(ConvertDialogToPixels(wxSize(200, -1)));
    flex_grid_sizer2->Add(m_treeIconSize, wxSizerFlags().Border(wxALL, 0));

    auto staticText7 = new wxStaticText(page2, wxID_ANY, wxT("笔记字体样式"));
    flex_grid_sizer2->Add(staticText7, wxSizerFlags().CenterVertical().Border(wxALL));

    m_stcFont = new wxFontPickerCtrl(page2, wxID_ANY, wxNullFont, wxDefaultPosition, wxDefaultSize,
        wxFNTP_FONTDESC_AS_LABEL | wxFNTP_USEFONT_FOR_LABEL);
    flex_grid_sizer2->Add(m_stcFont, wxSizerFlags().Border(wxALL));

    auto staticText8 = new wxStaticText(page2, wxID_ANY, wxT("笔记字体颜色"));
    flex_grid_sizer2->Add(staticText8, wxSizerFlags().CenterVertical().Border(wxALL));

    m_stcFGcolor = new wxColourPickerCtrl(page2, wxID_ANY, *wxBLACK);
    m_stcFGcolor->SetMinSize(ConvertDialogToPixels(wxSize(50, 10)));
    flex_grid_sizer2->Add(m_stcFGcolor, wxSizerFlags().Border(wxALL));

    auto staticText9 = new wxStaticText(page2, wxID_ANY, wxT("笔记背景颜色"));
    flex_grid_sizer2->Add(staticText9, wxSizerFlags().CenterVertical().Border(wxALL));

    m_stcBGcolor = new wxColourPickerCtrl(page2, wxID_ANY, *wxBLACK);
    m_stcBGcolor->SetMinSize(ConvertDialogToPixels(wxSize(50, 10)));
    flex_grid_sizer2->Add(m_stcBGcolor, wxSizerFlags().Border(wxALL));

    m_hideNavbar = new wxCheckBox(page2, wxID_ANY, wxT("隐藏导航条"));
    flex_grid_sizer2->Add(m_hideNavbar, wxSizerFlags().Border(wxALL));
    page2->SetSizerAndFit(flex_grid_sizer2);
    //page 3
    auto page3 = new wxPanel(GetBookCtrl(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    GetBookCtrl()->AddPage(page3, wxT("功能"));

    auto box_sizer = new wxBoxSizer(wxVERTICAL);

    auto static_box = new wxStaticBoxSizer(wxVERTICAL, page3, wxT("自定义链接或命令"));

    auto box_sizer2 = new wxBoxSizer(wxVERTICAL);

    auto staticText10 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY,
        wxT("参数中出现的第一个\'%s\'将被替换为选中的字符串；\n如果不存在\'%s\'或未选中文字，将不会执行替换;\n尽量使用正斜杠\'/\'，对于反斜杠需要转义\'\\\\\'。"));
    staticText10->Wrap(1);
    box_sizer2->Add(staticText10, wxSizerFlags().Border(wxALL));

    auto flex_grid_sizer = new wxFlexGridSizer(6, 4, 0, 0);

    flex_grid_sizer->AddSpacer(0);

    auto staticText17 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, wxT("名称"));
    flex_grid_sizer->Add(staticText17, wxSizerFlags().Border(wxALL));

    auto staticText18 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, wxT("类型"));
    flex_grid_sizer->Add(staticText18, wxSizerFlags().Border(wxALL));

    auto staticText19 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, wxT("参数"));
    flex_grid_sizer->Add(staticText19, wxSizerFlags().Border(wxALL));

    auto staticText20 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, "1.", wxDefaultPosition,
        ConvertDialogToPixels(wxSize(5, -1)));
    flex_grid_sizer->Add(staticText20, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link1name = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(30, -1)));
    flex_grid_sizer->Add(m_link1name, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link1 = new wxChoice(static_box->GetStaticBox(), wxID_ANY);
    m_link1->Append(wxT("不使用"));
    m_link1->Append(wxT("超链接"));
    m_link1->Append(wxT("命令"));
    flex_grid_sizer->Add(m_link1, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link1Content = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString);
    m_link1Content->SetMinSize(ConvertDialogToPixels(wxSize(165, -1)));
    flex_grid_sizer->Add(m_link1Content, wxSizerFlags().CenterVertical().Border(wxALL));

    auto staticText11 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, "2.");
    flex_grid_sizer->Add(staticText11, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link2name = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(30, -1)));
    flex_grid_sizer->Add(m_link2name, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link2 = new wxChoice(static_box->GetStaticBox(), wxID_ANY);
    m_link2->Append(wxT("不使用"));
    m_link2->Append(wxT("超链接"));
    m_link2->Append(wxT("命令"));
    flex_grid_sizer->Add(m_link2, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link2Content = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString);
    m_link2Content->SetMinSize(ConvertDialogToPixels(wxSize(165, -1)));
    flex_grid_sizer->Add(m_link2Content, wxSizerFlags().CenterVertical().Border(wxALL));

    auto staticText12 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, "3.");
    flex_grid_sizer->Add(staticText12, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link3name = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(30, -1)));
    flex_grid_sizer->Add(m_link3name, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link3 = new wxChoice(static_box->GetStaticBox(), wxID_ANY);
    m_link3->Append(wxT("不使用"));
    m_link3->Append(wxT("超链接"));
    m_link3->Append(wxT("命令"));
    flex_grid_sizer->Add(m_link3, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link3Content = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString);
    m_link3Content->SetMinSize(ConvertDialogToPixels(wxSize(165, -1)));
    flex_grid_sizer->Add(m_link3Content, wxSizerFlags().CenterVertical().Border(wxALL));

    auto staticText13 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, "4.");
    flex_grid_sizer->Add(staticText13, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link4name = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(30, -1)));
    flex_grid_sizer->Add(m_link4name, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link4 = new wxChoice(static_box->GetStaticBox(), wxID_ANY);
    m_link4->Append(wxT("不使用"));
    m_link4->Append(wxT("超链接"));
    m_link4->Append(wxT("命令"));
    flex_grid_sizer->Add(m_link4, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link4Content = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString);
    m_link4Content->SetMinSize(ConvertDialogToPixels(wxSize(165, -1)));
    flex_grid_sizer->Add(m_link4Content, wxSizerFlags().CenterVertical().Border(wxALL));

    auto staticText14 = new wxStaticText(static_box->GetStaticBox(), wxID_ANY, "5.");
    flex_grid_sizer->Add(staticText14, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link5name = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(30, -1)));
    flex_grid_sizer->Add(m_link5name, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link5 = new wxChoice(static_box->GetStaticBox(), wxID_ANY);
    m_link5->Append(wxT("不使用"));
    m_link5->Append(wxT("超链接"));
    m_link5->Append(wxT("命令"));
    flex_grid_sizer->Add(m_link5, wxSizerFlags().CenterVertical().Border(wxALL));

    m_link5Content = new wxTextCtrl(static_box->GetStaticBox(), wxID_ANY, wxEmptyString);
    m_link5Content->SetMinSize(ConvertDialogToPixels(wxSize(165, -1)));
    flex_grid_sizer->Add(m_link5Content, wxSizerFlags().CenterVertical().Border(wxALL));

    box_sizer2->Add(flex_grid_sizer, wxSizerFlags().Border(wxALL));

    static_box->Add(box_sizer2, wxSizerFlags().Expand().Border(wxALL));

    box_sizer->Add(static_box, wxSizerFlags().Expand().Border(wxALL, 0));

    box_sizer->AddSpacer(30);

    m_ClipboardGetMoreInfo = new wxCheckBox(page3, wxID_ANY,
        wxT("监听剪贴板时收集来源窗口名称和时间信息"));
    box_sizer->Add(m_ClipboardGetMoreInfo, wxSizerFlags().Border(wxALL));

    auto staticText15 = new wxStaticText(page3, wxID_ANY,
        wxT("监听剪贴板时排除的关键词(\';\'隔开) "));
    box_sizer->Add(staticText15, wxSizerFlags().Border(wxALL));

    m_ClipboardBlackList = new wxTextCtrl(page3, wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(248, -1)));
    box_sizer->Add(m_ClipboardBlackList, wxSizerFlags().Border(wxALL));

    auto box_sizer9 = new wxBoxSizer(wxHORIZONTAL);

    auto staticText16 = new wxStaticText(page3, wxID_ANY, wxT("预览图片尺寸限制 "));
    box_sizer9->Add(staticText16, wxSizerFlags().Center().Border(wxALL));

    m_ImagePreviewMaxSize = new wxTextCtrl(page3, wxID_ANY, wxEmptyString);
    m_ImagePreviewMaxSize->SetValidator(wxIntegerValidator<unsigned int>(&ImagePreviewMaxSize));
    wxStaticCast(m_ImagePreviewMaxSize->GetValidator(), wxIntegerValidator<unsigned int>)->SetMin(50);
    m_ImagePreviewMaxSize->SetMinSize(ConvertDialogToPixels(wxSize(30, -1)));
    box_sizer9->Add(m_ImagePreviewMaxSize, wxSizerFlags().Center().Border(wxALL, 0));

    box_sizer->Add(box_sizer9, wxSizerFlags().Border(wxALL, 0));

    auto box_sizer3 = new wxBoxSizer(wxHORIZONTAL);

    auto staticText21 = new wxStaticText(page3, wxID_ANY,
        wxT("最近笔记最大条目限制"));
    box_sizer3->Add(staticText21, wxSizerFlags().Center().Border(wxALL));

    m_RecentItemMaxValue = new wxTextCtrl(page3, wxID_ANY, wxEmptyString);
    m_RecentItemMaxValue->SetValidator(wxIntegerValidator<unsigned int>(&RecentItemMaxValue));
    wxStaticCast(m_RecentItemMaxValue->GetValidator(), wxIntegerValidator<unsigned int>)->SetMin(5);
    m_RecentItemMaxValue->SetMinSize(ConvertDialogToPixels(wxSize(30, -1)));
    box_sizer3->Add(m_RecentItemMaxValue, wxSizerFlags().Center().Border(wxALL, 0));

    box_sizer->Add(box_sizer3, wxSizerFlags().Border(wxALL, 0));
    page3->SetSizerAndFit(box_sizer);
    //page 4
    auto page4 = new wxPanel(GetBookCtrl(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    GetBookCtrl()->AddPage(page4, wxT("网络"));
    auto box_sizer4 = new wxBoxSizer(wxVERTICAL);
    auto box_sizer5 = new wxBoxSizer(wxHORIZONTAL);
    auto m_staticText = new wxStaticText(page4, wxID_ANY, L"自定义端口号");
    box_sizer5->Add(m_staticText, wxSizerFlags().Center().Border(wxALL));
    m_portSpin = new wxSpinCtrl(page4, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1024, 65535, 8080);
    box_sizer5->Add(m_portSpin, wxSizerFlags().Center().Border(wxALL, 0));
    box_sizer4->Add(box_sizer5, wxSizerFlags().Border(wxALL));
    auto box_sizer6 = new wxBoxSizer(wxHORIZONTAL);
    auto  m_staticText2 = new wxStaticText(page4, wxID_ANY, wxT("自定义Token"));
    box_sizer6->Add(m_staticText2, wxSizerFlags().Border(wxALL));
    m_token = new wxTextCtrl(page4, wxID_ANY, wxEmptyString, wxDefaultPosition,
        ConvertDialogToPixels(wxSize(200, -1)));
    m_token->SetMaxLength(64);
    box_sizer6->Add(m_token, wxSizerFlags().Border(wxALL));
    box_sizer4->Add(box_sizer6, wxSizerFlags().Expand().Border(wxALL));
    auto box_sizer7 = new wxBoxSizer(wxHORIZONTAL);
    m_netApply = new wxButton(page4, wxID_ANY, wxT("应用设置"));
    box_sizer7->Add(m_netApply, wxSizerFlags().Center().Border(wxALL));
    auto m_staticText3 = new wxStaticText(page4, wxID_ANY,
        wxT("修改后请点击应用设置，并在弹出的页面中重新安装油猴脚本或者bookmarklet"));
    box_sizer7->Add(m_staticText3, wxSizerFlags().Border(wxALL));

    box_sizer4->Add(box_sizer7, wxSizerFlags().Border(wxALL));
    page4->SetSizerAndFit(box_sizer4);
    //page 4 end
    SetMinSize(ConvertDialogToPixels(wxSize(260, 200)));
    Fit();
    LayoutDialog(wxBOTH);
    //自定义部分
    //读取配置文件设置originData，并设置控件状态
    LoadConfig();
    SetControlState();

    Bind(wxEVT_BUTTON, &SettingDialog::OnOK, this, wxID_OK);
    m_ImagePreviewMaxSize->Bind(wxEVT_KILL_FOCUS, &SettingDialog::OnTextCtrlKillFocus, this);
    m_RecentItemMaxValue->Bind(wxEVT_KILL_FOCUS, &SettingDialog::OnTextCtrlKillFocus, this);
    m_hotkey->Bind(wxEVT_KEY_DOWN, &SettingDialog::OnHotkeyPressed, this);

    m_netApply->Bind(wxEVT_BUTTON, &SettingDialog::OnNetApply, this);
    return true;
}

void SettingDialog::LoadConfig() {
    auto App = GetMyAppInstance();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };

    originData.dm_ClipboardGetMoreInfo = config.ReadBool("Scintilla/ClipboardGetMoreInfo", false);
    originData.dm_CloseToTray = config.ReadBool("MainWindow/CloseToTray", true);
    originData.dm_MinimizeToTray = config.ReadBool("MainWindow/MinimizeToTray", true);
    originData.dm_darkmode = config.ReadBool("MainWindow/DarkMode", false);
    originData.dm_hideNavbar = config.ReadBool("Toolbar/HideNavbar", false);
    wxString tempColor;
    config.Read("MainWindow/DefaultThemeColor", &tempColor, "#1F1F1F");
    if (!originData.dm_DefaultThemeColor.Set(tempColor)) {
        originData.dm_DefaultThemeColor = wxColour(0x1F, 0x1F, 0x1F);
    }
    config.Read("MainWindow/DarkModeThemeColor", &tempColor, "#B0B0B0");
    if (!originData.dm_DarkModeThemeColor.Set(tempColor)) {
        originData.dm_DarkModeThemeColor = wxColour(0xB0, 0xB0, 0xB0);
    }
    config.Read("Scintilla/Colours/Foreground", &tempColor, "#000000");
    if (!originData.dm_stcFGcolor.Set(tempColor)) {
        originData.dm_stcFGcolor = wxColour(*wxBLACK);
    }
    config.Read("Scintilla/Colours/Background", &tempColor, "#FFFFFF");
    if (!originData.dm_stcBGcolor.Set(tempColor)) {
        originData.dm_stcBGcolor = wxColour(*wxWHITE);
    }
    originData.dm_toolbarIconSize = config.ReadLong("Toolbar/IconSize", 32);
    if (originData.dm_toolbarIconSize < 16 || originData.dm_toolbarIconSize>256)originData.dm_toolbarIconSize = 32;
    originData.dm_treeIconSize = config.ReadLong("Tree/IconSize", 64);
    if (originData.dm_treeIconSize < 16 || originData.dm_treeIconSize>256)originData.dm_toolbarIconSize = 64;
    originData.dm_ImagePreviewMaxSize = config.ReadLong("Scintilla/ImagePreviewMaxSize", 500);
    if (originData.dm_ImagePreviewMaxSize < 50)originData.dm_ImagePreviewMaxSize = 50;
    originData.dm_RecentItemMaxValue = config.ReadLong("Tree/RecentMaxCount", 10);
    if (originData.dm_RecentItemMaxValue < 5)originData.dm_RecentItemMaxValue = 5;
    config.Read("Scintilla/ClipboardBlackList", &originData.dm_ClipboardBlackList);
    config.Read("MainWindow/GlobalHotkey", &originData.dm_hotkey, L"Ctrl+Shift+F1");
    originData.dm_stcFont = ReadFontFromConfig();
    wxString linkValue;
    for (int i = 0; i < 5; i++) {
        if (!config.Read(wxString::Format("Scintilla/Link%d", i + 1), &linkValue))continue;

        int linkId = 0;
        wxString linkName = wxEmptyString;
        wxString linkContent = wxEmptyString;

        wxString idStr = linkValue.BeforeFirst(';').Trim(true).Trim(false);
        wxString remaining = linkValue.AfterFirst(';');
        long id_long = 0;
        if (idStr.ToLong(&id_long)) {
            if (id_long == 1 || id_long == 2) {
                linkId = (int)id_long;
            }
        }
        int secondSeparatorPos = remaining.Find(';');
        if (secondSeparatorPos == wxNOT_FOUND) {
            linkName = remaining.Trim(true).Trim(false);
        } else {
            linkName = remaining.BeforeFirst(';').Trim(true).Trim(false);
            linkContent = remaining.AfterFirst(';').Trim(true).Trim(false);
        }

        originData.dm_link[i] = linkId;
        originData.dm_linkName[i] = linkName;
        originData.dm_linkContent[i] = linkContent;
    }
    originData.dm_port = config.ReadLong("Network/Port", 8080);
    originData.dm_token = config.Read("Network/APIToken", L"default_token");
}

void SettingDialog::SaveConfig() const {
    auto App = GetMyAppInstance();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };

    config.Write("Scintilla/ClipboardGetMoreInfo", changedData.dm_ClipboardGetMoreInfo);
    config.Write("MainWindow/CloseToTray", changedData.dm_CloseToTray);
    config.Write("MainWindow/MinimizeToTray", changedData.dm_MinimizeToTray);
    config.Write("MainWindow/DarkMode", changedData.dm_darkmode);
    config.Write("Toolbar/HideNavbar", changedData.dm_hideNavbar);
    config.Write("MainWindow/DefaultThemeColor", changedData.dm_DefaultThemeColor.GetAsString(wxC2S_HTML_SYNTAX));
    config.Write("MainWindow/DarkModeThemeColor", changedData.dm_DarkModeThemeColor.GetAsString(wxC2S_HTML_SYNTAX));
    config.Write("Scintilla/Colours/Foreground", changedData.dm_stcFGcolor.GetAsString(wxC2S_HTML_SYNTAX));
    config.Write("Scintilla/Colours/Background", changedData.dm_stcBGcolor.GetAsString(wxC2S_HTML_SYNTAX));
    config.Write("Toolbar/IconSize", changedData.dm_toolbarIconSize);
    config.Write("Tree/IconSize", changedData.dm_treeIconSize);
    config.Write("Scintilla/ImagePreviewMaxSize", changedData.dm_ImagePreviewMaxSize);
    config.Write("Tree/RecentMaxCount", changedData.dm_RecentItemMaxValue);
    config.Write("Scintilla/ClipboardBlackList", changedData.dm_ClipboardBlackList);
    config.Write("MainWindow/GlobalHotkey", changedData.dm_hotkey);
    config.Write("Scintilla/Font/PointSize", changedData.dm_stcFont.GetPointSize());
    config.Write("Scintilla/Font/FaceName", changedData.dm_stcFont.GetFaceName());
    config.Write("Network/Port", changedData.dm_port);
    config.Write("Network/APIToken", changedData.dm_token);
    GetMyAppInstance()->SetPort(changedData.dm_port);
    GetMyAppInstance()->SetToken(changedData.dm_token);

    for (int i = 0; i < 5; i++) {
        config.Write(wxString::Format("Scintilla/Link%d", i + 1), wxString::Format("%d;%s;%s", changedData.dm_link[i], changedData.dm_linkName[i], changedData.dm_linkContent[i]));
    }
    config.Flush();
}

void SettingDialog::SetControlState() {
    m_ClipboardGetMoreInfo->SetValue(originData.dm_ClipboardGetMoreInfo);
    m_CloseToTray->SetValue(originData.dm_CloseToTray);
    m_MinimizeToTray->SetValue(originData.dm_MinimizeToTray);
    m_darkmode->SetValue(originData.dm_darkmode);
    m_hideNavbar->SetValue(originData.dm_hideNavbar);
    m_link1->SetSelection(originData.dm_link[0]);
    m_link2->SetSelection(originData.dm_link[1]);
    m_link3->SetSelection(originData.dm_link[2]);
    m_link4->SetSelection(originData.dm_link[3]);
    m_link5->SetSelection(originData.dm_link[4]);
    m_DefaultThemeColor->SetColour(originData.dm_DefaultThemeColor);
    m_DarkModeThemeColor->SetColour(originData.dm_DarkModeThemeColor);
    m_stcBGcolor->SetColour(originData.dm_stcBGcolor);
    m_stcFGcolor->SetColour(originData.dm_stcFGcolor);
    m_stcFont->SetSelectedFont(originData.dm_stcFont);
    m_toolbarIconSize->SetValue(originData.dm_toolbarIconSize);
    m_treeIconSize->SetValue(originData.dm_treeIconSize);
    m_ClipboardBlackList->SetValue(originData.dm_ClipboardBlackList);
    ImagePreviewMaxSize = originData.dm_ImagePreviewMaxSize;
    RecentItemMaxValue = originData.dm_RecentItemMaxValue;
    m_hotkey->SetValue(originData.dm_hotkey);
    m_link1Content->SetValue(originData.dm_linkContent[0]);
    m_link1name->SetValue(originData.dm_linkName[0]);
    m_link2Content->SetValue(originData.dm_linkContent[1]);
    m_link2name->SetValue(originData.dm_linkName[1]);
    m_link3Content->SetValue(originData.dm_linkContent[2]);
    m_link3name->SetValue(originData.dm_linkName[2]);
    m_link4Content->SetValue(originData.dm_linkContent[3]);
    m_link4name->SetValue(originData.dm_linkName[3]);
    m_link5Content->SetValue(originData.dm_linkContent[4]);
    m_link5name->SetValue(originData.dm_linkName[4]);
    m_portSpin->SetValue(originData.dm_port);
    m_token->SetValue(originData.dm_token);
}

void SettingDialog::GetControlState() {
    changedData.dm_ClipboardGetMoreInfo = m_ClipboardGetMoreInfo->GetValue();
    changedData.dm_CloseToTray = m_CloseToTray->GetValue();
    changedData.dm_MinimizeToTray = m_MinimizeToTray->GetValue();
    changedData.dm_darkmode = m_darkmode->GetValue();
    changedData.dm_hideNavbar = m_hideNavbar->GetValue();
    changedData.dm_link[0] = m_link1->GetSelection();
    changedData.dm_link[1] = m_link2->GetSelection();
    changedData.dm_link[2] = m_link3->GetSelection();
    changedData.dm_link[3] = m_link4->GetSelection();
    changedData.dm_link[4] = m_link5->GetSelection();
    changedData.dm_DefaultThemeColor = m_DefaultThemeColor->GetColour();
    changedData.dm_DarkModeThemeColor = m_DarkModeThemeColor->GetColour();
    changedData.dm_stcBGcolor = m_stcBGcolor->GetColour();
    changedData.dm_stcFGcolor = m_stcFGcolor->GetColour();
    changedData.dm_stcFont = m_stcFont->GetSelectedFont();
    changedData.dm_toolbarIconSize = m_toolbarIconSize->GetValue();
    changedData.dm_treeIconSize = m_treeIconSize->GetValue();
    changedData.dm_ClipboardBlackList = m_ClipboardBlackList->GetValue();

    changedData.dm_ImagePreviewMaxSize = ImagePreviewMaxSize;
    changedData.dm_RecentItemMaxValue = RecentItemMaxValue;
    changedData.dm_hotkey = m_hotkey->GetValue();
    changedData.dm_linkContent[0] = m_link1Content->GetValue();
    changedData.dm_linkName[0] = m_link1name->GetValue();
    changedData.dm_linkContent[1] = m_link2Content->GetValue();
    changedData.dm_linkName[1] = m_link2name->GetValue();
    changedData.dm_linkContent[2] = m_link3Content->GetValue();
    changedData.dm_linkName[2] = m_link3name->GetValue();
    changedData.dm_linkContent[3] = m_link4Content->GetValue();
    changedData.dm_linkName[3] = m_link4name->GetValue();
    changedData.dm_linkContent[4] = m_link5Content->GetValue();
    changedData.dm_linkName[4] = m_link5name->GetValue();

    changedData.dm_port = m_portSpin->GetValue();
    changedData.dm_token = m_token->GetValue();
}

void SettingDialog::OnOK(wxCommandEvent& event) {
    GetControlState();
    if (changedData != originData) {
        SaveConfig();
        if (changedData.dm_darkmode != originData.dm_darkmode ||
            changedData.dm_DarkModeThemeColor != originData.dm_DarkModeThemeColor ||
            changedData.dm_DefaultThemeColor != originData.dm_DefaultThemeColor ||
            changedData.dm_toolbarIconSize != originData.dm_toolbarIconSize ||
            changedData.dm_treeIconSize != originData.dm_treeIconSize ||
            changedData.dm_hideNavbar != originData.dm_hideNavbar) {
            wxMessageBox(
                wxT("部分设置需要在重启程序后才能生效。"),
                L"设置已更新",
                wxOK | wxICON_INFORMATION,
                this
            );
        }
    }
    EndModal(wxID_OK);
}

void SettingDialog::OnTextCtrlKillFocus(wxFocusEvent& event) {
    wxTextCtrl* textCtrl = wxDynamicCast(event.GetEventObject(), wxTextCtrl);
    if (textCtrl) {
        wxValidator* validator = textCtrl->GetValidator();
        if (validator) {
            if (validator->Validate(textCtrl)) {
                validator->TransferFromWindow();
            }
        }
    }
    event.Skip();
}

void SettingDialog::OnHotkeyPressed(wxKeyEvent& event) {
    int keycode = event.GetKeyCode();
    int modifiers = event.GetModifiers();

    if (keycode == WXK_CONTROL || keycode == WXK_ALT || keycode == WXK_SHIFT) {
        event.Skip();
        m_hotkey->SetValue(L"None");
        return;
    }
    wxAcceleratorEntry accelEntry(modifiers, keycode, wxID_ANY);
    wxString hotkeyString = accelEntry.ToString();
    m_hotkey->SetValue(hotkeyString);
    event.Skip(false);
}

void SettingDialog::OnNetApply(wxCommandEvent& event) {
    auto App = GetMyAppInstance();
    changedData.dm_port = m_portSpin->GetValue();
    changedData.dm_token = m_token->GetValue();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };
    config.Write("Network/Port", changedData.dm_port);
    config.Write("Network/APIToken", changedData.dm_token);
    GetMyAppInstance()->SetPort(changedData.dm_port);
    GetMyAppInstance()->SetToken(changedData.dm_token);

    m_parent->RestartServer();
    wxLaunchDefaultBrowser(wxString::Format("http://localhost:%d", changedData.dm_port));
    event.Skip(false);
}
