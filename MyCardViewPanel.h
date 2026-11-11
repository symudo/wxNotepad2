#pragma once


#include <vector>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/scrolwin.h> 
#include <wx/sizer.h>
#include <wx/statbmp.h> 
#include <wx/stattext.h>
#include <wx/treebase.h>

#define HIGHLIGHT_COLOR wxColour(255, 192, 203)
#define NORMAL_COLOR wxColour(165, 216, 198)

// 定义卡片的数据结构
struct CardData{
    int type;        // 0: 文件夹, 1: 笔记
    wxString name;   // 标题
    wxString info;   // 信息，如文件路径或笔记标签
    wxString date; // 日期，如笔记修改时间，文件夹不显示
    wxString contentPreview; // 内容预览或子项标题
    int id;          // 对应MyTreeItemData的id
    wxTreeItemId treeItemId; //子节点的句柄
    int noteId;      // 如果是笔记，关联的NoteID
};

class MyCard: public wxPanel{
public:
    MyCard(wxWindow* parent, const CardData& data, wxWindowID id);
    wxTreeItemId GetTreeItemId() const{ return m_treeItemId; }
    int GetCardId() const{ return Id; }
    int GetCardNoteId() const{ return noteId; }
    void UpdateContentWrap(int newParentWidth);
private:
    int Id = 0;
    int noteId = -1;
    wxTreeItemId m_treeItemId;
    wxStaticBitmap* m_iconBitmap = nullptr;
    wxStaticText* m_titleText = nullptr;
    wxStaticText* m_infoText = nullptr;
    wxStaticText* m_dateText = nullptr;
    wxStaticText* m_contentPreviewText = nullptr;
    void OnLeftDClick(wxMouseEvent& event);
};

class MyCardViewPanel: public wxScrolledWindow{
public:
    MyCardViewPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
    ~MyCardViewPanel();

    void LoadCards(const std::vector<CardData>& cards);
    void ShowEmptyMessage();
    void ClearView();
    wxBitmapBundle m_folderIcon;
    wxBitmapBundle m_noteIcon;
private:
    
    wxBoxSizer* m_mainSizer;
    wxStaticText* m_emptyMessageText;
    std::vector<MyCard*> m_cardList;
    void OnShowFolderContent(wxTreeEvent& event);
    void OnSize2(wxSizeEvent& event);
};

