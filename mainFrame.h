#pragma once
#ifndef _MAINFRAME_H_
#define _MAINFRAME_H_

#include "wx/wxprec.h" 
#ifndef WX_PRECOMP
#include "wx/wx.h" 
#endif
#include "wx/splitter.h"  
#include "wx/simplebook.h"  
#include "MySTC.h"
#include "wx/fdrepdlg.h"

class MyTreeCtrl;
class MyCardViewPanel;
class wxTreeEvent;
class MyTrayIcon;
class TagDialog;
class FontStylePanel;
//class MyStatBar;
struct OutlineItem {
    wxString text;
    int position;
};
class MongooseServer;
class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title);
    ~MainFrame();
    void OnExit();
    void OnRestart();
    void switchToCardViewPanel();
    void switchToScintillaCtrl();
    bool SaveNoteContent(int noteID);
    void ScheduleAutoSave();
    void FlushAutoSave();
    void ResetAutoSaveSession();
    bool SaveCurrentNoteIfModified();
    int GetCurrentNoteID() const { return m_currentNoteID; }
    int GetCurrentSimplebookPageID() { return m_simplebook->GetSelection(); }
    void FreezeSplitter()const { m_splitter->Freeze(); }
    void ThawSplitter()const { m_splitter->Thaw(); }
    void RestartServer();

    MySTC* m_scintilla;
    MyCardViewPanel* m_cardViewPanel;
    MyTreeCtrl* m_tree;
    wxFindReplaceDialog* m_findReplaceDlg = nullptr;
    wxFindReplaceData* m_findReplaceData = new wxFindReplaceData();
    wxFindReplaceDialog* m_findSqlDlg = nullptr;
    wxFindReplaceData* m_findSqlData = new wxFindReplaceData();

    bool m_showRecent;
    bool m_showTags;
    bool m_autoLineBreak;
    bool m_showMatchesIndicator;
    bool m_outlineNeedUpdate = true;
    int m_recentMaxCount = 10;

    enum menuID {
        //主窗口菜单和工具栏按钮
        //---总是有效
        ID_Quit = wxID_EXIT,
        ID_About = wxID_ABOUT,
        //文件和编辑
        ID_Restart = wxID_HIGHEST + 1,
        MENU_NEW_FOLDER,
        MENU_NEW_NOTE,
        MENU_DELETE_ITEM,
        MENU_IMPORT_FOLDER,
        MENU_IMPORT_NOTE,
        MENU_FIND_SQL,
        // 导航菜单
        MENU_PREV_VIEW,
        MENU_EXPAND_ALL,
        MENU_COLLAPSE_ALL,
        MENU_EMPTY_RECYCLE_BIN,
        MENU_QUICK_SWITCH,
        MENU_DELETE_HOTKEY,
        // 工具菜单
        MENU_GLOBAL_SETTINGS,
        MENU_EDIT_CONFIGFILE,
        MENU_CHECK_UPDATE,
        MENU_HELP_TOPICS,

        //---bool值控制
        MENU_SHOW_TAGS,
        MENU_SHOW_RECENT,
        MENU_ALWAYS_TOP,
        MENU_NET_SERVER,
        //---仅笔记节点（不包括回收站的），编辑菜单和sci显示设置
        MENU_UNDO,
        MENU_REDO,
        MENU_RESTORE,
        MENU_SELECTALL,
        MENU_CUT,
        MENU_COPY,
        MENU_PASTE,
        MENU_CHANGE_ENCODING,
        MENU_ENCODING_BASE,
        MENU_ENCODING_TOP = MENU_ENCODING_BASE + 100,
        MENU_SEARCH_REPLACE,
        MENU_INSERT_DATE_TIME1,
        MENU_INSERT_DATE_TIME2,
        MENU_INSERT_DATE_TIME3,
        MENU_INSERT_SERIALS,
        MENU_INSERT_SERIALS_1,
        MENU_INSERT_SERIALS_2,
        MENU_INSERT_SERIALS_3,
        MENU_INSERT_SERIALS_4,
        // MENU_INSERT_SYMBOL,
        MENU_LINE_UP,
        MENU_LINE_DOWN,
        MENU_LINE,
        MENU_INSERT,
        MENU_REMOVE_EMPTYLINE,
        MENU_LINE_DUPLICATE,
        MENU_LINE_DELETE,
        MENU_APPEND_CLIPBOARD,
        MENU_EDIT_TAGS,
        MENU_AUTO_LINEBREAK,
        MENU_SHOW_MATCHES_INDICATOR,
        MENU_SHOW_SYMBOL,
        MENU_SHOW_WHITESPACE,
        MENU_SHOW_EOL,
        MENU_SET_STYLE,
        TOOLBAR_SET_STYLE,
        MENU_SET_STYLE_DEFAULT,
        MENU_SET_STYLE_BOLD,
        MENU_SET_STYLE_ITALICGREY,
        MENU_SET_STYLE_RED,
        MENU_SET_STYLE_BLUE,
        MENU_SET_STYLE_YELLOW_BG,
        MENU_SET_STYLE_PINK_BG,
        MENU_SET_STYLE_LIGHTBLUE_BG,
        MENU_SET_STYLE_TITLE,
        MENU_SET_STYLE_LEVEL1,
        MENU_SET_STYLE_LEVEL2,

        //笔记、文件夹、标签
        MENU_RENAME_ITEM,
        //MENU_PROPERTIES,
        //笔记和文件夹（不包括回收站的）
        MENU_ASSIGN_HOTKEY,
        MENU_MOVE_TO_RECYCLE_BIN,
        MENU_EXPORT_ITEM,

        //非updateui管理
        //控件
        ID_Notebook,
        ID_Scintilla,
        ID_TextCtrl,
        ID_TreeCtrl,
        ID_CardViewPanel,
        ID_Searchbox,
        ID_Choice,
        //Tree菜单项
        MENU_NEW_TAG,
        MENU_HIDE_TAGS,
        MENU_HIDE_RECENT,
        MENU_HIDE_SEARCH,
        //MENU_ADD_EXTENSION_LIST,
        //MENU_ADD_EXTENSION_ALARM,
        //MENU_ADD_EXTENSION_LINK,
        //MENU_DELETE_EXTENSION,
        MENU_ICON_CHANGE_STYLE_RESTORE,
        MENU_ICON_CHANGE_STYLE_BASE,
        MENU_ICON_CHANGE_STYLE_TOP = MENU_ICON_CHANGE_STYLE_BASE + 20,
        MENU_ICON_CHANGE_COLOR_BASE,
        MENU_ICON_CHANGE_COLOR_TOP = MENU_ICON_CHANGE_COLOR_BASE + 10,
        MENU_ADD_TAG,
        MENU_REMOVE_TAG,
        MENU_CUSTOM_LINK_BASE,
        MENU_CUSTOM_LINK_TOP = MENU_CUSTOM_LINK_BASE + 10,

        //tray菜单
        MENU_TRAY_RESTORE,
        MENU_TRAY_EXIT,
        //MENU_TRAY_NEWNOTE_AND_CLIPBOARD,

        //快捷键
        ID_HOTKEY_SETNAME,
        ID_HOTKEY_FOCUSTREE,
        ID_HOTKEY_FOCUSEDITOR,
        ID_TIMER_AUTOSAVE,
        //快速切换菜单，由OnUpdateQuickSwitchMenu管理
        MENU_QUICK_SWITCH_BASE = wxID_HIGHEST + 200, // 快速切换子菜单的基ID
        MENU_DELETE_HOTKEY_BASE = wxID_HIGHEST + 250,
        MENU_ASSIGN_HOTKEY_BASE = wxID_HIGHEST + 300,

    };
    enum simplebookPageID {
        ID_CardViewPage = 0,
        ID_ScintillaPage,
    };

    void ApplyTheme(bool isDark);
    void UpdateToolBarIcons();

private:
    wxSplitterWindow* m_splitter;
    wxSimplebook* m_simplebook;
    MyTrayIcon* m_trayIcon;
    wxArrayString* m_ClipboardBlackList;
    wxChoice* m_Choice = nullptr;
    wxStaticText* m_labelNav = nullptr;
    wxStatusBar* m_statusBar = nullptr;
    FontStylePanel* m_stylePopup = nullptr;
    std::vector<OutlineItem> m_outlineList;
    int m_quickSwitchMenuIds[9] = { 0 };
    bool m_EnableClipboardToSTC = false;
    bool m_closeToTray;
    bool m_minimizeToTray;
    bool m_ClipboardGetMoreInfo;
    bool m_PinToTop = false;
    bool m_enableNetServer = false;
    int m_currentNoteID = -1;
    int m_autoSaveDelayMs = 1000;
    wxTimer m_autoSaveTimer;
    HWND m_hMainFrame;
    MongooseServer* m_server;

    virtual bool MSWHandleMessage(WXLRESULT* result, WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
    void RegisterGlobalHotkey();
    void UnregisterGlobalHotKey();
    void OnClipboardUpdate();
    void EnableClipboardListener(bool enable);
    void SetClipboardBlackList();

    void CreatMenuBar();
    void CreateMyToolBar();
    void InitUI();
    void InitBind();
    void StartServer();

    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnAutoSaveTimer(wxTimerEvent& event);
    void OnActivate(wxActivateEvent& event);
    void OnShowNoteContent(wxTreeEvent& event);
    void OnEditTags(wxCommandEvent& event);
    void OnGlobalSettings(wxCommandEvent& event);
    void OnCheckUpdate(wxCommandEvent& event);

    void OnUpdateEditorMenu(wxUpdateUIEvent& event);//编辑器菜单
    void OnUpdateQuickSwitchMenu(wxUpdateUIEvent& event);//特殊：快速切换菜单
    void OnUpdateItemMenu(wxUpdateUIEvent& event);//未分类的菜单项
    void OnUpdateBoolMenu(wxUpdateUIEvent& event);//由bool值确定的，且其他方法会更改bool值的
    void OnToolbarStyleButtonClicked(wxCommandEvent& event);
    void OnSearchAndReplace(wxCommandEvent& event);
    void OnFindSql(wxCommandEvent& event);
    void OnFind(wxFindDialogEvent& event);
    void OnCharHook(wxKeyEvent& event);
    void OnFocusChoice(wxMouseEvent& event);
    void OnOutlineSelect(wxCommandEvent& event);
    void OnSysColorChange2(wxSysColourChangedEvent& event);//解决黑暗模式时主题切换时toolbar恢复白色背景的bug
    void OnHttpRequest(wxThreadEvent& event);
    void OnRepairEncoding(wxCommandEvent& event);
};

class FontStylePanel : public wxPopupTransientWindow {
public:
    FontStylePanel(MainFrame* parent, wxSize size);
private:
    wxSize iconSize;
    MainFrame* m_parent;
};
#endif 

