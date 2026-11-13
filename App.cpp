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

    //单实例限制
    m_singleInstanceChecker = new wxSingleInstanceChecker(L"wxNotePad2");
    if (m_singleInstanceChecker->IsAnotherRunning()) {
        HWND hWnd = FindWindow(NULL, L"wxNotePad2");
        if (hWnd != NULL) {
            ::SendMessage(hWnd, WM_RESTORE_FRAME, 0, 0);
        }
        delete m_singleInstanceChecker;
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
        return false;
    }
    rc = sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable extension loading: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    char* zErrMsg = 0;
    rc = register_char_tokenizer(db, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to initialize char tokenizer: %s\n", zErrMsg ? zErrMsg : sqlite3_errmsg(db));
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return 1;
    }
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);//启用了约束和级联删除功能
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    //rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);//启用WAL
    //if (rc != SQLITE_OK) {
    //    sqlite3_close(db);
    //    return false;
    //}
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
        sqlite3_close(db);
        return false;
    }
    //临时增加字段（在后继版本中合并到sql_batch中）
    sqlite3_exec(db, "ALTER TABLE NOTES ADD COLUMN URL TEXT; ", 0, 0, 0);
    sqlite3_exec(db, "CREATE INDEX IF NOT EXISTS IDX_NOTES_URL ON NOTES(URL);", 0, 0, 0);


    //设置数据库缓存16MB
    //sqlite3_exec(db, "PRAGMA cache_size = -16384;", 0, 0, 0);
    //sqlite3_exec(db, "PRAGMA temp_store = MEMORY;", 0,0,0);

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
    frame->Show(true);

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

}

bool MyApp::OnCmdLineParsed(wxCmdLineParser& parser) {
    if (!wxApp::OnCmdLineParsed(parser))return false;
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

const wxString MyApp::GetOriginalArgs() const {
    wxString args;
    for (int i = 1; i < wxTheApp->argc; i++) {
        wxString arg = wxTheApp->argv[i];
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