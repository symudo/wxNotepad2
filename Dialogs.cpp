#include "pch.h"
#include <algorithm>
#include "Dialogs.h"
#include "mainFrame.h"
#include "MyTreeCtrl.h"
#include "App.h"
#include "events.h"


// 使用一个私有的枚举来定义控件 ID，避免与 wxID_ 冲突
enum {
    ID_TAG_PANEL = wxID_HIGHEST + 1,
    ID_TAG_TEXT_CTRL
    // 删除按钮的ID将使用动态生成的 wxID_ANY
};



// ----------------------------------------------------------------------
// 构造函数实现
// ----------------------------------------------------------------------
TagDialog::TagDialog(MainFrame* parent, const std::unordered_set<int>& existingTagIds, const std::unordered_map<int, wxString>& allTagsList) :
    wxDialog(parent, wxID_ANY, L"批量编辑标签", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    m_allTagsList(allTagsList),
    m_initialTagIds(existingTagIds), // 保存初始ID
    m_parent(parent) {
    SetSize(wxSize(FromDIP(600), FromDIP(520)));
    // 1. 创建 UI 控件和布局
    CreateControls();

    // 2. 应用对话框整体深浅色主题（在创建自定义配色 Token 前调用，避免覆盖芯片配色）
    ApplyDialogTheme(this, GetMyAppInstance()->GetDarkMode());

    // 3. 填充当前笔记已有标签 Token 及全局可选标签库
    PopulateExistingTags();
    PopulateAvailableTags();

    Layout();
    CenterOnParent();

    Bind(wxEVT_BUTTON, &TagDialog::OnOK, this, wxID_OK);
    Bind(wxEVT_BUTTON, &TagDialog::OnCancel, this, wxID_CANCEL);
}


// ----------------------------------------------------------------------
// 核心方法实现
// ----------------------------------------------------------------------



void TagDialog::CreateControls() {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // ===================================================================
    // 上半部分：当前笔记已有标签展示区域
    // ===================================================================

    mainSizer->Add(new wxStaticText(this, wxID_ANY, L"已有标签 (点击 'X' 移除):"), 0, wxALL, 5);

    m_tagPanel = new wxScrolledWindow(this, ID_TAG_PANEL, wxDefaultPosition, wxSize(-1, FromDIP(110)), wxVSCROLL | wxHSCROLL | wxBORDER_THEME);
    m_tagPanel->SetScrollRate(5, 5);

    // 使用 wxWrapSizer 实现自动换行的标签布局
    m_tagSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
    m_tagPanel->SetSizer(m_tagSizer);

    mainSizer->Add(m_tagPanel, 1, wxEXPAND | wxALL, 5);

    // ===================================================================
    // 中间部分：标签库快速复用区域
    // ===================================================================

    mainSizer->Add(new wxStaticText(this, wxID_ANY, L"已有标签库 (点击快速添加到下方输入框):"), 0, wxLEFT | wxRIGHT | wxTOP, 5);

    m_availableTagPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(100)), wxVSCROLL | wxHSCROLL | wxBORDER_THEME);
    m_availableTagPanel->SetScrollRate(5, 5);
    m_availableTagSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
    m_availableTagPanel->SetSizer(m_availableTagSizer);

    mainSizer->Add(m_availableTagPanel, 1, wxEXPAND | wxALL, 5);

    // ===================================================================
    // 下半部分：新增标签输入区域
    // ===================================================================

    mainSizer->Add(new wxStaticText(this, wxID_ANY, L"输入新标签 (用空格分隔多个标签):"), 0, wxLEFT | wxRIGHT | wxTOP, 5);

    m_newTagsTextCtrl = new wxTextCtrl(this, ID_TAG_TEXT_CTRL, wxEmptyString, wxDefaultPosition, wxSize(-1, FromDIP(75)), wxTE_MULTILINE);
    mainSizer->Add(m_newTagsTextCtrl, 1, wxEXPAND | wxALL, 5);

    // ===================================================================
    // 底部：确定/取消按钮
    // ===================================================================
    wxStdDialogButtonSizer* buttonSizer = new wxStdDialogButtonSizer();
    buttonSizer->AddButton(new wxButton(this, wxID_OK, L"确定"));
    buttonSizer->AddButton(new wxButton(this, wxID_CANCEL, L"取消"));
    buttonSizer->Realize();

    mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

    SetSizer(mainSizer);
}

void TagDialog::PopulateExistingTags() {
    // 根据初始标签ID，从所有标签列表中查找名称并创建 Token
    for (int tagId : m_initialTagIds) {
        auto it = m_allTagsList.find(tagId);
        if (it != m_allTagsList.end()) {
            AddTagToken(tagId, it->second);
        }
    }
    m_tagPanel->Layout();
    m_tagPanel->FitInside();
}

void TagDialog::PopulateAvailableTags() {
    if (!m_availableTagPanel || !m_availableTagSizer) return;
    bool isDarkMode = GetMyAppInstance()->GetDarkMode();

    std::vector<std::pair<int, wxString>> candidates;
    for (const auto& [tagId, tagName] : m_allTagsList) {
        if (m_initialTagIds.count(tagId) == 0) {
            candidates.emplace_back(tagId, tagName);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    if (candidates.empty()) {
        wxStaticText* emptyHint = new wxStaticText(m_availableTagPanel, wxID_ANY, L"暂无其他可复用标签");
        emptyHint->SetForegroundColour(isDarkMode ? ThemeColours::DarkTextSecondary : ThemeColours::LightTextSecondary);
        if (isDarkMode) {
            emptyHint->SetBackgroundColour(ThemeColours::DarkBgBase);
        }
        m_availableTagSizer->Add(emptyHint, 0, wxALL, 6);
    } else {
        for (const auto& [tagId, tagName] : candidates) {
            wxButton* chipBtn = new wxButton(m_availableTagPanel, wxID_ANY, L"+ " + tagName, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
            chipBtn->SetToolTip(L"点击快速添加标签：" + tagName);
            if (isDarkMode) {
                HWND hwndBtn = (HWND)chipBtn->GetHWND();
                if (hwndBtn) ::SetWindowTheme(hwndBtn, L"DarkMode_Explorer", NULL);
                chipBtn->SetBackgroundColour(ThemeColours::DarkBgSecondary);
                chipBtn->SetForegroundColour(ThemeColours::DarkTextPrimary);
            }
            chipBtn->Bind(wxEVT_BUTTON, &TagDialog::OnQuickAddTag, this);
            m_availableTagSizer->Add(chipBtn, 0, wxALL, 3);
        }
    }

    m_availableTagPanel->Layout();
    m_availableTagPanel->FitInside();
}

void TagDialog::OnQuickAddTag(wxCommandEvent& event) {
    wxButton* btn = wxDynamicCast(event.GetEventObject(), wxButton);
    if (!btn || !m_newTagsTextCtrl) return;

    wxString label = btn->GetLabel();
    if (label.StartsWith(L"+ ")) {
        label = label.Mid(2);
    }
    if (label.IsEmpty()) return;

    wxString current = m_newTagsTextCtrl->GetValue();
    // 拆分检查是否已经存在于输入框中，避免重复添加
    wxString token;
    bool exists = false;
    auto checkToken = [&]() {
        if (!token.IsEmpty() && token == label) {
            exists = true;
        }
        token.Clear();
    };
    for (size_t i = 0; i < current.length(); ++i) {
        wxUniChar ch = current[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            checkToken();
        } else {
            token += ch;
        }
    }
    checkToken();

    if (!exists) {
        wxString trimmed = current;
        trimmed.Trim(true);
        if (!trimmed.IsEmpty()) {
            trimmed += L" ";
        }
        trimmed += label;
        m_newTagsTextCtrl->SetValue(trimmed);
        m_newTagsTextCtrl->SetInsertionPointEnd();
    }
    btn->Disable();
}

// C++ 代码
void TagDialog::AddTagToken(int tagId, const wxString& tagName) {
    // --- 颜色定义 ---
    bool isDarkMode = GetMyAppInstance()->GetDarkMode();

    // 标签 Token 颜色
    wxColour tokenBgColor;
    wxColour tokenFgColor;
    // 删除按钮颜色
    wxColour btnBgColor;

    if (isDarkMode) {
        // 暗黑模式颜色：
        tokenBgColor = wxColour("#34495E"); // 柔和的深蓝色
        tokenFgColor = wxColour("#E0E0E0"); // 浅色文本
        btnBgColor = wxColour("#34495E");   // 按钮背景与Token一致
    } else {
        // 明亮模式颜色：
        tokenBgColor = wxColour(230, 230, 250); // 浅紫色背景
        tokenFgColor = *wxBLACK; // 默认黑色文本
        btnBgColor = *wxWHITE;     // 按钮使用白色或透明（这里保持原逻辑的白色）
    }

    // 1. 创建一个用于容纳文本和按钮的小面板（作为 Token）
    wxPanel* tagToken = new wxPanel(m_tagPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

    // 应用背景色
    tagToken->SetBackgroundColour(tokenBgColor);

    tagToken->SetToolTip(L"已有标签：点击 'X' 移除");

    wxBoxSizer* tokenSizer = new wxBoxSizer(wxHORIZONTAL);

    // 2. 标签文本
    wxStaticText* tagText = new wxStaticText(tagToken, wxID_ANY, tagName);
    tagText->SetFont(tagText->GetFont());

    // 应用文本颜色和背景色，确保文字在 Token 上正确显示
    tagText->SetForegroundColour(tokenFgColor);
    tagText->SetBackgroundColour(tokenBgColor); // 确保 StaticText 背景与 Panel 一致

    // 3. 删除按钮
    // 假设 wxArtProvider::GetBitmap(wxART_CLOSE...) 返回的图标颜色能适应两种主题。
    // 在旧版 wxWidgets 中，可能需要手动更换为白色图标。
    wxBitmapButton* deleteBtn = new wxBitmapButton(
        tagToken,
        wxID_ANY,
        wxArtProvider::GetBitmap(wxART_CLOSE, wxART_BUTTON, wxSize(12, 12))
    );

    // 应用按钮背景色
    deleteBtn->SetBackgroundColour(btnBgColor);

    // 4. 布局 Token
    tokenSizer->Add(tagText, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    tokenSizer->Add(deleteBtn, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    tagToken->SetSizerAndFit(tokenSizer);

    // 5. 绑定删除事件
    m_tokenTagIdMap[deleteBtn->GetId()] = tagId;
    deleteBtn->Bind(wxEVT_BUTTON, &TagDialog::OnTagTokenDelete, this);

    // 6. 添加到 Sizer
    m_tagSizer->Add(tagToken, 0, wxALL, 3);
}

void TagDialog::OnTagTokenDelete(wxCommandEvent& event) {
    // 1. 查找对应的标签 ID
    int tagId = m_tokenTagIdMap[event.GetId()];
    if (tagId == 0) return; // 未找到，安全退出

    // 2. 将此 ID 标记为待删除（即使重复点击也不影响）
    m_deletedTagIds->push_back(tagId);

    // 3. 移除映射，防止重复删除
    m_tokenTagIdMap.erase(event.GetId());

    // 4. 移除 UI 控件
    wxWindow* deleteBtn = wxDynamicCast(event.GetEventObject(), wxWindow);
    if (deleteBtn) {
        wxWindow* tagToken = deleteBtn->GetParent();
        m_tagSizer->Detach(tagToken);
        tagToken->Destroy();

        // 5. 更新布局
        m_tagSizer->Layout();
        m_tagPanel->FitInside();
    }
}

void TagDialog::OnOK(wxCommandEvent& event) {
    // 1. 解析新标签文本框内容
    wxString input = m_newTagsTextCtrl->GetValue();
    wxTreeEvent evt(EVT_UPDATE_NOTE_TAGS);

    evt.SetItem(m_parent->m_tree->GetSelection());
    evt.SetString(input);
    evt.SetClientData(m_deletedTagIds);
    m_deletedTagIds = nullptr;
    wxPostEvent(m_parent, evt);
    //TODO：在这里构造结果和事件数据结构，发送到父窗口
    EndModal(wxID_OK);
}

void TagDialog::OnCancel(wxCommandEvent& event) {
    // 退出对话框
    EndModal(wxID_CANCEL);
}

