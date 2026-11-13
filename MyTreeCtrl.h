#pragma once
#ifndef _MYTREECTRL_H_
#define _MYTREECTRL_H_
#include "wx/treectrl.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "wx/fdrepdlg.h"

struct sqlite3;
class MainFrame;

struct PathRecord{
    int ID;
    int parentID;
    wxString name;
    int nodeOrder;
    bool isFolder;
    bool isTrashed;
    int stateIcon;
    int noteID;
};

//常规节点，来自paths表
class MyTreeItemData: public wxTreeItemData{
public:
    MyTreeItemData(const PathRecord& record){
        m_ID = record.ID;
        m_parentID = record.parentID;
        m_name = record.name;
        m_nodeOrder = record.nodeOrder;
        m_stateIcon = record.stateIcon;
        m_noteID = record.noteID;
        m_isFolder = record.isFolder;
        m_isTrashed = record.isTrashed;
        m_isChanged = false;
    }
    MyTreeItemData(int ID, int parentID, const wxString& name, int nodeOrder, int stateIcon, int noteID, bool isFolder, bool isChanged, bool isTrashed){
        m_ID = ID;
        m_parentID = parentID;
        m_name = name;
        m_nodeOrder = nodeOrder;
        m_stateIcon = stateIcon;
        m_noteID = noteID;
        m_isFolder = isFolder;
        m_isChanged = isChanged;
        m_isTrashed = isTrashed;
    }
    int GetID() const{ return m_ID; }
    void SetID(int ID){ m_ID = ID; }
    int GetParentID() const{ return m_parentID; }
    void SetParentID(int parentID){ m_parentID = parentID; }
    wxString GetName() const{ return m_name; }
    void SetName(const wxString& name){ m_name = name; }
    int GetNodeOrder() const{ return m_nodeOrder; }
    void SetNodeOrder(int nodeOrder){ m_nodeOrder = nodeOrder; }
    int GetStateIcon() const{ return m_stateIcon; }
    void SetStateIcon(int stateIcon){ m_stateIcon = stateIcon; }
    int GetNoteID() const{ return m_noteID; }
    void SetNoteID(int noteID){ m_noteID = noteID; }
    bool IsFolder() const{ return m_isFolder; }
    bool IsChanged() const{ return m_isChanged; }
    void SetChanged(){ m_isChanged = true; }
    bool IsTrashed() const{ return m_isTrashed; }
    void SetTrashed(bool isTrashed){ m_isTrashed = isTrashed; }
private:
    int m_ID;
    int m_parentID;
    wxString m_name;
    int m_nodeOrder;
    int m_stateIcon;
    int m_noteID;
    bool m_isFolder;
    bool m_isChanged;//标记文件夹打开状态
    bool m_isTrashed;
};

//使用两个map来分别映射id和节点、id和&name，注意，如果涉及移动节点（或其他重建的操作），需手动重建相关值；虽然只会涉及note节点，但是这里也保存了文件夹节点的映射，以备扩展
//tags的重命名、删除操作，直接读取节点对应数据库记录并操作，不使用map
class MyTreeCtrl: public wxTreeCtrl{
public:
    MyTreeCtrl(wxWindow* parent, MainFrame* mainFrame, wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
    ~MyTreeCtrl();

    void InitFunctions();
    void InitBind();
    void CommitPendingLabelEdit();
    void ApplyTheme(bool isDark);

    void AddWebCollection(const wxString& url, const wxString& title, const wxString& content);
    void SwitchToWebCollectionNote();

    void OnFindSQL(wxFindDialogEvent& event);
    void RenameCurrentItem(wxString& newName);
    void OnMyCardViewPanelDClick(wxCommandEvent& event){
        wxTreeItemId* itemPtr = static_cast<wxTreeItemId*>(event.GetClientData());
        if (itemPtr && itemPtr->IsOk()){
            wxTreeItemId item = *itemPtr;
            SelectItem(item);
            delete itemPtr;
        }
        event.Skip();
    }
    void OnChangeNoteTags(wxTreeEvent& event);
    inline int GetTreeItemType(wxTreeItemId item);
    void RestoreNoteContent();//通过重新发送事件来还原笔记内容
    std::unordered_set<int> GetNoteTagList(sqlite3* db, int pathID);
    //菜单方法
    void OnEmptyRecycleBin(wxCommandEvent& event);
    void OnDeleteItem(wxCommandEvent& event);
    void OnRemoveToTrash(wxCommandEvent& event);
    void OnRenameItem(wxCommandEvent& event);
    void OnNewFolder(wxCommandEvent& event);
    void OnNewNote(wxCommandEvent& event);
    void OnNewTag(wxCommandEvent& event);
    void OnRemoveTag(wxCommandEvent& event);
    void OnImportNote(wxCommandEvent& event);
    void OnExportItem(wxCommandEvent& event);
    void OnImportFolder(wxCommandEvent& event);
    void OnShowTags(wxCommandEvent& event);
    void OnHideTags(wxCommandEvent& event);
    void OnShowRecent(wxCommandEvent& event);
    void OnHideRecent(wxCommandEvent& event);
    void OnChangeIconStyle(wxCommandEvent& event);
    void OnChangeIconColor(wxCommandEvent& event);

    std::unordered_map<int, wxTreeItemId> m_idToItem;
    std::unordered_map<int, wxString> m_TagsList;		//保存tag表，用于右键菜单
    wxTreeItemId m_prev{ 0 };
    //特殊节点ID，从-1开始的负数（0留空，避免与数据库ID冲突）
    enum SpecialTreeIDs{
        SearchItemID = -20,
        TagItemID,
        TrashItemID,
        FolderItemID,
        ErrorTypeID,
        WebCollectionItemID,
        WebCollectionRootID = -11,
        SearchRootID,
        RecentAccessedID,
        RecentModifiedID,
        RecentCreatedID,
        RecentRootID,
        TagsRootID,
        TrashRootID = -3,
        FolderRootID = -2,
        HiddenRootID = -1,
    };
    wxTreeItemId GetCurrentItem() const{ return m_currentItem; }
private:
    MainFrame* m_mainFrame;
    wxImageList* m_imageListTreeCtrl = nullptr;
    wxTreeItemId m_hiddenRoot;
    wxTreeItemId m_currentItem;	//当前节点
    wxTreeItemId m_draggedItem;	//拖动节点
    bool m_isDragging;	//是否正在拖动
    int m_dropPosition;	//拖动位置
    std::unordered_map<int, MyTreeItemData*> m_idToData;
    int m_IconCache[12][8] = { 0 };

    void Initialization();	//初始化文件夹，回收站，右键tags
    void ShowRecent();
    void HideRecent();
    void ShowTags();
    void HideTags();

    //事件方法
    void OnBeginDrag(wxTreeEvent& event);
    void OnEndDrag(wxTreeEvent& event);
    void OnBeginLabelEdit(wxTreeEvent& event);
    void OnEndLabelEdit(wxTreeEvent& event);
    void OnTreeKeyDown(wxTreeEvent& event);
    void OnSelChanged(wxTreeEvent& event);
    void OnSelChanging(wxTreeEvent& event);
    void OnItemMenu(wxTreeEvent& event);

    void InitTreeCtrlIcons();

    //事件辅助，非数据库操作
    inline bool IsTreeItemDescendant(wxTreeItemId parent, wxTreeItemId child);
    //事件辅助，数据库操作
    wxTreeItemId TreeMoveItem(wxTreeItemId fromItem, wxTreeItemId toItem, int flag);
    long long AppendNewRecordToPaths(sqlite3* db, wxString& name, int parentID, int type, int order, int stateIcon, int noteID, int trashed);
    long long AppendNewRecordToNotes(sqlite3* db, const wxString& content, int pathId, const wxString& url);
    void UpdateStateIcon(sqlite3* db, wxTreeItemId item);
    void AddTagForNote(wxTreeItemId item, int tagID);
    void RemoveTagForNote(wxTreeItemId item, int tagID);
    int AddNewTag(sqlite3* db, wxString& name);
    //获取笔记的标签
    wxString GetNoteTags(sqlite3* db, int pathID);
    //获取标签的笔记构造的字符串
    wxString GetTagNotes(sqlite3* db, int tagID);
    //获取包含标签的笔记项
    std::vector<int> GetNotesWithTag(sqlite3* db, int tagID);
    int CheckTagExits(wxString& name);

    //获取最近笔记的路径ID,type=0为创建时间，1为修改时间,2为访问时间
    std::vector<int> GetRecentNotes(sqlite3* db, int type, int maxCount);
    //获取笔记时间戳,type=0为创建时间，1为修改时间,2为访问时间
    wxString GetNoteTimestamp(sqlite3* db, int noteID, int type);
    void RemoveTagFromTags(sqlite3* db, int tagID);
    void DeleteItemAndData(sqlite3* db, wxTreeItemId item);
    //构造文件夹路径字符串
    wxString MakePath(const wxTreeItemId& item);
    wxString MakePath(const wxTreeItemId& item, const wxTreeItemId& root);
    //获取笔记内容
    wxString GetNoteContent(sqlite3* db, wxTreeItemId item, std::vector<UINT16>& styleData);
    wxString GetWebNoteUrl(sqlite3* db, wxTreeItemId& item);

    void ExportNode(const wxString& path, const wxTreeItemId& item);
    void RecursiveExportNode(const wxString& currentBaseDir, const wxTreeItemId& item);
    bool ImportSingleNote(const wxTreeItemId& root, const wxString& filepath);
    void RecursiveImportFolder(const wxTreeItemId& parentTreeItem, const wxString& currentFolderPath);
    //递归复制节点，并处理data，不涉及数据库
    wxTreeItemId CopyItemRecursivelyAndMoveData(const wxTreeItemId& itemToCopy, const wxTreeItemId& targetParent, std::unordered_map<int, wxTreeItemId>& mapToUpdate);
    //需要更新来源与目标处的order
    bool UpdateNodeOrderForParent(sqlite3* db, wxTreeItemId parentItem);
    void PostEventToCardViewPanel(const wxTreeItemId& currentItem);
    int GetIconIndex(int stateicon);
    wxTreeItemId TreeSwapSiblings(wxTreeItemId& currentItem, int direction);

    wxTreeItemId FindOrCreateWebCollectionNote(const wxTreeItemId& parentItem, wxString& targetText);
    void ScheduleEditLabel(const wxTreeItemId& item);
    void ScheduleSetItemText(const wxTreeItemId& item, const wxString& text);

};

#endif