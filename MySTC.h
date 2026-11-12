#pragma once
#include "pch.h"
#include <set>
#define HYPLNK_REGEX_FULL  "\(?:(?:https?|ftp|file)://|www\\.|ftp\\.)(?:\\./|[^\\s/$.?#]).[^\\s]*"
static const char* s_pUrlRegEx = HYPLNK_REGEX_FULL;
static const char* s_pColorRegEx = "#([0-9a-fA-F]){8}|#([0-9a-fA-F]){6}";

class MainFrame;

struct OutlineItem;
struct OutlineRange {
    int position;   // 样式变化的起始字节位置
    int styleId;    // 样式 ID
};

class ImagePreviewPopup : public wxPopupWindow {
public:
    ImagePreviewPopup(wxWindow* parent);
    void RequestImage(const wxString& filePath);
    void SetTarget(const wxString& filePath) { m_targetPath = filePath; }
    void ClearTarget() { m_targetPath = wxEmptyString; }
    int m_maxsize = 500;
private:
    void OnTimer(wxTimerEvent& event) { CleanCache(); };
    void OnImageLoaded(wxThreadEvent& event);

    wxStaticBitmap* m_bitmap;
    wxTimer m_expiryTimer;
    wxString m_targetPath;
    struct CacheItem {
        wxString path;
        wxImage bitmap;
    };
    std::set<wxString> m_loadingPaths;
    std::list<CacheItem> m_cacheList;
    std::unordered_map<wxString, std::list<CacheItem>::iterator> m_cacheMap;
    const size_t MAX_CACHE_SIZE = 20;
    const int expiryTime = 30 * 60 * 1000; // 30分钟超时
    void AddToCache(const wxString& path, const wxImage& img);
    wxImage* GetFromCache(const wxString& path);
    void CleanCache() {
        m_cacheMap.clear();
        m_cacheList.clear();
    };
};

class MySTC : public wxStyledTextCtrl {
public:
    bool m_styleModified = false;//标识样式的更改

    MySTC(wxWindow* parent, MainFrame* mainFrame, wxWindowID id);
    ~MySTC();
    void LoadContent(wxString& content, int state, std::vector<UINT16>& styles);
    void GetStyleData(std::vector<UINT16>& styles);
    void SetStyleData(std::vector<UINT16>& styles);
    void InitHotLinks();
    void RemoveEmptyLines();
    void SwitchLine(bool);
    void InsertDateTime(int);
    void InsertSerials(wxArrayString& serials);

    void GetOutlineStyle(std::vector<OutlineItem>& items);
    void SetStyleForSelected(wxCommandEvent& event);
    //使用这三个函数来设置所有样式的同步修改
    void ChangeAllStyleFont(int pointSize, const wxString& faceName);
    void ChangeAllStyleBackground(wxColour& color);
    void ChangeAllStyleForeground(wxColour& color);
    void ChangeEditorFont(const wxFont& rawFont);
    void UpdateLineNumberMarginWidth(bool force = false);

    void SetPreviewPicLength(int length) { m_imagePreview->m_maxsize = length; }
    enum STYLE_TYPE {
        STYLE_BOLD = 40,
        STYLE_ITALICGREY,
        STYLE_RED,
        STYLE_BLUE,
        STYLE_YELLOW_BG,
        STYLE_PINK_BG,
        STYLE_LIGHTBLUE_BG,
        STYLE_TITLE,
        STYLE_LEVEL1,
        STYLE_LEVEL2,
    };
private:
    enum INDICATOR_TYPE {
        INDIC_URL_HIGHLIGHT = 8,
        INDIC_URL_UNDERLINE,
        //INDIC_COLOR_UNDERLINE,
        INDIC_COLOR_FULLBOX,
        INDIC_COLOR_TEXTFORE,
        INDIC_MATCHES_HIGHLIGHT,
        //INDIC_ALARM_MATCH,//显示生效中、过期等状态
        //INDIC_LINK_MATCH,
    };

    struct CustomLinkData {
        int id;
        wxString name;
        wxString command;
    };
    // 悬停预览窗口实例
    ImagePreviewPopup* m_imagePreview = nullptr;

    // 跟踪上一个悬停的位置，避免重复处理
    int m_lastHoverPos = -1;

    CustomLinkData m_customLinks[5] = { 0 };

    bool m_updatingIndicators = false;
    bool m_updatingCustomFolding = false;
    bool m_isInitializing = true;
    int m_lastLineDigits = 0;
    wxTimer* m_updatingTimer;
    //int m_currentType = 0;
    MainFrame* m_mainFrame;
    OnigRegex s_urlRegex;
    OnigRegex s_colorRegex;
    //OnigRegex s_ALARMRegex;
    //OnigRegex s_LINKRegex;
    struct HeaderInfo { int line; int depth; };
    std::vector<HeaderInfo> m_headerStack;
    std::vector<int> m_pendingEmptyLines;

    void initBind();
    void initBind2();
    void OnStcUpdateUI(wxStyledTextEvent& event);
    void OnStcZoom(wxStyledTextEvent& event);
    void OnStcModified(wxStyledTextEvent& event);
    void OnStcPainted(wxStyledTextEvent& event);
    void OnStcDwellStart(wxStyledTextEvent& event);
    //void OnStcDwellEnd(wxStyledTextEvent& event);
    void OnStcIndicatorReleased(wxStyledTextEvent& event);
    void OnStcCharAdded(wxStyledTextEvent& event);
    void OnMarginClicked(wxStyledTextEvent& event);
    void OnSizeNew(wxSizeEvent& event);
    void OnContextMenu(wxCommandEvent& event);
    void CustomLink(wxCommandEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);
    void OnKeyDown2(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);
    void OnUpdatingTimer(wxTimerEvent& event);
    // 核心功能函数
    void InitRegExprAndIndicators();
    void InitStyles();
    void UpdateUrlIndicators();
    void UpdateColorIndicators();
    void UpdateCustomFolding();
    //void SetLineFoldLevel(int line, int level);
    void UpdateMatchesIndicator();
    void ShowImagePreview(int x, int y, wxString& url);
    //所有的可视范围内更新indicator都在同样的事件处理函数中调用。
    void UpdateRegionIndicators();
    //void UpdateExtensionIndicators(int);//根据节点类型专用的指示器
    void SetInitializingState(bool isInitializing) { m_isInitializing = isInitializing; };
    //bool IsMouseInLineText(wxPoint pt, int pos);
    void ApplyStyle(int pos, int style, int length);
    void UpdateSubStylesFont(const wxFont& contentFont, const wxString& englishFaceName);
    void MarkCurrentNoteModified();
    void FindOutlineRanges(std::vector<OutlineRange>& ranges);
    void ExtractOutlineItems(const std::vector<OutlineRange>& ranges, std::vector<OutlineItem>& items);
};

class CompositeDropTarget : public wxDropTarget {
public:
    CompositeDropTarget(MySTC* stc);
    virtual bool OnDrop(wxCoord x, wxCoord y) override;
    virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult defResult) override;
private:
    MySTC* m_stc;
};


