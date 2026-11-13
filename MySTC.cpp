#include "pch.h"

#include "MySTC.h"
#include "MainFrame.h"
#include "App.h"
#include "Helper.h"
#include "SVG_ICON.h"
#include <thread>
MySTC::MySTC(wxWindow* parent, MainFrame* mainFrame, wxWindowID id) : wxStyledTextCtrl(parent, id), m_mainFrame(mainFrame) {
    wxConfigBase* config = wxConfigBase::Get();
    SetCodePage(wxSTC_CP_UTF8);
    SetTechnology(wxSTC_TECHNOLOGY_DIRECTWRITE);
    StyleClearAll();
    //stc的默认样式
    SetWrapMode(2 * m_mainFrame->m_autoLineBreak);
    SetScrollWidth(10);
    SetScrollWidthTracking(true);

    SetMarginType(0, wxSTC_MARGIN_NUMBER);
    ApplyTheme(GetMyAppInstance()->GetDarkMode());
    wxFont contentFont = ReadFontFromConfig();
    ChangeEditorFont(contentFont);

    /*SetEdgeMode(wxSTC_EDGE_LINE);
    SetEdgeColumn(80);*/
    SetTabWidth(4);
    SetUseTabs(false);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);

    //初始化命令和链接
    InitHotLinks();
    // 初始化指示器和正则表达式
    InitRegExprAndIndicators();
    InitStyles();

    initBind();
    SetDropTarget(new CompositeDropTarget(this));
    m_imagePreview = new ImagePreviewPopup(this);
    m_imagePreview->Hide();
    //多重编辑
    SetMultipleSelection(true);
    SetMultiPaste(wxSTC_MULTIPASTE_EACH);
    SetAdditionalSelectionTyping(true);

    SetVirtualSpaceOptions(wxSTC_VS_RECTANGULARSELECTION);

    SetAdditionalCaretsBlink(true);
    SetAdditionalCaretsVisible(true);
    //设置换行和缩进模式
    SetWrapVisualFlags(wxSTC_WRAPVISUALFLAG_END);
    SetWrapIndentMode(wxSTC_WRAPINDENT_DEEPINDENT);
    //设置折叠标识
    SetMarginType(2, wxSTC_MARGIN_SYMBOL);

    SetMarginWidth(2, FromDIP(16));
    SetMarginMask(2, wxSTC_MASK_FOLDERS);
    SetMarginSensitive(2, true);
    SetProperty("fold", "1");
    wxColor foldMarginColor = *wxYELLOW;

    SetFoldFlags(wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS, foldMarginColor, *wxRED); // 展开时
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS, foldMarginColor, *wxRED);  // 折叠时
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE, foldMarginColor, *wxRED);    // 子级连接线
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER, foldMarginColor, *wxRED);  // 底部拐角
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED, foldMarginColor, *wxRED);
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED, foldMarginColor, *wxRED);
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER, foldMarginColor, *wxRED);
    //
    m_updatingTimer = new wxTimer(this);
    m_headerStack.reserve(16);
    m_pendingEmptyLines.reserve(32);
}

void MySTC::initBind() {
    Bind(wxEVT_STC_MODIFIED, &MySTC::OnStcModified, this);
    Bind(wxEVT_STC_ZOOM, &MySTC::OnStcZoom, this);
    Bind(wxEVT_STC_PAINTED, &MySTC::OnStcPainted, this);
    Bind(wxEVT_STC_DWELLSTART, &MySTC::OnStcDwellStart, this);
    // Bind(wxEVT_STC_DWELLEND, &MySTC::OnStcDwellEnd, this);
    Bind(wxEVT_STC_INDICATOR_RELEASE, &MySTC::OnStcIndicatorReleased, this);
    Bind(wxEVT_STC_CHARADDED, &MySTC::OnStcCharAdded, this);
    Bind(wxEVT_SIZE, &MySTC::OnSizeNew, this);
    Bind(wxEVT_CONTEXT_MENU, &MySTC::OnContextMenu, this);

    Bind(wxEVT_MENU, &MySTC::CustomLink, this, MainFrame::MENU_CUSTOM_LINK_BASE, MainFrame::MENU_CUSTOM_LINK_TOP);
    Bind(wxEVT_MOTION, &MySTC::OnMouseMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &MySTC::OnMouseLeave, this);
    Bind(wxEVT_KEY_DOWN, &MySTC::OnKeyDown2, this);
    Bind(wxEVT_KEY_UP, &MySTC::OnKeyUp, this);
    Bind(wxEVT_STC_MARGINCLICK, &MySTC::OnMarginClicked, this);
    Bind(wxEVT_TIMER, &MySTC::OnUpdatingTimer, this);
}

void MySTC::InitHotLinks() {
    auto App = GetMyAppInstance();
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };
    wxString linkValue;
    for (int i = 0; i < 5; i++) {
        m_customLinks[i] = { 0 };
    }
    for (int i = 0; i < 5; i++) {
        if (!config.Read(wxString::Format("Scintilla/Link%d", i + 1), &linkValue))continue;

        wxString idStr = linkValue.BeforeFirst(';').Trim(true).Trim(false);
        long id_long = 0;
        if (!idStr.ToLong(&id_long) || (id_long != 1 && id_long != 2)) continue;
        wxString remaining = linkValue.AfterFirst(';');
        int secondSeparatorPos = remaining.Find(';');
        if (secondSeparatorPos == wxNOT_FOUND) continue;
        wxString nameStr = remaining.BeforeFirst(';').Trim(true).Trim(false);
        wxString commandStr = remaining.AfterFirst(';').Trim(true).Trim(false);

        m_customLinks[i].id = (int)id_long;
        m_customLinks[i].name = nameStr;
        m_customLinks[i].command = commandStr;
    }
}

void MySTC::InitRegExprAndIndicators() {
    //定义指示器样式
    //url高亮和下划线
    IndicatorSetStyle(INDIC_URL_HIGHLIGHT, wxSTC_INDIC_TEXTFORE);
    IndicatorSetForeground(INDIC_URL_HIGHLIGHT, URL_INDICATOR_COLOR);
    IndicatorSetHoverStyle(INDIC_URL_HIGHLIGHT, wxSTC_INDIC_TEXTFORE);
    IndicatorSetHoverForeground(INDIC_URL_HIGHLIGHT, *wxCYAN);

    IndicatorSetStyle(INDIC_URL_UNDERLINE, wxSTC_INDIC_PLAIN);
    IndicatorSetForeground(INDIC_URL_UNDERLINE, URL_INDICATOR_COLOR);
    IndicatorSetHoverStyle(INDIC_URL_UNDERLINE, wxSTC_INDIC_PLAIN);
    IndicatorSetHoverForeground(INDIC_URL_UNDERLINE, *wxCYAN);
    //颜色代码，下划线，悬停背景色，悬停前景色
    //IndicatorSetStyle(INDIC_COLOR_UNDERLINE, wxSTC_INDIC_PLAIN);
    //IndicatorSetForeground(INDIC_COLOR_UNDERLINE, *wxBLACK);

    IndicatorSetStyle(INDIC_COLOR_FULLBOX, wxSTC_INDIC_HIDDEN);
    IndicatorSetHoverStyle(INDIC_COLOR_FULLBOX, wxSTC_INDIC_FULLBOX);
    IndicatorSetFlags(INDIC_COLOR_FULLBOX, wxSTC_INDICFLAG_VALUEFORE);
    IndicatorSetUnder(INDIC_COLOR_FULLBOX, true);

    IndicatorSetStyle(INDIC_COLOR_TEXTFORE, wxSTC_INDIC_HIDDEN);
    IndicatorSetFlags(INDIC_COLOR_TEXTFORE, wxSTC_INDICFLAG_VALUEFORE);
    IndicatorSetHoverStyle(INDIC_COLOR_TEXTFORE, wxSTC_INDIC_TEXTFORE);
    //选中高亮
    IndicatorSetStyle(INDIC_MATCHES_HIGHLIGHT, wxSTC_INDIC_ROUNDBOX);
    IndicatorSetForeground(INDIC_MATCHES_HIGHLIGHT, *wxYELLOW);
    IndicatorSetAlpha(INDIC_MATCHES_HIGHLIGHT, 60);
    IndicatorSetUnder(INDIC_MATCHES_HIGHLIGHT, true);

    SetMouseDwellTime(0);
    //准备 oniguruma 正则表达式
    OnigEncoding enc = ONIG_ENCODING_UTF8;
    onig_initialize(&enc, 1);

    const UChar* pattern = (const UChar*)s_pUrlRegEx;
    OnigErrorInfo einfo;
    int r = onig_new(&s_urlRegex, pattern, pattern + strlen(s_pUrlRegEx),
        ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &einfo);
    if (r != ONIG_NORMAL) {
        _wxLogError("Failed to compile regex s_pUrlRegEx: %d", r);
        s_urlRegex = nullptr;
    }

    pattern = (const UChar*)s_pColorRegEx;
    r = onig_new(&s_colorRegex, pattern, pattern + strlen(s_pColorRegEx),
        ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &einfo);
    if (r != ONIG_NORMAL) {
        _wxLogError("Failed to compile regex s_pColorRegEx: %d", r);
        s_colorRegex = nullptr;
    }
}

void MySTC::InitStyles() {
    StyleSetForeground(STYLE_BOLD, StyleGetForeground(0));
    StyleSetBackground(STYLE_BOLD, StyleGetBackground(0));
    StyleSetFaceName(STYLE_BOLD, StyleGetFaceName(0));
    StyleSetSize(STYLE_BOLD, StyleGetSize(0));
    StyleSetBold(STYLE_BOLD, true);

    StyleSetBackground(STYLE_ITALICGREY, StyleGetBackground(0));
    StyleSetFaceName(STYLE_ITALICGREY, StyleGetFaceName(0));
    StyleSetSize(STYLE_ITALICGREY, StyleGetSize(0));
    StyleSetItalic(STYLE_ITALICGREY, true);
    StyleSetForeground(STYLE_ITALICGREY, *wxLIGHT_GREY);

    StyleSetBackground(STYLE_RED, StyleGetBackground(0));
    StyleSetFaceName(STYLE_RED, StyleGetFaceName(0));
    StyleSetSize(STYLE_RED, StyleGetSize(0));
    StyleSetForeground(STYLE_RED, *wxRED);

    StyleSetBackground(STYLE_BLUE, StyleGetBackground(0));
    StyleSetFaceName(STYLE_BLUE, StyleGetFaceName(0));
    StyleSetSize(STYLE_BLUE, StyleGetSize(0));
    StyleSetForeground(STYLE_BLUE, *wxBLUE);

    StyleSetForeground(STYLE_YELLOW_BG, StyleGetForeground(0));
    StyleSetFaceName(STYLE_YELLOW_BG, StyleGetFaceName(0));
    StyleSetSize(STYLE_YELLOW_BG, StyleGetSize(0));
    StyleSetBackground(STYLE_YELLOW_BG, *wxYELLOW);

    StyleSetForeground(STYLE_PINK_BG, StyleGetForeground(0));
    StyleSetFaceName(STYLE_PINK_BG, StyleGetFaceName(0));
    StyleSetSize(STYLE_PINK_BG, StyleGetSize(0));
    StyleSetBackground(STYLE_PINK_BG, wxColour(255, 0, 255));

    StyleSetForeground(STYLE_LIGHTBLUE_BG, StyleGetForeground(0));
    StyleSetFaceName(STYLE_LIGHTBLUE_BG, StyleGetFaceName(0));
    StyleSetSize(STYLE_LIGHTBLUE_BG, StyleGetSize(0));
    StyleSetBackground(STYLE_LIGHTBLUE_BG, wxColour(0, 255, 255));

    StyleSetForeground(STYLE_TITLE, StyleGetForeground(0));
    StyleSetBackground(STYLE_TITLE, StyleGetBackground(0));
    //StyleSetBackground(STYLE_TITLE, *wxRED);
    StyleSetFaceName(STYLE_TITLE, StyleGetFaceName(0));
    StyleSetSize(STYLE_TITLE, StyleGetSize(0));
    StyleSetBold(STYLE_TITLE, true);
    //StyleSetEOLFilled(STYLE_TITLE, true);

    StyleSetForeground(STYLE_LEVEL1, StyleGetForeground(0));
    StyleSetBackground(STYLE_LEVEL1, StyleGetBackground(0));
    StyleSetFaceName(STYLE_LEVEL1, StyleGetFaceName(0));
    StyleSetSize(STYLE_LEVEL1, StyleGetSize(0));
    StyleSetBold(STYLE_LEVEL1, true);

    StyleSetForeground(STYLE_LEVEL2, StyleGetForeground(0));
    StyleSetBackground(STYLE_LEVEL2, StyleGetBackground(0));
    StyleSetFaceName(STYLE_LEVEL2, StyleGetFaceName(0));
    StyleSetSize(STYLE_LEVEL2, StyleGetSize(0));
    StyleSetBold(STYLE_LEVEL2, true);
}

void MySTC::UpdateUrlIndicators() {
    if (!s_urlRegex) {
        _wxLogError("URL regex not initialized");
        return;
    }
    int length = GetTextLength();
    if (length == 0)return;
    int totalLines = GetLineCount();
    int startLine = DocLineFromVisible(GetFirstVisibleLine());

    int endLine = std::min(startLine + LinesOnScreen(), totalLines - 1);
    // 获取可见区域的范围
    int startPos = PositionFromLine(startLine);
    int endPos = GetLineEndPosition(endLine);

    if (endPos > length) {
        endPos = length;
    }
    if (startPos < 0 || endPos <= startPos) {
        _wxLogDebug("Invalid text range from Url...: startPos=%d, endPos=%d", startPos, endPos);
        return;
    }
    // 清除可视区域内所有旧的URL指示器

    SetIndicatorCurrent(INDIC_URL_HIGHLIGHT);
    IndicatorClearRange(startPos, endPos - startPos);
    SetIndicatorCurrent(INDIC_URL_UNDERLINE);
    IndicatorClearRange(startPos, endPos - startPos);

    // 获取需要搜索的文本内容（使用wxSTC原生方法获取UTF-8文本）
    wxString text_wx = GetTextRange(startPos, endPos);
    std::string text_utf8{ text_wx.ToUTF8() };
    const UChar* search_target = (const UChar*)text_utf8.c_str();
    const UChar* search_target_end = search_target + text_utf8.length();

    int search_start = 0;
    OnigRegion* region = onig_region_new();

    while (search_start < text_utf8.length()) {
        int r = onig_search(s_urlRegex, search_target, search_target_end,
            search_target + search_start, search_target_end,
            region, ONIG_OPTION_NONE);

        if (r >= 0 && region->num_regs > 0) {
            int match_start_byte_offset = region->beg[0];
            int match_end_byte_offset = region->end[0];
            int indicator_length = match_end_byte_offset - match_start_byte_offset;

            // 确保匹配长度有效
            if (indicator_length <= 0) {
                _wxLogDebug("Zero-length match detected at offset %d", match_start_byte_offset);
                search_start = match_end_byte_offset + 1;
                continue;
            }
            int indicator_start = startPos + match_start_byte_offset;
            SetIndicatorCurrent(INDIC_URL_HIGHLIGHT);
            // 找到匹配，应用指示器
            IndicatorFillRange(indicator_start, indicator_length);
            SetIndicatorCurrent(INDIC_URL_UNDERLINE);
            IndicatorFillRange(indicator_start, indicator_length);

            // 下一次从匹配结束位置开始搜索
            search_start = match_end_byte_offset;
        } else {
            if (r != ONIG_MISMATCH) {
                _wxLogError("Oniguruma search error: %d", r);
            }
            break; // 无匹配或到达文本末尾
        }
    }

    onig_region_free(region, 1);
    //   m_updatingIndicators = false;
}

void MySTC::UpdateColorIndicators() {
    if (!s_colorRegex) {
        _wxLogError("Color regex not initialized");
        return;
    }
    int length = GetTextLength();
    if (length == 0)return;
    int totalLines = GetLineCount();
    int startLine = DocLineFromVisible(GetFirstVisibleLine());

    int endLine = std::min(startLine + LinesOnScreen(), totalLines - 1);
    // 获取可见区域的范围
    int startPos = PositionFromLine(startLine);
    int endPos = GetLineEndPosition(endLine);

    if (endPos > length) {
        endPos = length;
    }
    if (startPos < 0 || endPos <= startPos) {
        _wxLogDebug("Invalid text range from Color: startPos=%d, endPos=%d", startPos, endPos);
        return;
    }
    // 清除可视区域内所有旧的URL指示器

    //SetIndicatorCurrent(INDIC_COLOR_UNDERLINE);
    //IndicatorClearRange(startPos, endPos - startPos);
    SetIndicatorCurrent(INDIC_COLOR_FULLBOX);
    IndicatorClearRange(startPos, endPos - startPos);
    SetIndicatorCurrent(INDIC_COLOR_TEXTFORE);
    IndicatorClearRange(startPos, endPos - startPos);

    // 获取需要搜索的文本内容（使用wxSTC原生方法获取UTF-8文本）
    wxString text_wx = GetTextRange(startPos, endPos);

    std::string text_utf8{ text_wx.ToUTF8() };
    const UChar* search_target = (const UChar*)text_utf8.c_str();
    const UChar* search_target_end = search_target + text_utf8.length();

    int search_start = 0;
    OnigRegion* region = onig_region_new();

    while (search_start < static_cast<int>(text_utf8.length())) {
        int r = onig_search(s_colorRegex, search_target, search_target_end, search_target + search_start, search_target_end, region, ONIG_OPTION_NONE);

        if (r >= 0 && region->num_regs > 0) {
            /* int match_start_byte_offset = region->beg[0];
             int match_end_byte_offset = region->end[0];*/
            int match_start_byte_offset = ConvertBytePosToCharPos(text_wx, region->beg[0]);
            int match_end_byte_offset = ConvertBytePosToCharPos(text_wx, region->end[0]);
            int indicator_length = match_end_byte_offset - match_start_byte_offset;
            // 确保匹配长度有效
            if (indicator_length <= 0) {
                _wxLogDebug("Zero-length match detected at offset %d", match_start_byte_offset);
                search_start = region->end[0] + 1;
                continue;
            }
            int indicator_start = startPos + match_start_byte_offset;
            // 更改：添加范围验证
            if (indicator_start < startPos || indicator_start + indicator_length > endPos) {
                wxLogWarning("Indicator range out of bounds: start=%d, length=%d", indicator_start, indicator_length);
                search_start = region->end[0] + 1;
                continue;
            }
            wxString matchedColorCode = text_wx.Mid(match_start_byte_offset, indicator_length);//这里使用字符偏移量

            wxColour color;
            if (color.Set(matchedColorCode)) {
                // 将wxColour转换为RGB整数值
                auto colorValue = static_cast<int>(color.GetRGBA());
                indicator_start = startPos + region->beg[0];
                //SetIndicatorCurrent(INDIC_COLOR_UNDERLINE);
                //IndicatorFillRange(indicator_start, indicator_length);//直接使用oniguruma的返回值（字节偏移量）

                SetIndicatorCurrent(INDIC_COLOR_FULLBOX);
                SetIndicatorValue(colorValue);
                IndicatorFillRange(indicator_start, indicator_length);

                SetIndicatorCurrent(INDIC_COLOR_TEXTFORE);
                SetIndicatorValue(colorValue);//设置不同的value可以防止同类型指示器粘连
                IndicatorFillRange(indicator_start, indicator_length);
            }

            // 下一次从匹配结束位置开始搜索
            search_start = region->end[0];
        } else {
            if (r != ONIG_MISMATCH) {
                _wxLogError("Oniguruma search error: %d", r);
            }
            break; // 无匹配或到达文本末尾
        }
    }

    onig_region_free(region, 1);
    //   m_updatingIndicators = false;
}

#define SetLineFoldLevel(line, level) \
if(GetFoldLevel(line)!=level)SetFoldLevel(line,level)

void MySTC::UpdateCustomFolding() {
    int lineCount = GetLineCount();
    if (lineCount == 0) return;

    m_headerStack.clear();
    m_pendingEmptyLines.clear();

    for (int i = 0; i < lineCount; ++i) {
        int pos = PositionFromLine(i);
        int lineEndPos = GetLineEndPosition(i);
        int spaceCount = 0;
        bool isEmpty = true;
        bool curIsTitle = false;

        // 1. 扫描行特征
        while (pos < lineEndPos) {
            char c = GetCharAt(pos);
            if (c == ' ') spaceCount++;
            else if (c == '>') { curIsTitle = true; isEmpty = false; break; } else { isEmpty = false; break; }
            pos++;
        }

        // 2. 处理空行
        if (isEmpty) {
            m_pendingEmptyLines.push_back(i);
            continue;
        }

        // 3. 确定逻辑深度（解决问题 2 的关键）
        int curLogicDepth = 0;

        // 寻找当前行的父级标题
        // 规则：父级标题的物理缩进必须严格小于当前行的物理缩进
        while (!m_headerStack.empty() && m_headerStack.back().depth >= spaceCount) {
            m_headerStack.pop_back();
        }

        if (curIsTitle) {
            // 如果是标题行
            curLogicDepth = m_headerStack.empty() ? 0 : m_headerStack.back().depth + 1;
        } else {
            // 如果是内容行
            curLogicDepth = m_headerStack.empty() ? 0 : m_headerStack.back().depth + 1;
        }

        //  判定并设置折叠标记与空行层级
        if (!m_headerStack.empty() && curLogicDepth > m_headerStack.back().depth) {
            // 确认父标题为 Header
            int topRow = m_headerStack.back().line;
            SetLineFoldLevel(topRow, GetFoldLevel(topRow) | wxSTC_FOLDLEVELHEADERFLAG);

            for (int lineIdx : m_pendingEmptyLines) {
                SetLineFoldLevel(lineIdx, wxSTC_FOLDLEVELBASE + curLogicDepth);
            }
        } else {
            // 如果不构成包含关系（例如新标题比上一个标题浅，或者回到了顶级）
            // 空行层级跟随当前行的逻辑深度
            for (int lineIdx : m_pendingEmptyLines) {
                SetLineFoldLevel(lineIdx, wxSTC_FOLDLEVELBASE + curLogicDepth);
            }
        }
        m_pendingEmptyLines.clear();

        // 设置当前行并入栈
        SetLineFoldLevel(i, wxSTC_FOLDLEVELBASE + curLogicDepth);
        if (curIsTitle) {
            // 入栈时记录其实际物理缩进，作为后续行的判定依据
            m_headerStack.push_back({ i, spaceCount });
        }
    }
}
#undef SetLineFoldLevel

typedef struct {
    int cpMin; // 搜索的起始位置 (startPos)
    int cpMax; // 搜索的结束位置 (endPos)
} CharacterRange;

typedef struct {
    CharacterRange chrg;
    char* lpstrText;  // 要搜索的文本的 UTF-8 字节指针
    CharacterRange chrgFound; // 查找成功后，Scintilla 会将匹配项的范围填入这里
} TextToFind;

//#define SCI_SETMODEVENTMASK 2359
//#define SC_MOD_NONE 0
#define SCI_FINDTEXT 2150
#define SCI_INDICATORFILLRANGE 2504
#define SCI_POSITIONAFTER 2418
#define SC_MODEVENTMASKALL 0x7FFFFF
void MySTC::UpdateMatchesIndicator() {
    _wxLogDebug(wxT("%s [开始] 待测函数运行。"), GetCurrentTimestampWithMs());
    _wxLogDebug(wxT("%s [开始] 待测函数运行。渲染技术%d"), GetCurrentTimestampWithMs(), GetTechnology());
    //SendMsg(SCI_SETMODEVENTMASK, SC_MOD_NONE);
    int length = GetTextLength();
    if (length == 0)return;
    int totalLines = GetLineCount();
    int startLine = DocLineFromVisible(GetFirstVisibleLine());
    int endLine = std::min(startLine + LinesOnScreen(), totalLines - 1);
    // 获取可见区域的范围
    int startPos = PositionFromLine(startLine);
    int endPos = GetLineEndPosition(endLine);

    if (endPos > length) {
        endPos = length;
    }
    if (startPos < 0 || endPos <= startPos) {
        _wxLogDebug("Invalid text range from Matches: startPos=%d, endPos=%d", startPos, endPos);
        return;
    }
    // 清除可视区域内所有旧的指示器
    SetIndicatorCurrent(INDIC_MATCHES_HIGHLIGHT);
    IndicatorClearRange(startPos, endPos - startPos);

    auto selectedText = GetSelectedText();

    if (selectedText.IsEmpty() || selectedText.Find('\n') != wxNOT_FOUND || selectedText.Find(' ') != wxNOT_FOUND)return;
    Freeze();

    SetEvtHandlerEnabled(false);
    wxScopedCharBuffer utf8Buffer = selectedText.ToUTF8();
    const char* searchBytes = utf8Buffer.data();
    int searchLen = utf8Buffer.length(); // 以字节计的长度

    int searchFlags = wxSTC_FIND_MATCHCASE | wxSTC_FIND_WHOLEWORD; //匹配大小写
    SetSearchFlags(searchFlags);
    int matchEnd = -1;

    TextToFind ttf = { 0 };
    ttf.chrg.cpMax = endPos; // 终点保持不变
    ttf.lpstrText = (char*)searchBytes;

    while (true) {
        ttf.chrg.cpMin = startPos;

        // SCI_FINDTEXT(wParam=flags, lParam=TextToFind* 结构体指针)
        int foundPos = SendMsg(SCI_FINDTEXT, searchFlags, (LPARAM)&ttf);

        if (foundPos == -1) {
            break;
        }

        int matchStart = ttf.chrgFound.cpMin;
        int matchEnd = ttf.chrgFound.cpMax;

        // 填充指示器
        SendMsg(SCI_INDICATORFILLRANGE, matchStart, matchEnd - matchStart);

        // 更新下一次搜索的起始位置
        startPos = matchEnd;

        if (matchStart == matchEnd) {
            startPos = SendMsg(SCI_POSITIONAFTER, matchEnd);
        }

        if (startPos >= endPos) {
            break;
        }
    }
    //SendMsg(SCI_SETMODEVENTMASK, SC_MODEVENTMASKALL);
    Thaw();
    SetEvtHandlerEnabled(true);

    // HWND hwndSTC = (HWND)GetHandle();
    // // 使用 Win32 API 强制同步重绘
    // if (hwndSTC) {
    //     ::InvalidateRect(hwndSTC, NULL, TRUE);
    //     ::UpdateWindow(hwndSTC);
    // }
    _wxLogDebug(wxT("%s [结束] 待测函数运行。"), GetCurrentTimestampWithMs());
}

void MySTC::ShowImagePreview(int x, int y, wxString& url) {
    if (!url.StartsWith(L"file://")) return;
    if (url.Contains(L"file://./") && GetMyAppInstance()->GetPortableImageFold().IsEmpty()) return;

    wxString decodedUrl = url;
    decodedUrl.Replace(L"%20", L" ", true);
    wxString pathDecoded = decodedUrl.Mid(7);
    wxString pathRaw = url.Mid(7);

    auto GetValidPath = [](wxString p)->wxString {
        auto& portableImagesFold = GetMyAppInstance()->GetPortableImageFold();
        wxString targetPath = p;
        if (targetPath.StartsWith(L"./")) {
            if (portableImagesFold.IsEmpty())return wxEmptyString;
            wxFileName fn(targetPath);
            wxFileName finalFn(portableImagesFold, fn.GetFullName());
            targetPath = finalFn.GetFullPath();
        }
        if (wxFileName::FileExists(targetPath)) return targetPath;
        return wxEmptyString;
        };
    wxString finalPath = GetValidPath(pathDecoded);
    if (finalPath.IsEmpty()) {
        finalPath = GetValidPath(pathRaw);
    }
    if (finalPath.IsEmpty()) return;

    wxString lowerPath = pathDecoded.Lower();
    bool isImage = lowerPath.EndsWith(L".png") ||
        lowerPath.EndsWith(L".jpg") ||
        lowerPath.EndsWith(L".jpeg") ||
        lowerPath.EndsWith(L".bmp") ||
        lowerPath.EndsWith(L".webp");
    if (!isImage) return;
    if (!m_imagePreview) {
        m_imagePreview = new ImagePreviewPopup(this);
    }
    
    m_imagePreview->SetTarget(finalPath);
    _wxLogMessage(L"请求图片: %s", finalPath);
    m_imagePreview->RequestImage(finalPath);

    int screenX = x, screenY = y;
    ClientToScreen(&screenX, &screenY);
    m_imagePreview->SetPosition(wxPoint(screenX + 10, screenY + 10));
 }

void MySTC::UpdateRegionIndicators() {
    if (m_updatingCustomFolding) {
        UpdateCustomFolding();
        m_updatingCustomFolding = false;
    }
    UpdateUrlIndicators();
    UpdateColorIndicators();
    if (m_mainFrame->m_showMatchesIndicator) UpdateMatchesIndicator();
    m_updatingIndicators = false;
    // UpdateExtensionIndicators(m_currentType);
}

void MySTC::GetStyleData(std::vector<UINT16>& styles) {
    int endPos = GetTextLength();
    int currentStyle = 0;
    for (int i = 0; i < endPos; i++) {
        int nextStyle = GetStyleAt(i);
        if (nextStyle != currentStyle) {
            //保存样式
            styles.push_back(CompressStyleInfo(nextStyle, i));
            currentStyle = nextStyle;
        }
    }
}

void MySTC::SetStyleData(std::vector<UINT16>& styles) {
    if (styles.size() == 0)return;
    for (int i = 0; i < styles.size(); i++) {
        auto [style, position] = DecompressStyleInfo(styles[i]);
        int length;
        if (i == styles.size() - 1) {
            length = GetTextLength() - position;
        } else {
            auto [nextStyle, nextPosition] = DecompressStyleInfo(styles[i + 1]);
            length = nextPosition - position;
        }
        ApplyStyle(position, style, length);
    }
}

void MySTC::ApplyStyle(int pos, int style, int length) {
    StartStyling(pos);
    SetStyling(length, style);
}

void MySTC::FindOutlineRanges(std::vector<OutlineRange>& ranges) {
    ranges.clear();
    int docLength = GetTextLength();
    if (docLength == 0)return;
    int currentStyle = -1;
    for (int i = 0; i < docLength; i++) {
        int nextStyle = GetStyleAt(i);
        if (nextStyle != currentStyle) {
            ranges.emplace_back(i, nextStyle);
            currentStyle = nextStyle;
        }
    }
}

void MySTC::ExtractOutlineItems(const std::vector<OutlineRange>& ranges, std::vector<OutlineItem>& items) {
    items.clear();
    const int MAX_OUTLINE_LENGTH = 80; // 大纲标题的最大长度
    int docLength = GetTextLength();
    if (ranges.empty() || docLength == 0)return;
    for (int i = 0; i < ranges.size(); i++) {
        const OutlineRange& currentRangeInfo = ranges[i];
        int currentStyle = currentRangeInfo.styleId;
        if (currentStyle != STYLE_TITLE && currentStyle != STYLE_LEVEL1 && currentStyle != STYLE_LEVEL2) { continue; }

        int startPos = currentRangeInfo.position;
        int endPos;
        if (i + 1 < ranges.size()) {
            endPos = ranges[i + 1].position;
        } else {
            endPos = docLength;
        }
        if (startPos >= endPos)continue;

        wxString rawText = GetTextRange(startPos, endPos);
        wxArrayString lines = wxStringTokenize(rawText, wxT('\n'), wxTOKEN_RET_EMPTY_ALL);
        // 跟踪当前行在原始大纲块中的字节偏移量，用于重新计算文档位置
        int currentByteOffset = 0;

        // 遍历每一行文本
        for (wxString& line : lines) {
            // 计算当前行文本（包含尾部空白）的字节长度（用于更新偏移量）
            int lineByteLength = line.ToUTF8().length();

            // 提取有效内容并修剪
            wxString trimmedLine = line.Trim(true).Trim(false);

            if (!trimmedLine.IsEmpty()) {
                // 5. 格式化、截断和修饰
                wxString outputText = trimmedLine;
                if (outputText.length() > MAX_OUTLINE_LENGTH) {
                    // wxString::Left 截断是安全的（按字符）
                    outputText = outputText.Left(MAX_OUTLINE_LENGTH) + wxT("...");
                }

                // 6. 应用前缀
                wxString prefix = wxT("");
                if (currentStyle == STYLE_LEVEL1) {
                    prefix = wxT("* ");
                } else if (currentStyle == STYLE_LEVEL2) {
                    prefix = wxT("** ");
                }

                items.emplace_back(prefix + outputText, startPos + currentByteOffset);
            }

            // 8. 更新下一个子项的起始位置
            currentByteOffset += lineByteLength;

            // 跳过换行符
            // 注意：GetTextRange 返回的文本中，换行符占位符可能不同 (1-2 字节)。
            // 简单处理：如果原始块中该行后面还有内容，说明有换行符，至少跳过 1 字节（\n）
            if (startPos + currentByteOffset < endPos) {
                // 如果当前位置是 \r，跳过 \r
                if (GetCharAt(startPos + currentByteOffset) == '\r') {
                    currentByteOffset++;
                }
                // 如果当前位置是 \n，跳过 \n
                if (GetCharAt(startPos + currentByteOffset) == '\n') {
                    currentByteOffset++;
                }
            }
        }
    }
}

void MySTC::CustomLink(wxCommandEvent& event) {
    auto selected = GetSelectedText();
    int id = event.GetId() - MainFrame::MENU_CUSTOM_LINK_BASE;
    if (m_customLinks[id].id == 0)return;
    wxString command = ReplaceFirstOccurrence(m_customLinks[id].command, "%s", selected);
    if (m_customLinks[id].id == 1) {
        wxLaunchDefaultBrowser(command);
    } else if (m_customLinks[id].id == 2) {
        wxExecute(command);
    }
}

void MySTC::OnMouseMotion(wxMouseEvent& event) {
    event.Skip();
    wxPoint clientPt = event.GetPosition();
    int pos = PositionFromPoint(clientPt);
    bool ctrlPressed = event.ControlDown();
    // --- 1. 检查是否在有效的 URL 区域 ---
    if (IndicatorValueAt(INDIC_URL_HIGHLIGHT, pos) > 0) {
        if (pos >= GetTextLength()) {
            // 鼠标位置 pos 不在文件内容的末尾之后
            goto hide_preview;
        }
        int startPos = IndicatorStart(INDIC_URL_HIGHLIGHT, pos);
        // --- 2. 判断是否满足显示/切换条件 ---
        if (ctrlPressed) {
            if (m_lastHoverPos != startPos || (m_imagePreview && !m_imagePreview->IsShown())) {
                int endPos = IndicatorEnd(INDIC_URL_HIGHLIGHT, pos);
                wxString url = GetTextRange(startPos, endPos);

                m_lastHoverPos = startPos;
                ShowImagePreview(clientPt.x, clientPt.y, url);
                return;
            }
            return;
        } else {
            goto hide_preview;
        }
    }

    // --- 3. 隐藏预览的逻辑 ---
hide_preview:
    m_lastHoverPos = -1;
    if (m_imagePreview) {
        m_imagePreview->ClearTarget(); 
        if (m_imagePreview->IsShown()) {
            m_imagePreview->Hide();
        }
    }
}

void MySTC::OnMouseLeave(wxMouseEvent& event) {
    event.Skip();
    m_lastHoverPos = -1; 

    if (m_imagePreview) {
        m_imagePreview->ClearTarget(); 
        if (m_imagePreview->IsShown()) {
            m_imagePreview->Hide();
        }
    }
}

void MySTC::OnKeyDown2(wxKeyEvent& event) {
    if (event.GetKeyCode() != WXK_CONTROL) {
        event.Skip(); // 不是 Ctrl 键，继续处理
        return;
    }

    wxPoint screenPt = wxGetMousePosition();
    wxPoint clientPt = ScreenToClient(screenPt);
    int pos = PositionFromPoint(clientPt);
    int x = clientPt.x;
    int y = clientPt.y;

    if (IndicatorValueAt(INDIC_URL_HIGHLIGHT, pos) > 0) {
        //鼠标位置 pos 不在文件内容的末尾之后
        if (pos >= GetTextLength()) {
            event.Skip();
            return;
        }
        int startPos = IndicatorStart(INDIC_URL_HIGHLIGHT, pos);
        if (m_lastHoverPos != startPos || (m_imagePreview && !m_imagePreview->IsShown())) {
            // 窗口已显示，但 URL 位置已变更，更新预览
            int endPos = IndicatorEnd(INDIC_URL_HIGHLIGHT, pos);
            wxString url = GetTextRange(startPos, endPos);
            m_lastHoverPos = startPos;
            ShowImagePreview(clientPt.x, clientPt.y, url);
        }
        return;
    }
    // 如果按下 Ctrl 但不在 URL 上，或者不是图片 URL，则不做任何事
    event.Skip();
}

void MySTC::OnKeyUp(wxKeyEvent& event) {
    //ctrl键抬起强制隐藏预览窗口
    if (event.GetKeyCode() == WXK_CONTROL) {
        if (m_imagePreview && m_imagePreview->IsShown()) {
            m_imagePreview->Hide();
        }
        m_lastHoverPos = -1;
        return;
    }
    event.Skip();
}

void MySTC::OnUpdatingTimer(wxTimerEvent& event) {
    m_updatingIndicators = true;
    m_updatingCustomFolding = true;
}

void MySTC::RemoveEmptyLines() {
    int totalLines = GetLineCount();
    if (totalLines <= 0) {
        _wxLogDebug("Document is empty");
        return;
    }

    BeginUndoAction();
    // 从后向前遍历，避免行号偏移
    for (int line = totalLines - 1; line >= 0; line--) {
        int startPos = PositionFromLine(line);
        int endPos = GetLineEndPosition(line);
        int lineLength = GetLineLength(line); // 包含 EOL
        int contentLength = endPos - startPos; // 不含 EOL

        bool isEmpty = true;
        if (contentLength > 0) {
            wxString lineText = GetLine(line).Trim(true).Trim(false);
            isEmpty = lineText.IsEmpty();
        } else if (lineLength == 0 && line == totalLines - 1) {
            isEmpty = true;
        }

        if (isEmpty) {
            GotoLine(line);
            LineDelete();
        }
    }
    EndUndoAction();
}

void MySTC::SwitchLine(bool up) {
    int totalLines = GetLineCount();
    if (totalLines < 2) {
        return;
    }

    int currentLineNum = GetCurrentLine();
    int targetLineNum = up ? currentLineNum - 1 : currentLineNum + 1;

    if (up && currentLineNum == 0 || !up && currentLineNum == totalLines - 1) {
        return;
    }
    BeginUndoAction();
    // 1. 记录光标位置
    int cursorOffset = GetCurrentPos() - PositionFromLine(currentLineNum);
    int newCursorPos;

    // 2. 处理末行换行符问题
    bool isLastLineSwap = currentLineNum == totalLines - 1 || targetLineNum == totalLines - 1;
    if (isLastLineSwap) {
        InsertText(GetTextLength(), wxT("\n"));
        totalLines++;
    }

    // 3. 获取行文本和范围
    int currentLineStartPos = PositionFromLine(currentLineNum);
    int currentLineEndPos = PositionFromLine(currentLineNum + 1);
    wxString currentLineText = GetTextRange(currentLineStartPos, currentLineEndPos);

    int targetLineStartPos = PositionFromLine(targetLineNum);
    int targetLineEndPos = PositionFromLine(targetLineNum + 1);
    wxString targetLineText = GetTextRange(targetLineStartPos, targetLineEndPos);

    // 4. 确定一次性替换的完整范围
    int startPos = up ? targetLineStartPos : currentLineStartPos;
    int endPos = up ? currentLineEndPos : targetLineEndPos;

    // 5. 构建新的文本内容（已交换顺序）
    wxString combinedText = up ? currentLineText + targetLineText : targetLineText + currentLineText;

    // 6. 替换文本
    SetTargetStart(startPos);
    SetTargetEnd(endPos);
    ReplaceTarget(combinedText);

    EndUndoAction();

    // 7. 恢复光标位置
    newCursorPos = PositionFromLine(targetLineNum) + cursorOffset;
    GotoPos(newCursorPos);

    // 8. 删除多余的换行符
    if (isLastLineSwap) {
        // 在末尾删除之前添加的换行符
        SetTargetStart(GetTextLength() - 1);
        SetTargetEnd(GetTextLength());
        ReplaceTarget(wxString());
    }
}

void MySTC::InsertDateTime(int i) {
    wxDateTime now = wxDateTime::Now();
    wxString str;
    int offset = 0;
    if (i == 1) {
        str = now.Format("%y/%m/%d %H:%M");
    } else if (i == 2) {
        str = now.Format("%Y-%m-%d %H:%M:%S");
    } else {
        str = now.Format(L"%Y年%m月%d日 %H:%M:%S");
        offset = 6;
    }
    InsertText(GetCurrentPos(), str);
    GotoPos(GetCurrentPos() + str.length() + offset);
}

void MySTC::InsertSerials(wxArrayString& serials) {
    if (serials.empty()) return;
    int selCount = GetSelections();
    if (selCount <= 0) return;
    std::vector<int> originalCarets;
    originalCarets.reserve(selCount);
    for (int i = 0; i < selCount; ++i) {
        originalCarets.push_back(GetSelectionNCaret(i));
    }
    BeginUndoAction();

    for (int i = selCount - 1; i >= 0; --i) {
        int pos = originalCarets[i];
        wxString textToInsert = serials.Item(static_cast<size_t>(i) % serials.GetCount());

        SetTargetStart(pos);
        SetTargetEnd(pos);
        ReplaceTarget(textToInsert);
    }
    ClearSelections();
    int currentTotalOffset = 0;

    // 计算新位置
    for (int i = 0; i < selCount; ++i) {
        wxString textToInsert = serials.Item(static_cast<size_t>(i) % serials.GetCount());
        int insertedByteLen = textToInsert.ToUTF8().length();
        int newPos = originalCarets[i] + currentTotalOffset + insertedByteLen;
        if (i == 0) {
            SetSelection(newPos, newPos);
        } else {
            AddSelection(newPos, newPos);
        }
        currentTotalOffset += insertedByteLen;
    }

    EndUndoAction();
}

void MySTC::GetOutlineStyle(std::vector<OutlineItem>& items) {
    // 1. 查找大纲样式范围
    std::vector<OutlineRange> ranges;
    FindOutlineRanges(ranges);

    // 2. 根据范围提取并格式化大纲项目
    ExtractOutlineItems(ranges, items);
    m_mainFrame->m_outlineNeedUpdate = false;
}

void MySTC::SetStyleForSelected(wxCommandEvent& event) {
    if (GetReadOnly()) return;
    int styleId = event.GetId() - m_mainFrame->MENU_SET_STYLE_DEFAULT - 1 + STYLE_BOLD;
    int startPos = GetSelectionStart();
    int endPos = GetSelectionEnd();
    int length = endPos - startPos;
    if (length <= 0)return;
    m_styleModified = true;
    if (styleId == STYLE_BOLD - 1)styleId = 0;
    ApplyStyle(startPos, styleId, length);
    if (!m_isInitializing) {
        MarkCurrentNoteModified();
    }
}

void MySTC::UpdateSubStylesFont(const wxFont& contentFont, const wxString& englishFaceName) {
    int mainPointSize = contentFont.GetPointSize();
    int annotationPointSize = wxMax(1, (int)(mainPointSize * 0.75));
    // 行号字体与正文保持一致字号，贴近 Notepad++ 行号视觉效果
    StyleSetFont(wxSTC_STYLE_LINENUMBER, contentFont);
    if (!englishFaceName.IsEmpty()) {
        StyleSetFaceName(wxSTC_STYLE_LINENUMBER, englishFaceName);
    }
    wxFont subFont = contentFont;
    subFont.SetPointSize(annotationPointSize);
    StyleSetFont(wxSTC_EOLANNOTATION_BOXED, subFont);
    if (!englishFaceName.IsEmpty()) {
        StyleSetFaceName(wxSTC_EOLANNOTATION_BOXED, englishFaceName);
    }
}

void MySTC::UpdateLineNumberMarginWidth(bool force) {
    int lineCount = wxMax(1, GetLineCount());
    int digits = 1;
    for (int n = lineCount; n >= 10; n /= 10) {
        ++digits;
    }
    // 至少保留 2 位数字占位，并前置 "_" 留白，使宽度能随 DPI 和缩放比例同步自适应（Notepad++ 同款做法）
    int displayDigits = wxMax(digits, 2);
    if (!force && displayDigits == m_lastLineDigits) {
        return;
    }
    m_lastLineDigits = displayDigits;
    wxString sample = wxT("_") + wxString(displayDigits, wxT('9'));
    SetMarginWidth(0, TextWidth(wxSTC_STYLE_LINENUMBER, sample));
}

void MySTC::ChangeEditorFont(const wxFont& rawFont) {
    if (!rawFont.IsOk()) {
        return;
    }
    wxFont cleanFont(wxFontInfo(rawFont.GetPointSize()).FaceName(rawFont.GetFaceName()));
    if (!cleanFont.IsOk()) {
        cleanFont = rawFont;
    }
    wxString englishFaceName = GetEnglishFontFaceName(rawFont);
    int fontPointSize = cleanFont.GetPointSize();
    wxString targetFaceName = englishFaceName.IsEmpty() ? cleanFont.GetFaceName() : englishFaceName;

    StyleSetFont(0, cleanFont);
    if (!targetFaceName.IsEmpty()) {
        StyleSetFaceName(0, targetFaceName);
    }
    ChangeAllStyleFont(fontPointSize, targetFaceName);
    UpdateSubStylesFont(cleanFont, targetFaceName);
    UpdateLineNumberMarginWidth(true);
}

void MySTC::MarkCurrentNoteModified() {
    if (GetReadOnly() || m_isInitializing || !m_mainFrame || !m_mainFrame->m_tree) {
        return;
    }
    auto item = m_mainFrame->m_tree->GetSelection();
    if (item.IsOk()) {
        auto name = m_mainFrame->m_tree->GetItemText(item);
        if (!name.EndsWith("*")) {
            name += "*";
            m_mainFrame->m_tree->SetItemText(item, name);
        }
    }
    m_mainFrame->ScheduleAutoSave();
}

void MySTC::ChangeAllStyleFont(int pointSize, const wxString& faceName) {
    for (int i = STYLE_BOLD; i < STYLE_LEVEL2 + 1; i++) {
        if (!faceName.IsEmpty()) {
            StyleSetFaceName(i, faceName);
        }
        StyleSetSize(i, pointSize);
    }
}

void MySTC::ChangeAllStyleForeground(wxColour& color) {
    for (int i : {STYLE_BOLD, STYLE_YELLOW_BG, STYLE_PINK_BG, STYLE_LIGHTBLUE_BG, STYLE_TITLE, STYLE_LEVEL1, STYLE_LEVEL2}) {
        StyleSetForeground(i, color);
    }
}

void MySTC::ChangeAllStyleBackground(wxColour& color) {
    for (int i : { STYLE_BOLD, STYLE_ITALICGREY, STYLE_RED, STYLE_BLUE, STYLE_TITLE, STYLE_LEVEL1, STYLE_LEVEL2}) {
        StyleSetBackground(i, color);
    }
}

void MySTC::ApplyTheme(bool isDark) {
    HWND hwndSTC = (HWND)GetHWND();
    if (hwndSTC) {
        ::SetWindowTheme(hwndSTC, isDark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    }
    wxColour bgColor = isDark ? ThemeColours::DarkBgBase : ThemeColours::LightBgBase;
    wxColour fgColor = isDark ? ThemeColours::DarkTextPrimary : ThemeColours::LightTextPrimary;
    wxColour marginBg = isDark ? ThemeColours::DarkBgSecondary : ThemeColours::LightBgSecondary;
    wxColour marginFg = isDark ? ThemeColours::DarkTextSecondary : ThemeColours::LightLineNumberFg;
    wxColour selBg = isDark ? ThemeColours::DarkSelection : ThemeColours::LightSelection;
    wxColour caretColor = isDark ? ThemeColours::DarkCaret : ThemeColours::LightCaret;

    StyleSetBackground(0, bgColor);
    StyleSetForeground(0, fgColor);
    StyleSetBackground(wxSTC_STYLE_DEFAULT, bgColor);
    StyleSetForeground(wxSTC_STYLE_DEFAULT, fgColor);

    SetCaretForeground(caretColor);
    SetSelBackground(true, selBg);

    StyleSetBackground(wxSTC_STYLE_LINENUMBER, marginBg);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, marginFg);

    ChangeAllStyleBackground(bgColor);
    ChangeAllStyleForeground(fgColor);
    InitStyles();
    UpdateLineNumberMarginWidth(true);
    Refresh();
}

MySTC::~MySTC() {
    if (m_updatingTimer) {
        if (m_updatingTimer->IsRunning()) {
            m_updatingTimer->Stop();
        }
        delete m_updatingTimer;
        m_updatingTimer = nullptr;
    }
    if (s_urlRegex) onig_free(s_urlRegex);
    if (s_colorRegex) onig_free(s_colorRegex);
    onig_end();
}

void MySTC::LoadContent(wxString& content, int state, std::vector<UINT16>& styles) {
    bool readOnly = state & 1;
    int stateIcon = state >> 1;
    //根据图标判断
    if ((stateIcon & 0xF) == TREE_ICON_STYLE_LOCKED) readOnly = 1;
   
    SetReadOnly(0);
    SetInitializingState(true);
    //m_currentType = stateIcon;
    ClearAll();
    SetText(content);
    SetStyleData(styles);
    EmptyUndoBuffer();
    GotoPos(0);
    SetReadOnly(readOnly);
    SetSavePoint();
    m_styleModified = false;
    m_updatingCustomFolding = true;
    UpdateLineNumberMarginWidth(true);
    UpdateRegionIndicators();
    SetInitializingState(false);
    initBind2();
}

void MySTC::initBind2() {
    Bind(wxEVT_STC_UPDATEUI, &MySTC::OnStcUpdateUI, this);
}

void MySTC::OnStcUpdateUI(wxStyledTextEvent& event) {
    int updatedFlags = event.GetUpdated();
    if (updatedFlags & wxSTC_UPDATE_V_SCROLL) {
        UpdateRegionIndicators();
    } else {
        if (m_mainFrame->m_showMatchesIndicator) UpdateMatchesIndicator();
    }
    wxString statusText = wxString::Format(L"%d字节，%d行", GetTextLength(), GetLineCount());
    m_mainFrame->SetStatusText(statusText, 1);
    int selectionStart = GetSelectionStart();
    int selectionEnd = GetSelectionEnd();
    int length;
    wxString statustext2 = wxString::Format(L"当前行：%d，列：%d", GetCurrentLine() + 1, GetColumn(GetCurrentPos()));
    if (selectionStart != selectionEnd) {
        length = GetSelectedText().length();
        statustext2 += wxString::Format(L"，选中：%d字符", length);
    }
    m_mainFrame->SetStatusText(statustext2, 2);
    //UpdateMatchesIndicator();
    //event.Skip();
}

void MySTC::OnStcModified(wxStyledTextEvent& event) {
    int modType = event.GetModificationType();
    m_mainFrame->m_outlineNeedUpdate = true;
    // 1. 只有文本内容发生变化时，才设置 m_updatingIndicators = true;
    //    SC_MOD_INSERTTEXT 和 SC_MOD_DELETETEXT 代表用户对文本的编辑。
    //    SC_MOD_CHANGESTYLE 和 SC_MOD_CHANGEINDICATOR 应该被忽略，因为它们是内部操作。
    if (modType & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT)) {
        UpdateLineNumberMarginWidth(false);
        /*m_updatingIndicators = true;
        m_updatingCustomFolding = true;*/
        m_updatingTimer->Start(300, wxTIMER_ONE_SHOT);
        // 只有在文本发生变化时才修改 TreeCtrl 节点名称并调度实时保存
        if (!GetReadOnly() && !m_isInitializing && GetModify()) {
            MarkCurrentNoteModified();
        }
    }

    event.Skip();
}

void MySTC::OnStcPainted(wxStyledTextEvent& event) {
    if (m_updatingIndicators) {
        UpdateRegionIndicators();
    }
}

void MySTC::OnStcDwellStart(wxStyledTextEvent& event) {
    int pos = event.GetPosition();

    wxMouseState mouseState = wxGetMouseState();
    wxPoint clientPt(event.GetX(), event.GetY());

    if (pos == -1) {
        if (m_imagePreview && m_imagePreview->IsShown()) {
            m_imagePreview->Hide();
        }
        m_lastHoverPos = -1;
        return;
    }

    if (IndicatorValueAt(INDIC_COLOR_FULLBOX, pos) != 0) {
        auto colorValue = static_cast<unsigned int>(IndicatorValueAt(INDIC_COLOR_FULLBOX, pos));
        wxColour hoverColor;
        hoverColor.SetRGBA(colorValue);
        IndicatorSetAlpha(INDIC_COLOR_FULLBOX, hoverColor.Alpha());
        IndicatorSetHoverForeground(INDIC_COLOR_FULLBOX, hoverColor);
        wxColour foreground = GetInvertedColour(hoverColor);
        IndicatorSetHoverForeground(INDIC_COLOR_TEXTFORE, foreground);
    }
}

void MySTC::OnStcIndicatorReleased(wxStyledTextEvent& event) {
    int pos = event.GetPosition();
    wxMouseState mouseState = wxGetMouseState();
    //检查鼠标是否在 URL 指示器上，并且按下了 Ctrl 键
    if ((IndicatorValueAt(INDIC_URL_HIGHLIGHT, pos) > 0) && mouseState.ControlDown()) {
        int startPos = IndicatorStart(INDIC_URL_HIGHLIGHT, pos);
        int endPos = IndicatorEnd(INDIC_URL_HIGHLIGHT, pos);
        wxString url = GetTextRange(startPos, endPos);
        if (url.IsEmpty()) return;

        if (url.StartsWith(L"file://")) {
            auto& portableImagesFold = GetMyAppInstance()->GetPortableImageFold();

            auto GetValidPath = [&](wxString p) -> wxString {
                wxString path = p.Mid(7);
                if (path.StartsWith(L"./")) {
                    if (portableImagesFold.IsEmpty()) return wxEmptyString;
                    wxFileName fn(path);
                    wxFileName finalFn(portableImagesFold, fn.GetFullName());
                    path = finalFn.GetFullPath();
                }
                if (wxFileName::FileExists(path)) return path;
                return wxEmptyString;
                };

            wxString decodedUrl = url;
            decodedUrl.Replace(L"%20", L" ", true);
            wxString finalPath = GetValidPath(decodedUrl);

            if (finalPath.IsEmpty())finalPath = GetValidPath(url);
            if (!finalPath.IsEmpty()) {
                _wxLogMessage("打开本地文件: %s", finalPath);
                wxLaunchDefaultApplication(finalPath);
                return;
            }
        }

        url.Replace(L"%20", L" ", true);
        _wxLogMessage("URL clicked: %s", url);
        wxLaunchDefaultApplication(url);
    }
}

void MySTC::OnStcCharAdded(wxStyledTextEvent& event) {
    int const key = event.GetKey();
    /*_wxLogMessage("OnStcCharAdded: %d", key);
    _wxLogMessage("Current position: %d", GetStyleAt(GetCurrentPos()));*/
    switch (key) {
    case '\n':
    {
        int prevLine = GetCurrentLine() - 1;
        if (prevLine < 0) return;
        int prevLineEndPos = GetLineEndPosition(prevLine);
        int posBefore = prevLineEndPos;
        char chBefore = GetCharAt(posBefore);
        for (; IsBlankChar(chBefore) && (posBefore > 0);) {
            posBefore = PositionBefore(posBefore);
            chBefore = GetCharAt(posBefore);
        }
        if (chBefore == '=') {
            wxCharBuffer line_utf8 = GetLineRaw(prevLine);
            char* p = line_utf8.data();
            char* end = p + strlen(p);
            for (; p < end && (*(end - 1) == '=' || IsBlankChar(*(end - 1))); --end) {}
            *end = '\0';
            double dExprEval = 0.0;
            int exprErr = 1;
            while (p < end && exprErr) {
                // 跳过非 ASCII 字符
                if ((*p & 0x80) != 0) {
                    do {
                        ++p;
                    } while ((*p & 0x80) != 0);
                    continue;
                }
                dExprEval = te_interp(p, &exprErr);
                if (exprErr) {
                    // 如果求值出错，将指针移到下一个可能的表达式起点
                    while (++p < end && !(te_is_num(p) || te_is_op(p))) {
                        // 再次跳过非 ASCII 字符
                        if ((*p & 0x80) != 0) {
                            do {
                                ++p;
                            } while ((*p & 0x80) != 0);
                        }
                    }
                }
            }
            if (!exprErr) {
                // 检查结果是否为整数
                double intpart = 0.0;
                double fracpart = modf(dExprEval, &intpart);
                wxString resultStr;
                if (fabs(fracpart) < 1.0e-15) { // 使用一个小的阈值来判断浮点数是否接近于整数(sin(pi)结果阈值范围）
                    resultStr.Printf("%.21G", intpart);
                } else {
                    resultStr.Printf("%.8G", dExprEval);
                }
                InsertText(posBefore + 1, resultStr);
            }
        }
        {//自动缩进
            int lineInd = GetLineIndentation(prevLine);
            if (lineInd > 0) {
                int currentLine = GetCurrentLine();
                SetLineIndentation(currentLine, lineInd);
                GotoPos(PositionFromLine(currentLine) + lineInd);
            }
        }
        return;
    }
    case '=':
    {
        int endPos = GetCurrentPos();
        if (endPos >= 4 && GetCharAt(endPos - 2) == '=' && GetCharAt(endPos - 3) == '=' && GetCharAt(endPos - 4) == '=' && endPos - 4 == PositionFromLine(GetCurrentLine())) {
            CallAfter([this, endPos] {BeginUndoAction(); InsertText(endPos, "====================================\r\n"); GotoPos(PositionFromLine(GetCurrentLine() + 1)); EndUndoAction(); });
        }
        return;
    }
    case '*':
    {
        int endPos = GetCurrentPos();
        if (endPos >= 4 && GetCharAt(endPos - 2) == '*' && GetCharAt(endPos - 3) == '*' && GetCharAt(endPos - 4) == '*' && endPos - 4 == PositionFromLine(GetCurrentLine())) {
            CallAfter([this, endPos] {BeginUndoAction(); InsertText(endPos, "************************************\r\n"); GotoPos(PositionFromLine(GetCurrentLine() + 1)); EndUndoAction(); });
        }
        return;
    }
    case '#':
    {
        int endPos = GetCurrentPos();
        if (endPos >= 4 && GetCharAt(endPos - 2) == '#' && GetCharAt(endPos - 3) == '#' && GetCharAt(endPos - 4) == '#' && endPos - 4 == PositionFromLine(GetCurrentLine())) {
            CallAfter([this, endPos] {BeginUndoAction(); InsertText(endPos, "####################################\r\n"); GotoPos(PositionFromLine(GetCurrentLine() + 1)); EndUndoAction(); });
        }
        return;
    }
    case '-':
    {
        int endPos = GetCurrentPos();
        if (endPos >= 4 && GetCharAt(endPos - 2) == '-' && GetCharAt(endPos - 3) == '-' && GetCharAt(endPos - 4) == '-' && endPos - 4 == PositionFromLine(GetCurrentLine())) {
            CallAfter([this, endPos] {BeginUndoAction(); InsertText(endPos, "------------------------------------\r\n"); GotoPos(PositionFromLine(GetCurrentLine() + 1)); EndUndoAction(); });
        }
        return;
    }
    }
}

void MySTC::OnMarginClicked(wxStyledTextEvent& event) {
    if (event.GetMargin() == 2) {
        int lineClick = LineFromPosition(event.GetPosition());
        int levelClick = GetFoldLevel(lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold(lineClick);
        }
    }
}

void MySTC::OnSizeNew(wxSizeEvent& event) {
    OnSize(event);
    UpdateRegionIndicators();
}

void MySTC::OnStcZoom(wxStyledTextEvent& event) {
    // wxMessageBox("OnStcZoom事件");
    UpdateLineNumberMarginWidth(true);
    UpdateRegionIndicators();
    event.Skip();
}

void MySTC::OnContextMenu(wxCommandEvent& event) {
    wxMenu Menu;

    wxMenu* lineMenu = new wxMenu();
    wxMenu* insertMenu = new wxMenu();

    Menu.Append(MainFrame::MENU_UNDO, L"撤销\tCtrl-Z");
    Menu.Append(MainFrame::MENU_REDO, L"恢复\tCtrl-Y");
    Menu.AppendSeparator();
    Menu.Append(MainFrame::MENU_SELECTALL, L"全选\tCtrl-A");
    Menu.Append(MainFrame::MENU_CUT, L"剪切\tCtrl-X");
    Menu.Append(MainFrame::MENU_COPY, L"复制\tCtrl-C");
    Menu.Append(MainFrame::MENU_PASTE, L"粘贴\tCtrl-V");
    Menu.AppendSeparator();

    lineMenu->Append(MainFrame::MENU_LINE_UP, L"上移当前行\tCtrl-Up");
    lineMenu->Append(MainFrame::MENU_LINE_DOWN, L"下移当前行\tCtrl-Down");
    lineMenu->Append(MainFrame::MENU_REMOVE_EMPTYLINE, L"删除空行");

    insertMenu->Append(MainFrame::MENU_INSERT_DATE_TIME1, L"日期和时间(短格式)");
    insertMenu->Append(MainFrame::MENU_INSERT_DATE_TIME2, L"日期和时间(长格式)");
    insertMenu->Append(MainFrame::MENU_INSERT_DATE_TIME3, L"日期和时间(中文日期)");
    //    insertMenu->Append(MainFrame::MENU_INSERT_SYMBOL, L"符号");
    Menu.AppendSubMenu(lineMenu, L"行操作");
    Menu.AppendSubMenu(insertMenu, L"插入");
    wxMenu* styleMenu = new wxMenu;
    styleMenu->Append(MainFrame::MENU_SET_STYLE_DEFAULT, L"清除格式", L"恢复默认格式");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_TITLE, L"标题", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_LEVEL1, L"大纲级别一", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_LEVEL2, L"大纲级别二", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_BOLD, L"粗体", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_ITALICGREY, L"不醒目标记", L"灰色斜体");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_RED, L"红色", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_BLUE, L"蓝色", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_YELLOW_BG, L"黄色背景", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_PINK_BG, L"洋红色背景", L"");
    styleMenu->Append(MainFrame::MENU_SET_STYLE_LIGHTBLUE_BG, L"浅蓝色背景", L"");
    Menu.AppendSubMenu(styleMenu, L"样式");

    for (int i = 0; i < 5; i++) {
        if (m_customLinks[i].id == 0) continue;
        Menu.Append(MainFrame::MENU_CUSTOM_LINK_BASE + i, m_customLinks[i].name);
    }

    PopupMenu(&Menu);

}

CompositeDropTarget::CompositeDropTarget(MySTC* stc) :wxDropTarget(), m_stc(stc) {
    wxDataObjectComposite* data = new wxDataObjectComposite();
    data->Add(new wxTextDataObject(), true);
    data->Add(new wxFileDataObject());
    SetDataObject(data);
}

bool CompositeDropTarget::OnDrop(wxCoord x, wxCoord y) {
    if (GetDataObject()->IsSupported(wxDF_UNICODETEXT) || GetDataObject()->IsSupported(wxDF_FILENAME)) {
        return wxDragCopy;
    }
    return wxDragNone;
}

wxDragResult CompositeDropTarget::OnData(wxCoord x, wxCoord y, wxDragResult defResult) {
    if (!GetData()) return wxDragNone;

    wxDataObjectComposite* dataobjComp = static_cast<wxDataObjectComposite*>(GetDataObject());
    wxDataFormat format = dataobjComp->GetReceivedFormat();
    wxDataObject* dataobj = dataobjComp->GetObject(format);
    wxString textToInsert;

    bool isCtrlDown = wxGetKeyState(WXK_CONTROL);

    switch (format.GetType()) {
    case wxDF_TEXT:
    case wxDF_UNICODETEXT:
    {
        wxTextDataObject* textData = static_cast<wxTextDataObject*>(dataobj);
        textToInsert = textData->GetText();
        break;
    }
    case wxDF_FILENAME:
    {
        wxFileDataObject* fileData = static_cast<wxFileDataObject*>(dataobj);
        auto& files = fileData->GetFilenames();
        auto& portableImagesFold = GetMyAppInstance()->GetPortableImageFold();
        wxFileName targetDirFn;
        if (GetMyAppInstance()->GetPortableMode()) {
            targetDirFn = wxFileName::DirName(portableImagesFold);
        }
        textToInsert.Empty();

        for (const wxString& filePath : files) {
            wxFileName fn(filePath);
            wxString finalPath = filePath;

            if (GetMyAppInstance()->GetPortableMode()) {
                wxFileName currentDirFn;
                currentDirFn.AssignDir(fn.GetPath(wxPATH_GET_VOLUME));
                bool alreadyInTarget = (targetDirFn == currentDirFn);
                if (alreadyInTarget) {
                    finalPath = wxT("./wxNotePadFiles/") + fn.GetFullName();
                } else if (isCtrlDown) {
                    if (!wxDirExists(portableImagesFold)) wxMkdir(portableImagesFold);
                    wxString nameOnly = fn.GetName();
                    wxFileName targetFn(portableImagesFold, fn.GetFullName());
                    int counter = 1;
                    //文件名冲突处理
                    while (targetFn.FileExists()) {
                        targetFn.SetName(wxString::Format(wxT("%s_%d"), nameOnly, counter));
                        counter++;
                    }
                    if (wxCopyFile(filePath, targetFn.GetFullPath())) {
                        finalPath = L"./wxNotePadFiles/" + targetFn.GetFullName();
                    }
                }
            }
            wxString convertedPath = finalPath;
            convertedPath.Replace(wxT("\\"), wxT("/"), true);
            convertedPath.Prepend(wxT("file://"));
            convertedPath.Replace(wxT(" "), wxT("%20"), true);

            textToInsert += convertedPath;
            textToInsert += wxT("\n");
        }
        break;
    }
    default:
        _wxLogMessage(wxT("拖放了不支持的格式 (%s)。"), format.GetId());
        return wxDragNone;
    }

    // 4. 统一插入文本到 STC 控件
    if (!textToInsert.IsEmpty() && m_stc) {
        // 使用 OnDrop/OnData 传递进来的坐标 (x, y) 确定插入位置
        int position = m_stc->PositionFromPoint(wxPoint(x, y));
        int textLength = m_stc->GetTextLength();
        if (position < 0 || position > textLength) {
            position = textLength;
        }
        m_stc->InsertText(position, textToInsert);
        std::string text_utf8{ textToInsert.ToUTF8() };
        m_stc->GotoPos(position + text_utf8.length());

        return wxDragCopy;
    }

    return wxDragNone;
}

ImagePreviewPopup::ImagePreviewPopup(wxWindow* parent) : wxPopupWindow(parent, wxBORDER_SIMPLE) {
    wxConfigBase* config = wxConfigBase::Get();
    m_maxsize = config->ReadLong("Scintilla/ImagePreviewMaxSize", 500);
    if (m_maxsize < 50)m_maxsize = 50;
    m_bitmap = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_bitmap, 1, wxEXPAND | wxALL, 0); // 图片周围留一点边距
    SetSizerAndFit(sizer);
    m_expiryTimer.SetOwner(this);
    Bind(wxEVT_TIMER, &ImagePreviewPopup::OnTimer, this);
    Bind(wxEVT_THREAD, &ImagePreviewPopup::OnImageLoaded, this);
}

void ImagePreviewPopup::RequestImage(const wxString& filePath) {
    if (!wxFileExists(filePath))return;
    m_targetPath = filePath;

    wxImage* cached = GetFromCache(filePath);
    if (cached && cached->IsOk() && cached->GetWidth() > 0) {
        _wxLogMessage(L"图片在缓存中: %s", filePath);
        m_bitmap->SetBitmap(wxBitmap(*cached));
        this->Layout();
        this->Fit();
        if (!this->IsShown()) this->Show();
        return;
    } else if (cached){
        _wxLogMessage(L"缓存图片失效了: %s", filePath);
        auto it = m_cacheMap.find(filePath);
        if (it != m_cacheMap.end()){
            m_cacheList.erase(it->second);
            m_cacheMap.erase(it);
        }
    }
    if (m_loadingPaths.count(filePath)) {
        _wxLogMessage(L"已经在请求缓存了，但尚未准备好: %s", filePath);
        return;
    }
    m_loadingPaths.insert(filePath);

    std::thread([this, filePath]() {
        wxImage image;
        _wxLogMessage(L"后台载入图片中: %s", filePath);
        if (image.LoadFile(filePath)) {
            _wxLogMessage(L"载入完成: %s", filePath);
            int w = image.GetWidth();
            int h = image.GetHeight();
            if (w > m_maxsize || h > m_maxsize) {
                double scale = std::min((double)m_maxsize / w, (double)m_maxsize / h);
                image.Rescale(w * scale, h * scale);
            }

            wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, wxID_ANY);
            evt->SetPayload(image);
            evt->SetString(filePath);
            wxQueueEvent(this, evt);
        } else {
            wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, wxID_ANY);
            evt->SetString(L"LOAD_FAILED:" + filePath);
            wxQueueEvent(this, evt);
        }
        }).detach();
}

void ImagePreviewPopup::OnImageLoaded(wxThreadEvent& event) {
    wxString loadedPath = event.GetString();
    if (loadedPath.StartsWith(L"LOAD_FAILED:")) {
        m_loadingPaths.erase(loadedPath.Mid(12));
        return;
    }
    m_loadingPaths.erase(loadedPath);

    wxImage img = event.GetPayload<wxImage>();
    
    if (img.IsOk()) {
        _wxLogMessage(L"图片读取完成并加入至缓存: %s", loadedPath);
        AddToCache(loadedPath, img);
    }
    if (loadedPath == m_targetPath && !m_targetPath.IsEmpty()) {
        wxBitmap bmp(img);
        m_bitmap->SetBitmap(bmp);
        this->Layout();
        this->Fit();
        if (!this->IsShown()) this->Show();
    }
}

void ImagePreviewPopup::AddToCache(const wxString& path, const wxImage& img) {
    if (!img.IsOk()){
        _wxLogMessage(L"bitmap无效: %s", path);
        return;
    }
    
    auto it = m_cacheMap.find(path);
    if (it != m_cacheMap.end()) {
        m_cacheList.erase(it->second);
        m_cacheMap.erase(it);
    }

    if (m_cacheList.size() >= MAX_CACHE_SIZE) {
        m_cacheMap.erase(m_cacheList.back().path);
        m_cacheList.pop_back();
    }

    m_cacheList.push_front({ path, img });
    m_cacheMap[path] = m_cacheList.begin();
    
    m_expiryTimer.Start(expiryTime, wxTIMER_ONE_SHOT);
}

wxImage* ImagePreviewPopup::GetFromCache(const wxString& path) {
    if (m_cacheMap.count(path)){
        auto it = m_cacheMap[path];
        if (it->bitmap.IsOk()){ 
            m_cacheList.splice(m_cacheList.begin(), m_cacheList, it);
            return &m_cacheList.front().bitmap;
        } else{
            _wxLogMessage(L"图片缓存失效: %s", path);
            m_cacheMap.erase(path);
            m_cacheList.erase(it);
        }
    }
    return nullptr;
}
