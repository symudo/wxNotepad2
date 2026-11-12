#include "pch.h"

#include "App.h"
#include "events.h"
#include "mainFrame.h"
#include "MyCardViewPanel.h"
#include "MyTreeCtrl.h"
#include "Helper.h"
#include "SVG_ICON.h"
#include <commctrl.h>
#include <wx/weakref.h>

MyTreeCtrl::MyTreeCtrl(wxWindow* parent, MainFrame* mainFrame, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : wxTreeCtrl(parent, id, pos, size, style), m_mainFrame(mainFrame), m_isDragging(false), m_dropPosition(0){
    InitTreeCtrlIcons();
    SetImageList(m_imageListTreeCtrl);
    Initialization();
}

MyTreeCtrl::~MyTreeCtrl(){
    //记录退出时当前节点
    auto currentItem = GetSelection();
    auto currentItemData = currentItem.IsOk() ? static_cast<MyTreeItemData*>(GetItemData(currentItem)) : nullptr;
    int currentItemId = currentItemData ? currentItemData->GetID() : 0;
    //退出前解绑选择与标签编辑事件，避免析构期间触发回调访问已进入析构流程的对象
    Unbind(wxEVT_TREE_SEL_CHANGED, &MyTreeCtrl::OnSelChanged, this);
    Unbind(wxEVT_TREE_SEL_CHANGING, &MyTreeCtrl::OnSelChanging, this);
    Unbind(wxEVT_TREE_END_LABEL_EDIT, &MyTreeCtrl::OnEndLabelEdit, this);
    //遍历所有节点获取打开状态
    std::vector<int> stateExpanded;
    stateExpanded.reserve(m_idToItem.size());
    for (auto const& [id, item] : m_idToItem){
        if (id > 0 && item.IsOk() && IsExpanded(item)){
            stateExpanded.push_back(id);
        }
    }
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    int rc = sqlite3_exec(db, "UPDATE PATHS SET TYPE = TYPE &~6;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("Reset status failed.");
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);

        delete m_imageListTreeCtrl;
        return;
    }

    sqlite3_stmt* stmt_update = nullptr;
    const char* sql_update = "UPDATE PATHS SET TYPE = TYPE | 2 WHERE ID = ?;";
    rc = sqlite3_prepare_v2(db, sql_update, -1, &stmt_update, nullptr);
    if (rc == SQLITE_OK){
        for (int id : stateExpanded){
            sqlite3_reset(stmt_update);
            sqlite3_bind_int(stmt_update, 1, id);
            sqlite3_step(stmt_update);
        }
        sqlite3_finalize(stmt_update);
    } else{
        _wxLogError("Prepare update failed: %s", sqlite3_errmsg(db));
    }
    if (currentItemId > 0){
        sqlite3_stmt* stmt_last = nullptr;
        const char* sql_last = "UPDATE PATHS SET TYPE = TYPE | 4 WHERE ID = ?;";
        rc = sqlite3_prepare_v2(db, sql_last, -1, &stmt_last, nullptr);
        if (rc == SQLITE_OK){
            sqlite3_bind_int(stmt_last, 1, currentItemId);
            sqlite3_step(stmt_last);
            sqlite3_finalize(stmt_last);
        }
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

    delete m_imageListTreeCtrl;
}

void MyTreeCtrl::ShowRecent(){
    //Recent是主功能下第二个节点，从map中检查是否存在TagsRootId
    sqlite3* db = GetMyAppInstance()->db;
    Freeze();
    MyTreeItemData* recentRootData = new MyTreeItemData(RecentRootID, HiddenRootID, L"最近的笔记", 3, TreeIcon::ICON_HISTORY, -1, true, false, false);
    wxTreeItemId recentRootItem;
    if (m_idToItem.count(TagsRootID) == 0){
        recentRootItem = InsertItem(m_hiddenRoot, m_idToItem[TrashRootID], L"最近的笔记", TreeIcon::ICON_HISTORY, -1, recentRootData);
    } else{
        recentRootItem = InsertItem(m_hiddenRoot, m_idToItem[TagsRootID], L"最近的笔记", TreeIcon::ICON_HISTORY, -1, recentRootData);
    }
    //只把最近根加入到map，以便读取
    m_idToData[RecentRootID] = recentRootData;
    m_idToItem[RecentRootID] = recentRootItem;
    MyTreeItemData* recentCreatedData = new MyTreeItemData(RecentCreatedID, RecentRootID, L"最近创建", 0, TreeIcon::ICON_HISTORY, -1, true, false, false);
    MyTreeItemData* recentModifiedData = new MyTreeItemData(RecentModifiedID, RecentRootID, L"最近修改", 1, TreeIcon::ICON_HISTORY, -1, true, false, false);
    MyTreeItemData* recentAccessedData = new MyTreeItemData(RecentAccessedID, RecentRootID, L"最近访问", 2, TreeIcon::ICON_HISTORY, -1, true, false, false);
    wxTreeItemId recentCreatedItem = AppendItem(recentRootItem, L"最近创建", TreeIcon::ICON_HISTORY, -1, recentCreatedData);
    wxTreeItemId recentModifiedItem = AppendItem(recentRootItem, L"最近修改", TreeIcon::ICON_HISTORY, -1, recentModifiedData);
    wxTreeItemId recentAccessedItem = AppendItem(recentRootItem, L"最近访问", TreeIcon::ICON_HISTORY, -1, recentAccessedData);
    Thaw();
    //SelectItem(recentRootItem);
}

void MyTreeCtrl::HideRecent(){
    SetEvtHandlerEnabled(false);
    Delete(m_idToItem[RecentRootID]);
    SetEvtHandlerEnabled(true);
    m_idToData.erase(RecentRootID);
    m_idToItem.erase(RecentRootID);
}

void MyTreeCtrl::ShowTags(){
    //Tags是主功能下第一个节点，使用插入方法;从m_TagsList中读取
    sqlite3* db = GetMyAppInstance()->db;
    Freeze();
    MyTreeItemData* tagsRootData = new MyTreeItemData(TagsRootID, HiddenRootID, L"标签", 2, TreeIcon::ICON_TAGS, -1, true, false, false);
    wxTreeItemId tagsRootItem = InsertItem(m_hiddenRoot, m_idToItem[TrashRootID], L"标签", TreeIcon::ICON_TAGS, -1, tagsRootData);
    //只把标签根加入到map，以便读取（id不重复）；标签不加入map
    m_idToData[TagsRootID] = tagsRootData;
    m_idToItem[TagsRootID] = tagsRootItem;

    int id;
    wxString name;
    for (const auto& [id, name] : m_TagsList){
        MyTreeItemData* tagData = new MyTreeItemData(id, TagsRootID, name, 0, TreeIcon::ICON_TAG, -1, true, false, false);
        wxTreeItemId tagItem = AppendItem(tagsRootItem, name, TreeIcon::ICON_TAG, -1, tagData);
    }

    Thaw();
}

void MyTreeCtrl::HideTags(){
    SetEvtHandlerEnabled(false);
    Delete(m_idToItem[TagsRootID]);
    SetEvtHandlerEnabled(true);
    m_idToData.erase(TagsRootID);
    m_idToItem.erase(TagsRootID);
}

void MyTreeCtrl::Initialization(){
    sqlite3* db = GetMyAppInstance()->db;
    DeleteAllItems();
    //初始化文件夹和回收站
    MyTreeItemData* hiddenRootData = new MyTreeItemData(HiddenRootID, 0, "HiddenROOT", 0, TreeIcon::ICON_DEFAULT_FOLDER, -1, true, false, false);
    MyTreeItemData* folderRootData = new MyTreeItemData(FolderRootID, HiddenRootID, L"全部", 0, TreeIcon::ICON_DEFAULT_FOLDER, -1, true, false, false);
    MyTreeItemData* trashRootData = new MyTreeItemData(TrashRootID, HiddenRootID, L"回收站", 1, TreeIcon::ICON_RECYCLEBIN, -1, true, false, true);

    m_hiddenRoot = AddRoot("HiddenROOT", -1, -1, hiddenRootData);
    wxTreeItemId folderRootItem = AppendItem(m_hiddenRoot, L"全部", TreeIcon::ICON_DEFAULT_FOLDER, -1, folderRootData);
    //是否启用Extension功能
    /*wxConfigBase* config = wxConfigBase::Get();
    bool enableWebCollection = config->ReadBool("Tree/EnableWebCollection", false);*/
    bool enableWebCollection = true;
    if (enableWebCollection){
        auto extensionRootData = new MyTreeItemData(WebCollectionRootID, HiddenRootID, L"网络收集", 2, TreeIcon::ICON_WEBCOLLECTION, -1, true, false, false);
        auto extensionRootItem = AppendItem(m_hiddenRoot, L"网络收集", TreeIcon::ICON_WEBCOLLECTION, -1, extensionRootData);
        m_idToData[WebCollectionRootID] = extensionRootData;
        m_idToItem[WebCollectionRootID] = extensionRootItem;
    }
    wxTreeItemId trashRootItem = AppendItem(m_hiddenRoot, L"回收站", TreeIcon::ICON_RECYCLEBIN, -1, trashRootData);

    m_idToData[HiddenRootID] = hiddenRootData;
    m_idToData[FolderRootID] = folderRootData;
    m_idToData[TrashRootID] = trashRootData;
    m_idToItem[HiddenRootID] = m_hiddenRoot;
    m_idToItem[FolderRootID] = folderRootItem;
    m_idToItem[TrashRootID] = trashRootItem;


    // 开始读取paths并构建树
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sqlite3_stmt* stmt;
    // 查询所有 PATHS 表中的数据，并直接按 PARENT_ID 和 NODE_ORDER 进行排序
    const char* sql = "SELECT ID, NAME, PARENT_ID, TYPE, NODE_ORDER, STATEICON, NOTE_ID, TRASHED FROM PATHS ORDER BY PARENT_ID, NODE_ORDER ASC;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        return;
    }
    std::vector<PathRecord> pathRecords;
    pathRecords.reserve(64);
    std::vector<int> stateExpanded;
    stateExpanded.reserve(64);
    int focusItemId = 0;    //恢复关闭时节点
    while (sqlite3_step(stmt) == SQLITE_ROW){
        PathRecord record;
        record.ID = sqlite3_column_int(stmt, 0);
        record.name = wxString::FromUTF8((const char*)sqlite3_column_text(stmt, 1));
        record.parentID = sqlite3_column_int(stmt, 2);
        int typeBit = sqlite3_column_int(stmt, 3);
        record.isFolder = (typeBit & 1);
        record.nodeOrder = sqlite3_column_int(stmt, 4);
        record.stateIcon = sqlite3_column_int(stmt, 5);
        record.noteID = sqlite3_column_int(stmt, 6);
        record.isTrashed = (sqlite3_column_int(stmt, 7) == 1);
        if (record.isFolder && typeBit & 2){ stateExpanded.push_back(record.ID); };
        if (typeBit & 4)focusItemId = record.ID;
        pathRecords.push_back(std::move(record));
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    Freeze();
    int procesedCount = 0;
    bool ProcessInThisLoop = true;
    while (ProcessInThisLoop && procesedCount < pathRecords.size()){
        ProcessInThisLoop = false;
        for (const auto& record : pathRecords){
            if (m_idToData.count(record.ID) == 0){//未处理节点
                wxTreeItemId parentTreeId = m_idToItem[record.parentID];
                if (parentTreeId == NULL){//父节点不存在，跳过
                    continue;
                }
                MyTreeItemData* data = new MyTreeItemData(record);
                m_idToData[record.ID] = data;
                wxTreeItemId itemId = AppendItem(parentTreeId, record.name, GetIconIndex(record.stateIcon), -1, data);
                m_idToItem[record.ID] = itemId;
                procesedCount++;
                ProcessInThisLoop = true;
            }
        }
    }
    //展开指定节点
    Expand(m_idToItem[FolderRootID]);
    for (int i : stateExpanded){
        if (m_idToItem.count(i))Expand(m_idToItem[i]);
    }
    Thaw();

    //读取tag
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql2 = "SELECT ID, TAG_NAME FROM TAGS ORDER BY ID ASC;";
    rc = sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW){
        int id = sqlite3_column_int(stmt, 0);
        wxString name = wxString::FromUTF8((const char*)sqlite3_column_text(stmt, 1));
        m_TagsList[id] = name;
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    if (focusItemId && m_idToItem.count(focusItemId)){
        m_currentItem = m_idToItem[focusItemId];
    } else{
        m_currentItem = m_idToItem[FolderRootID];
    }

}

void MyTreeCtrl::InitFunctions(){
    //设置是否显示最近、标签
    if (m_mainFrame->m_showRecent){
        ShowRecent();
    }
    if (m_mainFrame->m_showTags){
        ShowTags();
    }
}

void MyTreeCtrl::InitBind(){
    Bind(wxEVT_TREE_SEL_CHANGED, &MyTreeCtrl::OnSelChanged, this);
    Bind(wxEVT_TREE_SEL_CHANGING, &MyTreeCtrl::OnSelChanging, this);
    Bind(wxEVT_TREE_BEGIN_DRAG, &MyTreeCtrl::OnBeginDrag, this);
    Bind(wxEVT_TREE_END_DRAG, &MyTreeCtrl::OnEndDrag, this);
    Bind(wxEVT_TREE_BEGIN_LABEL_EDIT, &MyTreeCtrl::OnBeginLabelEdit, this);
    Bind(wxEVT_TREE_END_LABEL_EDIT, &MyTreeCtrl::OnEndLabelEdit, this);
    Bind(wxEVT_TREE_KEY_DOWN, &MyTreeCtrl::OnTreeKeyDown, this);
    Bind(wxEVT_TREE_ITEM_MENU, &MyTreeCtrl::OnItemMenu, this);

    Bind(wxEVT_MENU, &MyTreeCtrl::OnHideRecent, this, MainFrame::MENU_HIDE_RECENT);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnNewTag, this, MainFrame::MENU_NEW_TAG);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnHideTags, this, MainFrame::MENU_HIDE_TAGS);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnChangeIconColor, this, MainFrame::MENU_ICON_CHANGE_COLOR_BASE, MainFrame::MENU_ICON_CHANGE_COLOR_TOP);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnChangeIconStyle, this, MainFrame::MENU_ICON_CHANGE_STYLE_RESTORE, MainFrame::MENU_ICON_CHANGE_STYLE_TOP);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnDeleteItem, this, MainFrame::MENU_DELETE_ITEM);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnRemoveTag, this, MainFrame::MENU_REMOVE_TAG);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnNewTag, this, MainFrame::MENU_NEW_TAG);
    //Bind(wxEVT_MENU, &MyTreeCtrl::OnDeleteItem, this, MainFrame::MENU_DELETE_EXTENSION);
}

void MyTreeCtrl::OnBeginLabelEdit(wxTreeEvent& event){
    //可重命名的节点，包括[全部]中的节点、tags节点、扩展节点
    auto itemType = GetTreeItemType(event.GetItem());
    if ((itemType == FolderItemID) || (itemType == TagItemID) || (itemType == WebCollectionItemID)){
        event.Skip();
    } else{
        event.Veto();
    }
}

void MyTreeCtrl::CommitPendingLabelEdit(){
    if (GetEditControl()){
        ::SendMessage((HWND)GetHWND(), TVM_ENDEDITLABELNOW, FALSE, 0);
    }
}

void MyTreeCtrl::ScheduleEditLabel(const wxTreeItemId& item){
    wxWeakRef<MyTreeCtrl> self(this);
    CallAfter([self, item](){
        if (self && !self->IsBeingDeleted() && item.IsOk()){
            self->EditLabel(item);
        }
    });
}

void MyTreeCtrl::ScheduleSetItemText(const wxTreeItemId& item, const wxString& text){
    wxWeakRef<MyTreeCtrl> self(this);
    CallAfter([self, item, text](){
        if (self && !self->IsBeingDeleted() && item.IsOk()){
            self->SetItemText(item, text);
        }
    });
}

void MyTreeCtrl::OnEndLabelEdit(wxTreeEvent& event){
    // 获取编辑的节点和标签
    wxTreeItemId itemId = event.GetItem();
    if (IsBeingDeleted() || !m_mainFrame || m_mainFrame->IsBeingDeleted()){
        return;
    }
    if (event.IsEditCancelled() || !itemId.IsOk()){
        return;
    }
    auto itemType = GetTreeItemType(itemId);
    bool fileModified = GetItemText(itemId).EndsWith("*");
    wxString newName = event.GetLabel();
    newName = SanitizeName(newName);
    if (newName.IsEmpty()){
        event.Veto();
        return;
    }

    // 获取节点数据并检查名称是否变化
    MyTreeItemData* data = static_cast<MyTreeItemData*>(GetItemData(itemId));
    if (!data){
        event.Veto();
        return;
    }
    auto oldName = data->GetName();
    if (oldName == newName){
        event.Veto();
        return;
    }

    //开始数据库事务
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);

    //处理 PATHS 或 TAGS 表的更新
    if ((itemType == FolderItemID) || (itemType == WebCollectionItemID)){
        // PATHS 表更新
        sqlite3_stmt* stmt;
        const char* sql = "UPDATE PATHS SET NAME = ? WHERE ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK){
            _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_bind_text(stmt, 1, newName.utf8_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK){
            _wxLogError("SQL bind error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_bind_int(stmt, 2, data->GetID());
        if (rc != SQLITE_OK){
            _wxLogError("SQL bind error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE){
            _wxLogError("SQL step error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        sqlite3_finalize(stmt);
        data->SetName(newName);
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        if (fileModified){
            newName += "*";
        }
        event.Veto();
        ScheduleSetItemText(itemId, newName);
        return;
    } else{
        // 检查 m_TagsList 中 newName 是否重复
        auto it = std::find_if(m_TagsList.begin(), m_TagsList.end(),
            [newName, data](const std::pair<int, wxString>& pair){
                return pair.second == newName && pair.first != data->GetID();
            });
        if (it != m_TagsList.end()){
            _wxLogError("Tag name '%s' already exists", newName);
            //wxMessageBox(wxString::Format("标签名称 '%s' 已存在，请选择其他名称", newName),"错误", wxOK | wxTreeIcon::ICON_ERROR);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }

        // TAGS 表更新
        sqlite3_stmt* stmt;
        const char* sql = "UPDATE TAGS SET TAG_NAME = ? WHERE ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK){
            _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_bind_text(stmt, 1, newName.utf8_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK){
            _wxLogError("SQL bind error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_bind_int(stmt, 2, data->GetID());
        if (rc != SQLITE_OK){
            _wxLogError("SQL bind error: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE){
            if (rc == SQLITE_CONSTRAINT){
                _wxLogError("Tag name '%s' already exists in TAGS table", newName);
                //	wxMessageBox(wxString::Format("标签名称 '%s' 已存在，请选择其他名称", newName),"错误", wxOK | wxTreeIcon::ICON_ERROR);
            } else{
                _wxLogError("SQL step error: %s", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            event.Veto();
            return;
        }
        sqlite3_finalize(stmt);
        m_TagsList[data->GetID()] = newName;
        data->SetName(newName);
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        event.Veto();
        ScheduleSetItemText(itemId, newName);
        return;
    }
}

void MyTreeCtrl::OnBeginDrag(wxTreeEvent& event){
    //允许拖动的节点，包括[全部]下，[回收站]下,网摘
    wxTreeItemId itemId = event.GetItem();
    int itemType = GetTreeItemType(itemId);
    if (itemType == FolderItemID || itemType == TrashItemID || itemType == WebCollectionItemID){
        m_draggedItem = itemId;
        event.Allow();
        m_isDragging = true;
        SelectItem(m_draggedItem);
        return;
    } else{
        event.Veto();
    }
}

void MyTreeCtrl::OnEndDrag(wxTreeEvent& event){
    /*功能
    * 1.移动：在[全部]下，[回收站]下，[网摘]下，移动（不跨根）；如果是文件夹，插入到文件夹下，如果是笔记，插入到笔记后一个位置
    * 2.移动：从[全部]下移动到[回收站]下、从[网摘]下移动到[回收站]下，要删除url字段
    * 3.移动：从[回收站]下移动到[全部]下
    * 4.移动：从[网摘]移动到[全部]下，要删除url字段
    */
    int state = 0;
    if (!event.IsAllowed() || !m_draggedItem.IsOk()){
        m_draggedItem.Unset();
        m_isDragging = false;
        return;
    }
    wxPoint dropPoint = event.GetPoint(); // 获取鼠标释放时的坐标
    int flags = 0;
    wxTreeItemId dropTargetItem = HitTest(dropPoint, flags);
    //判断状态，首先不允许拖动到自身或子节点
    if (m_draggedItem == dropTargetItem || IsTreeItemDescendant(m_draggedItem, dropTargetItem)){
        m_isDragging = false;
        m_draggedItem.Unset();
        return;
    }
    int dropTargetItemType = GetTreeItemType(dropTargetItem);
    int draggedItemType = GetTreeItemType(m_draggedItem);
    if (draggedItemType == FolderItemID && (dropTargetItemType == FolderItemID || dropTargetItemType == FolderRootID)){
        state = 1;
    } else if ((draggedItemType == FolderItemID || draggedItemType == WebCollectionItemID) && (dropTargetItemType == TrashItemID || dropTargetItemType == TrashRootID)){
        state = 2;
    } else if (draggedItemType == TrashItemID && (dropTargetItemType == FolderItemID || dropTargetItemType == FolderRootID)){
        state = 3;
    } else if (draggedItemType == TrashItemID && (dropTargetItemType == TrashItemID || dropTargetItemType == TrashRootID)){
        state = 1;
    } else if (draggedItemType == WebCollectionItemID && (dropTargetItemType == WebCollectionItemID || draggedItemType == WebCollectionRootID)){
        state = 1;
    } else if (draggedItemType == WebCollectionItemID && (dropTargetItemType == FolderItemID || dropTargetItemType == FolderRootID)){
        state = 4;
    }

    switch (state){
    case 1:
        TreeMoveItem(m_draggedItem, dropTargetItem, 1);
        break;
    case 2:
        TreeMoveItem(m_draggedItem, dropTargetItem, 2);
        break;
    case 3:
        TreeMoveItem(m_draggedItem, dropTargetItem, 3);
        break;
    case 4:
        TreeMoveItem(m_draggedItem, dropTargetItem, 4);
        break;
    }
    m_isDragging = false;
    m_draggedItem.Unset();
    return;
}

inline bool MyTreeCtrl::IsTreeItemDescendant(wxTreeItemId parent, wxTreeItemId child){
    //判断child是否是parent的子节点
    for (auto currentItem = GetItemParent(child); currentItem != m_hiddenRoot; currentItem = GetItemParent(currentItem)){
        if (currentItem == parent){
            return true;
        }
    }
    return false;
}

void MyTreeCtrl::OnChangeNoteTags(wxTreeEvent& event){
    //批量删除和添加tag的功能
    wxTreeItemId itemId = event.GetItem();
    auto str = event.GetString();
    auto tagToDel = static_cast<std::vector<int>*>(event.GetClientData());
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    //首先处理标签删除
    for (auto tagID : *tagToDel){
        RemoveTagForNote(itemId, tagID);
    }
    delete tagToDel;
    //然后处理标签添加
    auto TagStrs = wxSplit(str, ' ');
    for (auto tagName : TagStrs){
        if (tagName.IsEmpty())continue;
        int existID = CheckTagExits(tagName);
        if (existID == -1){
            //不存在新tag，先添加
            int newTagID = AddNewTag(db, tagName);
            m_TagsList[newTagID] = tagName;
            //bug修正：只有tags根节点显示时才添加实际节点
            if (m_idToItem.count(TagsRootID) && m_idToItem[TagsRootID].IsOk()){
                auto order = GetChildrenCount(m_idToItem[TagsRootID]);
                MyTreeItemData* newTagData = new MyTreeItemData(newTagID, TagsRootID, tagName, order, TreeIcon::ICON_TAG, -1, false, false, false);
                auto newTagItem = AppendItem(m_idToItem[TagsRootID], tagName, TreeIcon::ICON_TAG, -1, newTagData);
            }
            //再为当前笔记添加
            AddTagForNote(itemId, newTagID);
        } else{
            //存在tag，直接添加
            AddTagForNote(itemId, existID);
        }
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
}

inline int MyTreeCtrl::GetTreeItemType(wxTreeItemId item){
    // 检查 ID 是否存在于 map 中
    if (m_idToItem.count(HiddenRootID) && item == m_idToItem[HiddenRootID]) return HiddenRootID;
    if (m_idToItem.count(FolderRootID) && item == m_idToItem[FolderRootID]) return FolderRootID;
    if (m_idToItem.count(TrashRootID) && item == m_idToItem[TrashRootID]) return TrashRootID;
    if (m_idToItem.count(TagsRootID) && item == m_idToItem[TagsRootID]) return TagsRootID;
    if (m_idToItem.count(RecentRootID) && item == m_idToItem[RecentRootID]) return RecentRootID;
    if (m_idToItem.count(SearchRootID) && item == m_idToItem[SearchRootID]) return SearchRootID;
    if (m_idToItem.count(WebCollectionRootID) && item == m_idToItem[WebCollectionRootID]) return WebCollectionRootID;
    // 在调用 IsTreeItemDescendant 之前，先确保根节点有效
    if (m_idToItem.count(FolderRootID) && IsTreeItemDescendant(m_idToItem[FolderRootID], item)) return FolderItemID;
    if (m_idToItem.count(TrashRootID) && IsTreeItemDescendant(m_idToItem[TrashRootID], item)) return TrashItemID;
    if (m_idToItem.count(TagsRootID) && IsTreeItemDescendant(m_idToItem[TagsRootID], item)) return TagItemID;
    if (m_idToItem.count(RecentRootID) && IsTreeItemDescendant(m_idToItem[RecentRootID], item)){
        auto data = static_cast<MyTreeItemData*>(GetItemData(item));
        if (data){
            return data->GetID();
        }
    }
    if (m_idToItem.count(WebCollectionRootID) && IsTreeItemDescendant(m_idToItem[WebCollectionRootID], item))return WebCollectionItemID;
    return ErrorTypeID;
}

void MyTreeCtrl::RestoreNoteContent(){
    auto currentItem = GetSelection();
    int type = GetTreeItemType(currentItem);
    if (type != FolderItemID)return;
    auto data = static_cast<MyTreeItemData*>(GetItemData(currentItem));
    if (!data || data->IsFolder())return;

    sqlite3* db = GetMyAppInstance()->db;
    std::vector<UINT16>* styleData = new std::vector<UINT16>();
    wxString noteContent = GetNoteContent(db, currentItem, *styleData);
    wxTreeEvent evt(EVT_SHOW_NODE_CONTENT, GetId());
    evt.SetItem(currentItem);
    evt.SetString(noteContent); // 使用SetString传递字符串
    int state = data->GetStateIcon() << 1 | (type == TrashItemID ? 1 : 0);
    evt.SetInt(state);
    evt.SetClientData(styleData);
    wxPostEvent(m_mainFrame, evt);
}

long long MyTreeCtrl::AppendNewRecordToPaths(sqlite3* db, wxString& name, int parentID, int type, int order, int stateIcon, int noteID, int trashed = 0){
    sqlite3_stmt* stmt = nullptr;
    long long newID = -1;
    const char* sql = "INSERT INTO PATHS (NAME, PARENT_ID, TYPE, NODE_ORDER, STATEICON, NOTE_ID, TRASHED) VALUES (?,?,?,?,?,?,?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
        return -1;
    }
    if (sqlite3_bind_text(stmt, 1, name.utf8_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK){
        _wxLogError("SQL bind error on NAME: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    if (sqlite3_bind_int(stmt, 2, parentID) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, type) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, order) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 5, stateIcon) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 6, noteID) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 7, trashed) != SQLITE_OK){

        _wxLogError("SQL bind error on other parameters: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE){
        _wxLogError("SQL execution error: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    newID = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    return newID;
}

long long MyTreeCtrl::AppendNewRecordToNotes(sqlite3* db, const wxString& content, int pathId, const wxString& url = wxEmptyString){
    sqlite3_stmt* stmt = nullptr;
    long long newID = -1;
    const char* sql = "INSERT INTO NOTES (PATH_ID, CONTENT, URL) VALUES (?,?,?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare error: %s", sqlite3_errmsg(db));
        return -1;
    }
    if (sqlite3_bind_int(stmt, 1, pathId) != SQLITE_OK){
        _wxLogError("SQL bind error on PATH_ID: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    if (sqlite3_bind_text(stmt, 2, content.utf8_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK){
        _wxLogError("SQL bind error on CONTENT: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    if (sqlite3_bind_text(stmt, 3, url.utf8_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK){
        _wxLogError("SQL bind error on CONTENT: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE){
        _wxLogError("SQL execution error: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    newID = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    return newID;
}

void MyTreeCtrl::UpdateStateIcon(sqlite3* db, wxTreeItemId item){
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData)return;
    int recordID = itemData->GetID();
    int currentStateIcon = itemData->GetStateIcon();
    const char* sql = "UPDATE PATHS SET STATEICON = ? WHERE ID = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        return;
    }
    rc = sqlite3_bind_int(stmt, 1, currentStateIcon);
    if (rc != SQLITE_OK){
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_int(stmt, 2, recordID);
    if (rc != SQLITE_OK){
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MyTreeCtrl::AddTagForNote(wxTreeItemId item, int tagID){
    // 获取源节点的ID
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData) return;
    if (m_TagsList.count(tagID) == 0)return;
    int pathID = itemData->GetID();

    sqlite3* db = GetMyAppInstance()->db;
    const char* sql = "INSERT INTO PATHS_TAGS (PATH_ID, TAG_ID) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        return;
    }
    sqlite3_bind_int(stmt, 1, pathID);
    sqlite3_bind_int(stmt, 2, tagID);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MyTreeCtrl::RemoveTagForNote(wxTreeItemId item, int tagID){
    // 获取源节点的ID
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData)return;
    if (m_TagsList.count(tagID) == 0)return;
    int pathID = itemData->GetID();

    sqlite3* db = GetMyAppInstance()->db;
    const char* sql = "DELETE FROM PATHS_TAGS WHERE PATH_ID = ? AND TAG_ID = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        return;
    }
    sqlite3_bind_int(stmt, 1, pathID);
    sqlite3_bind_int(stmt, 2, tagID);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int MyTreeCtrl::AddNewTag(sqlite3* db, wxString& name){
    sqlite3_stmt* stmt;
    int newID = -1;
    const char* sql = "INSERT INTO TAGS (TAG_NAME) VALUES (?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL error preparing statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    if (sqlite3_bind_text(stmt, 1, name.utf8_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error inserting new record: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    newID = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return newID;
}

wxString MyTreeCtrl::GetNoteTags(sqlite3* db, int pathID){
    //获取笔记的标签
    const char* sql =
        "SELECT T.TAG_NAME "
        "FROM PATHS_TAGS PT "
        "JOIN TAGS T ON PT.TAG_ID = T.ID "
        "WHERE PT.PATH_ID = ?";

    sqlite3_stmt* stmt = nullptr;
    std::vector<wxString> tags;
    tags.reserve(8);
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return L"无标签";
    }
    sqlite3_bind_int(stmt, 1, pathID);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        const char* tagName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (tagName){
            tags.emplace_back(wxString::FromUTF8(tagName));
        }
    }
    if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return L"无标签";
    }
    sqlite3_finalize(stmt);

    // 拼接标签
    if (tags.empty()){
        return L"无标签";
    }
    wxString result;
    for (size_t i = 0; i < tags.size(); ++i){
        result += tags[i];
        if (i < tags.size() - 1){
            result += L", ";
        }
    }
    return result;
}

wxString MyTreeCtrl::GetTagNotes(sqlite3* db, int tagID){
    if (!db || tagID < 1){
        return wxString(); // 返回空字符串
    }

    const char* sql =
        "SELECT P.NAME "
        "FROM PATHS_TAGS PT "
        "JOIN PATHS P ON PT.PATH_ID = P.ID "
        "WHERE PT.TAG_ID = ? AND P.TRASHED = 0 ";
    sqlite3_stmt* stmt = nullptr;
    std::vector<wxString> names;
    names.reserve(8);
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return wxString();
    }
    sqlite3_bind_int(stmt, 1, tagID);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name){
            names.emplace_back(wxString::FromUTF8(name));
        }
    }
    if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return wxString();
    }
    sqlite3_finalize(stmt);

    // 拼接名称
    if (names.empty()){
        return wxString();
    }
    wxString result;
    for (size_t i = 0; i < names.size(); ++i){
        result += names[i];
        if (i < names.size() - 1){
            result += "\n";
        }
    }
    return result;
}

std::vector<int> MyTreeCtrl::GetNotesWithTag(sqlite3* db, int tagID){
    std::vector<int> pathIDs;
    if (!db || tagID < 1){
        return pathIDs;
    }

    const char* sql = "SELECT PT.PATH_ID FROM PATHS_TAGS PT"
        " JOIN PATHS P ON PT.PATH_ID = P.ID"
        " WHERE PT.TAG_ID = ? AND P.TRASHED = 0";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return pathIDs;
    }
    sqlite3_bind_int(stmt, 1, tagID);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        int pathID = sqlite3_column_int(stmt, 0);
        pathIDs.push_back(pathID);
    }
    if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return pathIDs;
    }
    sqlite3_finalize(stmt);
    return pathIDs;
}

int MyTreeCtrl::CheckTagExits(wxString& name){
    for (const auto& [id, tagName] : m_TagsList){
        if (tagName == name){
            return id;
        }
    }
    return -1;
}

std::unordered_set<int> MyTreeCtrl::GetNoteTagList(sqlite3* db, int pathID){
    std::unordered_set<int> tagIDs;
    sqlite3_stmt* stmt;

    const char* sql = "SELECT TAG_ID FROM PATHS_TAGS WHERE PATH_ID = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        return tagIDs;
    }
    if (sqlite3_bind_int(stmt, 1, pathID) != SQLITE_OK){
        sqlite3_finalize(stmt);
        return tagIDs;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW){
        int tagID = sqlite3_column_int(stmt, 0); // 获取第一列（TAG_ID）的值
        tagIDs.insert(tagID);
    }
    sqlite3_finalize(stmt);
    return tagIDs;
}

std::vector<int> MyTreeCtrl::GetRecentNotes(sqlite3* db, int type, int maxCount){
    std::vector<int> pathIDs;
    if (maxCount < 5)maxCount = 5;
    if (!db || type < 0 || type > 2){
        return pathIDs;
    }

    // 根据 type 选择时间字段
    const char* timeField = nullptr;
    switch (type){
    case 0: timeField = "CREATE_TIME"; break;
    case 1: timeField = "MODIFY_TIME"; break;
    case 2: timeField = "ACCESS_TIME"; break;
    default: return pathIDs;
    }

    wxString sql = wxString::Format(
        "SELECT NOTES.PATH_ID FROM NOTES "
        "JOIN PATHS ON NOTES.PATH_ID = PATHS.ID "
        "WHERE PATHS.TRASHED = 0 " // 添加过滤条件，跳过回收站中的项
        "ORDER BY %s DESC LIMIT ?",
        timeField
    );
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.mb_str(wxConvUTF8), -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return pathIDs;
    }
    pathIDs.reserve(maxCount);
    sqlite3_bind_int(stmt, 1, maxCount);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
        int pathID = sqlite3_column_int(stmt, 0);
        pathIDs.push_back(pathID);
    }
    if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return pathIDs;
    }
    sqlite3_finalize(stmt);
    return pathIDs;
}

wxString MyTreeCtrl::GetNoteTimestamp(sqlite3* db, int noteID, int type){
    if (!db || noteID < 1 || type < 0 || type > 2){
        return wxEmptyString;
    }

    // 根据 type 选择时间字段
    const char* timeField = nullptr;
    switch (type){
    case 0: timeField = "CREATE_TIME"; break;
    case 1: timeField = "MODIFY_TIME"; break;
    case 2: timeField = "ACCESS_TIME"; break;
    default: return wxEmptyString;
    }
    wxString sql = wxString::Format("SELECT %s FROM NOTES WHERE ID = ?", timeField);
    sqlite3_stmt* stmt = nullptr;
    wxString result;
    int rc = sqlite3_prepare_v2(db, sql.mb_str(wxConvUTF8), -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return wxEmptyString;
    }
    sqlite3_bind_int(stmt, 1, noteID);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW){
        // 成功获取到一行数据
        const char* timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (timestamp){
            //result = wxString::FromUTF8(timestamp); // 转换为 wxString
            wxString timeStr = wxString::FromUTF8(timestamp);
            wxDateTime dt;
            if (dt.ParseDateTime(timeStr)){
                dt.MakeFromTimezone(wxDateTime::UTC);
                result = dt.Format("%Y-%m-%d %H:%M:%S"); // 格式化为字符串
            }
        }
    } else if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return result;
}

void MyTreeCtrl::RemoveTagFromTags(sqlite3* db, int tagID){
    const char* sql = "DELETE FROM TAGS WHERE ID = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, tagID) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error deleting tag: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

void MyTreeCtrl::DeleteItemAndData(sqlite3* db, wxTreeItemId item){
    //删除map记录、删除数据库记录、移除节点和数据
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData->IsTrashed()){
        return;
    }
    int i = itemData->GetID();
    m_idToItem.erase(i);
    m_idToData.erase(i);
    const char* sql = "DELETE FROM PATHS WHERE ID = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, i) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error deleting item: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    wxTreeItemIdValue cookie;
    auto child = GetFirstChild(item, cookie);
    while (child.IsOk()){
        DeleteItemAndData(db, child);
        child = GetNextChild(item, cookie);
    }
}

void MyTreeCtrl::OnTreeKeyDown(wxTreeEvent& event){
    int keyCode = event.GetKeyCode();

    if (keyCode == WXK_DELETE){
        sqlite3* db = GetMyAppInstance()->db;
        //delete键，可以在[全部][回收站]节点，tag节点生效
        auto selectedItem = GetSelection();
        int type = GetTreeItemType(selectedItem);
        switch (type){
        case ErrorTypeID:return;
        case TrashItemID:
        {
            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
            DeleteItemAndData(db, selectedItem);
            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
            Delete(selectedItem);
            return;
        }
        case FolderItemID:
        case WebCollectionItemID:
        {
            TreeMoveItem(selectedItem, m_idToItem[TrashRootID], 2);
            return;
        }
        case TagItemID:
        {
            auto tagData = static_cast<MyTreeItemData*>(GetItemData(selectedItem));
            auto tagID = tagData->GetID();
            RemoveTagFromTags(db, tagID);
            m_TagsList.erase(tagID);
            Delete(selectedItem);
            return;
        }
        default: return;
        }
    }

    if (event.GetKeyEvent().GetModifiers() == wxMOD_CONTROL){
        int direction = 0;
        if (keyCode == WXK_UP){
            direction = -1;
        } else if (keyCode == WXK_DOWN){
            direction = 1;
        }
        if (direction != 0){
            auto selectedItem = GetSelection();
            int type = GetTreeItemType(selectedItem);
            switch (type){
            case TrashItemID:
            case WebCollectionItemID:
            case FolderItemID:
            {
                //保存当前节点
                auto selectedData = static_cast<MyTreeItemData*>(GetItemData(selectedItem));
                if (type == FolderItemID && selectedData->GetNoteID() != -1){
                    //储存笔记内容
                    m_mainFrame->SaveNoteContent(selectedData->GetNoteID());
                    //修改显示名称
                    auto name = GetItemText(selectedItem);
                    if (name.EndsWith("*")){
                        name.RemoveLast(1);
                        SetItemText(selectedItem, name);
                    }
                }
                //上下交换
                m_mainFrame->FreezeSplitter();
                auto newUp = TreeSwapSiblings(selectedItem, direction);
                if (direction == -1)SelectItem(newUp);
                m_mainFrame->ThawSplitter();
                return;
            }
            }
        }
    }
    event.Skip(); // 允许其他默认按键行为继续
}

void MyTreeCtrl::OnSelChanged(wxTreeEvent& event){
    //用于加载节点显示信息
    //首先调用mainFrame方法选择要显示的面板，然后传递数据
    //构造事件内容，发送自定义事件，事件内容：要显示的信息。
    //节点分类：文件夹、笔记、标签、剪贴板
    m_prev = event.GetOldItem();
    wxString info = wxEmptyString;
    auto currentItem = event.GetItem();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(currentItem));
    wxTreeEvent evt(EVT_SHOW_NODE_CONTENT, GetId());
    evt.SetItem(currentItem);
    int itemType = GetTreeItemType(currentItem);
    sqlite3* db = GetMyAppInstance()->db;

    switch (itemType){
    case ErrorTypeID:
    case HiddenRootID:
        m_mainFrame->SetStatusText(info, 0);
        return;
    case FolderRootID:
    case TrashRootID:
    case FolderItemID:
    case TrashItemID:
    case WebCollectionRootID:
    case WebCollectionItemID:
    {
        info = MakePath(currentItem) + GetItemText(currentItem);
        if (itemData->IsFolder()){
            m_mainFrame->SetStatusText(info, 0);

            m_mainFrame->switchToCardViewPanel();
            auto cardList = new std::vector<CardData>();
            wxTreeItemIdValue cookie;

            sqlite3_stmt* stmt;
            const char* sql = "SELECT CONTENT FROM NOTES WHERE ID = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
                _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
                delete cardList;
                return;
            }

            for (auto&& it = GetFirstChild(currentItem, cookie); it.IsOk(); it = GetNextChild(currentItem, cookie)){
                auto childData = static_cast<MyTreeItemData*>(GetItemData(it));
                CardData cardData;
                cardData.id = childData->GetID();
                cardData.treeItemId = it;
                cardData.name = childData->GetName();

                if (childData->IsFolder()){
                    wxString preview;
                    wxTreeItemIdValue childCookie;
                    for (auto&& child = GetFirstChild(it, childCookie); child.IsOk(); child = GetNextChild(it, childCookie)){
                        auto childChildData = static_cast<MyTreeItemData*>(GetItemData(child));
                        if (childChildData->IsFolder()){
                            preview += L"[" + childChildData->GetName() + L"]\n";
                        } else{
                            preview += L"· " + childChildData->GetName() + L"\n";
                        }
                    }
                    cardData.type = 0;
                    cardData.info = wxEmptyString;
                    cardData.date = wxEmptyString;
                    cardData.contentPreview = preview;
                } else{
                    int noteID = childData->GetNoteID();
                    cardData.type = 1;
                    cardData.noteId = noteID;

                    sqlite3_bind_int(stmt, 1, noteID);
                    if (sqlite3_step(stmt) == SQLITE_ROW){
                        cardData.contentPreview = CutStringForPreview(wxString::FromUTF8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
                    }
                    sqlite3_reset(stmt);

                    wxString noteTags = GetNoteTags(db, childData->GetID());
                    wxString noteTimestamp = GetNoteTimestamp(db, noteID, 1);
                    cardData.info = L"标签：" + noteTags;
                    cardData.date = L"修改时间：" + noteTimestamp;
                }
                cardList->push_back(std::move(cardData));
            }
            sqlite3_finalize(stmt);
            evt.SetClientData(cardList);
            wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
        } else{
            if (itemType == WebCollectionItemID){
                //为网摘节点生成网址的statustext
                info = GetWebNoteUrl(db, currentItem);
            } else{
                info += L"\t\t标签: " + GetNoteTags(db, itemData->GetID()) + L"\t\t创建时间: " + GetNoteTimestamp(db, itemData->GetNoteID(), 0);
            }
            m_mainFrame->SetStatusText(info, 0);
            m_mainFrame->switchToScintillaCtrl();
            auto styleData = new std::vector<UINT16>();
            wxString noteContent = GetNoteContent(db, currentItem, *styleData);
            evt.SetString(noteContent); // 使用SetString传递字符串
            int state = itemData->GetStateIcon() << 1 | (itemType == TrashItemID ? 1 : 0);
            evt.SetInt(state);
            evt.SetClientData(styleData);
            wxPostEvent(m_mainFrame, evt);
        }
        SetFocus();
        break;
    }
    case TagsRootID:
    {
        info = MakePath(currentItem) + GetItemText(currentItem);
        m_mainFrame->SetStatusText(info, 0);

        m_mainFrame->switchToCardViewPanel();
        auto cardList = new std::vector<CardData>();
        wxTreeItemIdValue childcookie;
        for (auto&& it = GetFirstChild(currentItem, childcookie); it.IsOk(); it = GetNextChild(currentItem, childcookie)){
            auto childData = static_cast<MyTreeItemData*>(GetItemData(it));
            wxString content = GetTagNotes(db, childData->GetID());
            CardData cardData{ 0, childData->GetName(), "", "", CutStringForPreview(content), childData->GetID(), it, -1 };
            cardList->push_back(std::move(cardData));
        }
        evt.SetClientData(cardList);
        wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
        break;
    }
    case TagItemID:
    {
        info = MakePath(currentItem);
        m_mainFrame->SetStatusText(info, 0);

        m_mainFrame->switchToCardViewPanel();
        auto cardList = new std::vector<CardData>();
        auto tagItemID = itemData->GetID();
        auto noteIDList = GetNotesWithTag(db, tagItemID);

        sqlite3_stmt* stmt;
        const char* sql = "SELECT CONTENT, MODIFY_TIME FROM NOTES WHERE ID = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
            _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
            delete cardList;
            return;
        }

        for (auto&& id : noteIDList){
            auto childItem = m_idToItem[id];
            auto childData = m_idToData[id];

            sqlite3_bind_int(stmt, 1, childData->GetNoteID());
            if (sqlite3_step(stmt) == SQLITE_ROW){
                wxString notePath = MakePath(childItem);
                wxString noteTimestamp = CovertToLocalTime(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                wxString noteTags = GetNoteTags(db, childData->GetID());
                wxString noteContent = wxString::FromUTF8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

                CardData cardData{ 1, childData->GetName(), L"标签：" + noteTags, L"修改时间：" + noteTimestamp, CutStringForPreview(noteContent), childData->GetID(), childItem, childData->GetNoteID() };
                cardList->push_back(std::move(cardData));
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        evt.SetClientData(cardList);
        wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
        break;
    }
    case RecentRootID:
    {
        info = MakePath(currentItem) + GetItemText(currentItem);
        m_mainFrame->SetStatusText(info, 0);

        auto cardList = new std::vector<CardData>();
        evt.SetClientData(cardList);
        wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
        break;
    }
    case RecentAccessedID:
    case RecentCreatedID:
    case RecentModifiedID:
    {
        info = MakePath(currentItem) + GetItemText(currentItem);
        m_mainFrame->SetStatusText(info, 0);
        m_mainFrame->switchToCardViewPanel();
        auto cardList = new std::vector<CardData>();
        int type = -1;

        if (itemType == RecentCreatedID){ type = 0; } else if (itemType == RecentModifiedID){ type = 1; } else if (itemType == RecentAccessedID){ type = 2; }

        if (type == -1){
            delete cardList;
            break;
        }

        auto recentNoteIDs = GetRecentNotes(db, type, m_mainFrame->m_recentMaxCount);
        cardList->reserve(m_mainFrame->m_recentMaxCount);
        sqlite3_stmt* stmt;
        const char* sql = "SELECT CONTENT, CREATE_TIME, MODIFY_TIME, ACCESS_TIME FROM NOTES WHERE ID = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
            _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
            delete cardList;
            return;
        }

        for (auto&& id : recentNoteIDs){
            auto childItem = m_idToItem[id];
            auto childData = m_idToData[id];

            sqlite3_bind_int(stmt, 1, childData->GetNoteID());
            if (sqlite3_step(stmt) == SQLITE_ROW){
                wxString notePath = MakePath(childItem);
                wxString noteContent = wxString::FromUTF8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));

                wxString datetime;
                if (type == 0) datetime = L"创建时间：" + CovertToLocalTime(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                else if (type == 1) datetime = L"修改时间：" + CovertToLocalTime(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
                else if (type == 2) datetime = L"访问时间：" + CovertToLocalTime(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));

                CardData cardData{ 1, childData->GetName(), L"位置：" + notePath, datetime, CutStringForPreview(noteContent), childData->GetID(), childItem, childData->GetNoteID() };
                cardList->push_back(std::move(cardData));
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        evt.SetClientData(cardList);
        wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
        break;
    }
    case SearchRootID:
    {
        info = MakePath(currentItem) + GetItemText(currentItem);

        m_mainFrame->switchToCardViewPanel();
        auto cardList = new std::vector<CardData>();
        auto searchRootData = static_cast<MyTreeItemData*>(GetItemData(m_idToItem[SearchRootID]));
        auto findText = searchRootData->GetName();
        //搜索也对extension条目生效
        const char* sql =
            "SELECT p.ID, snippet(NOTES_FTS, 0, '', '', '...', 10) AS preview " // 获取上下文
            "FROM NOTES_FTS "
            "JOIN NOTES n ON NOTES_FTS.rowid = n.ID " // FTS5的rowid就是NOTES表的ID
            "JOIN PATHS p ON n.PATH_ID = p.ID " // 路径表
            "WHERE NOTES_FTS MATCH ? "
            "ORDER BY p.ID"
            ";";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK){
            _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
            return;
        }

        rc = sqlite3_bind_text(stmt, 1, findText.utf8_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK){
            _wxLogError("SQL bind failed: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return;
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW){
            int pathID = sqlite3_column_int(stmt, 0);
            if (m_idToItem.count(pathID) == 0)continue;//跳过未被创建的节点
            const unsigned char* preview = sqlite3_column_text(stmt, 1);
            wxString context = wxString::FromUTF8(reinterpret_cast<const char*>(preview));

            //在这里构造结果树
            auto name = m_idToData[pathID]->GetName();
            auto notePath = MakePath(m_idToItem[pathID]);
            auto noteID = m_idToData[pathID]->GetNoteID();
            wxString noteTimestamp = GetNoteTimestamp(db, noteID, 1);
            CardData cardData{ 1,name,notePath ,L"修改时间：" + noteTimestamp ,context ,pathID,m_idToItem[pathID],noteID };
            cardList->push_back(std::move(cardData));
        }
        if (rc != SQLITE_DONE){
            _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_finalize(stmt);

        evt.SetClientData(cardList);
        wxPostEvent(m_mainFrame->m_cardViewPanel, evt);

        info += wxString::Format(L"\t\t找到%d条结果", static_cast<int>(cardList->size()));

        m_mainFrame->SetStatusText(info, 0);
        break;
    }
    }

    event.Skip();
}

void MyTreeCtrl::OnSelChanging(wxTreeEvent& event){
    if (!m_mainFrame || m_mainFrame->IsBeingDeleted()) {
        event.Skip();
        return;
    }
    m_mainFrame->ResetAutoSaveSession();
    auto selectedItem = event.GetOldItem();
    if (!selectedItem.IsOk())return;
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(selectedItem));
    if (!itemData) {
        event.Skip();
        return;
    }
    int itemType = GetTreeItemType(selectedItem);
    if (itemType == FolderItemID && itemData->GetNoteID() != -1 || itemType == WebCollectionItemID && itemData->GetNoteID() != -1){
        //储存笔记内容
        m_mainFrame->SaveNoteContent(itemData->GetNoteID());
        //修改显示名称
        auto name = GetItemText(selectedItem);
        if (name.EndsWith("*")){
            name.RemoveLast(1);
            SetItemText(selectedItem, name);
        }
    }
    event.Skip();
}

void MyTreeCtrl::AddWebCollection(const wxString& url, const wxString& title, const wxString& content){
    sqlite3* db = GetMyAppInstance()->db;
    wxString newNoteName = (title == wxEmptyString) ? wxString(L"新记录") : title;
    const auto& parentItem = m_idToItem[WebCollectionRootID];
    const auto& parentData = m_idToData[WebCollectionRootID];
    int order = GetChildrenCount(parentItem);
    int parentID = parentData->GetID();
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    auto newItemID = AppendNewRecordToPaths(db, newNoteName, parentID, TreeIcon::ICON_NOTE, order, TreeIcon::ICON_NOTE, -1);
    if (newItemID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库PATHS中创建新笔记。");
        return;
    }
    auto noteID = AppendNewRecordToNotes(db, content, newItemID, url);
    if (noteID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库NOTES中创建新笔记。");
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE PATHS SET NOTE_ID = ? WHERE ID = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, noteID) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, newItemID) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error updating NOTE_ID: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);

    }
    sqlite3_finalize(stmt);
    auto noteData = new MyTreeItemData(newItemID, parentID, newNoteName, order, TreeIcon::ICON_NOTE, noteID, false, false, false);
    auto noteItem = AppendItem(parentItem, newNoteName, TreeIcon::ICON_NOTE, -1, noteData);
    if (!noteItem.IsOk()){
        _wxLogError(L"无法创建树节点。");
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        delete noteData;
        return;
    }

    m_idToItem[newItemID] = noteItem;
    m_idToData[newItemID] = noteData;
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

}

void MyTreeCtrl::SwitchToWebCollectionNote(){
    //查找或新建目标笔记，并切换到，前台追加，因此不涉及数据库操作
    auto targetName = wxString(L"收集簿");
    auto target = FindOrCreateWebCollectionNote(m_idToItem[WebCollectionRootID], targetName);
    SelectItem(target);
}

void MyTreeCtrl::OnFindSQL(wxFindDialogEvent& event){
    //SQL搜索功能：事件仅用于创建搜索结果节点，并储存了要搜索的字符串
    //同时做了一些清理，如关闭对话框
    //具体的搜索实现，在OnSelChanged中根据搜索的字符串实现
    //这里发送一个OnSelchanged事件
    if (event.GetEventType() == wxEVT_FIND_CLOSE){
        if (m_mainFrame->m_findSqlDlg){
            m_mainFrame->m_findSqlDlg->Destroy();
            m_mainFrame->m_findSqlDlg = nullptr;
        }
        return;
    }
    wxString findText = event.GetFindString();
    if (findText.IsEmpty()){
        m_mainFrame->m_findSqlDlg->Destroy();
        m_mainFrame->m_findSqlDlg = nullptr;
        return;
    }
    //创建搜索结果节点
    if (m_idToItem.count(SearchRootID) != 0){
        //已存在，切换过去，并且重命名
        auto& SearchRootItem = m_idToItem[SearchRootID];
        SetItemText(SearchRootItem, L"搜索结果: " + findText);
        auto& SearchRootData = m_idToData[SearchRootID];
        SearchRootData->SetName(findText);
        if (GetSelection() == SearchRootItem){
            SelectItem(m_idToItem[FolderRootID]);
            SelectItem(SearchRootItem);
        } else{
            SelectItem(SearchRootItem);
        }

    } else{
        //不存在，创建
        auto SearchRootData = new MyTreeItemData(SearchRootID, HiddenRootID, findText, 5, TreeIcon::ICON_SEARCH, -1, true, false, false);
        wxTreeItemId SearchRootItem;
        if (m_idToItem.count(RecentRootID) == 1){
            SearchRootItem = InsertItem(m_hiddenRoot, m_idToItem[RecentRootID], L"搜索结果: " + findText, TreeIcon::ICON_SEARCH, -1, SearchRootData);
        } else if (m_idToItem.count(TagsRootID) == 1){
            SearchRootItem = InsertItem(m_hiddenRoot, m_idToItem[TagsRootID], L"搜索结果: " + findText, TreeIcon::ICON_SEARCH, -1, SearchRootData);
        } else{
            SearchRootItem = InsertItem(m_hiddenRoot, m_idToItem[TrashRootID], L"搜索结果: " + findText, TreeIcon::ICON_SEARCH, -1, SearchRootData);
        }
        m_idToItem[SearchRootID] = SearchRootItem;
        m_idToData[SearchRootID] = SearchRootData;
        SelectItem(SearchRootItem);
    }
    m_mainFrame->m_findSqlDlg->Destroy();
    m_mainFrame->m_findSqlDlg = nullptr;
    return;
}

void MyTreeCtrl::RenameCurrentItem(wxString& newName){
    auto selectedItem = GetSelection();
    if (!selectedItem.IsOk())return;
    auto name = SanitizeName(newName);
    if (name.IsEmpty())return;
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(selectedItem));
    auto oldName = itemData->GetName();
    if (oldName == name)return;
    bool fileModified = GetItemText(selectedItem).EndsWith("*");
    auto id = itemData->GetID();
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE PATHS SET NAME = ? WHERE ID = ?";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return;
    }
    rc = sqlite3_bind_text(stmt, 1, name.utf8_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK){
        _wxLogError("SQL bind failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    rc = sqlite3_bind_int(stmt, 2, id);
    if (rc != SQLITE_OK){
        _wxLogError("SQL bind failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE){
        _wxLogError("SQL step failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    itemData->SetName(name);
    if (fileModified)name += "*";
    SetItemText(selectedItem, name);
}

void MyTreeCtrl::OnItemMenu(wxTreeEvent& event){
    wxTreeItemId clickedItem = event.GetItem(); // 获取被右键点击的节点ID
    m_currentItem = clickedItem;          // 存储到成员变量，以便菜单回调函数使用
    wxMenu menu; // 创建一个空的菜单
    //单独处理最近根
    if (m_idToItem.count(RecentRootID) && clickedItem == m_idToItem[RecentRootID]){
        menu.Append(MainFrame::MENU_HIDE_RECENT, L"关闭最近");
        PopupMenu(&menu, event.GetPoint());
        return;
    }
    SelectItem(clickedItem); // 选中该节点，以便菜单显示正确
    // 2. 获取节点数据和类型
  //  MyTreeItemData* itemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
    int itemType = GetTreeItemType(clickedItem);

    switch (itemType){
    case ErrorTypeID:
    case HiddenRootID:
    case RecentRootID:
        return;
    case RecentCreatedID:
    case RecentModifiedID:
    case RecentAccessedID:
        return;
        /* {
            menu.Append(MainFrame::MENU_HIDE_RECENT, L"关闭最近");
            menu.Bind(wxEVT_MENU, &MyTreeCtrl::OnHideRecent, this, MainFrame::MENU_HIDE_RECENT);
            break;
        }*/
    case TrashRootID:
    {
        menu.Append(MainFrame::MENU_EMPTY_RECYCLE_BIN, L"清空回收站");
        break;
    }
    case FolderRootID:
    {
        menu.Append(MainFrame::MENU_NEW_FOLDER, L"新建文件夹");
        menu.Append(MainFrame::MENU_NEW_NOTE, L"新建笔记");
        menu.Append(MainFrame::MENU_IMPORT_NOTE, L"导入笔记...");
        menu.Append(MainFrame::MENU_IMPORT_FOLDER, L"导入文件夹...");
        menu.Append(MainFrame::MENU_EXPORT_ITEM, L"导出...");
        break;
    }
    case TagsRootID:
    {
        menu.Append(MainFrame::MENU_NEW_TAG, L"新建标签");
        menu.Append(MainFrame::MENU_HIDE_TAGS, L"关闭标签");
        break;
    }
    case FolderItemID:
    {
        menu.Append(MainFrame::MENU_NEW_FOLDER, L"新建文件夹");
        menu.Append(MainFrame::MENU_NEW_NOTE, L"新建笔记");
        menu.Append(MainFrame::MENU_RENAME_ITEM, L"重命名");
        menu.Append(MainFrame::MENU_MOVE_TO_RECYCLE_BIN, L"移动到回收站");

        auto iconStyleMenu = new wxMenu();
        auto iconColorMenu = new wxMenu();
        menu.AppendSubMenu(iconStyleMenu, L"更改图标");
        menu.AppendSubMenu(iconColorMenu, L"更改颜色");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_RESTORE, L"恢复默认");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 5, L"样式🎮");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 6, L"样式📅");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 7, L"样式🗄️");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 8, L"样式√");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 9, L"样式★");
        iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 10, L"样式❤");

        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE, L"深红");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 1, L"金黄");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 2, L"绿色");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 3, L"蓝色");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 4, L"浅蓝");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 5, L"紫红");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 6, L"黑色");
        iconColorMenu->Append(MainFrame::MENU_ICON_CHANGE_COLOR_BASE + 7, L"灰色");

        menu.AppendSeparator();

        auto itemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
        if (!itemData->IsFolder()){
            iconStyleMenu->Insert(1, MainFrame::MENU_ICON_CHANGE_STYLE_BASE, L"着色笔记");
            iconStyleMenu->Append(MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 11, L"锁定");
            auto addTagMenu = new wxMenu();
            auto removeTagMenu = new wxMenu();

            menu.AppendSubMenu(addTagMenu, L"添加标签");
            menu.AppendSubMenu(removeTagMenu, L"移除标签");

            int itemID = itemData->GetID();
            sqlite3* db = GetMyAppInstance()->db;
            std::unordered_set<int> currentTagIDs = GetNoteTagList(db, itemID);

            for (const auto& [tagID, tagName] : m_TagsList){
                if (currentTagIDs.count(tagID)){
                    wxMenuItem* menuItem = removeTagMenu->Append(wxID_ANY, tagName);
                    Bind(wxEVT_MENU, [this, clickedItem, tagID](wxCommandEvent&){
                        this->RemoveTagForNote(clickedItem, tagID);
                        }, menuItem->GetId());
                } else{
                    wxMenuItem* menuItem = addTagMenu->Append(wxID_ANY, tagName);
                    Bind(wxEVT_MENU, [this, clickedItem, tagID](wxCommandEvent&){
                        this->AddTagForNote(clickedItem, tagID);
                        }, menuItem->GetId());
                }
            }

            // 检查子菜单是否为空，并设置可用状态
            bool hasAddTags = !addTagMenu->GetMenuItems().empty();
            bool hasRemoveTags = !removeTagMenu->GetMenuItems().empty();

            menu.GetMenuItems()[menu.GetMenuItemCount() - 2]->Enable(hasAddTags);
            menu.GetMenuItems()[menu.GetMenuItemCount() - 1]->Enable(hasRemoveTags);
        } else{
            iconStyleMenu->Insert(1, MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 1, L"着色文件夹");
            iconStyleMenu->Insert(2, MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 2, L"√文件夹");
            iconStyleMenu->Insert(3, MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 3, L"★文件夹");
            iconStyleMenu->Insert(4, MainFrame::MENU_ICON_CHANGE_STYLE_BASE + 4, L"‹›文件夹");
            menu.Append(MainFrame::MENU_IMPORT_NOTE, L"导入笔记...");
            menu.Append(MainFrame::MENU_IMPORT_FOLDER, L"导入文件夹...");
        }

        menu.Append(MainFrame::MENU_EXPORT_ITEM, L"导出...");
        break;
    }
    case TrashItemID:
    {
        menu.Append(MainFrame::MENU_DELETE_ITEM, L"删除");
        break;
    }
    case TagItemID:
    {
        menu.Append(MainFrame::MENU_RENAME_ITEM, L"重命名");
        menu.Append(MainFrame::MENU_REMOVE_TAG, L"删除");
        menu.Append(MainFrame::MENU_NEW_TAG, L"新建标签");
        break;
    }
    case SearchRootID:
    {
        menu.Append(MainFrame::MENU_HIDE_SEARCH, L"关闭搜索");
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&){
            SelectItem(m_idToItem[FolderRootID]);
            if (m_idToItem.count(SearchRootID)){
                SetEvtHandlerEnabled(false);
                Delete(m_idToItem[SearchRootID]);
                SetEvtHandlerEnabled(true);
                m_idToData.erase(SearchRootID);
                m_idToItem.erase(SearchRootID);
            }
            }, MainFrame::MENU_HIDE_SEARCH);
        menu.Append(MainFrame::MENU_FIND_SQL, L"新的搜索");
        break;
    }
    case WebCollectionRootID:
    {
        menu.Append(MainFrame::MENU_NEW_FOLDER, L"新建文件夹");
        menu.Append(MainFrame::MENU_EXPORT_ITEM, L"导出...");
        break;
    }
    case WebCollectionItemID:
    {
        menu.Append(MainFrame::MENU_NEW_FOLDER, L"新建文件夹");
        menu.Append(MainFrame::MENU_RENAME_ITEM, L"重命名");
        menu.Append(MainFrame::MENU_MOVE_TO_RECYCLE_BIN, L"移动到回收站");
        menu.AppendSeparator();
        auto itemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
        if (!itemData->IsFolder()){
            auto addTagMenu = new wxMenu();
            auto removeTagMenu = new wxMenu();

            menu.AppendSubMenu(addTagMenu, L"添加标签");
            menu.AppendSubMenu(removeTagMenu, L"移除标签");

            int itemID = itemData->GetID();
            sqlite3* db = GetMyAppInstance()->db;
            std::unordered_set<int> currentTagIDs = GetNoteTagList(db, itemID);

            for (const auto& [tagID, tagName] : m_TagsList){
                if (currentTagIDs.count(tagID)){
                    wxMenuItem* menuItem = removeTagMenu->Append(wxID_ANY, tagName);
                    Bind(wxEVT_MENU, [this, clickedItem, tagID](wxCommandEvent&){
                        this->RemoveTagForNote(clickedItem, tagID);
                        }, menuItem->GetId());
                } else{
                    wxMenuItem* menuItem = addTagMenu->Append(wxID_ANY, tagName);
                    Bind(wxEVT_MENU, [this, clickedItem, tagID](wxCommandEvent&){
                        this->AddTagForNote(clickedItem, tagID);
                        }, menuItem->GetId());
                }
            }

            // 检查子菜单是否为空，并设置可用状态
            bool hasAddTags = !addTagMenu->GetMenuItems().empty();
            bool hasRemoveTags = !removeTagMenu->GetMenuItems().empty();

            menu.GetMenuItems()[menu.GetMenuItemCount() - 2]->Enable(hasAddTags);
            menu.GetMenuItems()[menu.GetMenuItemCount() - 1]->Enable(hasRemoveTags);
        }
        menu.Append(MainFrame::MENU_EXPORT_ITEM, L"导出...");
        break;
    }
    }
    if (menu.GetMenuItemCount() > 0){
        PopupMenu(&menu, event.GetPoint()); // 弹出菜单
    }
}

void MyTreeCtrl::InitTreeCtrlIcons(){
    wxConfigBase* config = wxConfigBase::Get();
    auto TreeiconSize = config->ReadLong("Tree/IconSize", 64);
    if (TreeiconSize < 16 || TreeiconSize > 256)  TreeiconSize = 64;
    auto Size_j = wxSize(TreeiconSize, TreeiconSize);
    m_imageListTreeCtrl = new wxImageList(TreeiconSize, TreeiconSize);
    wxBitmapBundle bundle;
    wxBitmap bitmap;
    char buff[4096];
    auto color = GetMyAppInstance()->GetThemeColor();

#define AS_LIST_ADD(name, svg) \
    m_imageListTreeCtrl->Add(wxBitmapBundle::FromSVG(ChangeSVGColor(buff, svg, color, 73), Size_j).GetBitmap(Size_j));

    TREE_ICON_TABLE(AS_LIST_ADD)
#undef AS_LIST_ADD
}

wxString MyTreeCtrl::MakePath(const wxTreeItemId& item){
    return MakePath(item, m_hiddenRoot);
}

wxString MyTreeCtrl::MakePath(const wxTreeItemId& Id, const wxTreeItemId& rootId){
    if (!Id.IsOk()){
        return wxEmptyString; // 无效节点返回空字符串
    }
    wxArrayString pathParts;
    wxTreeItemId current = Id;
    while (current.IsOk() && current != rootId){
        pathParts.Insert(GetItemText(current), 0); // 在开头插入，自动反转
        current = GetItemParent(current);
    }

    wxString path;
    for (size_t i = 0; i < pathParts.GetCount() - 1; ++i){
        path += pathParts[i];
        path += R"(/)";
    }

    return path;
}

wxString MyTreeCtrl::GetNoteContent(sqlite3* db, wxTreeItemId item, std::vector<UINT16>& styleData){
    wxString content = wxEmptyString;
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));

    // 检查itemData和noteID是否有效
    if (itemData == nullptr || itemData->GetNoteID() == -1){
        return content;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT CONTENT, STYLEDATA FROM NOTES WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return content;
    }
    if (sqlite3_bind_int(stmt, 1, itemData->GetNoteID()) != SQLITE_OK){
        _wxLogError("SQL bind failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return content;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW){
        const unsigned char* columnText = sqlite3_column_text(stmt, 0);
        if (columnText){
            content = wxString::FromUTF8(reinterpret_cast<const char*>(columnText));
        }
        const void* blobData = sqlite3_column_blob(stmt, 1);
        int totalBytes = sqlite3_column_bytes(stmt, 1);
        if (blobData && totalBytes){
            int elementCount;
            memcpy(&elementCount, blobData, sizeof(int));
            styleData.resize(elementCount);
            memcpy(styleData.data(), static_cast<const unsigned char*>(blobData) + sizeof(int), elementCount * sizeof(UINT16));
        }
    } else{
        _wxLogError("No note content found or query failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return content;
}

wxString MyTreeCtrl::GetWebNoteUrl(sqlite3* db, wxTreeItemId& item){
    wxString url = wxEmptyString;
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (itemData == nullptr || itemData->GetNoteID() == -1){
        return url;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT URL FROM NOTES WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        return url;
    }
    if (sqlite3_bind_int(stmt, 1, itemData->GetNoteID()) != SQLITE_OK){
        _wxLogError("SQL bind failed: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return url;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW){
        const unsigned char* urlText = sqlite3_column_text(stmt, 0);
        if (urlText){
            url = wxString::FromUTF8(reinterpret_cast<const char*>(urlText));
        }
    } else{
        _wxLogError("No note url found or query failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return url;
}

void MyTreeCtrl::ExportNode(const wxString& currentBaseDir, const wxTreeItemId& item){
    sqlite3* db = GetMyAppInstance()->db;
    std::vector<UINT16> styleData;
    wxString content = GetNoteContent(db, item, styleData);
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData) return;
    wxString safeName = SanitizeAndGetUniqueName(currentBaseDir, itemData->GetName(), false);
    // 构建完整的文件路径
    wxFileName fileName(currentBaseDir, safeName, _T("txt"));
    wxString fullFilePath = fileName.GetFullPath();

    //写入文件
    wxFile outFile;
    if (!outFile.Open(fullFilePath, wxFile::write)){
        _wxLogError("无法打开文件进行写入：%s", fullFilePath);
        return;
    }
    if (!content.IsEmpty()){
        // 转换为 UTF-8 字节序列
        wxCharBuffer utf8_buffer = content.ToUTF8();
        const char* data = utf8_buffer.data();
        size_t len = (data != nullptr) ? strlen(data) : 0;

        if (len > 0){
            if (outFile.Write(data, len) != len){
                _wxLogError("文件内容写入失败：%s", fullFilePath);
            }
        }
    }
    outFile.Close();
}

void MyTreeCtrl::OnEmptyRecycleBin(wxCommandEvent& event){
    //处理焦点
    auto currentItem = GetSelection();
    int itemType = GetTreeItemType(currentItem);
    if (itemType != TrashItemID){
        SelectItem(m_idToItem[TrashRootID]);
        currentItem = m_idToItem[TrashRootID];
    }

    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    std::vector<int> itemsToDelete;
    const char* sql = "DELETE FROM PATHS WHERE TRASHED = 1 RETURNING ID;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError("SQL error preparing statement: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_finalize(stmt);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW){
        int id = sqlite3_column_int(stmt, 0);
        itemsToDelete.push_back(id);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

    for (auto&& id : itemsToDelete){
        m_idToItem.erase(id);
        m_idToData.erase(id);
    }
    DeleteChildren(m_idToItem[TrashRootID]);
    wxTreeEvent evt(EVT_SHOW_NODE_CONTENT, GetId());
    evt.SetItem(currentItem);
    auto cardList = new std::vector<CardData>();
    evt.SetClientData(cardList);
    wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
    event.Skip();
}

void MyTreeCtrl::OnDeleteItem(wxCommandEvent& event){
    auto item = GetSelection();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (itemData == nullptr || !itemData->IsTrashed())
        return;
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    DeleteItemAndData(db, item);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    //  SelectItem(GetItemParent(item));
    Delete(item);
    event.Skip();
}

void MyTreeCtrl::OnRemoveToTrash(wxCommandEvent& event){
    auto item = GetSelection();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if ((GetTreeItemType(item) != FolderItemID) && (GetTreeItemType(item) != WebCollectionItemID)) return;

    TreeMoveItem(item, m_idToItem[TrashRootID], 2);
}

void MyTreeCtrl::OnRenameItem(wxCommandEvent& event){
    wxTreeItemId selected = GetSelection();
    if (selected.IsOk()){
        EditLabel(selected);
    }
}

void MyTreeCtrl::OnNewFolder(wxCommandEvent& event){
    auto clickedItem = GetSelection();
    int itemType = GetTreeItemType(clickedItem);
    MyTreeItemData* clickedItemData = nullptr;

    if (itemType != FolderRootID && itemType != FolderItemID && itemType != WebCollectionRootID && itemType != WebCollectionItemID){
        clickedItem = m_idToItem[FolderRootID];
    }
    clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));

    wxTreeItemId parentItem, folderItem;
    MyTreeItemData* folderData = nullptr;
    int order, parentID;
    sqlite3* db = GetMyAppInstance()->db;
    wxString newFolderName = L"新文件夹";

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 根据clickedItem是否为文件夹来确定父节点和顺序
    if (clickedItemData->IsFolder()){
        parentItem = clickedItem;
        order = GetChildrenCount(parentItem);
        parentID = clickedItemData->GetID();
    } else{
        parentItem = GetItemParent(clickedItem);
        order = clickedItemData->GetNodeOrder() + 1;
        parentID = clickedItemData->GetParentID();
    }

    // 1. 在数据库中插入新文件夹记录
    auto newItemID = AppendNewRecordToPaths(db, newFolderName, parentID, TreeIcon::ICON_DEFAULT_FOLDER, order, TreeIcon::ICON_DEFAULT_FOLDER, -1);
    if (newItemID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库中创建新文件夹。");
        return;
    }

    // 2. 在map和wxTreeCtrl中创建新文件夹节点
    folderData = new MyTreeItemData(newItemID, parentID, newFolderName, order, TreeIcon::ICON_DEFAULT_FOLDER, -1, true, false, false);

    if (clickedItemData->IsFolder()){
        // 如果是文件夹，作为子节点追加
        folderItem = AppendItem(parentItem, newFolderName, TreeIcon::ICON_DEFAULT_FOLDER, -1, folderData);
    } else{
        // 如果是笔记，作为兄弟节点插入
        folderItem = InsertItem(parentItem, clickedItem, newFolderName, TreeIcon::ICON_DEFAULT_FOLDER, -1, folderData);
    }

    // 检查 folderItem 是否创建成功
    if (!folderItem.IsOk()){
        _wxLogError(L"无法创建树节点。");
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        delete folderData;
        return;
    }

    m_idToItem[newItemID] = folderItem;
    m_idToData[newItemID] = folderData;

    // 3. 更新后续兄弟节点的NODE_ORDER（仅当新文件夹插入到中间时）
    if (!clickedItemData->IsFolder()){
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "UPDATE PATHS SET NODE_ORDER = ? WHERE ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        if (rc != SQLITE_OK){
            _wxLogError(L"SQL 语句准备失败: %s", sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return;
        }

        int i = order + 1;
        for (auto it = GetNextSibling(folderItem); it.IsOk(); it = GetNextSibling(it)){
            auto itData = static_cast<MyTreeItemData*>(GetItemData(it));
            itData->SetNodeOrder(i);

            sqlite3_bind_int(stmt, 1, i);
            sqlite3_bind_int(stmt, 2, itData->GetID());

            if (sqlite3_step(stmt) != SQLITE_DONE){
                _wxLogError("SQL error updating NODE_ORDER: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return;
            }
            sqlite3_reset(stmt);
            i++;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (!IsExpanded(parentItem)){ Expand(parentItem); }

    SelectItem(folderItem);
    ScheduleEditLabel(folderItem);
}

void MyTreeCtrl::OnNewNote(wxCommandEvent& event){
    auto clickedItem = GetSelection();
    int itemType = GetTreeItemType(clickedItem);
    MyTreeItemData* clickedItemData = nullptr;
    if (itemType != FolderRootID && itemType != FolderItemID){
        clickedItem = m_idToItem[FolderRootID];
    }
    clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));

    wxTreeItemId parentItem, noteItem;
    MyTreeItemData* noteData = nullptr;
    int order, parentID;
    sqlite3* db = GetMyAppInstance()->db;
    wxString newNoteName = L"新笔记";

    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 根据clickedItem是否为文件夹来确定父节点和顺序
    if (clickedItemData->IsFolder()){
        parentItem = clickedItem;
        order = GetChildrenCount(parentItem);
        parentID = clickedItemData->GetID();
    } else{
        parentItem = GetItemParent(clickedItem);
        order = clickedItemData->GetNodeOrder() + 1;
        parentID = clickedItemData->GetParentID();
    }

    // 在数据库中插入新文件夹记录
    auto newItemID = AppendNewRecordToPaths(db, newNoteName, parentID, TreeIcon::ICON_NOTE, order, TreeIcon::ICON_NOTE, -1);
    if (newItemID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库PATHS中创建新笔记。");
        return;
    }
    wxString noteString = wxEmptyString;
    auto noteID = AppendNewRecordToNotes(db, noteString, newItemID);
    if (noteID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库NOTES中创建新笔记。");
        return;
    }
    // 更新PATHS表中的NOTE_ID字段
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE PATHS SET NOTE_ID = ? WHERE ID = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, noteID) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, newItemID) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error updating NOTE_ID: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);

    }
    sqlite3_finalize(stmt);
    // 2. 在map和wxTreeCtrl中创建新文件夹节点

    noteData = new MyTreeItemData(newItemID, parentID, newNoteName, order, TreeIcon::ICON_NOTE, noteID, false, false, false);
    if (clickedItemData->IsFolder()){
        // 如果是文件夹，作为子节点追加
        noteItem = AppendItem(parentItem, newNoteName, TreeIcon::ICON_NOTE, -1, noteData);
    } else{
        // 如果是笔记，作为兄弟节点插入
        noteItem = InsertItem(parentItem, clickedItem, newNoteName, TreeIcon::ICON_NOTE, -1, noteData);
    }

    if (!noteItem.IsOk()){
        _wxLogError(L"无法创建树节点。");
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        delete noteData;
        return;
    }

    m_idToItem[newItemID] = noteItem;
    m_idToData[newItemID] = noteData;

    // 3. 更新后续兄弟节点的NODE_ORDER（仅当新文件夹插入到中间时）
    if (!clickedItemData->IsFolder()){
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "UPDATE PATHS SET NODE_ORDER = ? WHERE ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        if (rc != SQLITE_OK){
            _wxLogError(L"SQL 语句准备失败: %s", sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return;
        }

        int i = order + 1;
        for (auto it = GetNextSibling(noteItem); it.IsOk(); it = GetNextSibling(it)){
            auto itData = static_cast<MyTreeItemData*>(GetItemData(it));
            itData->SetNodeOrder(i); // 更新内存中的数据

            // 绑定并执行更新
            sqlite3_bind_int(stmt, 1, i);
            sqlite3_bind_int(stmt, 2, itData->GetID());

            if (sqlite3_step(stmt) != SQLITE_DONE){
                _wxLogError("SQL error updating NODE_ORDER: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return;
            }
            sqlite3_reset(stmt); // 重置stmt
            i++;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (!IsExpanded(parentItem)){ Expand(parentItem); }
    // UI操作
    SelectItem(noteItem);
    ScheduleEditLabel(noteItem);
}

void MyTreeCtrl::OnNewTag(wxCommandEvent& event){
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    wxString baseName = L"新标签";
    wxString tagName = baseName;
    int counter = 0;
    bool found;
    do{
        found = false;
        for (const auto& pair : m_TagsList){
            if (pair.second == tagName){
                found = true;
                break;
            }
        }
        if (found){
            counter++;
            tagName = wxString::Format(L"%s%d", baseName, counter);
        }
    } while (found); // 直到找到不重复的名称

    int newTagID = AddNewTag(db, tagName);
    auto parentItem = m_idToItem[TagsRootID];
    auto order = GetChildrenCount(parentItem);
    m_TagsList[newTagID] = tagName;
    MyTreeItemData* newTagData = new MyTreeItemData(newTagID, TagsRootID, tagName, order, TreeIcon::ICON_TAG, -1, false, false, false);
    auto newTagItem = AppendItem(parentItem, tagName, TreeIcon::ICON_TAG, -1, newTagData);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    SelectItem(newTagItem);
    ScheduleEditLabel(newTagItem);
}

void MyTreeCtrl::OnRemoveTag(wxCommandEvent& event){
    auto item = GetSelection();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (itemData == nullptr)        return;
    int ID = itemData->GetID();
    sqlite3* db = GetMyAppInstance()->db;
    RemoveTagFromTags(db, ID);
    m_TagsList.erase(ID);
    Delete(item);
    event.Skip();
}

void MyTreeCtrl::OnImportNote(wxCommandEvent& event){
    auto clickedItem = GetSelection();
    if (!clickedItem.IsOk()) return;
    int itemType = GetTreeItemType(clickedItem);
    if (itemType != FolderRootID && itemType != FolderItemID){
        clickedItem = m_idToItem[FolderRootID];
    }
    auto clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
    if (!clickedItemData) return;

    if (!clickedItemData->IsFolder()){
        clickedItem = GetItemParent(clickedItem);
        clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
    }

    // 弹出文件选择对话框 (支持多选)
    wxFileDialog openFileDialog(
        this,
        L"选择要导入的 TXT 文件",
        wxEmptyString,
        wxEmptyString,
        L"文本文件 (*.txt)|*.txt",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE
    );

    if (openFileDialog.ShowModal() == wxID_CANCEL)return;

    // 获取所有选中的文件路径
    wxArrayString paths;
    openFileDialog.GetPaths(paths);
    sqlite3* db = GetMyAppInstance()->db;
    Freeze();
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    // 遍历并导入每个文件
    bool allSuccess = true;
    for (const wxString& path : paths){
        if (!ImportSingleNote(clickedItem, path)){
            allSuccess = false; // 标记失败
            break;
        }
    }
    if (allSuccess){
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    } else{
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogMessage(L"导入中断，已回滚更改。");
    }
    Thaw();
    Expand(clickedItem);
    PostEventToCardViewPanel(clickedItem);
}

void MyTreeCtrl::OnExportItem(wxCommandEvent& event){
    auto item = GetSelection();
    auto type = GetTreeItemType(item);
    if (type != FolderItemID && type != FolderRootID && type != WebCollectionItemID && type != WebCollectionRootID)return;
    wxDirDialog dirDialog(this, L"导出");
    if (dirDialog.ShowModal() != wxID_OK) return;

    wxString base_dir = dirDialog.GetPath();
    RecursiveExportNode(base_dir, item);
}

void MyTreeCtrl::OnImportFolder(wxCommandEvent& event){
    auto clickedItem = GetSelection();
    int itemType = GetTreeItemType(clickedItem);
    MyTreeItemData* clickedItemData = nullptr;
    if (itemType != FolderRootID && itemType != FolderItemID){
        clickedItem = m_idToItem[FolderRootID];
    }
    clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
    if (!clickedItemData->IsFolder()){
        clickedItem = GetItemParent(clickedItem);
        clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
    }
    wxDirDialog dirDialog(
        this,
        L"导入文件夹",
        wxEmptyString,
        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST
    );
    if (dirDialog.ShowModal() == wxID_CANCEL)return;

    wxString rootPath = dirDialog.GetPath();

    // 3. 开始递归导入
    RecursiveImportFolder(clickedItem, rootPath);
    PostEventToCardViewPanel(clickedItem);
}

//void MyTreeCtrl::OnNewExtenstion(wxCommandEvent& event) {
//    auto clickedItem = GetSelection();
//    auto clickedItemData = static_cast<MyTreeItemData*>(GetItemData(clickedItem));
//    int menuID = event.GetId();
//    wxString newNoteName;
//    int stateIcon;
//    switch (menuID) {
//    case MainFrame::MENU_ADD_EXTENSION_LIST:
//    {
//        newNoteName = L"待办";
//        stateIcon = TreeIcon::ICON_TODO;
//        break;
//    }
//    case MainFrame::MENU_ADD_EXTENSION_ALARM:
//    {
//        newNoteName = L"定时提醒";
//        stateIcon = TreeIcon::ICON_ALARM;
//        break;
//    }
//    case MainFrame::MENU_ADD_EXTENSION_LINK:
//    {
//        newNoteName = L"链接";
//        stateIcon = TreeIcon::ICON_LINK;
//        break;
//    }
//    }
//    sqlite3* db = GetMyAppInstance()->db;
//    int order = GetChildrenCount(clickedItem);
//    int parentID = clickedItemData->GetID();
//
//    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
//    auto newItemID = AppendNewRecordToPaths(db, newNoteName, parentID, TreeIcon::ICON_NOTE, order, stateIcon, -1, 1);
//    if (newItemID == -1) {
//        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
//        _wxLogError(L"无法在数据库PATHS中创建新笔记。");
//        return;
//    }
//    wxString noteString = wxEmptyString;
//    auto noteID = AppendNewRecordToNotes(db, noteString, newItemID);
//    if (noteID == -1) {
//        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
//        _wxLogError(L"无法在数据库NOTES中创建新笔记。");
//        return;
//    }
//    // 更新PATHS表中的NOTE_ID字段
//    sqlite3_stmt* stmt;
//    const char* sql = "UPDATE PATHS SET NOTE_ID = ? WHERE ID = ?;";
//    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
//    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, noteID) != SQLITE_OK ||
//        sqlite3_bind_int(stmt, 2, newItemID) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE) {
//        _wxLogError("SQL error updating NOTE_ID: %s", sqlite3_errmsg(db));
//        sqlite3_finalize(stmt);
//
//    }
//
//    sqlite3_finalize(stmt);
//    // 2. 在map和wxTreeCtrl中创建新文件夹节点
//
//    auto noteData = new MyTreeItemData(newItemID, parentID, newNoteName, order, stateIcon, noteID, false, false, false);
//    auto noteItem = AppendItem(clickedItem, newNoteName, stateIcon, -1, noteData);
//
//    if (!noteItem.IsOk()) {
//        _wxLogError(L"无法创建树节点。");
//        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
//        delete noteData;
//        return;
//    }
//
//    m_idToItem[newItemID] = noteItem;
//    m_idToData[newItemID] = noteData;
//    // 所有数据库操作成功，提交事务
//    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
//    if (!IsExpanded(clickedItem)) { Expand(clickedItem); }
//    // UI操作
//    SelectItem(noteItem);
//    CallAfter([this, noteItem]() { EditLabel(noteItem); });
//}

void MyTreeCtrl::OnShowTags(wxCommandEvent& event){
    ShowTags();
    m_mainFrame->m_showTags = true;
}

void MyTreeCtrl::OnHideTags(wxCommandEvent& event){
    HideTags();
    m_mainFrame->m_showTags = false;
}

void MyTreeCtrl::OnShowRecent(wxCommandEvent& event){
    ShowRecent();
    m_mainFrame->m_showRecent = true;
}

void MyTreeCtrl::OnHideRecent(wxCommandEvent& event){
    HideRecent();
    m_mainFrame->m_showRecent = false;
}

void MyTreeCtrl::OnChangeIconStyle(wxCommandEvent& event){
    int id = event.GetId();
    auto item = GetSelection();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    sqlite3* db = GetMyAppInstance()->db;
    if (id == MainFrame::MENU_ICON_CHANGE_STYLE_RESTORE){
        itemData->SetStateIcon(itemData->IsFolder());
        SetItemImage(item, itemData->GetStateIcon());
        UpdateStateIcon(db, item);
        return;
    }
    auto stateicon = itemData->GetStateIcon();
    auto newStyle = id - MainFrame::MENU_ICON_CHANGE_STYLE_BASE;
    //修改过样式
    if (stateicon & 0x80){
        //覆写样式位
        itemData->SetStateIcon(stateicon & ~0xF | newStyle);
    } else{
        itemData->SetStateIcon(0x80 | 6 << 4 | newStyle);
    }
    SetItemImage(item, GetIconIndex(itemData->GetStateIcon()));
    //在这里手动发送一个事件通知stc切换可写状态
    if (m_mainFrame->GetCurrentSimplebookPageID() == MainFrame::ID_ScintillaPage){
        if (newStyle == TREE_ICON_STYLE_LOCKED){
            m_mainFrame->SaveNoteContent(itemData->GetNoteID());
            m_mainFrame->m_scintilla->SetReadOnly(true);
        } else{
            m_mainFrame->m_scintilla->SetReadOnly(false);
        }
    }
    UpdateStateIcon(db, item);
}

void MyTreeCtrl::OnChangeIconColor(wxCommandEvent& event){
    int id = event.GetId() - MainFrame::MENU_ICON_CHANGE_COLOR_BASE;
    auto item = GetSelection();
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    sqlite3* db = GetMyAppInstance()->db;
    auto stateicon = itemData->GetStateIcon();
    //修改过样式
    if (stateicon & 0x80){
        itemData->SetStateIcon(stateicon & ~0x70 | (id << 4));
    } else{
        itemData->SetStateIcon(0x80 | id << 4 | itemData->IsFolder());
    }
    SetItemImage(item, GetIconIndex(itemData->GetStateIcon()));
    UpdateStateIcon(db, item);
}
void MyTreeCtrl::RecursiveExportNode(const wxString& currentBaseDir, const wxTreeItemId& item){
    if (!item.IsOk()) return;

    auto itemData = static_cast<MyTreeItemData*>(GetItemData(item));
    if (!itemData) return;

    // A. 文件夹处理（重点修改）
    if (itemData->IsFolder()){

        // **1. 获取唯一的文件夹名称**
        wxString uniqueDirName = SanitizeAndGetUniqueName(
            currentBaseDir,
            itemData->GetName(),
            true // 目标是文件夹
        );

        // 2. 构建当前节点的完整绝对路径
        wxFileName current_path_obj(currentBaseDir, wxEmptyString);
        current_path_obj.AppendDir(uniqueDirName); // 使用唯一名称作为目录
        wxString current_full_path = current_path_obj.GetFullPath();

        // 3. 创建目录
        if (!wxFileName::DirExists(current_full_path)){
            wxFileName::Mkdir(current_full_path, 0777, wxPATH_MKDIR_FULL);
        }

        // 4. 遍历子节点并递归调用
        if (HasChildren(item)){
            wxTreeItemIdValue cookie;
            wxTreeItemId child = GetFirstChild(item, cookie);
            while (child.IsOk()){
                // 递归调用，将当前创建的唯一路径作为子节点的 BaseDir
                RecursiveExportNode(current_full_path, child);
                child = GetNextChild(item, cookie);
            }
        }
    }
    // B. 笔记处理（将路径拼接交给 ExportNodeContent）
    else{
        ExportNode(currentBaseDir, item);
    }
}

// 读取文本
inline static wxString ReadFileContentWithEncoding(const wxString& filePath){
    wxFile file;
    if (!file.Open(filePath, wxFile::read)){
        return wxEmptyString;
    }

    const wxFileOffset len = file.Length();
    if (len <= 0){
        return wxEmptyString;
    }
    char* rawBuffer = static_cast<char*>(malloc(static_cast<size_t>(len)));
    if (!rawBuffer){
        return wxEmptyString;
    }

    if (file.Read(rawBuffer, static_cast<size_t>(len)) != static_cast<ssize_t>(len)){
        free(rawBuffer);
        return wxEmptyString;
    }
    file.Close();

    size_t bomOffset = 0;
    const wxMBConv* convToUse = &wxConvUTF8;
    const unsigned char* uBuf = reinterpret_cast<const unsigned char*>(rawBuffer);
    if (len >= 3 && uBuf[0] == 0xEF && uBuf[1] == 0xBB && uBuf[2] == 0xBF){
        // UTF-8 with BOM
        bomOffset = 3;
        convToUse = &wxConvUTF8;
    } else if (len >= 2 && uBuf[0] == 0xFF && uBuf[1] == 0xFE){
        // UTF-16 Little Endian
        bomOffset = 2;
        static wxMBConvUTF16LE utf16le;
        convToUse = &utf16le;
    } else if (len >= 2 && uBuf[0] == 0xFE && uBuf[1] == 0xFF){
        // UTF-16 Big Endian
        bomOffset = 2;
        static wxMBConvUTF16BE utf16be;
        convToUse = &utf16be;
    }
    wxString content(rawBuffer + bomOffset, *convToUse, static_cast<size_t>(len - bomOffset));

    if (content.IsEmpty() && len > 0){
        content = wxString(rawBuffer, wxConvISO8859_1, static_cast<size_t>(len));
    }

    free(rawBuffer);

    return content;
}

bool MyTreeCtrl::ImportSingleNote(const wxTreeItemId& root, const wxString& filepath){
    if (!root.IsOk()) return false;
    auto rootData = static_cast<MyTreeItemData*>(GetItemData(root));
    if (!rootData) return false;

    int order = GetChildrenCount(root);
    int parentID = rootData->GetID();
    wxFileName file{ filepath };
    auto name = file.GetName();
    sqlite3* db = GetMyAppInstance()->db;

    auto newItemID = AppendNewRecordToPaths(db, name, parentID, TreeIcon::ICON_NOTE, order, TreeIcon::ICON_NOTE, -1);
    if (newItemID == -1){
        _wxLogError(L"无法在数据库PATHS中创建新笔记。");
        return false;
    }
    wxString noteString = ReadFileContentWithEncoding(filepath);
    auto noteID = AppendNewRecordToNotes(db, noteString, newItemID);
    if (noteID == -1){
        _wxLogError(L"无法在数据库NOTES中创建新笔记。");
        return false;
    }
    // 更新PATHS表中的NOTE_ID字段
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE PATHS SET NOTE_ID = ? WHERE ID = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK){
        sqlite3_bind_int(stmt, 1, noteID);
        sqlite3_bind_int(stmt, 2, newItemID);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); // 确保只在这里释放
    }
    if (rc != SQLITE_DONE){
        _wxLogError("SQL error updating NOTE_ID: %s", sqlite3_errmsg(db));
        return false;
    }

    auto noteData = new MyTreeItemData(newItemID, parentID, name, order, TreeIcon::ICON_NOTE, noteID, false, false, false);
    auto noteItem = AppendItem(root, name, TreeIcon::ICON_NOTE, -1, noteData);
    if (!noteItem.IsOk()){
        _wxLogError(L"无法创建树节点。");
        delete noteData;
        return false;
    }
    m_idToItem[newItemID] = noteItem;
    m_idToData[newItemID] = noteData;
    return true;
}

void MyTreeCtrl::RecursiveImportFolder(const wxTreeItemId& root, const wxString& currentFolderPath){
    int maxDepth;
    auto Paths = GetTextFilesAndDirs(currentFolderPath, maxDepth);
    //用于储存当前节点层级的数组
    std::vector<int> folderIndex(maxDepth + 1, 0);
    auto rootData = static_cast<MyTreeItemData*>(GetItemData(root));
    folderIndex[0] = rootData->GetID();

    sqlite3* db = GetMyAppInstance()->db;
    Freeze();
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    bool shouldRollback = false;
    for (auto& currentPath : Paths){
        int parentIndex = currentPath.depth - 1;
        auto parentID = folderIndex[parentIndex];
        auto parentItem = m_idToItem[parentID];
        if (currentPath.isDir){
            auto order = GetChildrenCount(parentItem);
            auto name = currentPath.name;

            auto newItemID = AppendNewRecordToPaths(db, name, parentID, TreeIcon::ICON_DEFAULT_FOLDER, order, TreeIcon::ICON_DEFAULT_FOLDER, -1);
            if (newItemID == -1){
                shouldRollback = true;
                _wxLogError(L"无法在数据库中创建新文件夹。");
                return;
            }
            auto folderData = new MyTreeItemData(newItemID, parentID, name, order, TreeIcon::ICON_DEFAULT_FOLDER, -1, true, false, false);
            auto folderItem = AppendItem(parentItem, name, TreeIcon::ICON_DEFAULT_FOLDER, -1, folderData);
            if (!folderItem.IsOk()){
                _wxLogError(L"无法创建树节点。");
                shouldRollback = true;
                delete folderData;
                return;
            }

            m_idToItem[newItemID] = folderItem;
            m_idToData[newItemID] = folderData;
            folderIndex[currentPath.depth] = newItemID;
        } else{
            wxFileName rootNameOnly(currentFolderPath);
            auto rootDirName = rootNameOnly.GetName();
            wxString redundantPrefix = rootDirName + wxFileName::GetPathSeparator();
            wxString pathAfterCorrection = currentPath.fullpath;
            if (!rootDirName.IsEmpty() && pathAfterCorrection.StartsWith(redundantPrefix)){
                // 如果 fullpath 是 "a/xxx.txt"，则移除 "a/"
                pathAfterCorrection.Remove(0, redundantPrefix.Length());
            }

            wxString fullFilePath = currentFolderPath + wxFileName::GetPathSeparator() + pathAfterCorrection;

            if (!ImportSingleNote(parentItem, fullFilePath)){
                shouldRollback = true;
                break;
            }
        }
    }
    if (shouldRollback){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
    } else{
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    }
    Thaw();
    Expand(root);
}

wxTreeItemId MyTreeCtrl::CopyItemRecursivelyAndMoveData(const wxTreeItemId& itemToCopy, const wxTreeItemId& parentItem, std::unordered_map<int, wxTreeItemId>& mapToUpdate){
    auto itemData = static_cast<MyTreeItemData*>(GetItemData(itemToCopy));
    if (!itemData) return wxTreeItemId();
    SetItemData(itemToCopy, nullptr); //从原节点解绑数据
    wxTreeItemId newItem = AppendItem(parentItem, itemData->GetName(), GetIconIndex(itemData->GetStateIcon()), -1, itemData);
    mapToUpdate[itemData->GetID()] = newItem;
    m_idToItem[itemData->GetID()] = newItem;//直接覆盖原map值
    if (ItemHasChildren(itemToCopy)){
        wxTreeItemIdValue cookie;
        wxTreeItemId child = GetFirstChild(itemToCopy, cookie);
        while (child.IsOk()){
            CopyItemRecursivelyAndMoveData(child, newItem, mapToUpdate);
            child = GetNextChild(itemToCopy, cookie);
        }
    }
    return newItem;
}

bool MyTreeCtrl::UpdateNodeOrderForParent(sqlite3* db, wxTreeItemId parentItem){
    wxTreeItemIdValue cookie;
    wxTreeItemId currentItem = GetFirstChild(parentItem, cookie);
    int order = 0;
    const char* sql = "UPDATE PATHS SET NODE_ORDER = ? WHERE ID = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK){
        _wxLogError(wxT("SQL 预处理错误: %s, SQL: %s"), wxString::FromUTF8(sqlite3_errmsg(db)), wxString::FromUTF8(sql));
        return false;
    }
    while (currentItem.IsOk()){
        MyTreeItemData* currentData = static_cast<MyTreeItemData*>(GetItemData(currentItem));
        if (currentData){
            // 1. 更新内存数据
            currentData->SetNodeOrder(order);
            // 2. 更新数据库
            if (sqlite3_bind_int(stmt, 1, order) != SQLITE_OK || sqlite3_bind_int(stmt, 2, currentData->GetID()) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE){
                _wxLogError(wxT("SQL 执行错误或绑定失败: %s, ID: %d"), wxString::FromUTF8(sqlite3_errmsg(db)), currentData->GetID());
                sqlite3_finalize(stmt);
                return false;
            }
            sqlite3_reset(stmt);
        }
        order++;
        currentItem = GetNextSibling(currentItem);
    }
    sqlite3_finalize(stmt);
    return true;
}

void MyTreeCtrl::PostEventToCardViewPanel(const wxTreeItemId& currentItem){
    auto data = static_cast<MyTreeItemData*>(GetItemData(currentItem));
    if (!data->IsFolder())return;

    wxTreeEvent evt(EVT_SHOW_NODE_CONTENT, GetId());
    evt.SetItem(currentItem);
    int itemType = GetTreeItemType(currentItem);
    sqlite3* db = GetMyAppInstance()->db;

    m_mainFrame->switchToCardViewPanel();
    auto cardList = new std::vector<CardData>();
    wxTreeItemIdValue cookie;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT CONTENT FROM NOTES WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK){
        _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
        delete cardList;
        return;
    }

    for (auto&& it = GetFirstChild(currentItem, cookie); it.IsOk(); it = GetNextChild(currentItem, cookie)){
        auto childData = static_cast<MyTreeItemData*>(GetItemData(it));
        CardData cardData;
        cardData.id = childData->GetID();
        cardData.treeItemId = it;
        cardData.name = childData->GetName();

        if (childData->IsFolder()){
            wxString preview;
            wxTreeItemIdValue childCookie;
            for (auto&& child = GetFirstChild(it, childCookie); child.IsOk(); child = GetNextChild(it, childCookie)){
                auto childChildData = static_cast<MyTreeItemData*>(GetItemData(child));
                if (childChildData->IsFolder()){
                    preview += L"[" + childChildData->GetName() + L"]\n";
                } else{
                    preview += L"· " + childChildData->GetName() + L"\n";
                }
            }
            cardData.type = 0;
            cardData.info = wxEmptyString;
            cardData.date = wxEmptyString;
            cardData.contentPreview = preview;
        } else{
            int noteID = childData->GetNoteID();
            cardData.type = 1;
            cardData.noteId = noteID;

            sqlite3_bind_int(stmt, 1, noteID);
            if (sqlite3_step(stmt) == SQLITE_ROW){
                cardData.contentPreview = CutStringForPreview(wxString::FromUTF8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
            }
            sqlite3_reset(stmt);

            wxString noteTags = GetNoteTags(db, childData->GetID());
            wxString noteTimestamp = GetNoteTimestamp(db, noteID, 1);
            cardData.info = L"标签：" + noteTags;
            cardData.date = L"修改时间：" + noteTimestamp;
        }
        cardList->push_back(std::move(cardData));
    }
    sqlite3_finalize(stmt);
    evt.SetClientData(cardList);
    wxPostEvent(m_mainFrame->m_cardViewPanel, evt);
}

int MyTreeCtrl::GetIconIndex(int stateicon){
    if ((stateicon & 0x80) == 0)return stateicon;
    int index_icon = stateicon & 0xF;
    int index_color = (stateicon >> 4) & 0x7;
    if (index_icon >= 12 || index_color >= 8){
        return 0;
    }
    if (m_IconCache[index_icon][index_color])return m_IconCache[index_icon][index_color];
    auto imgList = GetImageList();
    auto size = imgList->GetSize();
    wxBitmapBundle bundle;
    wxBitmap bitmap;
    char buff[4096];
    const char* color;
    const char* svgStr;
    switch (index_icon){
    case 0:svgStr = TREE_ICON_STYLE_NOTE; break;
    case 1:svgStr = TREE_ICON_STYLE_FOLDER; break;
    case 2:svgStr = TREE_ICON_STYLE_2; break;
    case 3:svgStr = TREE_ICON_STYLE_3; break;
    case 4:svgStr = TREE_ICON_STYLE_4; break;
    case 5:svgStr = TREE_ICON_STYLE_5; break;
    case 6:svgStr = TREE_ICON_STYLE_6; break;
    case 7:svgStr = TREE_ICON_STYLE_7; break;
    case 8:svgStr = TREE_ICON_STYLE_8; break;
    case 9:svgStr = TREE_ICON_STYLE_9; break;
    case 10:svgStr = TREE_ICON_STYLE_10; break;
    case 11:svgStr = TREE_ICON_STYLE_11; break;

        //最多16个样式
    }
    switch (index_color){
    case 0:color = TREE_ICON_COLOR_0; break;
    case 1:color = TREE_ICON_COLOR_1; break;
    case 2:color = TREE_ICON_COLOR_2; break;
    case 3:color = TREE_ICON_COLOR_3; break;
    case 4:color = TREE_ICON_COLOR_4; break;
    case 5:color = TREE_ICON_COLOR_5; break;
    case 6:color = TREE_ICON_COLOR_6; break;
    case 7:color = TREE_ICON_COLOR_7; break;
        //最多8个样式
    }
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, svgStr, color, 73), size);
    m_IconCache[index_icon][index_color] = imgList->Add(bundle.GetBitmap(size));
    return m_IconCache[index_icon][index_color];
}

wxTreeItemId MyTreeCtrl::TreeSwapSiblings(wxTreeItemId& currentItem, int direction){
    Freeze();
    wxTreeItemId targetItem;
    if (direction == -1){
        targetItem = GetPrevSibling(currentItem);
    } else if (direction == 1){
        targetItem = GetNextSibling(currentItem);
    } else{
        Thaw();
        return wxTreeItemId();
    }

    if (!targetItem.IsOk()){
        Thaw();
        return currentItem;
    }
    wxTreeItemId oldUp, oldDown;
    if (direction == -1){
        oldUp = targetItem;
        oldDown = currentItem;
    } else{
        oldUp = currentItem;
        oldDown = targetItem;
    }
    //要把oldDown插入到oldUp之前
    //先用InsertItem方法在oldUp之前插入oldDown副本，然后转移所有oldDown子节点到副本下
    auto oldUpData = static_cast<MyTreeItemData*>(GetItemData(oldUp));
    auto oldDownData = static_cast<MyTreeItemData*>(GetItemData(oldDown));
    SetItemData(oldDown, nullptr);
    auto newUp = InsertItem(GetItemParent(oldDown), oldDownData->GetNodeOrder() - 1, oldDownData->GetName(), GetIconIndex(oldDownData->GetStateIcon()), -1, oldDownData);
    std::unordered_map<int, wxTreeItemId> mapToUpdate;
    m_idToItem[oldDownData->GetID()] = newUp;
    wxTreeItemIdValue cookie;
    auto child = GetFirstChild(oldDown, cookie);
    while (child.IsOk()){
        CopyItemRecursivelyAndMoveData(child, newUp, mapToUpdate);
        child = GetNextChild(oldDown, cookie);
    }
    Delete(oldDown);
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    if (!UpdateNodeOrderForParent(db, GetItemParent(newUp))){
        _wxLogError("SQL error updating NODE_ORDER: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        wxExit();
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    Thaw();
    return newUp;
}

wxTreeItemId MyTreeCtrl::FindOrCreateWebCollectionNote(const wxTreeItemId& parentItem, wxString& targetText){
    if (!parentItem.IsOk())return wxTreeItemId();
    wxTreeItemIdValue cookie;
    wxTreeItemId child = GetFirstChild(parentItem, cookie);
    while (child.IsOk()){
        auto itemData = static_cast<MyTreeItemData*>(GetItemData(child));
        if (itemData && !itemData->IsFolder() && itemData->GetName() == targetText){
            return child;
        }
        child = GetNextChild(parentItem, cookie);
    }
    //没找到，新建节点
    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    auto parentData = static_cast<MyTreeItemData*>(GetItemData(parentItem));
    int order = GetChildrenCount(parentItem);
    int parentID = parentData->GetID();
    auto newItemID = AppendNewRecordToPaths(db, targetText, parentID, TreeIcon::ICON_NOTE, order, TreeIcon::ICON_NOTE, -1);
    if (newItemID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库PATHS中创建新笔记。");
        return wxTreeItemId();
    }
    wxString noteString = wxEmptyString;
    auto noteID = AppendNewRecordToNotes(db, noteString, newItemID);
    if (noteID == -1){
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        _wxLogError(L"无法在数据库NOTES中创建新笔记。");
        return wxTreeItemId();
    }
    // 更新PATHS表中的NOTE_ID字段
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE PATHS SET NOTE_ID = ? WHERE ID = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, noteID) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, newItemID) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error updating NOTE_ID: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);

    }
    sqlite3_finalize(stmt);
    auto noteData = new MyTreeItemData(newItemID, parentID, targetText, order, TreeIcon::ICON_NOTE, noteID, false, false, false);
    auto noteItem = AppendItem(parentItem, targetText, TreeIcon::ICON_NOTE, -1, noteData);
    if (!noteItem.IsOk()){
        _wxLogError(L"无法创建树节点。");
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        delete noteData;
        return wxTreeItemId();
    }

    m_idToItem[newItemID] = noteItem;
    m_idToData[newItemID] = noteData;
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    if (!IsExpanded(parentItem)){ Expand(parentItem); }
    return noteItem;
}

//wxTreeItemId MyTreeCtrl::DoRecursiveSearchNote(wxTreeItemId parent, const wxString& text){
//    wxTreeItemIdValue cookie;
//    wxTreeItemId child = this->GetFirstChild(parent, cookie);
//
//    while (child.IsOk()){
//        auto itemData = static_cast<MyTreeItemData*>(this->GetItemData(child));
//
//        if (itemData && !itemData->IsFolder() && this->GetItemText(child) == text){
//            return child;
//        }
//
//        // 递归检查子节点的子节点
//        if (this->ItemHasChildren(child)){
//            wxTreeItemId grandChild = DoRecursiveSearchNote(child, text);
//            if (grandChild.IsOk()){
//                return grandChild;
//            }
//        }
//
//        child = this->GetNextChild(parent, cookie);
//    }
//
//    return wxTreeItemId();
//}

//flag:2，3是跨根，1是不跨根
wxTreeItemId MyTreeCtrl::TreeMoveItem(wxTreeItemId fromItem, wxTreeItemId toItem, int flag){
    Freeze();
    MyTreeItemData* tempData = static_cast<MyTreeItemData*>(GetItemData(fromItem));
    if (!tempData){ Thaw(); return wxTreeItemId(); }
    int oldID = tempData->GetID();
    //处理笔记保存
    int fromItemType = GetTreeItemType(fromItem);
    if ((fromItemType == FolderItemID || fromItemType == WebCollectionItemID) && tempData->GetNoteID() != -1){
        //储存笔记内容
        m_mainFrame->SaveNoteContent(tempData->GetNoteID());
        //修改显示名称
        auto name = GetItemText(fromItem);
        if (name.EndsWith("*")){
            name.RemoveLast(1);
            SetItemText(fromItem, name);
        }
    }
    //容器，记录要处理数据库的节点
    std::unordered_map<int, wxTreeItemId> mapToUpdate;
    //记录要清理的节点，包括来源的父节点、目标本身（如果是文件夹）或目标的父节点（作为兄弟插入）
    wxTreeItemId oldParent = GetItemParent(fromItem);

    //确定新的父节点和插入点
    wxTreeItemId newItem, newParentItem, insertBeforeItem;
    auto newParentItemData = static_cast<MyTreeItemData*>(GetItemData(toItem));
    if (newParentItemData->IsFolder()){
        newParentItem = toItem;
    } else{
        newParentItem = GetItemParent(toItem);
        newParentItemData = static_cast<MyTreeItemData*>(GetItemData(newParentItem));
        insertBeforeItem = toItem;
    }
    //获取新父节点的ID
    int newParentID = newParentItemData->GetID();
    //复制所有节点
    if (insertBeforeItem.IsOk()){
        SetItemData(fromItem, nullptr);
        auto insertBeforeItemData = static_cast<MyTreeItemData*>(GetItemData(insertBeforeItem));
        newItem = InsertItem(newParentItem, insertBeforeItemData->GetNodeOrder(), tempData->GetName(), GetIconIndex(tempData->GetStateIcon()), -1, tempData);//插入在节点前
        mapToUpdate[oldID] = newItem;
        m_idToItem[oldID] = newItem;
        wxTreeItemIdValue cookie;
        auto child = GetFirstChild(fromItem, cookie);
        while (child.IsOk()){
            CopyItemRecursivelyAndMoveData(child, newItem, mapToUpdate);
            child = GetNextChild(fromItem, cookie);
        }
    } else{
        newItem = CopyItemRecursivelyAndMoveData(fromItem, newParentItem, mapToUpdate);
    }
    Delete(fromItem);

    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_exec(db, "BEGIN;", nullptr, nullptr, nullptr);
    //数据库操作:1.更新影响到的节点的order，2.更新isTrashed字段,3.更新顶层节点的parentId
    if (!UpdateNodeOrderForParent(db, newParentItem) || !UpdateNodeOrderForParent(db, oldParent)){
        _wxLogError("SQL error updating NODE_ORDER: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        wxExit();
    }

    m_idToData[oldID]->SetParentID(newParentID);
    const char* sql = "UPDATE PATHS SET PARENT_ID = ? WHERE ID = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK || sqlite3_bind_int(stmt, 1, tempData->GetParentID()) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, tempData->GetID()) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE){
        _wxLogError("SQL error updating PARENT_ID: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        wxExit();
    }
    sqlite3_finalize(stmt);

    if (flag == 2 || flag == 3){
        bool newTrashed = (flag == 2);

        sqlite3_stmt* stmt;
        const char* sql = "UPDATE PATHS SET TRASHED = ? WHERE ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK){
            _wxLogError(wxT("SQL 预处理错误: %s"), wxString::FromUTF8(sqlite3_errmsg(db)));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            wxExit();
        }
        int isTrashed_int = newTrashed ? 1 : 0;

        for (const auto& [mapItemid, mapItem] : mapToUpdate){
            m_idToData[mapItemid]->SetTrashed(newTrashed);

            if ((sqlite3_bind_int(stmt, 1, isTrashed_int) != SQLITE_OK) || (sqlite3_bind_int(stmt, 2, mapItemid) != SQLITE_OK) || (sqlite3_step(stmt) != SQLITE_DONE)){
                _wxLogError(wxT("SQL 执行 UPDATE TRASHED 失败: %s, ID: %d"), wxString::FromUTF8(sqlite3_errmsg(db)), mapItemid);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                wxExit();
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }
    if (flag == 2 || flag == 4){
        //清除url字段
        sqlite3_stmt* stmt;
        const char* sql = "UPDATE NOTES SET URL = NULL WHERE PATH_ID = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK){
            _wxLogError(wxT("SQL 预处理错误: %s"), wxString::FromUTF8(sqlite3_errmsg(db)));
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            wxExit();
        }
        for (const auto& [mapItemid, _] : mapToUpdate){
            if ((sqlite3_bind_int(stmt, 1, mapItemid) != SQLITE_OK) || (sqlite3_step(stmt) != SQLITE_DONE)){
                _wxLogError(wxT("SQL 执行 UPDATE URL 失败: %s, ID: %d"), wxString::FromUTF8(sqlite3_errmsg(db)), mapItemid);
                sqlite3_finalize(stmt);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                wxExit();
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    Thaw();
    SelectItem(newItem);
    return newItem;
}