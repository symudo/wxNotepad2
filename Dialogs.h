#include <wx/dialog.h>
#include <wx/arrstr.h>
#include <wx/textctrl.h>
#include <wx/wrapsizer.h> // 用于标签的流式布局
#include <wx/scrolwin.h>   // 用于容纳过多标签时提供滚动条
#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <wx/hyperlink.h>
#include <wx/statline.h>
#include "Helper.h"
#include "SVG_ICON.h"

class MainFrame;

class TagDialog : public wxDialog {
public:
    TagDialog(MainFrame* parent, const std::unordered_set<int>& existingTagIds, const std::unordered_map<int, wxString>& allTagsList);
    virtual ~TagDialog() override { if (m_deletedTagIds != nullptr) delete m_deletedTagIds; }
    // 返回结果：获取用户标记为删除的标签ID列表
    const std::vector<int>* GetDeletedTagIds() const { return m_deletedTagIds; }

private:
    // UI 控件
    wxScrolledWindow* m_tagPanel = nullptr;     // 标签 Token 的滚动容器
    wxWrapSizer* m_tagSizer = nullptr;          // 标签 Token 的布局管理器
    wxTextCtrl* m_newTagsTextCtrl = nullptr;    // 用户输入新标签的文本框
    MainFrame* m_parent;
    // 输入数据：只读引用
    const std::unordered_map<int, wxString>& m_allTagsList;

    // 状态和输出数据
    std::vector<int>* m_deletedTagIds = new std::vector<int>();                   // 最终要返回的被删除标签ID
    std::unordered_set<int> m_initialTagIds;                   // 初始标签ID（用于清理和判断）
    std::unordered_map<wxWindowID, int> m_tokenTagIdMap; // 映射 Token 中删除按钮的ID到标签ID

    // 内部方法
    void CreateControls();
    void PopulateExistingTags();
    void AddTagToken(int tagId, const wxString& tagName);
    void OnTagTokenDelete(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
};

class AboutDialog : public wxDialog {
public:
    AboutDialog(MainFrame* parent) :wxDialog((wxFrame*)parent, wxID_ANY, L"关于", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
        wxIcon appIcon;
        appIcon.LoadFile(L"MAINICON");
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* IconSizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticBitmap* appIconBitmap = new wxStaticBitmap(this, wxID_ANY, appIcon);
        IconSizer->Add(appIconBitmap, 0, wxALL, 7);

        wxString versionText = wxString::Format(
            L"wxNotepad2 v%d.%d.%d\n"
            L"Copyright © 2025-2026 ibulis\n"
            , MAJOR_VERSION, MINOR_VERSION, RELEASE_NUMBER);

        IconSizer->Add(new wxStaticText(this, wxID_ANY, versionText), 0, wxALL, 0);

        wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(IconSizer, 0, wxALL, 0);

        topSizer->Add(new wxStaticText(this, wxID_ANY, L"项目主页："), 0, wxALL, 0);
        wxHyperlinkCtrl* link = new wxHyperlinkCtrl(this, wxID_ANY,
            L"https://codeberg.org/ibulis/wxNotepad2",
            L"https://codeberg.org/ibulis/wxNotepad2");
        topSizer->Add(link, 0, wxALL, 0);

        wxString dataInfo = L"\n本产品根据GNU GPLv3许可提供给您。";
        topSizer->Add(new wxStaticText(this, wxID_ANY, dataInfo), 0, wxALL, 0);

        mainSizer->Add(topSizer, 0, wxALL,25);

        wxButton* okButton = new wxButton(this, wxID_OK, L"确定");
        mainSizer->Add(okButton, 0, wxALIGN_CENTER_HORIZONTAL|wxALL , 15);

        SetSizerAndFit(mainSizer);
        CenterOnParent();
    }
};