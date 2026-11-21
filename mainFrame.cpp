#include "pch.h"
#include <wx/webrequest.h>
#include "mainFrame.h"
#include "App.h"
#include "events.h"
#include "MyCardViewPanel.h" 
#include "MyTreeCtrl.h"
#include "Dialogs.h"
#include "Tray.h"
#include "SettingDialog.h"
#include "Helper.h"
#include "SVG_ICON.h"
#include <picojson/picojson.h>
#include "MongooseServer.h"
#include "resource.h"

class FindReplaceBar : public wxPanel {
public:
    FindReplaceBar(wxWindow* parent, MySTC* stc)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE), m_stc(stc) {
        wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);

        // 第一行：查找输入框 + 上/下导航 + 关闭
        wxBoxSizer* findRow = new wxBoxSizer(wxHORIZONTAL);
        m_findLabel = new wxStaticText(this, wxID_ANY, L"查找:");
        m_findCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(80), -1), wxTE_PROCESS_ENTER);
        m_prevBtn = new wxButton(this, wxID_ANY, L"上一个", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_nextBtn = new wxButton(this, wxID_ANY, L"下一个", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_closeBtn = new wxButton(this, wxID_ANY, L"✕", wxDefaultPosition, wxSize(FromDIP(26), -1), wxBU_EXACTFIT);
        m_closeBtn->SetToolTip(L"关闭查找栏 (Esc)");

        findRow->Add(m_findLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        findRow->Add(m_findCtrl, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        findRow->Add(m_prevBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        findRow->Add(m_nextBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        findRow->Add(m_closeBtn, 0, wxALIGN_CENTER_VERTICAL);

        // 第二行：替换输入框 + 替换 / 全部替换
        wxBoxSizer* replaceRow = new wxBoxSizer(wxHORIZONTAL);
        m_replaceLabel = new wxStaticText(this, wxID_ANY, L"替换:");
        m_replaceCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(80), -1), wxTE_PROCESS_ENTER);
        m_replaceBtn = new wxButton(this, wxID_ANY, L"替换", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_replaceAllBtn = new wxButton(this, wxID_ANY, L"全部替换", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);

        replaceRow->Add(m_replaceLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        replaceRow->Add(m_replaceCtrl, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        replaceRow->Add(m_replaceBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        replaceRow->Add(m_replaceAllBtn, 0, wxALIGN_CENTER_VERTICAL);

        // 第三行：匹配选项 + 匹配计数/状态
        wxBoxSizer* optionRow = new wxBoxSizer(wxHORIZONTAL);
        m_caseCheck = new wxCheckBox(this, wxID_ANY, L"区分大小写");
        m_wholeCheck = new wxCheckBox(this, wxID_ANY, L"全词");
        m_statusLabel = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(100), -1), wxALIGN_RIGHT | wxST_NO_AUTORESIZE);

        optionRow->Add(m_caseCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
        optionRow->Add(m_wholeCheck, 0, wxALIGN_CENTER_VERTICAL);
        optionRow->AddStretchSpacer(1);
        optionRow->Add(m_statusLabel, 0, wxALIGN_CENTER_VERTICAL);

        rootSizer->Add(findRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(5));
        rootSizer->Add(replaceRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(5));
        rootSizer->Add(optionRow, 0, wxEXPAND | wxALL, FromDIP(5));
        SetSizer(rootSizer);

        // 事件绑定
        m_findCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { OnFindTextChanged(); });
        m_prevBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { FindNext(false); });
        m_nextBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { FindNext(true); });
        m_caseCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { UpdateMatchCount(); });
        m_wholeCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { UpdateMatchCount(); });
        m_replaceBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ReplaceCurrent(); });
        m_replaceAllBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ReplaceAll(); });
        m_closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CloseBar(); });

        Bind(wxEVT_CHAR_HOOK, &FindReplaceBar::OnCharHook, this);
    }

    void Open(const wxString& initialText) {
        if (!IsShown()) {
            Show();
            if (GetParent()) {
                GetParent()->Layout();
            }
        }
        if (!initialText.IsEmpty() && initialText.Find('\n') == wxNOT_FOUND && initialText.Find('\r') == wxNOT_FOUND) {
            m_findCtrl->ChangeValue(initialText);
        }
        m_findCtrl->SetFocus();
        m_findCtrl->SelectAll();
        UpdateMatchCount();
    }

    void CloseBar() {
        if (IsShown()) {
            Hide();
            if (GetParent()) {
                GetParent()->Layout();
            }
            if (m_stc) {
                m_stc->SetFocus();
            }
        }
    }

    void ApplyTheme(bool isDark) {
        wxColour barBg = isDark ? ThemeColours::DarkBgSecondary : ThemeColours::LightBgSecondary;
        wxColour inputBg = isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase;
        wxColour fg = isDark ? ThemeColours::DarkTextPrimary : ThemeColours::LightTextPrimary;
        wxColour subFg = isDark ? ThemeColours::DarkTextSecondary : ThemeColours::LightTextSecondary;

        SetBackgroundColour(barBg);
        SetForegroundColour(fg);

        auto styleCtrl = [&](wxWindow* w, const wxColour& bg, const wxColour& textFg) {
            if (!w) return;
            HWND hwnd = (HWND)w->GetHWND();
            if (hwnd) {
                ::SetWindowTheme(hwnd, isDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
            }
            w->SetBackgroundColour(bg);
            w->SetForegroundColour(textFg);
        };

        styleCtrl(m_findLabel, barBg, fg);
        styleCtrl(m_replaceLabel, barBg, fg);
        styleCtrl(m_statusLabel, barBg, subFg);
        auto styleCheck = [&](wxCheckBox* chk) {
            if (!chk) return;
            HWND hwnd = (HWND)chk->GetHWND();
            if (hwnd) {
                ::SetWindowTheme(hwnd, isDark ? L"" : L"Explorer", isDark ? L"" : NULL);
            }
            chk->SetBackgroundColour(barBg);
            chk->SetForegroundColour(fg);
        };
        styleCheck(m_caseCheck);
        styleCheck(m_wholeCheck);
        styleCtrl(m_findCtrl, inputBg, fg);
        styleCtrl(m_replaceCtrl, inputBg, fg);
        styleCtrl(m_prevBtn, barBg, fg);
        styleCtrl(m_nextBtn, barBg, fg);
        styleCtrl(m_replaceBtn, barBg, fg);
        styleCtrl(m_replaceAllBtn, barBg, fg);
        styleCtrl(m_closeBtn, barBg, fg);

        Refresh();
    }

private:
    int GetSearchFlags() const {
        int flags = 0;
        if (m_caseCheck && m_caseCheck->GetValue()) flags |= wxSTC_FIND_MATCHCASE;
        if (m_wholeCheck && m_wholeCheck->GetValue()) flags |= wxSTC_FIND_WHOLEWORD;
        return flags;
    }

    void OnCharHook(wxKeyEvent& event) {
        int key = event.GetKeyCode();
        if (key == WXK_ESCAPE) {
            CloseBar();
            return;
        }
        if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
            wxWindow* focused = wxWindow::FindFocus();
            if (focused == m_findCtrl) {
                FindNext(!event.ShiftDown());
                return;
            }
            if (focused == m_replaceCtrl) {
                ReplaceCurrent();
                return;
            }
        }
        event.Skip();
    }

    void OnFindTextChanged() {
        if (!m_stc) return;
        wxString findText = m_findCtrl->GetValue();
        if (findText.IsEmpty()) {
            m_statusLabel->SetLabel(wxEmptyString);
            return;
        }
        const long findByteLen = static_cast<long>(findText.ToUTF8().length());
        const int flags = GetSearchFlags();
        const long docLen = m_stc->GetLength();
        long startPos = m_stc->GetSelectionStart();
        long foundPos = m_stc->FindText(startPos, docLen, findText, flags);
        if (foundPos == -1 && startPos > 0) {
            foundPos = m_stc->FindText(0, docLen, findText, flags);
        }
        if (foundPos != -1) {
            m_stc->SetSelection(foundPos, foundPos + findByteLen);
            m_stc->EnsureCaretVisible();
        }
        UpdateMatchCount();
    }

    void UpdateMatchCount() {
        if (!m_stc) return;
        wxString findText = m_findCtrl->GetValue();
        if (findText.IsEmpty()) {
            m_statusLabel->SetLabel(wxEmptyString);
            return;
        }
        const long findByteLen = static_cast<long>(findText.ToUTF8().length());
        if (findByteLen <= 0) {
            m_statusLabel->SetLabel(wxEmptyString);
            return;
        }
        const int flags = GetSearchFlags();
        const long docLen = m_stc->GetLength();
        const long selStart = m_stc->GetSelectionStart();
        const long selEnd = m_stc->GetSelectionEnd();

        int total = 0;
        int currentIdx = 0;
        long pos = 0;
        constexpr int kMaxCount = 5000;
        while (pos < docLen) {
            long found = m_stc->FindText(pos, docLen, findText, flags);
            if (found == -1) break;
            ++total;
            if (found == selStart && (found + findByteLen) == selEnd) {
                currentIdx = total;
            }
            pos = found + findByteLen;
            if (total >= kMaxCount) break;
        }

        if (total == 0) {
            m_statusLabel->SetLabel(L"无匹配");
        } else if (total >= kMaxCount) {
            m_statusLabel->SetLabel(currentIdx > 0 ? wxString::Format(L"%d/%d+", currentIdx, kMaxCount) : wxString::Format(L"%d+ 处", kMaxCount));
        } else if (currentIdx > 0) {
            m_statusLabel->SetLabel(wxString::Format(L"%d/%d", currentIdx, total));
        } else {
            m_statusLabel->SetLabel(wxString::Format(L"共 %d 处", total));
        }
    }

    void FindNext(bool forward) {
        if (!m_stc) return;
        wxString findText = m_findCtrl->GetValue();
        if (findText.IsEmpty()) {
            m_statusLabel->SetLabel(wxEmptyString);
            return;
        }
        const long findByteLen = static_cast<long>(findText.ToUTF8().length());
        if (findByteLen <= 0) return;
        const int flags = GetSearchFlags();
        const long docLen = m_stc->GetLength();
        long foundPos = -1;

        if (forward) {
            long startPos = m_stc->GetSelectionEnd();
            foundPos = m_stc->FindText(startPos, docLen, findText, flags);
            if (foundPos == -1) {
                foundPos = m_stc->FindText(0, startPos, findText, flags);
            }
        } else {
            long startPos = m_stc->GetSelectionStart();
            if (startPos > 0) {
                foundPos = m_stc->FindText(startPos, 0, findText, flags);
            }
            if (foundPos == -1) {
                foundPos = m_stc->FindText(docLen, startPos, findText, flags);
            }
        }

        if (foundPos != -1) {
            m_stc->SetSelection(foundPos, foundPos + findByteLen);
            m_stc->EnsureCaretVisible();
        }
        UpdateMatchCount();
    }

    void ReplaceCurrent() {
        if (!m_stc) return;
        if (m_stc->GetReadOnly()) {
            m_statusLabel->SetLabel(L"只读模式");
            return;
        }
        wxString findText = m_findCtrl->GetValue();
        if (findText.IsEmpty()) return;
        wxString replaceText = m_replaceCtrl->GetValue();
        bool caseSensitive = m_caseCheck && m_caseCheck->GetValue();

        // 根据区分大小写状态准确比对当前选区（修复忽略大小写时无法替换的 Bug）
        wxString selected = m_stc->GetSelectedText();
        if (selected.IsSameAs(findText, caseSensitive)) {
            m_stc->ReplaceSelection(replaceText);
        }
        FindNext(true);
    }

    void ReplaceAll() {
        if (!m_stc) return;
        if (m_stc->GetReadOnly()) {
            m_statusLabel->SetLabel(L"只读模式");
            return;
        }
        wxString findText = m_findCtrl->GetValue();
        if (findText.IsEmpty()) return;
        wxString replaceText = m_replaceCtrl->GetValue();

        const long findByteLen = static_cast<long>(findText.ToUTF8().length());
        const long replaceByteLen = static_cast<long>(replaceText.ToUTF8().length());
        if (findByteLen <= 0) return;

        const int flags = GetSearchFlags();
        int replaceCount = 0;

        m_stc->BeginUndoAction();
        long currentPos = 0;
        long endPos = m_stc->GetLength();

        while (currentPos < endPos) {
            long foundPos = m_stc->FindText(currentPos, endPos, findText, flags);
            if (foundPos == -1) break;

            m_stc->SetSelection(foundPos, foundPos + findByteLen);
            m_stc->ReplaceSelection(replaceText);

            // 严格使用 UTF-8 字节长度推进游标，彻底杜绝中文替换死循环
            currentPos = foundPos + replaceByteLen;
            endPos = m_stc->GetLength();
            ++replaceCount;
        }
        m_stc->EndUndoAction();

        m_statusLabel->SetLabel(wxString::Format(L"已替换 %d 处", replaceCount));
    }

    MySTC* m_stc = nullptr;
    wxStaticText* m_findLabel = nullptr;
    wxStaticText* m_replaceLabel = nullptr;
    wxStaticText* m_statusLabel = nullptr;
    wxTextCtrl* m_findCtrl = nullptr;
    wxTextCtrl* m_replaceCtrl = nullptr;
    wxButton* m_prevBtn = nullptr;
    wxButton* m_nextBtn = nullptr;
    wxButton* m_replaceBtn = nullptr;
    wxButton* m_replaceAllBtn = nullptr;
    wxButton* m_closeBtn = nullptr;
    wxCheckBox* m_caseCheck = nullptr;
    wxCheckBox* m_wholeCheck = nullptr;
};


void MainFrame::OnClipboardUpdate() {
    if (m_simplebook->GetSelection() != ID_ScintillaPage)return;
    HWND activeHwnd = ::GetForegroundWindow();
    if (activeHwnd && activeHwnd == m_hMainFrame)return;

    wxString windowTitle;
    if (activeHwnd) {
        wchar_t wTitle[256];
        if (::GetWindowTextW(activeHwnd, wTitle, 256) > 0) {
            windowTitle = wxString(wTitle);
        }
    }
    wxString lowerTitle = windowTitle.Lower();

    for (const wxString& bannedWord : *m_ClipboardBlackList) {
        if (lowerTitle.Contains(bannedWord.Lower())) {
            return;
        }
    }

    wxString textContent;
    ReadClipboardUnicodeText(m_hMainFrame, textContent);
    if (textContent.IsEmpty())return;
    wxString CurrentClipboardInfo;

    if (m_ClipboardGetMoreInfo) {
        wxString currentTime = wxDateTime::Now().Format("%Y-%m-%d %H:%M:%S");
        CurrentClipboardInfo = "\r\n" + windowTitle + "\t\t" + currentTime + "\r\n" + textContent;
    } else {
        CurrentClipboardInfo = "\r\n" + textContent;
    }
    m_scintilla->AppendText(CurrentClipboardInfo);
}

void MainFrame::CreatMenuBar() {
    // 文件菜单
    wxMenu* fileMenu = new wxMenu;
    fileMenu->Append(MENU_NEW_FOLDER, L"新建文件夹(&F)\tCtrl-Shift-N");
    fileMenu->Append(MENU_NEW_NOTE, L"新建笔记(&N)\tCtrl-N");
    fileMenu->Append(MENU_NEW_NOTE_FROM_CLIPBOARD, L"从剪贴板新建笔记(&C)\tCtrl-Alt-N", L"将剪贴板中的文本直接存为新笔记");
    fileMenu->Append(MENU_SAVE_NOTE, L"保存(&S)\tCtrl-S");
    fileMenu->AppendSeparator();
    fileMenu->Append(MENU_RENAME_ITEM, L"重命名(&R)\tF2");
    // fileMenu->Append(MENU_PROPERTIES, L"属性\tAlt-Enter");
    fileMenu->AppendSeparator();
    fileMenu->Append(MENU_MOVE_TO_RECYCLE_BIN, L"移动到回收站(&D)");
    fileMenu->Append(MENU_EXPORT_ITEM, L"导出(&E)...", L"导出当前文件夹或笔记");
    fileMenu->Append(MENU_IMPORT_FOLDER, L"导入文件夹(&I)...");
    fileMenu->Append(MENU_IMPORT_NOTE, L"导入笔记(&M)...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_Restart, L"重启(&T)");
    fileMenu->Append(ID_Quit, L"退出(&X)\tAlt-F4");

    // 编辑菜单
    wxMenu* editMenu = new wxMenu;
    editMenu->Append(MENU_UNDO, L"撤销(&U)\tCtrl-Z");
    editMenu->Append(MENU_REDO, L"恢复(&R)\tCtrl-Y");
    editMenu->Append(MENU_RESTORE, L"还原初始内容(&A)", L"还原当前笔记的初始内容");
    editMenu->AppendSeparator();
    editMenu->Append(MENU_SELECTALL, L"全选(&S)\tCtrl-A");
    editMenu->Append(MENU_CUT, L"剪切(&T)\tCtrl-X");
    editMenu->Append(MENU_COPY, L"复制(&C)\tCtrl-C");
    editMenu->Append(MENU_PASTE, L"粘贴(&P)\tCtrl-V");
    editMenu->Append(MENU_COPY_ALL, L"复制全文(&W)\tCtrl-Shift-C", L"一键复制当前笔记的全部内容");
    editMenu->Append(MENU_REPLACE_PASTE, L"覆盖粘贴(&O)\tCtrl-Shift-V", L"清空当前笔记并替换为剪贴板内容");
    editMenu->Append(MENU_COPY_AS_TXT, L"复制为 TXT 文件(&K)\tCtrl-Shift-T", L"将当前笔记生成为临时 .txt 文件并复制到剪贴板");
    editMenu->Append(MENU_COPY_AS_MD, L"复制为 MD 文件(&M)\tCtrl-Shift-M", L"将当前笔记生成为临时 .md 文件并复制到剪贴板");
    editMenu->AppendSeparator();
    wxMenu* encodingMenu = new wxMenu;
    // --- Unicode 系列 ---
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_UTF8, L"UTF-8");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_UTF16LE, L"UTF-16 LE");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_UTF16BE, L"UTF-16 BE");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_UTF32LE, L"UTF-32 LE");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_UTF32BE, L"UTF-32 BE");

    // --- 中日韩 (CJK) 系列 ---
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP936, L"GBK / Windows-936");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP950, L"Big5 / Windows-950");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP932, L"Shift-JIS / Windows-932");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_EUC_JP, L"EUC-JP");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP949, L"EUC-KR / Windows-949");

    // --- Windows (CP) 系列 ---
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1250, L"Windows-1250 (Central European)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1251, L"Windows-1251 (Cyrillic)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1252, L"Windows-1252 (Western)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1253, L"Windows-1253 (Greek)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1254, L"Windows-1254 (Turkish)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1255, L"Windows-1255 (Hebrew)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1256, L"Windows-1256 (Arabic)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1257, L"Windows-1257 (Baltic)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP1258, L"Windows-1258 (Vietnamese)");

    // --- ISO-8859 系列 ---
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_1, L"ISO-8859-1 (Latin-1)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_2, L"ISO-8859-2 (Latin-2)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_5, L"ISO-8859-5 (Cyrillic)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_7, L"ISO-8859-7 (Greek)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_9, L"ISO-8859-9 (Turkish)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_11, L"ISO-8859-11 (Thai)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_ISO8859_15, L"ISO-8859-15 (Latin-9)");

    // --- 其他常用 ---
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP437, L"IBM-CP437 (OEM United States)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_CP850, L"IBM-CP850 (OEM Multilingual Latin-1)");
    encodingMenu->Append(MENU_ENCODING_BASE + wxFONTENCODING_KOI8, L"KOI8-R (Russian)");

    wxMenuItem* encodingMenuItem = new wxMenuItem(editMenu, MENU_CHANGE_ENCODING, L"修复编码(&A)", L"如果导入文本乱码，并且原始编码已知", wxITEM_NORMAL, encodingMenu);
    editMenu->Append(encodingMenuItem);

    editMenu->AppendSeparator();
    editMenu->Append(MENU_APPEND_CLIPBOARD, L"监听剪贴板(&L)", L"剪贴板内容将被自动追加到当前笔记中", wxITEM_CHECK)->Check(m_EnableClipboardToSTC);
    editMenu->Append(MENU_SEARCH_REPLACE, L"查找和替换...(&F)\tCtrl-F");
    editMenu->Append(MENU_FIND_SQL, L"查找所有...(&E)\tCtrl-Shift-F", L"在所有笔记中查找");
    editMenu->AppendSeparator();
    wxMenu* lineMenu = new wxMenu;
    lineMenu->Append(MENU_LINE_UP, L"上移当前行(&M)\tCtrl-Shift-Up");
    lineMenu->Append(MENU_LINE_DOWN, L"下移当前行(&O)\tCtrl-Shift-Down");
    lineMenu->AppendSeparator();
    lineMenu->Append(MENU_REMOVE_EMPTYLINE, L"删除空行(&D)");
    lineMenu->Append(MENU_COLLAPSE_EMPTYLINE, L"合并连续空行(&K)");
    lineMenu->Append(MENU_TRIM_LINES, L"去除每行首尾空白(&T)");
    lineMenu->Append(MENU_JOIN_LINES, L"合并为单行(&J)");
    lineMenu->AppendSeparator();
    lineMenu->Append(MENU_LINE_DUPLICATE, L"复制当前行(&C)\tCtrl-D");
    lineMenu->Append(MENU_LINE_DELETE, L"删除当前行(&E)\tCtrl-L");
    wxMenuItem* lineMneuItem = new wxMenuItem(editMenu, MENU_LINE, L"行操作(&I)", "", wxITEM_NORMAL, lineMenu);
    editMenu->Append(lineMneuItem);
    // 插入子菜单
    wxMenu* insertMenu = new wxMenu;
    insertMenu->Append(MENU_INSERT_DATE_TIME1, L"日期和时间(短格式)(&D)", L"如：20/5/04 0:14");
    insertMenu->Append(MENU_INSERT_DATE_TIME2, L"日期和时间(长格式)(&A)\tCtrl-F5", L"如：2020-5-04 0:14");
    insertMenu->Append(MENU_INSERT_DATE_TIME3, L"日期和时间(中文日期)(&T)\tCtrl-Shift-F5", L"如：2020年5月04日 0:14");
    insertMenu->AppendSeparator();
    insertMenu->Append(wxID_ANY, L"插入序号", L"在列编辑模式下插入序号")->Enable(false);
    insertMenu->Append(MENU_INSERT_SERIALS, L"1.2.3.");
    insertMenu->Append(MENU_INSERT_SERIALS_1, L"一.二.三.");
    insertMenu->Append(MENU_INSERT_SERIALS_2, L"①②③");
    insertMenu->Append(MENU_INSERT_SERIALS_3, L"I.II.III.");
    insertMenu->Append(MENU_INSERT_SERIALS_4, L"a.b.c.");
    //insertMenu->Append(MENU_INSERT_SYMBOL, L"符号");

    wxMenuItem* insertMenuItem = new wxMenuItem(editMenu, MENU_INSERT, L"插入(&N)", "", wxITEM_NORMAL, insertMenu);
    editMenu->AppendSeparator();
    editMenu->Append(MENU_EDIT_TAGS, L"编辑标签(&D)...", L"查看和编辑标签");
    editMenu->Append(insertMenuItem);
    editMenu->AppendSeparator();

    wxMenu* styleMenu = new wxMenu;
    styleMenu->Append(MENU_SET_STYLE_DEFAULT, L"清除格式(&C)\tCtrl-`", L"恢复默认格式");
    styleMenu->Append(MENU_SET_STYLE_TITLE, L"标题(&T)\tCtrl-1", L"");
    styleMenu->Append(MENU_SET_STYLE_LEVEL1, L"大纲级别一(&O)\tCtrl-2", L"");
    styleMenu->Append(MENU_SET_STYLE_LEVEL2, L"大纲级别二(&U)\tCtrl-3", L"");
    styleMenu->Append(MENU_SET_STYLE_BOLD, L"粗体(&B)\tCtrl-4", L"");
    styleMenu->Append(MENU_SET_STYLE_ITALICGREY, L"不醒目标记(&N)\tCtrl-5", L"灰色斜体");
    styleMenu->Append(MENU_SET_STYLE_RED, L"红色(&R)\tCtrl-6", L"");
    styleMenu->Append(MENU_SET_STYLE_BLUE, L"蓝色(&L)\tCtrl-7", L"");
    styleMenu->Append(MENU_SET_STYLE_YELLOW_BG, L"黄色背景(&Y)\tCtrl-8", L"");
    styleMenu->Append(MENU_SET_STYLE_PINK_BG, L"洋红色背景(&P)\tCtrl-9", L"");
    styleMenu->Append(MENU_SET_STYLE_LIGHTBLUE_BG, L"浅蓝色背景(&S)\tCtrl-0", L"");
    wxMenuItem* styleMenuItem = new wxMenuItem(editMenu, MENU_SET_STYLE, L"样式(&Y)", "", wxITEM_NORMAL, styleMenu);
    editMenu->Append(styleMenuItem);
    // 导航菜单
    wxMenu* navMenu = new wxMenu;
    navMenu->Append(MENU_TOGGLE_TREE, L"显示目录树(&B)\tCtrl-B", L"折叠或展开左侧目录树面板", wxITEM_CHECK)->Check(m_treeVisible);
    navMenu->Append(MENU_PREV_VIEW, L"上一视图(&L)\tAlt-Left");
    navMenu->Append(MENU_EXPAND_ALL, L"展开所有(&E)\tCtrl-Shift-E");
    navMenu->Append(MENU_COLLAPSE_ALL, L"折叠所有(&C)\tCtrl-Alt-C");
    navMenu->AppendSeparator();

    // 快速切换功能
    wxMenu* quickSwitchMenu = new wxMenu;
    wxMenu* deleteHotkeyMenu = new wxMenu;
    wxMenu* assignHotkeyMenu = new wxMenu;
    for (int i = 0; i < 9; ++i) {
        quickSwitchMenu->Append(MENU_QUICK_SWITCH_BASE + i, wxString::Format(L"快速切换 %d\tAlt-%d", i + 1, i + 1));
        deleteHotkeyMenu->Append(MENU_DELETE_HOTKEY_BASE + i, wxString::Format(L"删除热键 %d", i + 1));
        assignHotkeyMenu->Append(MENU_ASSIGN_HOTKEY_BASE + i, wxString::Format(L" %d", i + 1));
        //绑定菜单项点击事件的逻辑，无需判断节点或数组值有效性
        //有效性在OnUpdateQuickSwitchMenu事件中判断
        Bind(wxEVT_MENU, [this, i](wxCommandEvent&) {this->m_tree->SelectItem(m_tree->m_idToItem[this->m_quickSwitchMenuIds[i]]); }, MENU_QUICK_SWITCH_BASE + i);
        Bind(wxEVT_MENU, [this, i](wxCommandEvent&) {
            wxTreeItemId selectedItem = this->m_tree->GetSelection();
            if (selectedItem.IsOk()) {
                MyTreeItemData* data = static_cast<MyTreeItemData*>(this->m_tree->GetItemData(selectedItem));
                if (data && data->GetID() > 0) {
                    this->m_quickSwitchMenuIds[i] = data->GetID();
                }
            }
            }, MENU_ASSIGN_HOTKEY_BASE + i);
        Bind(wxEVT_MENU, [this, i](wxCommandEvent&) {this->m_quickSwitchMenuIds[i] = 0; }, MENU_DELETE_HOTKEY_BASE + i);
    }
    wxMenuItem* quckSwitchItem = new wxMenuItem(navMenu, MENU_QUICK_SWITCH, L"快速切换(&F)", L"快速切换到已分配热键的节点", wxITEM_NORMAL, quickSwitchMenu);
    wxMenuItem* assignHotkeyItem = new wxMenuItem(navMenu, MENU_ASSIGN_HOTKEY, L"分配热键(&A)", L"为当前所选节点分配一个热键", wxITEM_NORMAL, assignHotkeyMenu);
    wxMenuItem* deleteHotkeyItem = new wxMenuItem(navMenu, MENU_DELETE_HOTKEY, L"删除热键(&D)", L"删除已设置的热键", wxITEM_NORMAL, deleteHotkeyMenu);
    navMenu->Append(quckSwitchItem);
    navMenu->Append(assignHotkeyItem);
    navMenu->Append(deleteHotkeyItem);
    navMenu->AppendSeparator();
    navMenu->Append(MENU_SHOW_TAGS, L"显示标签(&N)", "", wxITEM_CHECK)->Check(m_showTags);
    navMenu->Append(MENU_SHOW_RECENT, L"显示最近(&R)", "", wxITEM_CHECK)->Check(m_showRecent);
    navMenu->AppendSeparator();
    navMenu->Append(MENU_EMPTY_RECYCLE_BIN, L"清空回收站(&M)");

    // 选项菜单
    wxMenu* toolsMenu = new wxMenu;
    toolsMenu->Append(MENU_AUTO_LINEBREAK, L"自动换行(&A)", "", wxITEM_CHECK)->Check(m_autoLineBreak);
    toolsMenu->Append(MENU_SHOW_MATCHES_INDICATOR, L"高亮选中匹配(&H)", L"在编辑器中高亮与选中匹配的文本，禁用此功能可以提高性能", wxITEM_CHECK)->Check(m_showMatchesIndicator);
    wxMenu* symbolMenu = new wxMenu;
    symbolMenu->Append(MENU_SHOW_WHITESPACE, L"显示空格和制表符(&S)", "", wxITEM_CHECK)->Check(false);
    symbolMenu->Append(MENU_SHOW_EOL, L"显示换行符(&H)", "", wxITEM_CHECK)->Check(false);
    wxMenuItem* symbolMenuItem = new wxMenuItem(toolsMenu, MENU_SHOW_SYMBOL, L"显示符号(&S)", "", wxITEM_NORMAL, symbolMenu);
    toolsMenu->Append(symbolMenuItem);

    toolsMenu->AppendSeparator();
    toolsMenu->Append(MENU_ALWAYS_TOP, L"切换总在最前(&P)", "", wxITEM_CHECK)->Check(m_PinToTop);
    toolsMenu->Append(MENU_NET_SERVER, L"启用网络功能(&N)", L"启用网络监听", wxITEM_CHECK)->Check(m_enableNetServer);
    toolsMenu->AppendSeparator();
    toolsMenu->Append(MENU_GLOBAL_SETTINGS, L"全局设置(&G)...");
    toolsMenu->Append(MENU_EDIT_CONFIGFILE, L"编辑配置文件(&E)");
    //toolsMenu->Append(MENU_WINDOW_SNAP, L"窗口吸附", "", wxITEM_CHECK);
    //toolsMenu->Append(MENU_TOOLBAR_DISPLAY, L"工具栏显示", "", wxITEM_CHECK);
    //toolsMenu->AppendSeparator();
    //toolsMenu->Append(MENU_SETTINGS, L"设置...");

    // 帮助菜单
    wxMenu* helpMenu = new wxMenu;
    helpMenu->Append(MENU_HELP_TOPICS, L"帮助主题(&H)\tF1");
    helpMenu->Append(MENU_CHECK_UPDATE, L"检查更新(&U)...");
    helpMenu->Append(ID_About, L"关于(&A)...");

    // 创建菜单栏
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(fileMenu, L"文件(&F)");
    menuBar->Append(editMenu, L"编辑(&E)");
    menuBar->Append(navMenu, L"导航(&N)");
    menuBar->Append(toolsMenu, L"设置(&S)");
    menuBar->Append(helpMenu, L"帮助(&H)");

    SetMenuBar(menuBar);
}

void MainFrame::CreateMyToolBar() {
    wxConfigBase* config = wxConfigBase::Get();
    auto ToolbariconSize = config->ReadLong("Toolbar/IconSize", 32);
    if (ToolbariconSize < 16 || ToolbariconSize>256)ToolbariconSize = 32;
    auto Size_i = wxSize(ToolbariconSize, ToolbariconSize);

    wxToolBar* toolBar = CreateToolBar();
    wxBitmapBundle bundle;
    const char* color = GetMyAppInstance()->GetThemeColor();
    char buff[4096];

    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_toggle_sidebar, color, 73), Size_i);
    toolBar->AddTool(MENU_TOGGLE_TREE, L"显示目录树", bundle, L"折叠/展开左侧目录树 (Ctrl+B)", wxITEM_CHECK);
    toolBar->ToggleTool(MENU_TOGGLE_TREE, m_treeVisible);
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_new_note, color, 73), Size_i);
    toolBar->AddTool(MENU_NEW_NOTE, L"新建笔记", bundle, L"新建笔记");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_new_folder, color, 73), Size_i);
    toolBar->AddTool(MENU_NEW_FOLDER, L"新建文件夹", bundle, L"新建文件夹");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_tag, color, 73), Size_i);
    toolBar->AddTool(MENU_EDIT_TAGS, L"编辑标签...", bundle, L"查看和编辑标签");
    toolBar->AddSeparator();
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_copy_all, color, 73), Size_i);
    toolBar->AddTool(MENU_COPY_ALL, L"复制全文", bundle, L"一键复制当前笔记全文 (Ctrl+Shift+C)");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_replace_paste, color, 73), Size_i);
    toolBar->AddTool(MENU_REPLACE_PASTE, L"覆盖粘贴", bundle, L"清空当前笔记并替换为剪贴板内容 (Ctrl+Shift+V)");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_clipboard_watch, color, 73), Size_i);
    toolBar->AddTool(MENU_APPEND_CLIPBOARD, L"监听剪贴板", bundle, L"自动将剪贴板新内容追加到当前笔记", wxITEM_CHECK);
    toolBar->ToggleTool(MENU_APPEND_CLIPBOARD, m_EnableClipboardToSTC);
    toolBar->AddSeparator();
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style, color, 73), Size_i);
    toolBar->AddTool(TOOLBAR_SET_STYLE, L"设置样式", bundle, L"设置选中文本样式");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_undo, color, 73), Size_i);
    toolBar->AddTool(MENU_UNDO, L"撤销", bundle, L"撤销");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_redo, color, 73), Size_i);
    toolBar->AddTool(MENU_REDO, L"重做", bundle, L"重做");
    //bundle = wxBitmapBundle::FromSVG(icon_restore_note, Size_i);
    //toolBar->AddTool(MENU_RESTORE, L"还原初始内容", bundle, L"还原初始内容");
    toolBar->AddSeparator();
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_last_view, color, 73), Size_i);
    toolBar->AddTool(MENU_PREV_VIEW, L"上一视图", bundle, L"上一视图");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_pin, color, 73), Size_i);
    toolBar->AddTool(MENU_ALWAYS_TOP, L"总在最前", bundle, L"总在最前", wxITEM_CHECK);

    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_search, color, 73), Size_i);
    toolBar->AddTool(MENU_SEARCH_REPLACE, L"查找替换", bundle, L"查找和替换");
    bundle = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_database_search, color, 73), Size_i);
    toolBar->AddTool(MENU_FIND_SQL, L"查找所有", bundle, L"在所有笔记中查找");


    bool hideNavbar = config->ReadBool("Toolbar/HideNavbar", false);
    if (!hideNavbar) {
        toolBar->AddStretchableSpace();
        m_labelNav = new wxStaticText(toolBar, wxID_ANY, L"跳转到: ");
        toolBar->AddControl(m_labelNav);
        m_Choice = new wxChoice(toolBar, ID_Choice, wxDefaultPosition, wxSize(FromDIP(240), -1), wxArrayString());
        toolBar->AddControl(m_Choice);
    }

    toolBar->Realize(); // 确保工具栏被正确创建和显示
}

bool MainFrame::MSWHandleMessage(WXLRESULT* result, WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
    // 跨进程唤醒消息处理
    if (message == WM_RESTORE_FRAME) {
        if (IsIconized()) {
            Iconize(false);
        }
        if (!IsShown()) {
            Show(true);
        }
        Raise();
        if (m_scintilla && m_simplebook && m_simplebook->GetSelection() == ID_ScintillaPage) {
            m_scintilla->SetFocus();
        }
        *result = 0;
        return true;
    }

    // 剪贴板监听消息 (如果 m_EnableClipboardToSTC 为真)
    if (message == WM_CLIPBOARDUPDATE && m_EnableClipboardToSTC) {
        OnClipboardUpdate();
        *result = 0;
        return true;
    }
    if (message == WM_HOTKEY) {
        int hotkeyId = (int)wParam;
        if (hotkeyId == HOTKEY_ID_GLOBAL_FUNC) {
            // 全局快捷键消息
            if (IsIconized() || !IsShown()) {
                if (!IsShown()) Show(true);
                if (IsIconized()) Iconize(false);
                Raise();
                if (m_scintilla && m_simplebook && m_simplebook->GetSelection() == ID_ScintillaPage) {
                    m_scintilla->SetFocus();
                }
            } else {
                if (m_minimizeToTray) {
                    Hide();
                } else {
                    Iconize(true);
                }
            }
            *result = 0;
            return true;
        }
    }

    return wxFrame::MSWHandleMessage(result, message, wParam, lParam);
}

void MainFrame::RegisterGlobalHotkey() {
    wxFileConfig config{ wxEmptyString,wxEmptyString,GetMyAppInstance()->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };
    wxString hotkeyString;
    config.Read("MainWindow/GlobalHotkey", &hotkeyString, L"Ctrl+Shift+F1");
    if (hotkeyString.IsEmpty() || hotkeyString.CmpNoCase(L"None") == 0 || hotkeyString == L"无") {
        return;
    }

    wxAcceleratorEntry entry;
    bool parseSuccess = entry.FromString(hotkeyString);
    if (parseSuccess && entry.GetKeyCode() != 0) {
        // 解析成功，获取 wxWidgets 值
        int wxFlags = entry.GetFlags();
        int wxKey = entry.GetKeyCode();

        //转换为 WinAPI 值
        int winMods = ConvertWxFlagsToWinMods(wxFlags);
        int winVK = ConvertWxKeyToWinVK(wxKey);

        if (winVK != 0) {
            // 调用 Windows API 注册快捷键
            if (!::RegisterHotKey((HWND)GetHWND(), HOTKEY_ID_GLOBAL_FUNC, winMods, winVK)) {
                // 注册失败，弹窗提示
                wxMessageBox(
                    wxString::Format(L"全局快捷键 [%s] 注册失败！可能已被其他程序占用。", hotkeyString),
                    L"快捷键注册错误",
                    wxOK | wxICON_ERROR,
                    this
                );
            } else {
                wxLogDebug(L"全局快捷键注册成功: %s", hotkeyString);
            }
        } else {
            // 键码转换失败
            wxMessageBox(
                wxString::Format(L"快捷键 [%s] 中的主键无法识别或不支持。", hotkeyString),
                L"快捷键解析错误",
                wxOK | wxICON_WARNING,
                this
            );
        }
    } else {
        // 字符串解析失败或没有找到主键
        wxMessageBox(
            wxString::Format(L"配置文件中的快捷键字符串 [%s] 格式错误。", hotkeyString),
            L"快捷键解析错误",
            wxOK | wxICON_WARNING,
            this
        );
    }
}

void MainFrame::UnregisterGlobalHotKey() {
    ::UnregisterHotKey((HWND)GetHWND(), HOTKEY_ID_GLOBAL_FUNC);
}

MainFrame::MainFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title) {
#if (defined(_DEBUG) || defined(DEBUG)) && !defined(NDEBUG)
    wxLogWindow* logWindow = new wxLogWindow(this, "Log", true, false);
    wxLog::SetActiveTarget(logWindow);
#endif

    wxConfigBase* config = wxConfigBase::Get();
    m_closeToTray = config->ReadBool("MainWindow/CloseToTray", true);
    m_minimizeToTray = config->ReadBool("MainWindow/MinimizeToTray", true);
    m_showRecent = config->ReadBool("Tree/ShowRecent", true);
    m_showTags = config->ReadBool("Tree/ShowTags", true);
    m_ClipboardGetMoreInfo = config->ReadBool("Scintilla/ClipboardGetMoreInfo", false);
    m_autoLineBreak = config->ReadBool("Scintilla/AutoLineBreak", true);
    m_showMatchesIndicator = config->ReadBool("Scintilla/ShowMatchesIndicator", true);
    m_recentMaxCount = config->ReadLong("Tree/RecentMaxCount", 10);
    if (m_recentMaxCount < 5)m_recentMaxCount = 5;
    m_autoSaveDelayMs = NormalizeAutoSaveDelayMs(config->ReadLong("Scintilla/AutoSaveDelayMs", 1000));
    for (int i = 0; i < 9; ++i) {
        m_quickSwitchMenuIds[i] = config->ReadLong("Shortcuts/Shortcut" + std::to_string(i + 1), 0);
    }
    //剪贴板监听黑名单
    SetClipboardBlackList();

    wxIcon appIcon;
    if (appIcon.LoadFile(wxT("MAINICON"))) {
        SetIcon(appIcon);
    }
    // 设置窗口大小和位置
    int x = config->ReadLong("MainWindow/X", 100);
    int y = config->ReadLong("MainWindow/Y", 100);
    int width = config->ReadLong("MainWindow/Width", 800);
    int height = config->ReadLong("MainWindow/Height", 600);
    SetSize(x, y, width, height);

    InitUI();
    InitBind();
    //在这里设置tree节点的选择
    m_tree->SelectItem(m_tree->GetCurrentItem());

    m_autoSaveTimer.SetOwner(this, ID_TIMER_AUTOSAVE);
    m_hMainFrame = (HWND)this->GetHandle();
    RegisterGlobalHotkey();
    //快捷键
    wxAcceleratorEntry entry[3];
    entry[0].Set(wxACCEL_NORMAL, WXK_F10, ID_HOTKEY_SETNAME);
    entry[1].Set(wxACCEL_NORMAL, WXK_F3, ID_HOTKEY_FOCUSTREE);
    entry[2].Set(wxACCEL_NORMAL, WXK_F4, ID_HOTKEY_FOCUSEDITOR);
    wxAcceleratorTable accel(3, entry);
    SetAcceleratorTable(accel);
    m_server = new MongooseServer(this);
    ApplyTheme(GetMyAppInstance()->GetDarkMode());
    CleanupClipboardTempFiles(3, 600);

    /*std::string html = "<html><body><h1>wxNotepad2 Server Running</h1></body></html>";
    if (m_server->Start(8080, html)){
        wxLogMessage("服务器已在 8080 端口启动");
    }*/
}

MainFrame::~MainFrame() {
    m_autoSaveTimer.Stop();
    UnregisterGlobalHotKey();
    // 移除监听器
    EnableClipboardListener(false);
    if (m_server) {
        delete m_server;
        m_server = nullptr;
    }
    delete m_trayIcon;
    delete m_ClipboardBlackList;
    delete m_stylePopup;
}

// 初始化UI元素
void MainFrame::InitUI() {
    wxConfigBase* config = wxConfigBase::Get();

    // --- 拆分窗口 ---
    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    m_splitter->SetMinimumPaneSize(FromDIP(40)); // 设置最小窗格大小

    // 左侧：树形控件
    m_tree = new MyTreeCtrl(m_splitter, this, ID_TreeCtrl, wxDefaultPosition, wxDefaultSize,
        wxTR_HIDE_ROOT | wxTR_EDIT_LABELS | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_TWIST_BUTTONS);
    m_tree->InitFunctions();
    // 右侧：Simplebook
    m_simplebook = new wxSimplebook(m_splitter, ID_Notebook);
    m_editorPage = new wxPanel(m_simplebook, wxID_ANY);
    m_scintilla = new MySTC(m_editorPage, this, ID_Scintilla);
    m_findBar = new FindReplaceBar(m_editorPage, m_scintilla);
    m_findBar->Hide();
    wxBoxSizer* editorSizer = new wxBoxSizer(wxVERTICAL);
    editorSizer->Add(m_findBar, 0, wxEXPAND);
    editorSizer->Add(m_scintilla, 1, wxEXPAND);
    m_editorPage->SetSizer(editorSizer);

    m_cardViewPanel = new MyCardViewPanel(m_simplebook, ID_CardViewPanel);
    m_simplebook->AddPage(m_cardViewPanel, L"列表视图");
    m_simplebook->AddPage(m_editorPage, L"笔记编辑");

    // 设置拆分窗口的左右视图
    int width_splitter = config->ReadLong("MainWindow/Width_Splitter", 400);
    if (width_splitter < FromDIP(40)) width_splitter = FromDIP(240);
    m_savedSplitterPos = width_splitter;
    m_splitter->SplitVertically(m_tree, m_simplebook, width_splitter);

    // 设置主窗口的布局为拆分窗口
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_splitter, 1, wxEXPAND | wxALL, 0);
    SetSizer(sizer);
    sizer->Layout();

    CreatMenuBar();
    CreateMyToolBar();
    //m_statusBar = new MyStatBar(this);
    m_statusBar = new wxStatusBar(this);
    m_statusBar->SetFieldsCount(3);
    int widths[3] = { -2, -1,-1 };
    m_statusBar->SetStatusWidths(3, widths);
    SetStatusBar(m_statusBar);
    m_trayIcon = new MyTrayIcon(this);

}

void MainFrame::InitBind() {
    //控件事件
    Bind(EVT_SHOW_NODE_CONTENT, &MainFrame::OnShowNoteContent, this);
    Bind(EVT_CARD_LEFT_DCLICK, &MyTreeCtrl::OnMyCardViewPanelDClick, m_tree);
    m_tree->InitBind();
    Bind(EVT_UPDATE_NOTE_TAGS, &MyTreeCtrl::OnChangeNoteTags, m_tree);
    Bind(wxEVT_SYS_COLOUR_CHANGED, &MainFrame::OnSysColorChange2, this);
    //工具栏控件事件
    if (m_Choice) {
        m_Choice->Bind(wxEVT_ENTER_WINDOW, &MainFrame::OnFocusChoice, this);
        m_Choice->Bind(wxEVT_CHOICE, &MainFrame::OnOutlineSelect, this);
    }
    Bind(wxEVT_TOOL, &MainFrame::OnToolbarStyleButtonClicked, this, TOOLBAR_SET_STYLE);
    //窗口事件
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_ICONIZE, [this](wxIconizeEvent&) {if (IsIconized() && m_minimizeToTray) { if (m_tree) { m_tree->CommitPendingLabelEdit(); } FlushAutoSave(); Hide(); }; });
    Bind(wxEVT_ACTIVATE, &MainFrame::OnActivate, this);
    Bind(wxEVT_TIMER, &MainFrame::OnAutoSaveTimer, this, ID_TIMER_AUTOSAVE);
    Bind(wxEVT_QUERY_END_SESSION, [this](wxCloseEvent&) {OnExit(); });
    // 检查更新回调（只绑定一次，避免重复弹窗）
    Bind(wxEVT_WEBREQUEST_STATE, &MainFrame::OnCheckUpdateResponse, this);

    //文件菜单
    Bind(wxEVT_MENU, &MyTreeCtrl::OnNewFolder, m_tree, MENU_NEW_FOLDER);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnNewNote, m_tree, MENU_NEW_NOTE);
    Bind(wxEVT_MENU, &MainFrame::OnNewNoteFromClipboard, this, MENU_NEW_NOTE_FROM_CLIPBOARD);
    Bind(wxEVT_MENU, &MainFrame::OnSaveNote, this, MENU_SAVE_NOTE);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnRenameItem, m_tree, MENU_RENAME_ITEM);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnExportItem, m_tree, MENU_EXPORT_ITEM);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnImportFolder, m_tree, MENU_IMPORT_FOLDER);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnImportNote, m_tree, MENU_IMPORT_NOTE);
    //Bind(wxEVT_MENU, &MyTreeCtrl::OnProperties, this, MENU_PROPERTIES);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnRemoveToTrash, m_tree, MENU_MOVE_TO_RECYCLE_BIN);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {OnRestart(); }, ID_Restart);
    Bind(wxEVT_MENU, &MainFrame::OnQuit, this, ID_Quit);

    //编辑菜单
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {if (!m_scintilla->CanUndo()) return; m_scintilla->Undo(); }, MENU_UNDO);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {if (!m_scintilla->CanRedo()) return; m_scintilla->Redo(); }, MENU_REDO);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_tree->RestoreNoteContent(); }, MENU_RESTORE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SelectAll(); }, MENU_SELECTALL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {if (!m_scintilla->CanCut()) { m_scintilla->LineCut(); return; } m_scintilla->Cut(); }, MENU_CUT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->CopyAllowLine(); }, MENU_COPY);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {if (!m_scintilla->CanPaste()) return; m_scintilla->Paste(); }, MENU_PASTE);
    Bind(wxEVT_MENU, &MainFrame::OnCopyAll, this, MENU_COPY_ALL);
    Bind(wxEVT_MENU, &MainFrame::OnReplacePaste, this, MENU_REPLACE_PASTE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnCopyAsFile(L"txt"); }, MENU_COPY_AS_TXT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { OnCopyAsFile(L"md"); }, MENU_COPY_AS_MD);
    Bind(wxEVT_MENU, &MainFrame::OnRepairEncoding, this, MENU_ENCODING_BASE, MENU_ENCODING_TOP);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {EnableClipboardListener(!m_EnableClipboardToSTC); }, MENU_APPEND_CLIPBOARD);
    Bind(wxEVT_MENU, &MainFrame::OnSearchAndReplace, this, MENU_SEARCH_REPLACE);
    Bind(wxEVT_MENU, &MainFrame::OnFindSql, this, MENU_FIND_SQL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->RemoveEmptyLines(); }, MENU_REMOVE_EMPTYLINE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->CollapseEmptyLines(); }, MENU_COLLAPSE_EMPTYLINE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->TrimLines(); }, MENU_TRIM_LINES);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->JoinLines(); }, MENU_JOIN_LINES);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SwitchLine(true); }, MENU_LINE_UP);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SwitchLine(false); }, MENU_LINE_DOWN);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->LineDuplicate(); }, MENU_LINE_DUPLICATE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->LineDelete(); }, MENU_LINE_DELETE);
    Bind(wxEVT_MENU, [this](wxCommandEvent& event) {m_scintilla->InsertDateTime(event.GetId() - MENU_INSERT_DATE_TIME1 + 1); }, MENU_INSERT_DATE_TIME1, MENU_INSERT_DATE_TIME3);
    Bind(wxEVT_MENU, [this](wxCommandEvent& event) {
        wxArrayString serials;
        switch (event.GetId()) {
        case MENU_INSERT_SERIALS:
            serials = { L"1.",L"2.",L"3.",L"4.",L"5.",L"6.",L"7.",L"8.",L"9.",L"10.",L"11.",L"12.",L"13.",L"14.",L"15.",L"16.",L"17.",L"18.",L"19.",L"20." }; break;
        case MENU_INSERT_SERIALS_1:
            serials = { L"一.",L"二.",L"三.",L"四.",L"五.",L"六.",L"七.",L"八.",L"九.",L"十.",L"十一.",L"十二.",L"十三.",L"十四.",L"十五.",L"十六.",L"十七.",L"十八.",L"十九.",L"二十." }; break;
        case MENU_INSERT_SERIALS_2:
            serials = { L"①",L"②",L"③",L"④",L"⑤",L"⑥",L"⑦",L"⑧",L"⑨",L"⑩" }; break;
        case MENU_INSERT_SERIALS_3:
            serials = { L"I.", L"II.", L"III.", L"IV.", L"V.", L"VI.", L"VII.", L"VIII.", L"IX.", L"X.", L"XI.", L"XII.", L"XIII.", L"XIV.", L"XV.", L"XVI.", L"XVIII.", L"XIX.", L"XX." }; break;
        case MENU_INSERT_SERIALS_4:
            serials = { L"a.",L"b.",L"c.",L"d.",L"e.",L"f.",L"g.",L"h.",L"i.",L"j.",L"k.",L"l.",L"m.",L"n.",L"o.",L"p.",L"q.",L"r.",L"s.",L"t.",L"u.",L"v.",L"w.",L"x.",L"y.",L"z." }; break;
        }
        m_scintilla->InsertSerials(serials);
        }, MENU_INSERT_SERIALS, MENU_INSERT_SERIALS_4);
    Bind(wxEVT_MENU, [this](wxCommandEvent& event) {m_scintilla->SetStyleForSelected(event); }, MENU_SET_STYLE_DEFAULT, MENU_SET_STYLE_LEVEL2);
    Bind(wxEVT_MENU, &MainFrame::OnEditTags, this, MENU_EDIT_TAGS);
    //导航菜单
    Bind(wxEVT_MENU, &MainFrame::OnToggleTree, this, MENU_TOGGLE_TREE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {if (m_tree->m_prev)m_tree->SelectItem(m_tree->m_prev); }, MENU_PREV_VIEW);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_tree->ExpandAll(); }, MENU_EXPAND_ALL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_tree->CollapseAll(); }, MENU_COLLAPSE_ALL);
    Bind(wxEVT_MENU, [this](wxCommandEvent& event) {if (m_showTags) { m_tree->OnHideTags(event); } else { m_tree->OnShowTags(event); }}, MENU_SHOW_TAGS);
    Bind(wxEVT_MENU, [this](wxCommandEvent& event) {if (m_showRecent) { m_tree->OnHideRecent(event); } else { m_tree->OnShowRecent(event); }}, MENU_SHOW_RECENT);
    Bind(wxEVT_MENU, &MyTreeCtrl::OnEmptyRecycleBin, m_tree, MENU_EMPTY_RECYCLE_BIN);

    //工具菜单
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {SetWindowStyleFlag(GetWindowStyleFlag() ^ wxSTAY_ON_TOP); m_PinToTop = !m_PinToTop; }, MENU_ALWAYS_TOP);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SetWrapMode(2 * !m_autoLineBreak); m_autoLineBreak = !m_autoLineBreak; }, MENU_AUTO_LINEBREAK);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_showMatchesIndicator = !m_showMatchesIndicator; }, MENU_SHOW_MATCHES_INDICATOR);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SetViewWhiteSpace(m_scintilla->GetViewWhiteSpace() ^ 1); }, MENU_SHOW_WHITESPACE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_scintilla->SetViewEOL(!m_scintilla->GetViewEOL()); }, MENU_SHOW_EOL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        m_enableNetServer = !m_enableNetServer;
        if (m_enableNetServer) {
            StartServer();
        } else {
            m_server->Stop();
        }
        }, MENU_NET_SERVER);
    Bind(wxEVT_MENU, &MainFrame::OnGlobalSettings, this, MENU_GLOBAL_SETTINGS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {auto app = GetMyAppInstance(); wxLaunchDefaultApplication(app->GetConfigFilePath()); }, MENU_EDIT_CONFIGFILE);
    //帮助菜单
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, ID_About);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {wxLaunchDefaultApplication(L"wxNotepad2帮助.md"); }, MENU_HELP_TOPICS);
    Bind(wxEVT_MENU, &MainFrame::OnCheckUpdate, this, MENU_CHECK_UPDATE);
    //UpdateUI事件
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateQuickSwitchMenu, this, MENU_QUICK_SWITCH_BASE, MENU_ASSIGN_HOTKEY_BASE + 50);
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateEditorMenu, this, MENU_UNDO, MENU_SET_STYLE_LEVEL2);//仅笔记节点（不包括回收站的），编辑菜单和sci显示设置
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateItemMenu, this, MENU_RENAME_ITEM, MENU_EXPORT_ITEM);//笔记和文件夹（不包括回收站的）
    Bind(wxEVT_UPDATE_UI, &MainFrame::OnUpdateBoolMenu, this, MENU_SHOW_TAGS, MENU_TOGGLE_TREE);
    //快捷键
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        if (m_simplebook->GetSelection() != ID_ScintillaPage) return;
        wxString name = SanitizeName(m_scintilla->GetSelectedText());
        if (name.IsEmpty()) return;
        m_tree->RenameCurrentItem(name);
        }, ID_HOTKEY_SETNAME);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {m_tree->SetFocus(); }, ID_HOTKEY_FOCUSTREE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { if (m_simplebook->GetSelection() != ID_ScintillaPage) return; m_scintilla->SetFocus(); }, ID_HOTKEY_FOCUSEDITOR);

    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
    //网络请求
    Bind(wxEVT_THREAD, &MainFrame::OnHttpRequest, this, EVT_HTTP_REQUEST);
}

void MainFrame::switchToCardViewPanel() {
    ResetAutoSaveSession();
    if (m_findBar && m_findBar->IsShown()) {
        m_findBar->Hide();
        if (m_editorPage) m_editorPage->Layout();
    }
    m_cardViewPanel->ClearView();
    m_simplebook->SetSelection(ID_CardViewPage);
    wxString tmp = " ";
    SetStatusText(tmp, 1);
    SetStatusText(tmp, 2);
    if (m_Choice)m_Choice->Disable();
}

void MainFrame::switchToScintillaCtrl() {
    m_simplebook->SetSelection(ID_ScintillaPage);
    if (m_Choice)m_Choice->Enable();
}

void MainFrame::OnExit() {
    if (m_tree) {
        m_tree->CommitPendingLabelEdit();
    }
    FlushAutoSave();
    ResetAutoSaveSession();
    //关闭所有对话框
    wxWindowList::iterator iter;
    for (iter = wxTopLevelWindows.begin(); iter != wxTopLevelWindows.end(); ++iter) {
        wxWindow* win = *iter;
        wxDialog* dlg = wxDynamicCast(win, wxDialog);
        if (dlg && dlg != (wxWindow*)this) {
            if (dlg->IsShown()) {
                _wxLogMessage(L"主窗口关闭时强制关闭对话框: %s", dlg->GetTitle());
                dlg->EndModal(wxID_CANCEL);
            }
        }
    }

    //使用临时config，避免覆盖程序运行过程中的对配置的手动修改
    auto App = GetMyAppInstance();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };

    wxPoint pos = GetPosition();
    wxSize size = GetSize();
    config.Write("MainWindow/X", pos.x);
    config.Write("MainWindow/Y", pos.y);
    config.Write("MainWindow/Width", size.x);
    config.Write("MainWindow/Height", size.y);
    int splitterWidth = m_treeVisible ? m_splitter->GetSashPosition() : m_savedSplitterPos;
    config.Write("MainWindow/Width_Splitter", splitterWidth);
    config.Write("Tree/ShowTags", m_showTags);
    config.Write("Tree/ShowRecent", m_showRecent);
    config.Write("Scintilla/AutoLineBreak", m_autoLineBreak);
    config.Write("Scintilla/ShowMatchesIndicator", m_showMatchesIndicator);
    for (int i = 0; i < 9; i++) {
        config.Write("Shortcuts/Shortcut" + std::to_string(i + 1), m_quickSwitchMenuIds[i]);
    }
    config.Flush(); // 确保数据写入文件

    Destroy();
}

void MainFrame::OnRestart() {
    wxString exePath = wxStandardPaths::Get().GetExecutablePath() + GetMyAppInstance()->GetOriginalArgs();
    GetMyAppInstance()->DelSingleInstanceChecker();
    if (wxExecute(exePath)) {
        OnExit();
    } else {
        _wxLogError(L"重启失败");
    }
}

bool MainFrame::SaveNoteContent(int noteID) {
    if (!m_scintilla || noteID == -1) {
        return false;
    }
    const bool contentModified = m_scintilla->GetModify();
    const bool styleModified = m_scintilla->m_styleModified;

    // 若内容和样式均未改动，直接返回，避免无谓的磁盘写事务
    if (!contentModified && !styleModified) {
        if (noteID == m_currentNoteID) {
            m_autoSaveTimer.Stop();
        }
        return true;
    }

    sqlite3* db = GetMyAppInstance()->db;
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    auto bindStyleBlob = [this](sqlite3_stmt* targetStmt, int paramIndex) {
        std::vector<UINT16> styleData;
        m_scintilla->GetStyleData(styleData);
        INT32 elementCount = static_cast<INT32>(styleData.size());
        if (elementCount == 0) {
            sqlite3_bind_null(targetStmt, paramIndex);
            return;
        }
        size_t totalBytes = sizeof(INT32) + styleData.size() * sizeof(UINT16);
        std::vector<unsigned char> buffer(totalBytes);
        memcpy(buffer.data(), &elementCount, sizeof(INT32));
        memcpy(buffer.data() + sizeof(INT32), styleData.data(), elementCount * sizeof(UINT16));
        sqlite3_bind_blob(targetStmt, paramIndex, buffer.data(), static_cast<int>(totalBytes), SQLITE_TRANSIENT);
    };

    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        _wxLogError("SQL begin transaction failed: %s", sqlite3_errmsg(db));
        return false;
    }

    // 检查内容是否被修改
    if (contentModified) {
        // 内容有改动，更新CONTENT、MODIFY_TIME和ACCESS_TIME
        const char* sql_update = "UPDATE NOTES SET CONTENT = ?, STYLEDATA = ?, MODIFY_TIME = CURRENT_TIMESTAMP, ACCESS_TIME = CURRENT_TIMESTAMP WHERE ID = ?;";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, nullptr) != SQLITE_OK) {
            _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        wxString currentContent = m_scintilla->GetValue();
        sqlite3_bind_text(stmt, 1, currentContent.utf8_str(), -1, SQLITE_TRANSIENT);
        bindStyleBlob(stmt, 2);
        sqlite3_bind_int(stmt, 3, noteID);

        success = sqlite3_step(stmt) == SQLITE_DONE;
        if (!success) {
            _wxLogError(L"无法更新笔记内容: %s", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    } else if (styleModified) {
        const char* sql_update = "UPDATE NOTES SET STYLEDATA = ?, MODIFY_TIME = CURRENT_TIMESTAMP, ACCESS_TIME = CURRENT_TIMESTAMP WHERE ID = ?;";
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt, nullptr) != SQLITE_OK) {
            _wxLogError("SQL prepare failed: %s", sqlite3_errmsg(db));
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        bindStyleBlob(stmt, 1);
        sqlite3_bind_int(stmt, 2, noteID);

        success = sqlite3_step(stmt) == SQLITE_DONE;
        if (!success) {
            _wxLogError(L"无法更新样式内容: %s", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    }

    if (!success) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        _wxLogError("SQL commit failed: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (noteID == m_currentNoteID) {
        m_autoSaveTimer.Stop();
    }
    if (contentModified || styleModified) {
        m_scintilla->SetSavePoint();
        m_scintilla->m_styleModified = false;
        if (m_tree) {
            wxTreeItemId selItem = m_tree->GetSelection();
            if (selItem.IsOk()) {
                auto selData = static_cast<MyTreeItemData*>(m_tree->GetItemData(selItem));
                if (selData && selData->GetNoteID() == noteID) {
                    wxString name = m_tree->GetItemText(selItem);
                    if (name.EndsWith("*")) {
                        name.RemoveLast(1);
                        m_tree->SetItemText(selItem, name);
                    }
                }
            }
        }
    }
    return true;
}

void MainFrame::ScheduleAutoSave() {
    if (m_autoSaveDelayMs <= 0 || m_currentNoteID == -1 || !m_scintilla || m_scintilla->GetReadOnly()) {
        return;
    }
    if (!m_simplebook || m_simplebook->GetSelection() != ID_ScintillaPage) {
        return;
    }
    m_autoSaveTimer.Start(m_autoSaveDelayMs, wxTIMER_ONE_SHOT);
}

void MainFrame::ResetAutoSaveSession() {
    m_autoSaveTimer.Stop();
    m_currentNoteID = -1;
}

bool MainFrame::SaveCurrentNoteIfModified() {
    if (m_currentNoteID == -1 || !m_scintilla || m_scintilla->GetReadOnly()) {
        return false;
    }
    if (!m_scintilla->GetModify() && !m_scintilla->m_styleModified) {
        return false;
    }
    return SaveNoteContent(m_currentNoteID);
}

void MainFrame::FlushAutoSave() {
    m_autoSaveTimer.Stop();
    SaveCurrentNoteIfModified();
}

void MainFrame::OnAutoSaveTimer(wxTimerEvent& WXUNUSED(event)) {
    SaveCurrentNoteIfModified();
}

void MainFrame::OnActivate(wxActivateEvent& event) {
    if (m_autoSaveDelayMs > 0 && !event.GetActive() && !IsBeingDeleted()) {
        FlushAutoSave();
    }
    event.Skip();
}

void MainFrame::RestartServer() {
    if (m_enableNetServer)m_server->Stop();
    m_enableNetServer = true;
    StartServer();
}

void MainFrame::OnUpdateEditorMenu(wxUpdateUIEvent& event) {
    if (!m_simplebook || !m_scintilla) {
        event.Enable(false);
        return;
    }
    bool isScintillaPage = (m_simplebook->GetSelection() == ID_ScintillaPage);
    if (!isScintillaPage) {
        event.Enable(false);
        return;
    }
    int id = event.GetId();
    if (id == MENU_UNDO) {
        event.Enable(m_scintilla->CanUndo());
    } else if (id == MENU_REDO) {
        event.Enable(m_scintilla->CanRedo());
    } else if (id == MENU_APPEND_CLIPBOARD) {
        event.Enable(true);
        event.Check(m_EnableClipboardToSTC);
    } else {
        event.Enable(true);
    }
}

void MainFrame::OnUpdateQuickSwitchMenu(wxUpdateUIEvent& event) {
    if (!m_tree || !GetMenuBar())return;

    wxMenuBar* menuBar = GetMenuBar();
    wxMenu* navMenu = menuBar->GetMenu(menuBar->FindMenu(L"导航"));
    if (!navMenu)return;

    wxMenuItem* quickSwitchItem, * deleteHotkeyItem, * assignHotkeyItem;
    for (int i = 0; i < 9; ++i) {
        quickSwitchItem = navMenu->FindItem(MENU_QUICK_SWITCH_BASE + i);
        deleteHotkeyItem = navMenu->FindItem(MENU_DELETE_HOTKEY_BASE + i);
        assignHotkeyItem = navMenu->FindItem(MENU_ASSIGN_HOTKEY_BASE + i);

        //判断节点有效性，有效则设置显示名称，无效则设置不可用
        if (m_quickSwitchMenuIds[i] > 0 && this->m_tree->m_idToItem.count(m_quickSwitchMenuIds[i]) > 0) {
            wxString name = this->m_tree->GetItemText(this->m_tree->m_idToItem[m_quickSwitchMenuIds[i]]);
            quickSwitchItem->SetItemLabel(name + wxString::Format("\tAlt-%d", i + 1));
            quickSwitchItem->Enable(true);
            deleteHotkeyItem->SetItemLabel(wxString::Format("%d.", i + 1) + name);
            deleteHotkeyItem->Enable(true);
            assignHotkeyItem->SetItemLabel(wxString::Format("%d.", i + 1) + name);

        } else {
            wxString name{ L"未分配" };
            quickSwitchItem->SetItemLabel(name + wxString::Format("\tAlt-%d", i + 1));
            quickSwitchItem->Enable(false);
            deleteHotkeyItem->SetItemLabel(wxString::Format("%d.", i + 1) + name);
            deleteHotkeyItem->Enable(false);
            assignHotkeyItem->SetItemLabel(wxString::Format("%d.", i + 1) + name);
        }
    }
}

void MainFrame::OnUpdateItemMenu(wxUpdateUIEvent& event) {
    if (!m_tree) {
        event.Enable(false);
        return;
    }
    int type = m_tree->GetTreeItemType(m_tree->GetSelection());
    int id = event.GetId();
    if (id == MENU_RENAME_ITEM) {
        event.Enable(type == MyTreeCtrl::FolderItemID || type == MyTreeCtrl::TagItemID || type == MyTreeCtrl::WebCollectionItemID);
    } else if (id == MENU_EXPORT_ITEM) {
        event.Enable(type == MyTreeCtrl::FolderItemID || type == MyTreeCtrl::FolderRootID || type == MyTreeCtrl::WebCollectionRootID || type == MyTreeCtrl::WebCollectionItemID);
    } else {
        event.Enable(type == MyTreeCtrl::FolderItemID || type == MyTreeCtrl::WebCollectionItemID);
    }
}

void MainFrame::OnUpdateBoolMenu(wxUpdateUIEvent& event) {
    switch (event.GetId()) {
    case MENU_SHOW_TAGS:
        event.Check(m_showTags);
        break;
    case MENU_SHOW_RECENT:
        event.Check(m_showRecent);
        break;
    case MENU_ALWAYS_TOP:
        event.Check(m_PinToTop);
        break;
    case MENU_NET_SERVER:
        event.Check(m_enableNetServer);
        break;
    case MENU_TOGGLE_TREE:
        event.Check(m_treeVisible);
        break;
    default:
        event.Skip();
    }
}

#include <commctrl.h>
void MainFrame::OnToolbarStyleButtonClicked(wxCommandEvent& event) {
    auto toolbar = GetToolBar();
    if (!toolbar)return;
    if (!m_stylePopup) {
        auto size = toolbar->GetToolSize();
        m_stylePopup = new FontStylePanel(this, size);
    }

    int i = toolbar->GetToolPos(event.GetId());
    if (i == wxNOT_FOUND)return;
    RECT winRect;
    wxPoint screenPos;
    HWND hToolbar = (HWND)toolbar->GetHWND();
    if (::SendMessage(hToolbar, TB_GETITEMRECT, (WPARAM)i, (LPARAM)&winRect)) {
        wxPoint clientPos(winRect.left, winRect.bottom);
        screenPos = toolbar->ClientToScreen(clientPos);
    }
    m_stylePopup->Position(screenPos, wxSize(0, 0));
    m_stylePopup->Popup();
}

void MainFrame::OnSearchAndReplace(wxCommandEvent& event) {
    if (!m_scintilla || !m_findBar || m_simplebook->GetSelection() != ID_ScintillaPage) return;
    wxString selectedText = m_scintilla->GetSelectedText();
    m_findBar->Open(selectedText);
}

void MainFrame::OnFindSql(wxCommandEvent& event) {
    wxString initialText = m_lastGlobalSearchText;
    if (m_scintilla && m_simplebook->GetSelection() == ID_ScintillaPage) {
        wxString sel = m_scintilla->GetSelectedText();
        if (!sel.IsEmpty() && sel.Find('\n') == wxNOT_FOUND && sel.Find('\r') == wxNOT_FOUND) {
            initialText = sel;
        }
    }
    GlobalSearchDialog dlg(this, initialText);
    if (dlg.ShowModal() == wxID_OK) {
        wxString findText = dlg.GetSearchText();
        if (!findText.IsEmpty()) {
            m_lastGlobalSearchText = findText;
            m_tree->ExecuteGlobalSearch(findText);
        }
    }
}

void MainFrame::OnCharHook(wxKeyEvent& event) {
    int key = event.GetKeyCode();
    if (key == WXK_ESCAPE && m_findBar && m_findBar->IsShown()) {
        m_findBar->CloseBar();
        return;
    }
    if (key == WXK_CONTROL) {
        if (m_scintilla->IsMouseInWindow()) {
            m_scintilla->SetFocus();
        }
    }
    event.Skip();
}

void MainFrame::OnFocusChoice(wxMouseEvent& event) {
    if (!m_Choice)return;
    _wxLogMessage(L"OnMouseEnterChoice");
    if (!m_outlineNeedUpdate)return;
    m_Choice->Clear();
    if (m_simplebook->GetSelection() != ID_ScintillaPage) {
        m_outlineList.clear();
        return;
    }
    m_scintilla->GetOutlineStyle(m_outlineList);
    for (const auto& item : m_outlineList) {
        m_Choice->Append(item.text, (void*)item.position);
    }
}

void MainFrame::OnOutlineSelect(wxCommandEvent& event) {
    if (!m_Choice)return;
    if (m_simplebook->GetSelection() != ID_ScintillaPage)return;
    int selection = event.GetSelection();
    if (selection == wxNOT_FOUND)return;
    void* clientData = m_Choice->GetClientData(selection);
    int position = (int)clientData;
    m_scintilla->GotoPos(position);
    m_scintilla->SetFocus();
}

void MainFrame::OnSysColorChange2(wxSysColourChangedEvent& event) {
    event.Skip();
}

void MainFrame::UpdateToolBarIcons() {
    wxToolBar* toolBar = GetToolBar();
    if (!toolBar) return;
    wxConfigBase* config = wxConfigBase::Get();
    auto ToolbariconSize = config->ReadLong("Toolbar/IconSize", 32);
    if (ToolbariconSize < 16 || ToolbariconSize > 256) ToolbariconSize = 32;
    auto Size_i = wxSize(ToolbariconSize, ToolbariconSize);
    const char* color = GetMyAppInstance()->GetThemeColor();
    char buff[4096];

    toolBar->SetToolNormalBitmap(MENU_TOGGLE_TREE, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_toggle_sidebar, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_NEW_NOTE, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_new_note, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_NEW_FOLDER, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_new_folder, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_EDIT_TAGS, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_tag, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_COPY_ALL, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_copy_all, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_REPLACE_PASTE, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_replace_paste, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_APPEND_CLIPBOARD, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_clipboard_watch, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(TOOLBAR_SET_STYLE, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_UNDO, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_undo, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_REDO, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_redo, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_PREV_VIEW, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_last_view, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_ALWAYS_TOP, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_pin, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_SEARCH_REPLACE, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_search, color, 73), Size_i));
    toolBar->SetToolNormalBitmap(MENU_FIND_SQL, wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_database_search, color, 73), Size_i));

    bool isDark = GetMyAppInstance()->GetDarkMode();
    wxColour tbBg = isDark ? ThemeColours::DarkBgSecondary : ThemeColours::LightBgSecondary;
    toolBar->SetBackgroundColour(tbBg);

    HWND hwndTB = (HWND)toolBar->GetHWND();
    if (hwndTB) {
        ::SetWindowTheme(hwndTB, isDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
        ::SendMessageW(hwndTB, WM_THEMECHANGED, 0, 0);
    }

    if (m_labelNav) {
        m_labelNav->SetForegroundColour(isDark ? ThemeColours::DarkTextPrimary : ThemeColours::LightTextPrimary);
        m_labelNav->SetBackgroundColour(tbBg);
    }
    if (m_Choice) {
        HWND hwndChoice = (HWND)m_Choice->GetHWND();
        if (hwndChoice) {
            ::SetWindowTheme(hwndChoice, isDark ? L"DarkMode_CFD" : L"Explorer", NULL);
            ::SendMessageW(hwndChoice, WM_THEMECHANGED, 0, 0);
        }
        m_Choice->SetBackgroundColour(isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase);
        m_Choice->SetForegroundColour(isDark ? ThemeColours::DarkTextPrimary : ThemeColours::LightTextPrimary);
        m_Choice->Refresh();
    }
    toolBar->Realize();
    toolBar->Refresh();
}

void MainFrame::ApplyTheme(bool isDark) {
    auto app = GetMyAppInstance();
    app->SetDarkMode(isDark);

    // 1. 设置主窗口背景与标题栏 DWM 沉浸式暗色
    SetWindowDarkMode(m_hMainFrame, isDark);
    SetBackgroundColour(isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase);

    // 2. 更新工具栏及图标与下拉框
    UpdateToolBarIcons();

    // 3. 树形控件主题与图标刷新
    if (m_tree) {
        m_tree->ApplyTheme(isDark);
    }

    // 4. 编辑器与内嵌查找替换栏主题刷新
    if (m_editorPage) {
        m_editorPage->SetBackgroundColour(isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase);
        m_editorPage->Refresh();
    }
    if (m_findBar) {
        m_findBar->ApplyTheme(isDark);
    }
    if (m_scintilla) {
        m_scintilla->ApplyTheme(isDark);
    }

    // 5. 卡片视图刷新
    if (m_cardViewPanel) {
        m_cardViewPanel->ApplyTheme(isDark);
    }

    // 6. 容器层（m_simplebook、m_splitter）
    if (m_simplebook) {
        m_simplebook->SetBackgroundColour(isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase);
        m_simplebook->Refresh();
    }
    if (m_splitter) {
        m_splitter->SetBackgroundColour(isDark ? ThemeColours::DarkBgSecondary : ThemeColours::LightBgSecondary);
        m_splitter->Refresh();
    }

    // 7. 状态栏
    if (m_statusBar) {
        m_statusBar->SetBackgroundColour(isDark ? ThemeColours::DarkBgSecondary : ThemeColours::LightBgSecondary);
        m_statusBar->SetForegroundColour(isDark ? ThemeColours::DarkTextPrimary : ThemeColours::LightTextPrimary);
        m_statusBar->Refresh();
    }

    // 8. 顶层窗口刷新
    if (m_hMainFrame) {
        ::SendMessageW(m_hMainFrame, WM_THEMECHANGED, 0, 0);
        ::SetWindowPos(m_hMainFrame, NULL, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ::RedrawWindow(m_hMainFrame, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
    }

    Refresh();
    Update();
}
//TODO
static inline picojson::value GetNoteResponseByUrl(sqlite3* db, const wxString& url) {
    picojson::object res_obj;
    res_obj["status"] = picojson::value("error");
    res_obj["content"] = picojson::value("");

    const char* sql = "SELECT CONTENT FROM NOTES WHERE URL = ? ORDER BY ID DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, url.ToUTF8().data(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text) {
                res_obj["status"] = picojson::value("ok");
                res_obj["content"] = picojson::value(std::string(reinterpret_cast<const char*>(text)));;
            }
        }
        sqlite3_finalize(stmt);
    }

    return picojson::value(res_obj);
}
void MainFrame::OnHttpRequest(wxThreadEvent& event) {
    NoteTask* task = event.GetPayload<NoteTask*>();
    if (!task) return;

    sqlite3* db = GetMyAppInstance()->db;
    // 根据 action 分类处理业务逻辑
    if (task->action == L"get_by_url") {
        // 从数据库读取逻辑,跳过treectrl的操作直接读取数据库
        auto resultValue = GetNoteResponseByUrl(db, task->url);
        std::string responseJson = resultValue.serialize();
        m_server->SendResponse(task->requestId, responseJson);
        delete task;
    } else if (task->action == L"save_by_url") {
        // 写入到treectrl，调用treectrl的方法
        m_tree->AddWebCollection(task->url, task->title, task->content);
        std::string responseJson = "{\"status\":\"success\"}";
        m_server->SendResponse(task->requestId, responseJson);
        delete task;
    } else if (task->action == L"append_to_note") {
        m_tree->SwitchToWebCollectionNote();
        //用CallAfter异步执行
        CallAfter([this, task]() {
            this->m_scintilla->AppendText("\n----------------------------------------\n"
                + task->title + "\n" + task->content
            );

            std::string responseJson = "{\"status\":\"success\"}";
            m_server->SendResponse(task->requestId, responseJson);
            delete task;
            });
    } else {
        m_server->SendResponse(task->requestId, "{\"status\":\"error\",\"message\":\"unknown action\"}");
        delete task;
    }
}

void MainFrame::StartServer() {
    wxString htmlContent;

    HINSTANCE hInst = wxGetInstance();
    HRSRC hRes = ::FindResourceW(hInst, L"IDR_HTML_INDEX", MAKEINTRESOURCEW(23));

    if (hRes) {
        HGLOBAL hData = ::LoadResource(hInst, hRes);
        if (hData) {
            DWORD size = ::SizeofResource(hInst, hRes);
            const char* pData = (const char*)::LockResource(hData);
            htmlContent = wxString::FromUTF8(pData, size);
            _wxLogMessage(L"已资源加载 index.html");
        }
    } else {
        htmlContent = L"<html><body><h1>wxNotepad2 Server (Default)</h1></body></html>";
        _wxLogMessage(L"未找到外部 index.html，使用内置默认页面");
    }

    //替换
    wxString portStr = wxString::Format("%d", GetMyAppInstance()->GetPort());
    htmlContent.Replace("{{APP_PORT}}", portStr);
    wxString tokenStr = GetMyAppInstance()->GetToken();
    htmlContent.Replace("{{APP_TOKEN}}", tokenStr);

    if (m_server->Start(GetMyAppInstance()->GetPort(), htmlContent)) {
        _wxLogMessage(L"服务器已启动");
    }

}

void MainFrame::OnRepairEncoding(wxCommandEvent& event) {
    auto str = m_scintilla->GetText();
    auto rawBytes = str.mb_str(wxConvISO8859_1);
    size_t rawLen = rawBytes.length();
    //size_t rawLen = strlen(rawBytes.data());
    if (!rawLen)return;
    int id = event.GetId();
    wxCSConv targetConv((wxFontEncoding)(id - MENU_ENCODING_BASE));
    m_scintilla->Freeze();
    wxString fixedContent(rawBytes, targetConv, rawLen);
    m_scintilla->SetText(fixedContent);
    m_scintilla->Thaw();
}

void MainFrame::EnableClipboardListener(bool enable) {
    if (enable) {
        if (::AddClipboardFormatListener(GetHWND())) {
            m_EnableClipboardToSTC = true;
            SetStatusText(L"已开启剪贴板监听（复制文本将自动追加到当前笔记）", 0);
        } else {
            _wxLogError(L"无法开启剪贴板监听。");
        }
    } else {
        if (::RemoveClipboardFormatListener(GetHWND())) {
            m_EnableClipboardToSTC = false;
            SetStatusText(L"已关闭剪贴板监听", 0);
        } else {
            _wxLogError(L"无法关闭剪贴板监听。");
        }
    }
    if (wxToolBar* tb = GetToolBar()) {
        tb->ToggleTool(MENU_APPEND_CLIPBOARD, m_EnableClipboardToSTC);
    }
}

void MainFrame::SetClipboardBlackList() {
    auto App = GetMyAppInstance();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };
    wxString combinedString = wxEmptyString;
    config.Read("Scintilla/ClipboardBlackList", &combinedString);
    auto SEP = ';';
    if (!combinedString.IsEmpty()) {
        m_ClipboardBlackList = new wxArrayString(wxSplit(combinedString, SEP, '\0'));
    } else {
        m_ClipboardBlackList = new wxArrayString();
    }
}

void MainFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
    OnExit();
}

void MainFrame::OnAbout(wxCommandEvent& WXUNUSED(event)) {
    AboutDialog diag(this);
    diag.ShowModal();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (m_closeToTray) {
        if (m_tree) {
            m_tree->CommitPendingLabelEdit();
        }
        FlushAutoSave();
        Hide();
        event.Veto();
    } else {
        OnExit();
    }
}

void MainFrame::OnSaveNote(wxCommandEvent& WXUNUSED(event)) {
    if (!m_simplebook || m_simplebook->GetSelection() != ID_ScintillaPage || m_currentNoteID == -1) {
        return;
    }
    if (!m_scintilla || m_scintilla->GetReadOnly()) {
        return;
    }
    if (m_scintilla->GetModify() || m_scintilla->m_styleModified) {
        if (SaveNoteContent(m_currentNoteID)) {
            SetStatusText(L"已保存", 0);
        }
    } else {
        m_autoSaveTimer.Stop();
        SetStatusText(L"当前内容已是最新", 0);
    }
}

void MainFrame::OnCopyAll(wxCommandEvent& WXUNUSED(event)) {
    if (!m_simplebook || m_simplebook->GetSelection() != ID_ScintillaPage || !m_scintilla) {
        return;
    }
    wxString text = m_scintilla->GetText();
    if (text.IsEmpty()) {
        SetStatusText(L"当前笔记为空", 0);
        return;
    }
    if (WriteClipboardUnicodeText(m_hMainFrame, text)) {
        SetStatusText(wxString::Format(L"已复制全文 (%zu 字符)", text.length()), 0);
    } else {
        SetStatusText(L"无法访问剪贴板", 0);
    }
}

void MainFrame::OnReplacePaste(wxCommandEvent& WXUNUSED(event)) {
    if (!m_simplebook || m_simplebook->GetSelection() != ID_ScintillaPage || !m_scintilla) {
        return;
    }
    if (m_scintilla->GetReadOnly()) {
        SetStatusText(L"当前笔记为只读，无法粘贴", 0);
        return;
    }
    wxString clipText;
    ReadClipboardUnicodeText(m_hMainFrame, clipText);
    if (clipText.IsEmpty()) {
        SetStatusText(L"剪贴板中没有文本内容", 0);
        return;
    }
    m_scintilla->BeginUndoAction();
    m_scintilla->SetTargetRange(0, m_scintilla->GetTextLength());
    m_scintilla->ReplaceTarget(clipText);
    m_scintilla->GotoPos(m_scintilla->GetTextLength());
    m_scintilla->EnsureCaretVisible();
    m_scintilla->EndUndoAction();
    m_scintilla->SetFocus();
    SetStatusText(wxString::Format(L"已覆盖粘贴 (%zu 字符)", clipText.length()), 0);
}

void MainFrame::OnCopyAsFile(const wxString& ext) {
    if (!m_simplebook || m_simplebook->GetSelection() != ID_ScintillaPage || !m_scintilla) {
        return;
    }
    wxString text = m_scintilla->GetText();
    if (text.IsEmpty()) {
        SetStatusText(L"当前笔记内容为空，未生成文件", 0);
        return;
    }

    wxString noteTitle;
    if (m_tree) {
        wxTreeItemId selItem = m_tree->GetSelection();
        if (selItem.IsOk()) {
            auto selData = static_cast<MyTreeItemData*>(m_tree->GetItemData(selItem));
            if (selData) {
                noteTitle = selData->GetName();
            }
            if (noteTitle.IsEmpty()) {
                noteTitle = m_tree->GetItemText(selItem);
                if (noteTitle.EndsWith(L"*")) {
                    noteTitle.RemoveLast(1);
                }
            }
        }
    }

    wxString fileName;
    size_t byteCount = 0;
    if (CopyTextAsFileToClipboard(m_hMainFrame, noteTitle, ext, text, fileName, byteCount)) {
        wxString sizeStr = (byteCount >= 1024)
            ? wxString::Format(L"%.1f KB", byteCount / 1024.0)
            : wxString::Format(L"%zu 字节", byteCount);
        SetStatusText(wxString::Format(L"已复制为文件：%s（%s）", fileName, sizeStr), 0);
    } else {
        SetStatusText(L"复制为文件失败：无法写入临时文件或访问剪贴板", 0);
    }
}

void MainFrame::OnNewNoteFromClipboard(wxCommandEvent& WXUNUSED(event)) {
    if (!m_tree) return;
    wxString clipText;
    ReadClipboardUnicodeText(m_hMainFrame, clipText);
    if (clipText.IsEmpty()) {
        SetStatusText(L"剪贴板中没有文本内容", 0);
        return;
    }

    // 提取第一个非空行前 20 字作为默认标题
    wxString title;
    wxString normalized = clipText;
    normalized.Replace("\r\n", "\n");
    normalized.Replace("\r", "\n");
    wxArrayString lines = wxSplit(normalized, '\n', '\0');
    for (const auto& line : lines) {
        wxString trimmed = line;
        trimmed.Trim(true).Trim(false);
        if (!trimmed.IsEmpty()) {
            if (trimmed.length() > 20) {
                trimmed = trimmed.Left(20);
            }
            title = SanitizeName(trimmed);
            break;
        }
    }
    if (title.IsEmpty()) {
        title = L"剪贴板笔记";
    }

    if (m_tree->CreateNoteWithContent(title, clipText, false)) {
        CallAfter([this, title]() {
            if (m_scintilla && m_simplebook && m_simplebook->GetSelection() == ID_ScintillaPage) {
                m_scintilla->SetFocus();
            }
            SetStatusText(L"已从剪贴板创建笔记：" + title, 0);
        });
    }
}

void MainFrame::OnToggleTree(wxCommandEvent& WXUNUSED(event)) {
    if (!m_splitter || !m_tree || !m_simplebook) return;
    if (m_treeVisible) {
        int pos = m_splitter->GetSashPosition();
        if (pos >= FromDIP(40)) {
            m_savedSplitterPos = pos;
        }
        m_splitter->Unsplit(m_tree);
        m_treeVisible = false;
        if (m_scintilla && m_simplebook->GetSelection() == ID_ScintillaPage) {
            m_scintilla->SetFocus();
        }
    } else {
        int restorePos = (m_savedSplitterPos >= FromDIP(40)) ? m_savedSplitterPos : FromDIP(240);
        m_splitter->SplitVertically(m_tree, m_simplebook, restorePos);
        m_treeVisible = true;
    }
    if (wxToolBar* tb = GetToolBar()) {
        tb->ToggleTool(MENU_TOGGLE_TREE, m_treeVisible);
    }
}

void MainFrame::TouchNoteAccessTime(int noteID) {
    if (noteID <= 0) return;
    sqlite3* db = GetMyAppInstance()->db;
    if (!db) return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE NOTES SET ACCESS_TIME = CURRENT_TIMESTAMP WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, noteID);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void MainFrame::OnShowNoteContent(wxTreeEvent& event) {
    wxString content = event.GetString();
    int i = event.GetInt();
    auto styleData = (std::vector<UINT16>*)(event.GetClientData());
    ResetAutoSaveSession();
    m_scintilla->LoadContent(content, i, *styleData);
    wxTreeItemId item = event.GetItem();
    if (item.IsOk() && m_tree) {
        int type = m_tree->GetTreeItemType(item);
        auto itemData = static_cast<MyTreeItemData*>(m_tree->GetItemData(item));
        if (itemData && !itemData->IsFolder() && itemData->GetNoteID() != -1 &&
            (type == MyTreeCtrl::FolderItemID || type == MyTreeCtrl::WebCollectionItemID)) {
            m_currentNoteID = itemData->GetNoteID();
            TouchNoteAccessTime(m_currentNoteID);
        }
    }
    m_outlineNeedUpdate = true;
    if (m_Choice)m_Choice->Clear();
    //m_scintilla->SetFocus();
    delete styleData;
    event.Skip();
}

void MainFrame::OnEditTags(wxCommandEvent& event) {
    auto selectedItem = m_tree->GetSelection();
    if (!selectedItem.IsOk())return;
    int type = m_tree->GetTreeItemType(selectedItem);
    if (type != MyTreeCtrl::FolderItemID)return;
    MyTreeItemData* data = static_cast<MyTreeItemData*>(this->m_tree->GetItemData(selectedItem));
    if (data->IsFolder())return;

    sqlite3* db = GetMyAppInstance()->db;
    auto currentTaglist = m_tree->GetNoteTagList(db, data->GetID());
    TagDialog tagDialog(this, currentTaglist, m_tree->m_TagsList);
    tagDialog.ShowModal();
}

void MainFrame::OnGlobalSettings(wxCommandEvent& event) {
    SettingDialog settingsDialog(this);
    UnregisterGlobalHotKey();
    if (settingsDialog.ShowModal() == wxID_OK) {
        if (settingsDialog.originData == settingsDialog.changedData) {
            RegisterGlobalHotkey();
            return;
        }
        //调用相应的应用设置方法
        m_closeToTray = settingsDialog.changedData.dm_CloseToTray;
        m_minimizeToTray = settingsDialog.changedData.dm_MinimizeToTray;
        m_ClipboardGetMoreInfo = settingsDialog.changedData.dm_ClipboardGetMoreInfo;
        m_recentMaxCount = settingsDialog.changedData.dm_RecentItemMaxValue;
        m_autoSaveDelayMs = settingsDialog.changedData.dm_autoSaveDelayMs;
        if (m_autoSaveDelayMs <= 0) {
            m_autoSaveTimer.Stop();
        } else if (m_autoSaveTimer.IsRunning()) {
            m_autoSaveTimer.Start(m_autoSaveDelayMs, wxTIMER_ONE_SHOT);
        }
        SetClipboardBlackList();

        m_scintilla->InitHotLinks();

        if (settingsDialog.originData.dm_stcBGcolor != settingsDialog.changedData.dm_stcBGcolor) {
            m_scintilla->StyleSetBackground(0, settingsDialog.changedData.dm_stcBGcolor);
            m_scintilla->StyleSetBackground(wxSTC_STYLE_DEFAULT, settingsDialog.changedData.dm_stcBGcolor);
            m_scintilla->ChangeAllStyleBackground(settingsDialog.changedData.dm_stcBGcolor);
            m_scintilla->SetCaretForeground(GetInvertedColour(settingsDialog.changedData.dm_stcBGcolor));
        };

        if (settingsDialog.originData.dm_stcFGcolor != settingsDialog.changedData.dm_stcFGcolor) {
            m_scintilla->StyleSetForeground(0, settingsDialog.changedData.dm_stcFGcolor);
            m_scintilla->ChangeAllStyleForeground(settingsDialog.changedData.dm_stcFGcolor);
        };

        if (settingsDialog.originData.dm_stcFont != settingsDialog.changedData.dm_stcFont) {
            m_scintilla->ChangeEditorFont(settingsDialog.changedData.dm_stcFont);
        };

        m_scintilla->SetPreviewPicLength(settingsDialog.changedData.dm_ImagePreviewMaxSize);

        bool needRestart = (settingsDialog.originData.dm_themeMode != settingsDialog.changedData.dm_themeMode ||
                            settingsDialog.originData.dm_darkmode != settingsDialog.changedData.dm_darkmode ||
                            settingsDialog.originData.dm_DarkModeThemeColor != settingsDialog.changedData.dm_DarkModeThemeColor ||
                            settingsDialog.originData.dm_DefaultThemeColor != settingsDialog.changedData.dm_DefaultThemeColor ||
                            settingsDialog.originData.dm_toolbarIconSize != settingsDialog.changedData.dm_toolbarIconSize ||
                            settingsDialog.originData.dm_treeIconSize != settingsDialog.changedData.dm_treeIconSize ||
                            settingsDialog.originData.dm_hideNavbar != settingsDialog.changedData.dm_hideNavbar);
        if (needRestart) {
            auto app = GetMyAppInstance();
            app->SetThemeMode(static_cast<ThemeMode>(settingsDialog.changedData.dm_themeMode));
            if (wxMessageBox(L"部分设置（如外观模式、图标尺寸或工具栏大纲跳转栏）需要重启软件后生效。\n是否立即重启？",
                             L"设置已更新", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
                OnRestart();
                return;
            }
        }
    }
    RegisterGlobalHotkey();
}

void MainFrame::OnCheckUpdate(wxCommandEvent& event) {
    auto menuBar = GetMenuBar();
    if (menuBar) {
        menuBar->Enable(MENU_CHECK_UPDATE, false);
    }
    SetStatusText(L"正在检查更新...");
    wxString url = "https://api.github.com/repos/symudo/wxNotepad2/releases/latest";
    auto m_request = wxWebSession::GetDefault().CreateRequest(this, url);
    if (!m_request.IsOk()) {
        wxMessageBox(L"无法初始化网络请求，请检查网络连接。", L"错误", wxOK | wxICON_ERROR, this);
        if (menuBar) menuBar->Enable(MENU_CHECK_UPDATE, true);
        SetStatusText(L"就绪");
        return;
    }

    m_request.SetHeader("User-Agent", "wxNotepad2");
    m_request.SetHeader("Accept", "application/vnd.github+json");
    m_request.Start();
}

void MainFrame::OnCheckUpdateResponse(wxWebRequestEvent& evt) {
    auto menuBar = GetMenuBar();
    if (evt.GetState() == wxWebRequest::State_Completed) {
        std::string json_str = evt.GetResponse().AsString().ToStdString();
        picojson::value v;
        std::string err = picojson::parse(v, json_str);

        if (err.empty() && v.is<picojson::object>() &&
            v.contains("tag_name") && v.get("tag_name").is<std::string>()) {
            wxString latestVersion = wxString::FromUTF8(v.get("tag_name").get<std::string>());
            wxString currentVersion = wxString::Format(L"%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, RELEASE_NUMBER);

            if (CompareSemVer(latestVersion, currentVersion) > 0) {
                if (wxMessageBox(L"发现新版本: " + latestVersion + L"\n当前版本: v" + currentVersion + L"\n是否打开下载页面？",
                    L"更新提示", wxYES_NO | wxICON_INFORMATION, this) == wxYES) {
                    wxLaunchDefaultBrowser("https://github.com/symudo/wxNotepad2/releases");
                }
            } else {
                wxMessageBox(L"当前已是最新版本 (v" + currentVersion + L")。", L"检查更新", wxOK | wxICON_INFORMATION, this);
            }
        } else {
            wxMessageBox(L"解析服务器返回的数据失败。", L"错误", wxOK | wxICON_WARNING, this);
        }
    } else if (evt.GetState() == wxWebRequest::State_Failed) {
        wxMessageBox(L"无法连接到 GitHub 服务器：\n" + evt.GetErrorDescription(),
            L"错误", wxOK | wxICON_ERROR, this);
    }

    if (evt.GetState() == wxWebRequest::State_Completed ||
        evt.GetState() == wxWebRequest::State_Failed ||
        evt.GetState() == wxWebRequest::State_Cancelled) {
        if (menuBar) menuBar->Enable(MENU_CHECK_UPDATE, true);
        SetStatusText(L"就绪");
    }
}

FontStylePanel::FontStylePanel(MainFrame* parent, wxSize size) :wxPopupTransientWindow((wxWindow*)parent), m_parent(parent), iconSize(size) {
    bool isDark = GetMyAppInstance()->GetDarkMode();
    wxPanel* mainPanel = new wxPanel(this);
    if (isDark) {
        mainPanel->SetBackgroundColour(ThemeColours::DarkBgSecondary);
    }
    wxGridSizer* sizer = new wxGridSizer(3, 4, 2, 2);
    wxConfigBase* config = wxConfigBase::Get();
    auto ToolbariconSize = config->ReadLong("Toolbar/IconSize", 32);
    if (ToolbariconSize < 16 || ToolbariconSize>256)ToolbariconSize = 32;
    wxSize svgSize = wxSize(ToolbariconSize, ToolbariconSize);

    char buff[1024];
    wxBitmapBundle bundle[11];
    bundle[0] = wxBitmapBundle::FromSVG(icon_font_style_default, svgSize);
    bundle[1] = wxBitmapBundle::FromSVG(icon_font_style_bold, svgSize);
    bundle[2] = wxBitmapBundle::FromSVG(icon_font_style_italic, svgSize);
    bundle[3] = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style_default, "ff0000", 119), svgSize);
    bundle[4] = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style_default, "0000ff", 119), svgSize);
    bundle[5] = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style_default, "ffff00", 96), svgSize);
    bundle[6] = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style_default, "ff00ff", 96), svgSize);
    bundle[7] = wxBitmapBundle::FromSVG(ChangeSVGColor(buff, icon_font_style_default, "00ffff", 96), svgSize);
    bundle[8] = wxBitmapBundle::FromSVG(icon_font_style_bold, svgSize);
    bundle[9] = wxBitmapBundle::FromSVG(icon_font_style_bold, svgSize);
    bundle[10] = wxBitmapBundle::FromSVG(icon_font_style_bold, svgSize);

    wxString toolTips[11] = { L"清除格式",L"粗体",L"不醒目标记",L"红色",L"蓝色",L"黄色背景",L"洋红色背景",L"浅蓝色背景",L"标题",L"大纲级别一",L"大纲级别二" };

    for (int i = 0; i < 11; i++) {
        wxBitmapButton* btn = new wxBitmapButton(mainPanel, MainFrame::MENU_SET_STYLE_DEFAULT + i,
            bundle[i], wxDefaultPosition, iconSize);
        btn->SetToolTip(toolTips[i]);
        sizer->Add(btn, 0, wxALL, 1);
    }

    mainPanel->SetSizer(sizer);
    sizer->Fit(mainPanel);
    SetClientSize(mainPanel->GetSize());
    Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
        if (m_parent) {
            event.SetEventType(wxEVT_MENU);
            m_parent->ProcessWindowEvent(event);
        }
        Dismiss();
        });
}
