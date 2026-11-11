#include "pch.h"
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
    wxDialog(parent, wxID_ANY, L"批量编辑标签", wxDefaultPosition, wxDefaultSize,wxDEFAULT_DIALOG_STYLE ),
    m_allTagsList(allTagsList),
    m_initialTagIds(existingTagIds), // 保存初始ID
    m_parent(parent) {
    SetSize(wxSize(FromDIP(600),FromDIP(450)));
    // 1. 创建 UI 控件和布局
    CreateControls();

    // 2. 根据初始数据填充已有的标签 Token
    PopulateExistingTags();

    // 调整大小以适应内容
   // GetSizer()->SetSizeHints(this);
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
    // 上半部分：已有的标签展示区域
    // ===================================================================

    mainSizer->Add(new wxStaticText(this, wxID_ANY, L"已有标签 (点击 'X' 移除):"), 0, wxALL, 5);

    m_tagPanel = new wxScrolledWindow(this, ID_TAG_PANEL, wxDefaultPosition, wxSize(-1, 150), wxVSCROLL | wxHSCROLL | wxBORDER_THEME);
    m_tagPanel->SetScrollRate(5, 5);

    // 使用 wxWrapSizer 实现自动换行的标签布局
    m_tagSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
    m_tagPanel->SetSizer(m_tagSizer);

    mainSizer->Add(m_tagPanel, 1, wxEXPAND | wxALL, 5); // 占据更多空间

    // ===================================================================
    // 下半部分：新增标签输入区域
    // ===================================================================

    mainSizer->Add(new wxStaticText(this, wxID_ANY, L"输入新标签 (用空格分隔多个标签):"), 0, wxLEFT | wxRIGHT | wxTOP, 5);

    m_newTagsTextCtrl = new wxTextCtrl(this, ID_TAG_TEXT_CTRL, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    mainSizer->Add(m_newTagsTextCtrl, 1, wxEXPAND | wxALL, 5); // 占据更多空间

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
    //m_tagPanel->Layout();
    //m_tagPanel->FitInside(); // 适应内容并更新滚动条
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

