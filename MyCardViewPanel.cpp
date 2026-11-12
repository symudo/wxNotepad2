#include "pch.h"

#include "MyCardViewPanel.h"
#include "events.h"
#include "mainFrame.h"
#include "MyTreeCtrl.h"
#include "App.h"
#include "Helper.h"
#include "SVG_ICON.h"

MyCard::MyCard(wxWindow* parent, const CardData& data, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE | wxFULL_REPAINT_ON_RESIZE),
    Id(data.id), noteId(data.noteId), m_treeItemId(data.treeItemId){
    SetDoubleBuffered(true);

    wxColour bgColor;       // 主面板背景
    wxColour titleBgColor;  // 标题栏背景 
    wxColour primaryTextColor; // 标题、主要内容文字色
    wxColour secondaryTextColor; // 日期、信息等次要文字色

    char buff[4096];
    auto color = GetMyAppInstance()->GetThemeColor();

    if (GetMyAppInstance()->GetDarkMode()){
        bgColor = DARK_BG_BASE;
        titleBgColor = DARK_BG_ACCENT_GREEN;
        primaryTextColor = DARK_TEXT_PRIMARY;
        secondaryTextColor = DARK_TEXT_SECONDARY;
    } else{
        bgColor = *wxWHITE;
        titleBgColor = LIGHT_BG_ACCENT_GREEN;
        primaryTextColor = *wxBLACK;
        secondaryTextColor = *wxBLACK;
    }
    const auto& icon_note = static_cast<MyCardViewPanel*>(parent)->m_noteIcon;
    const auto& icon_folder = static_cast<MyCardViewPanel*>(parent)->m_folderIcon;
    SetBackgroundColour(bgColor);

    // 标题栏
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxPanel* titleBar = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    titleBar->SetBackgroundColour(titleBgColor);

    wxBoxSizer* titleSizer = new wxBoxSizer(wxHORIZONTAL);

    wxBitmap iconBitmap;
    if (data.type == 0){
        iconBitmap = icon_folder.GetBitmap(wxDefaultSize);
    } else{
        iconBitmap = icon_note.GetBitmap(wxDefaultSize);;
    }
    m_iconBitmap = new wxStaticBitmap(titleBar, wxID_ANY, iconBitmap);
    titleSizer->Add(m_iconBitmap, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

    m_titleText = new wxStaticText(titleBar, wxID_ANY, data.name, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    m_titleText->SetForegroundColour(primaryTextColor);
    wxFont titleFont = GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 5);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_titleText->SetFont(titleFont);

    // 确保 StaticText 继承 titleBar 的背景色（尤其在 Windows 上经常需要）
    m_titleText->SetBackgroundColour(titleBgColor);

    titleSizer->Add(m_titleText, 0, wxALIGN_CENTER_VERTICAL | wxALL, 0);
    titleSizer->AddSpacer(FromDIP(40));
    m_infoText = new wxStaticText(titleBar, wxID_ANY, data.info, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    m_infoText->SetForegroundColour(secondaryTextColor);
    m_infoText->SetBackgroundColour(titleBgColor); // 继承背景色
    titleSizer->Add(m_infoText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, // 垂直居中
        5);

    m_dateText = new wxStaticText(titleBar, wxID_ANY, data.date, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    m_dateText->SetForegroundColour(secondaryTextColor);
    m_dateText->SetBackgroundColour(titleBgColor); // 继承背景色
    titleSizer->Add(m_dateText, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 5);
    if (data.type == 0){
        m_dateText->Hide();
    }

    titleBar->SetSizerAndFit(titleSizer);
    titleBar->Layout();
    mainSizer->Add(titleBar, 0, wxEXPAND);

    //内容预览文本
    m_contentPreviewText = new wxStaticText(this, wxID_ANY, data.contentPreview, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END | wxALIGN_LEFT);
    m_contentPreviewText->SetForegroundColour(primaryTextColor);
    wxFont contentFont = GetFont();
    m_contentPreviewText->SetFont(contentFont);
    m_contentPreviewText->Wrap(FromDIP(200));
    mainSizer->Add(m_contentPreviewText, 1, wxEXPAND | wxALL, 5);

    // 确保内容预览文本继承 MyCard 的背景色
    m_contentPreviewText->SetBackgroundColour(bgColor);

    SetSizerAndFit(mainSizer);
    Layout();

    // 绑定事件
    auto bindEvents = [this](wxWindow* window){ window->Bind(wxEVT_LEFT_DCLICK, &MyCard::OnLeftDClick, this); };
    bindEvents(this);
    bindEvents(titleBar);
    bindEvents(m_iconBitmap);
    bindEvents(m_titleText);
    bindEvents(m_infoText);
    bindEvents(m_dateText);
    bindEvents(m_contentPreviewText);
}

void MyCard::UpdateContentWrap(int newParentWidth){
    if (!m_contentPreviewText || !GetSizer()){
        return;
    }

    // 2. 使用传入的父窗口宽度来计算换行宽度
    // 假设 MyCard 自身占据 MyCardViewPanel 的完整宽度，但有两侧 FromDIP(5) 的边距
    int cardWidth = newParentWidth - FromDIP(5) * 2;

    // 减去 m_contentPreviewText 在 MyCard 内部的边距 (wxALL, 5 * 2)
    int border = FromDIP(5);
    int wrapWidth = cardWidth - (2 * border);

    if (wrapWidth > 0){
        m_contentPreviewText->Wrap(wrapWidth);

        // 重新布局 MyCard 自身 Sizer 以适应新的换行高度
        GetSizer()->Layout();
    }
}

void MyCard::OnLeftDClick(wxMouseEvent& event){
    event.StopPropagation();
    wxCommandEvent customEvent(EVT_CARD_LEFT_DCLICK, GetId());
    customEvent.SetClientData(new wxTreeItemId(m_treeItemId));
    wxPostEvent(wxGetTopLevelParent(this), customEvent);
    //event.Skip();
}

MyCardViewPanel::MyCardViewPanel(wxWindow* parent, wxWindowID id)
    : wxScrolledWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxHSCROLL | wxFULL_REPAINT_ON_RESIZE),
    m_emptyMessageText(nullptr), m_mainSizer(new wxBoxSizer(wxVERTICAL)){

    SetDoubleBuffered(true);
    SetScrollRate(0, 20);
    SetSizer(m_mainSizer);

    m_emptyMessageText = new wxStaticText(this, wxID_ANY, L"这里空空如也", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
    wxFont font = m_emptyMessageText->GetFont();
    font.SetStyle(wxFONTSTYLE_ITALIC);
    m_emptyMessageText->SetFont(font);
    m_emptyMessageText->SetForegroundColour(wxColour(128, 128, 128));

    m_mainSizer->Add(m_emptyMessageText, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
    m_mainSizer->Hide(m_emptyMessageText);

    Bind(EVT_SHOW_NODE_CONTENT, &MyCardViewPanel::OnShowFolderContent, this);
    Bind(wxEVT_SIZE, &MyCardViewPanel::OnSize2, this);
    // SetFocus();
}

MyCardViewPanel::~MyCardViewPanel(){
    for (MyCard* card : m_cardList){
        if (card) card->Destroy();
    }
    m_cardList.clear();
}

void MyCardViewPanel::LoadCards(const std::vector<CardData>& cards){
    Freeze();

    auto color = GetMyAppInstance()->GetThemeColor();
    char buff[4096];
    m_folderIcon = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_default_folder, color, 73), wxSize(24, 24));
    m_noteIcon = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_default_note, color, 73), wxSize(24, 24));
    // 销毁旧卡片
    for (MyCard* card : m_cardList){
        card->Destroy();
    }
    m_cardList.clear();
    if (m_emptyMessageText && m_mainSizer->IsShown(m_emptyMessageText)){
        m_mainSizer->Hide(m_emptyMessageText);
    }
    if (cards.empty()){
        ShowEmptyMessage();
    } else{
        for (const CardData& data : cards){
            MyCard* card = new MyCard(this, data, wxID_ANY);
            m_cardList.push_back(card);
            m_mainSizer->Add(card, 0, wxEXPAND | wxALL, FromDIP(5));
        }
    }
    m_mainSizer->Layout();
    FitInside();
    Scroll(0, 0);
    Thaw();
    Refresh();
}

void MyCardViewPanel::ShowEmptyMessage(){
    m_mainSizer->Show(m_emptyMessageText);
    m_mainSizer->Layout();
}

void MyCardViewPanel::ClearView(){
    Freeze();
    // 销毁旧卡片
    for (MyCard* card : m_cardList){
        card->Destroy();
    }
    m_cardList.clear();
    Layout();
    Thaw();
    Refresh();
}

void MyCardViewPanel::OnShowFolderContent(wxTreeEvent& event){
    std::unique_ptr<std::vector<CardData>> cardList(static_cast<std::vector<CardData>*>(event.GetClientData()));

    if (!cardList) return;

    auto item1 = static_cast<MainFrame*>(wxGetTopLevelParent(this))->m_tree->GetSelection();
    auto item2 = event.GetItem();
    if (item1 == item2){
        LoadCards(*cardList);
    }
}

void MyCardViewPanel::OnSize2(wxSizeEvent& event){
    int panelWidth = GetClientSize().GetWidth();

    // 2. 遍历所有卡片并更新换行
    for (MyCard* card : m_cardList){
        if (card){
            // 调用 MyCard 的更新方法，传入当前面板宽度
            card->UpdateContentWrap(panelWidth);
            int maxWidth = panelWidth - FromDIP(5) * 2;
            if (maxWidth > 0){
                card->SetMaxSize(wxSize(maxWidth, wxDefaultCoord)); // 设置最大宽度
            }
        }
    }

    // 3. 重新布局 MyCardViewPanel 的主 Sizer
    m_mainSizer->Layout();

    // 4. 强制重新计算滚动条范围
    FitInside();

    event.Skip();
}
