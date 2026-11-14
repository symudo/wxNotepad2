#pragma once
#ifndef _APP_H_
#define _APP_H_
#define MAJOR_VERSION 2
#define MINOR_VERSION 0
#define RELEASE_NUMBER 8

#include <wx/snglinst.h>

#define WM_RESTORE_FRAME (WM_USER + 100)
#define HOTKEY_ID_GLOBAL_FUNC 1002 

enum class ThemeMode {
    System = 0, // 跟随系统
    Light  = 1, // 浅色模式
    Dark   = 2  // 深色模式
}; 

struct sqlite3;
class MyApp : public wxApp {
public:
    virtual bool OnInit() override;
    virtual int OnExit() override;
    virtual void OnInitCmdLine(wxCmdLineParser& parser) override;
    virtual bool OnCmdLineParsed(wxCmdLineParser& parser) override;
    MyApp() {
#ifdef _MSC_VER
#ifdef _DEBUG
        // 在构造函数中设置内存泄漏标志
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

#endif
#endif
    }
    wxString GetConfigFilePath() const { return ConfigFilePath; }
    //wxString GetDBFilePath() const{ return DBFilePath; }
    ThemeMode GetThemeMode() const { return m_themeMode; }
    void SetThemeMode(ThemeMode mode) { m_themeMode = mode; }
    bool GetDarkMode() const { return m_DarkMode; }
    void SetDarkMode(bool isDark) {
        m_DarkMode = isDark;
        UpdateThemeColor();
    }
    void UpdateThemeColor();
    const char* GetThemeColor() const { return (const char*)themeColor; }
    bool GetPortableMode() const { return m_useExternalDB; }
    const wxString& GetPortableImageFold() const { return PortableImagesFold; }
    const wxString& GetToken()const { return g_apiToken; }
    void SetToken(const wxString& token) { g_apiToken = token; }
    int GetPort() const { return m_port; }
    void SetPort(int port) { m_port = port; }

    //重启功能使用
    void DelSingleInstanceChecker();
    const wxString GetOriginalArgs()const;

    sqlite3* db = nullptr;
private:
    wxSingleInstanceChecker* m_singleInstanceChecker = nullptr; // 单例检查器
    wxString DBFilePath; // 当前数据库文件路径
    wxString ConfigFilePath; // 配置文件路径
    wxString PortableImagesFold = wxEmptyString;
    wxString g_apiToken;
    int m_port;
    bool m_useExternalDB = false;
    ThemeMode m_themeMode = ThemeMode::System;
    bool m_DarkMode = false;
    char themeColor[7];

};

inline MyApp* GetMyAppInstance() {
    return static_cast<MyApp*>(wxTheApp);
}

#endif // _APP_H_

