#include "pch.h"
#include "App.h"
#include "MainFrame.h" 
#include "SQLdefs.h"
#include "wx/cmdline.h"
#include "Helper.h"
#include "SVG_ICON.h"
#include "wx/imagwebp.h"
static int register_char_tokenizer(sqlite3* db, char** pzErrMsg);

wxIMPLEMENT_APP(MyApp);

void MyApp::UpdateThemeColor() {
    wxConfigBase* config = wxConfigBase::Get();
    if (m_DarkMode) {
        wxTheApp->SetAppearance(wxApp::Appearance::Dark);
        wxString full_color_str = config ? config->Read("MainWindow/DarkModeThemeColor", "#B0B0B0") : wxString("#B0B0B0");
        wxString final_color = ValidateAndExtractColor(full_color_str, "B0B0B0");
        const char* final_color_code = final_color.c_str();
        strcpy(themeColor, final_color_code);
    } else {
        wxTheApp->SetAppearance(wxApp::Appearance::Light);
        wxString full_color_str = config ? config->Read("MainWindow/DefaultThemeColor", "#1F1F1F") : wxString("#1F1F1F");
        wxString final_color = ValidateAndExtractColor(full_color_str, "1F1F1F");
        const char* final_color_code = final_color.c_str();
        strcpy(themeColor, final_color_code);
    }
}

bool MyApp::OnInit() {
    if (!wxApp::OnInit())
        return false;

    // 若由重启拉起，先等待旧进程完全退出并释放 SQLite 锁、全局热键与单实例互斥量，避免新旧进程并发竞态
    if (m_waitParentPid > 0) {
        HANDLE hOldProc = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(m_waitParentPid));
        if (hOldProc) {
            ::WaitForSingleObject(hOldProc, 5000);
            ::CloseHandle(hOldProc);
        }
        m_waitParentPid = 0;
    }

    //单实例限制
    m_singleInstanceChecker = new wxSingleInstanceChecker(L"wxNotePad2");
    if (m_singleInstanceChecker->IsAnotherRunning()) {
        HWND hWnd = FindWindow(NULL, L"wxNotePad2");
        if (hWnd == NULL) {
            struct FindCtx {
                DWORD selfPid;
                HWND found;
            } ctx{ ::GetCurrentProcessId(), NULL };
            ::EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                auto* p = reinterpret_cast<FindCtx*>(lParam);
                DWORD pid = 0;
                ::GetWindowThreadProcessId(hwnd, &pid);
                if (pid == p->selfPid) return TRUE;
                wchar_t title[256] = { 0 };
                if (::GetWindowTextW(hwnd, title, 256) > 0 && wcsncmp(title, L"wxNotePad2", 10) == 0) {
                    p->found = hwnd;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));
            hWnd = ctx.found;
        }
        if (hWnd != NULL) {
            ::SendMessage(hWnd, WM_RESTORE_FRAME, 0, 0);
        }
        delete m_singleInstanceChecker;
        m_singleInstanceChecker = nullptr;
        return false;
    }

    //配置相关
    wxFileName CurrentExePath(wxStandardPaths::Get().GetExecutablePath());
    auto CurrentAppDir = CurrentExePath.GetPath();
    wxString localConfigPath = CurrentAppDir + wxFileName::GetPathSeparator() + L"wxNotePad.ini";
    wxFileName localConfigFile(localConfigPath);
    wxString UserDataDir;
    if (localConfigFile.FileExists()) {
        UserDataDir = CurrentAppDir;
        ConfigFilePath = localConfigPath;
    } else {
        //在Roaming目录下创建目录
        UserDataDir = wxStandardPaths::Get().GetUserConfigDir() + wxFileName::GetPathSeparator() + L"wxNotePad";
        ConfigFilePath = UserDataDir + wxFileName::GetPathSeparator() + L"wxNotePad.ini";

        if (!wxFileName::DirExists(UserDataDir)) {
            if (!wxFileName::Mkdir(UserDataDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
                return false;
            }
        }
    }

    if (!m_useExternalDB) {
        DBFilePath = UserDataDir + wxFileName::GetPathSeparator() + L"wxNotePad.db";
    } else {
        wxFileName fn(DBFilePath);
        PortableImagesFold = fn.GetPath() + wxFileName::GetPathSeparator() + L"wxNotePadFiles";
    }

    wxConfigBase::Set(new wxFileConfig(wxEmptyString, wxEmptyString, ConfigFilePath, wxEmptyString, wxCONFIG_USE_LOCAL_FILE));

    //数据库初始化
    wxScopedCharBuffer dbPathBuffer = DBFilePath.ToUTF8();

    int rc = sqlite3_open_v2(dbPathBuffer.data(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        return false;
    }
    // 设置 5 秒锁等待超时，避免短暂文件/WAL 锁冲突导致 SQLITE_BUSY 直接退出
    sqlite3_busy_timeout(db, 5000);
    rc = sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable extension loading: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    char* zErrMsg = 0;
    rc = register_char_tokenizer(db, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to initialize char tokenizer: %s\n", zErrMsg ? zErrMsg : sqlite3_errmsg(db));
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);//启用了约束和级联删除功能
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    // 启用 WAL 模式与内存性能优化
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA cache_size = -16384;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA temp_store = MEMORY;", NULL, NULL, NULL);

    char* err_msg = 0;
    const char* sql_batch =
        "BEGIN TRANSACTION;"
        SQLITE_CREATE_TABLE_PATHS
        SQLITE_CREATE_TABLE_NOTES
        //SQLITE_NOTES_ADD_URL
        SQLITE_CREATE_TABLE_TAGS
        SQLITE_CREATE_TABLE_PATHS_TAGS
        SQLITE_CREATE_VIRTUAL_TABLE_NOTES_FTS
        SQLITE_CREATE_TRIGGER_NOTES_AI
        SQLITE_CREATE_TRIGGER_NOTES_AU
        SQLITE_CREATE_TRIGGER_NOTES_AD
        "COMMIT;"
        ;
    rc = sqlite3_exec(db, sql_batch, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_close(db);
        db = nullptr;
        return false;
    }
    //临时增加字段（在后继版本中合并到sql_batch中）
    sqlite3_exec(db, "ALTER TABLE NOTES ADD COLUMN URL TEXT; ", 0, 0, 0);
    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS IDX_NOTES_URL ON NOTES(URL);", 0, 0, 0);
    sqlite3_exec(db, SQLITE_CREATE_CORE_INDEXES, 0, 0, 0);

    // 平滑迁移旧版全列 TRIGGER_NOTES_AU 与错误删除触发器 TRIGGER_NOTES_AD
    {
        sqlite3_stmt* trgStmt = nullptr;
        bool needUpgradeAu = false;
        bool needUpgradeAd = false;
        if (sqlite3_prepare_v2(db, "SELECT name, sql FROM sqlite_master WHERE type='trigger' AND name IN ('TRIGGER_NOTES_AU', 'TRIGGER_NOTES_AD');", -1, &trgStmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(trgStmt) == SQLITE_ROW) {
                const unsigned char* nameTxt = sqlite3_column_text(trgStmt, 0);
                const unsigned char* sqlTxt = sqlite3_column_text(trgStmt, 1);
                if (!nameTxt || !sqlTxt) continue;
                std::string name(reinterpret_cast<const char*>(nameTxt));
                std::string sql(reinterpret_cast<const char*>(sqlTxt));
                if (name == "TRIGGER_NOTES_AU" && sql.find("OF CONTENT") == std::string::npos) {
                    needUpgradeAu = true;
                } else if (name == "TRIGGER_NOTES_AD" && sql.find("DELETE FROM NOTES_FTS") != std::string::npos) {
                    needUpgradeAd = true;
                }
            }
            sqlite3_finalize(trgStmt);
        }
        if (needUpgradeAu) {
            sqlite3_exec(db, "BEGIN TRANSACTION; DROP TRIGGER IF EXISTS TRIGGER_NOTES_AU; " SQLITE_CREATE_TRIGGER_NOTES_AU " COMMIT;", nullptr, nullptr, nullptr);
        }
        if (needUpgradeAd) {
            // 修复删除触发器并重建 FTS5，彻底清除历史残留的已删除笔记幽灵索引
            sqlite3_exec(db, "BEGIN TRANSACTION; DROP TRIGGER IF EXISTS TRIGGER_NOTES_AD; " SQLITE_CREATE_TRIGGER_NOTES_AD " INSERT INTO NOTES_FTS(NOTES_FTS) VALUES('rebuild'); COMMIT;", nullptr, nullptr, nullptr);
        }
    }

    sqlite3_free(err_msg);
    err_msg = nullptr;
    //数据库初始化结束
    //注册图像处理器
    wxImage::AddHandler(new wxPNGHandler);
    wxImage::AddHandler(new wxJPEGHandler);
    wxImage::AddHandler(new wxWEBPHandler);
    // 黑暗模式与外观支持
    wxConfigBase* config = wxConfigBase::Get();
    long modeVal = config->ReadLong("MainWindow/ThemeMode", -1);
    if (modeVal >= 0 && modeVal <= 2) {
        m_themeMode = static_cast<ThemeMode>(modeVal);
    } else {
        bool legacyDark = config->ReadBool("MainWindow/DarkMode", false);
        m_themeMode = legacyDark ? ThemeMode::Dark : ThemeMode::System;
    }
    m_DarkMode = (m_themeMode == ThemeMode::Dark) ||
                 (m_themeMode == ThemeMode::System && IsWindowsSystemDarkMode());
    UpdateThemeColor();

    m_port = config->ReadLong("Network/Port", 8080);
    g_apiToken = config->Read("Network/APIToken", L"default_token");

    MainFrame* frame;
    if (m_useExternalDB) {
        frame = new MainFrame("wxNotePad2 - " + DBFilePath);
    } else {
        frame = new MainFrame("wxNotePad2");
    }

    //frame->InitUI();
    frame->ShowPreRendered();

    return true;
}

int MyApp::OnExit() {
    if (db != NULL) {
        sqlite3_close_v2(db);
        db = NULL;
    }
    delete m_singleInstanceChecker;

    return wxApp::OnExit();
}

void MyApp::OnInitCmdLine(wxCmdLineParser& parser) {
    wxApp::OnInitCmdLine(parser);
    parser.AddSwitch("o", "open", L"手动选择数据库文件");
    parser.AddOption("d", "db", L"直接指定wxNotePad.db路径", wxCMD_LINE_VAL_STRING);
    parser.AddOption("", "wait-pid", L"重启时等待旧进程退出", wxCMD_LINE_VAL_NUMBER);

}

bool MyApp::OnCmdLineParsed(wxCmdLineParser& parser) {
    if (!wxApp::OnCmdLineParsed(parser))return false;
    long waitPid = 0;
    if (parser.Found("wait-pid", &waitPid) && waitPid > 0) {
        m_waitParentPid = static_cast<unsigned long>(waitPid);
    }
    wxString providedPath;
    if (parser.Found("d", &providedPath)) {
        wxFileName fn(providedPath);
        if (fn.GetFullName().Lower() == wxString("wxnotepad.db")) {
            if (fn.IsDirWritable() || wxFileName::DirExists(fn.GetPath())) {
                DBFilePath = providedPath;
                m_useExternalDB = true;
            } else {
                wxMessageBox(L"无法在指定路径创建或打开数据库：\n" + providedPath, L"错误", wxOK | wxICON_ERROR);
                return false;
            }
        } else {
            wxMessageBox(L"错误的文件名，请指定\"wxNotePad.db\"文件", L"错误", wxOK | wxICON_ERROR);
            return false;
        }
    } else if (parser.Found("o")) {
        wxFileDialog openFileDialog(nullptr, L"选择数据库文件", "", "", L"wxNotePad数据库|wxNotePad.db", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openFileDialog.ShowModal() == wxID_OK) {
            DBFilePath = openFileDialog.GetPath();
            m_useExternalDB = true;
        } else {
            return false;
        }
    }
    return true;
}

void MyApp::DelSingleInstanceChecker() {
    if (m_singleInstanceChecker) {
        delete m_singleInstanceChecker;
        m_singleInstanceChecker = nullptr;
    }
}

void MyApp::EnsureSingleInstanceChecker() {
    if (!m_singleInstanceChecker) {
        m_singleInstanceChecker = new wxSingleInstanceChecker(L"wxNotePad2");
    }
}

const wxString MyApp::GetOriginalArgs() const {
    // 若通过 -o 手动选择了外部数据库，重启时直接改传 -d "<DBFilePath>"，避免反复弹窗重选
    if (m_useExternalDB && !DBFilePath.IsEmpty()) {
        return wxString::Format(L" -d \"%s\"", DBFilePath);
    }
    wxString args;
    for (int i = 1; i < wxTheApp->argc; i++) {
        wxString arg = wxTheApp->argv[i];
        // 过滤内部重启交接参数，防止多次连续重启导致参数重复累积或解析报错
        if (arg == L"--wait-pid") {
            ++i;
            continue;
        }
        if (arg.StartsWith(L"--wait-pid=")) {
            continue;
        }
        if (arg.Contains(L" ")) {
            args += wxString::Format(L" \"%s\"", arg);
        } else {
            args += L" " + arg;
        }
    }
    return args;
}

//---分词器实现---
typedef struct fts5_api fts5_api;
typedef struct Fts5Tokenizer Fts5Tokenizer;
typedef struct fts5_tokenizer fts5_tokenizer;

static fts5_api* fts5_api_from_db(sqlite3* db) {
    fts5_api* pRet = 0;
    sqlite3_stmt* pStmt = 0;

    if (SQLITE_OK == sqlite3_prepare(db, "SELECT fts5(?1)", -1, &pStmt, 0)) {
        sqlite3_bind_pointer(pStmt, 1, (void*)&pRet, "fts5_api_ptr", NULL);
        sqlite3_step(pStmt);
        sqlite3_finalize(pStmt);
    }
    return pRet;
}

// xTokenize 实现，跳过分割符
static int charTokenize(Fts5Tokenizer* pTokenizer, void* pCtx, int flags, const char* pText, int nText, int (*xToken)(void* pCtx, int tflags, const char* pToken, int nToken, int iStart, int iEnd)) {
    int i = 0;         // 当前字节偏移
    int iToken = 0;    // token 计数
    while (i < nText) {
        // 跳过无效的 UTF-8 延续字节
        while (i < nText && !isUtf8StartByte((unsigned char)pText[i])) {
            i++;
        }
        if (i >= nText) break;

        // 计算当前字符的字节数
        int nCharBytes = utf8CharLen(pText + i, nText - i);
        if (nCharBytes == 0) break;

        // 非分割符生成 token
        if (!isDelimiter(pText + i, nCharBytes)) {
            int rc = xToken(pCtx, 0, pText + i, nCharBytes, i, i + nCharBytes);
            if (rc != SQLITE_OK) return rc;
            iToken++;
        }
        i += nCharBytes;
    }
    return SQLITE_OK;
}

// 创建分词器
static int charCreate(void* pUnused, const char** azArg, int nArg, Fts5Tokenizer** ppOut) {
    *ppOut = (Fts5Tokenizer*)sqlite3_malloc(sizeof(void*));
    if (*ppOut == NULL) return SQLITE_NOMEM;
    return SQLITE_OK;
}

// 销毁分词器
static void charDelete(Fts5Tokenizer* pTokenizer) {
    sqlite3_free(pTokenizer);
}

// 分词器模块定义
static  fts5_tokenizer charTokenizerModule = {
    charCreate,   // xCreate
    charDelete,   // xDelete
    charTokenize  // xTokenize
};

// 注册分词器
static int register_char_tokenizer(sqlite3* db, char** pzErrMsg) {
    fts5_api* pFts5Api = fts5_api_from_db(db);
    if (pFts5Api == NULL) {
        *pzErrMsg = sqlite3_mprintf("Failed to get fts5_api");
        return SQLITE_ERROR;
    }

    int rc = pFts5Api->xCreateTokenizer(
        pFts5Api,
        "char",
        NULL,
        &charTokenizerModule,
        NULL
    );
    if (rc != SQLITE_OK) {
        *pzErrMsg = sqlite3_mprintf("Failed to register char tokenizer: %s", sqlite3_errmsg(db));
        return rc;
    }

    return SQLITE_OK;
}
//---分词器实现结束---