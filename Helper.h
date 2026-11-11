#pragma once
#include "MyTreeCtrl.h"
#include "wx/filename.h"
#include <set>
#include <wx/convauto.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/mstream.h>
#include <wx/string.h>
#include <Windows.h>
#include <wx/fileconf.h>
#include <wx/app.h>

#define MAX_PREVIEW_LEN 250 //预览最大长度;
#define MAX_PREVIEW_LINES 10 //预览最大行数;
inline wxString CutStringForPreview(const wxString& str) {
    if (str.IsEmpty()) {
        return wxEmptyString;
    }
    if (str.Length() <= MAX_PREVIEW_LEN) {
        return str;
    }

    wxString result;
    wxArrayString lines = wxSplit(str, '\n');
    size_t currentLen = 0;
    size_t lineCount = 0;

    for (size_t i = 0; i < lines.GetCount() && lineCount < MAX_PREVIEW_LINES; ++i) {
        const wxString& line = lines[i];
        size_t lineLen = line.Length();

        if (currentLen + lineLen + (lineCount > 0 ? 1 : 0) > MAX_PREVIEW_LEN - 3) {
            size_t remaining = MAX_PREVIEW_LEN - 3 - currentLen - (lineCount > 0 ? 1 : 0);
            if (remaining > 0) {
                if (lineCount > 0) result += "\n";
                result += line.Left(remaining);
            }
            result += "...";
            return result;
        }

        if (lineCount > 0) {
            result += "\n";
            currentLen++;
        }
        result += line;
        currentLen += lineLen;
        lineCount++;
    }

    if (lines.GetCount() > MAX_PREVIEW_LINES) {
        result += "...";
    }

    return result;
}

inline wxString CutStringForTitle(const wxString& str) {
    const size_t maxLength = 24;

    // 第一步：检查空字符串或仅含空字符
    bool isEmptyOrWhitespace = true;
    for (size_t i = 0; i < str.Length(); ++i) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n' && str[i] != '\r') {
            isEmptyOrWhitespace = false;
            break;
        }
    }
    if (str.IsEmpty() || isEmptyOrWhitespace) {
        return wxT("<空>");
    }

    // 第二步：跳过开头的空字符（空格和制表符）
    size_t startPos = 0;
    while (startPos < str.Length() && (str[startPos] == ' ' || str[startPos] == '\t')) {
        startPos++;
    }

    // 第三步：从有效内容开始处理
    wxString content = str.Mid(startPos);

    // 第四步：查找换行符位置
    size_t newlinePos = content.Find('\n');

    // 第五步：若存在换行符且在24个字符内，截取到换行符前
    if (newlinePos != wxString::npos && newlinePos <= maxLength) {
        return content.Left(newlinePos);
    }

    // 第六步：若长度超过24，截取前24个字符并添加省略号
    if (content.Length() > maxLength) {
        return content.Left(maxLength) + wxT("...");
    }

    // 第七步：若长度不超过24且无换行符，直接返回有效内容
    return content;
}

inline wxColour GetInvertedColour(const wxColour& colour) {
    // 获取原始颜色的RGB分量
    unsigned char red = colour.Red();
    unsigned char green = colour.Green();
    unsigned char blue = colour.Blue();

    double luminance = 0.299 * red + 0.587 * green + 0.114 * blue;

    // 根据亮度选择前景色
    if (luminance < 128) {
        return wxColour(255, 255, 255); // 白色前景色
    } else {
        return wxColour(0, 0, 0); // 黑色前景色
    }
}
#include "App.h"
inline wxFont ReadFontFromConfig() {
    auto App = static_cast<MyApp*>(wxTheApp);
    wxFileConfig config{ wxEmptyString,wxEmptyString,App->GetConfigFilePath(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE };
   
    config.SetPath("/Scintilla/Font");
    long pointSize = 10;
    config.Read("PointSize", &pointSize);
    if (pointSize <= 0) {
        pointSize = 10;
    }
    wxString faceName = wxEmptyString;
    config.Read("FaceName", &faceName);
    config.SetPath("/");

    wxFont font(pointSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, faceName);
    if (!font.IsOk()) {
        font = wxFont(wxFontInfo(pointSize));
    }
    return font;
}

inline size_t ConvertBytePosToCharPos(const wxString& str_utf8, size_t byte_pos_in_str) {
    // 将 wxString 转换为 C 风格的 UTF-8 字节数组
    const char* p_str = str_utf8.ToUTF8();
    if (!p_str) {
        return std::string::npos;
    }

    size_t char_count = 0; // 字符计数器
    size_t byte_pos_current = 0; // 当前遍历到的字节位置
    size_t byte_length = strlen(p_str);
    // 如果给定的字节位置超过了字符串长度，直接返回无效值
    if (byte_pos_in_str > byte_length) {
        return std::string::npos;
    }

    // 循环遍历字节
    while (p_str[byte_pos_current] != '\0' && byte_pos_current < byte_pos_in_str) {
        // 根据 UTF-8 编码规则判断当前字符占用的字节数
        unsigned char current_byte = static_cast<unsigned char>(p_str[byte_pos_current]);
        size_t char_byte_len = 0;

        if ((current_byte & 0x80) == 0x00) {
            // 单字节字符 (0xxxxxxx)
            char_byte_len = 1;
        } else if ((current_byte & 0xE0) == 0xC0) {
            // 双字节字符 (110xxxxx)
            char_byte_len = 2;
        } else if ((current_byte & 0xF0) == 0xE0) {
            // 三字节字符 (1110xxxx)
            char_byte_len = 3;
        } else if ((current_byte & 0xF8) == 0xF0) {
            // 四字节字符 (11110xxx)
            char_byte_len = 4;
        } else {
            // 遇到非法的 UTF-8 序列，直接跳过一个字节
            char_byte_len = 1;
        }

        // 移动到下一个字符的起始位置
        byte_pos_current += char_byte_len;
        // 字符计数加一
        char_count++;
    }

    // 最终的字符计数器值就是目标字符位置
    return char_count;
}

/* invalid default char for conversion */
inline unsigned te_is_num(const char* const pch) { return (pch && (*pch > 47) && (*pch < 58)); }

/* check for operator or special character. */
inline int te_is_op(const char* const expr) {
    if (!expr)
        return !0;
    switch (*expr) {
    case '\0':
    case '+':
    case '-':
    case '*':
    case -41:
    case '/':
    case ':':
    case -9:
    case '^':
    case '%':
    case '!':
    case '<':
    case '>':
    case '(':
    case ')':
    case ',':
    case ' ':
    case '\t':
    case '\n':
    case '\r':
        return !0;
    case '=':
        if (expr[1] == '=')
            return !0;
        break;
    case '&':
        if (expr[1] == '&')
            return !0;
        break;
    case '|':
        if (expr[1] == '|')
            return !0;
        break;
    default:
        break;
    }
    return 0;
}

inline bool IsBlankChar(const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}
inline int isDelimiter(const char* p, int nBytes) {
    if (nBytes < 1) return 0;
    unsigned char c = (unsigned char)p[0];
    return (c == ' ' || c == '\r' || c == '\t' || c == '\n');
}

// 检查是否为有效的 UTF-8 字符开头
inline int isUtf8StartByte(unsigned char c) {
    return (c & 0xC0) != 0x80;
}

// 获取 UTF-8 字符的字节数
inline int utf8CharLen(const char* p, int nBytes) {
    if (nBytes == 0) return 0;
    unsigned char c = (unsigned char)p[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0 && nBytes >= 2) return 2;
    if ((c & 0xF0) == 0xE0 && nBytes >= 3) return 3;
    if ((c & 0xF8) == 0xF0 && nBytes >= 4) return 4;
    return 1;
}

//SQL返回的时间字符串转换
inline wxString CovertToLocalTime(const char* timestamp) {
    wxString timeStr = wxString::FromUTF8(timestamp);
    wxDateTime dt;
    if (dt.ParseDateTime(timeStr)) {
        dt.MakeFromTimezone(wxDateTime::UTC);
        return dt.Format("%Y-%m-%d %H:%M:%S");
    } else {
        return wxEmptyString;
    }
}

inline wxString SanitizeName(wxString str) {
    str.Trim(true);
    const wxChar charsToTrim[] = { ' ', '*', '\t', '\n', '\r', '\0' };
    while (!str.IsEmpty()) {
        wxChar lastChar = str.Last();
        bool shouldTrim = false;
        for (const wxChar c : charsToTrim) {
            if (c == '\0') break;
            if (lastChar == c) {
                shouldTrim = true;
                break;
            }
        }
        if (shouldTrim) {
            str.RemoveLast();
        } else {
            break;
        }
    }
    wxString result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        wxChar c = str[i];
        if (c == ' ') {
            result += '_';
        } else if (c == '\t' || c == '\r' || c == '\n') {
            continue;
        } else {
            result += c;
        }
    }
    return result;
}

inline wxString SanitizeAndGetUniqueName(const wxString& basePath, const wxString& originalName, bool isFolder) {
    // 1. 清理文件名中的非法字符
    // Windows 文件名不允许的特殊字符：\ / : * ? " < > |
    wxString cleanedName = originalName;

    // 清理非法字符：使用下划线 '_' 替换
    cleanedName.Replace("\\", "_");
    cleanedName.Replace("/", "_");
    cleanedName.Replace(":", "_");
    cleanedName.Replace("*", "_");
    cleanedName.Replace("?", "_");
    cleanedName.Replace("\"", "_");
    cleanedName.Replace("<", "_");
    cleanedName.Replace(">", "_");
    cleanedName.Replace("|", "_");

    // 清理文件名首尾的空格和点
    cleanedName.Trim(true).Trim(false);

    // 如果清理后文件名为空，给一个默认名
    if (cleanedName.IsEmpty()) {
        cleanedName = L"未命名";
    }

    // 2. 文件名去重
    wxString finalName = cleanedName;
    int counter = 1;
    bool exists = true;

    // 假设导出文件后缀名为 ".txt"
    while (exists) {
        // 构建完整的待检查路径
        // 注意：wxFileName 构造函数可以处理目录和文件名
        wxFileName checkFile(basePath, finalName, isFolder ? "" : "txt");
        if (isFolder) {
            // **【修复】** 检查文件夹是否存在
            exists = wxFileName::DirExists(checkFile.GetFullPath());
        } else {
            // **【修复】** 检查文件是否存在
            exists = wxFileName::FileExists(checkFile.GetFullPath());
        }

        if (exists) {
            // 如果文件存在，则重命名： originalName(1), originalName(2)...
            finalName = wxString::Format(L"%s(%d)", cleanedName, counter++);
        }
    }

    return finalName; // 返回不含后缀的最终文件名
}


//获取指定路径下所有txt以及包含它们的目录层级
struct PathInfo {
    wxString name;
    wxString fullpath;
    bool isDir;
    int depth;
    bool operator<(const PathInfo& other) const {
        if (this->fullpath != other.fullpath) {
            return this->fullpath < other.fullpath;
        }
        return this->isDir > other.isDir;
    }
};

inline std::set<PathInfo> GetTextFilesAndDirs(const wxString& path, int& maxDepth) {
    std::set<PathInfo> result;
    wxDir dir(path);
    maxDepth = 0;
    if (!dir.IsOpened()) return result;

    wxFileName basePath(path);
    basePath.MakeAbsolute();
    wxString basePathWithSep = basePath.GetPathWithSep();

    wxArrayString allFiles;
    dir.GetAllFiles(path, &allFiles, "*.txt", wxDIR_DEFAULT);

    for (const wxString& fullFilePath : allFiles) {
        wxFileName file(fullFilePath);
        wxFileName relativeFile(file);
        relativeFile.MakeRelativeTo(basePathWithSep);
        wxString relativePathStr = relativeFile.GetFullPath();
        wxArrayString dirs = relativeFile.GetDirs();

        // --- 1. 提取并添加所有父文件夹信息 ---
        wxString currentRelativePath = wxEmptyString;

        for (size_t i = 0; i < dirs.GetCount(); ++i) {
            // 拼接文件夹的相对路径，不带末尾分隔符
            if (!currentRelativePath.IsEmpty()) {
                currentRelativePath += wxFileName::GetPathSeparator();
            }
            currentRelativePath += dirs[i]; // 示例: "a", 然后 "a/b"

            // 深度从 1 开始
            int dirDepth = i + 1;
            result.insert({ dirs[i], currentRelativePath, true, dirDepth });
        }

        // --- 2. 添加文件信息 ---
        // 文件深度等于其所在目录的深度 + 1 (例如 /a/b/c.txt 深度为 3)
        int fileDepth = dirs.GetCount() + 1;
        maxDepth = wxMax(maxDepth, fileDepth);

        // 文件 fullpath 是完整的相对路径 (不以分隔符结尾)
        result.insert({ file.GetFullName(), relativePathStr, false, fileDepth });
    }

    return result;
}

// 将 wxAcceleratorEntry 的标志转换为 WinAPI 的 MOD_XXX
inline int ConvertWxFlagsToWinMods(int wxFlags) {
    int winMods = 0;
    if (wxFlags & wxACCEL_ALT) {
        winMods |= MOD_ALT;
    }
    if (wxFlags & wxACCEL_CTRL) {
        winMods |= MOD_CONTROL;
    }
    if (wxFlags & wxACCEL_SHIFT) {
        winMods |= MOD_SHIFT;
    }
    // 注意：MOD_WIN (Windows键) 不在标准的 wxACCEL_XXX 中，但可以根据需要添加
    return winMods;
}

//将 wxAcceleratorEntry 的键码转换为 WinAPI 的 VK_XXX
// 仅处理 F 键和字母
inline int ConvertWxKeyToWinVK(int wxKey) {
    // C++代码：处理 Fn 键
    if (wxKey >= WXK_F1 && wxKey <= WXK_F12) {
        // WXK_F1 通常是 340，VK_F1 是 0x70，它们不是直接对应的
        // 但它们是顺序排列的，所以可以通过偏移量计算
        return VK_F1 + (wxKey - WXK_F1);
    }

    // C++代码：处理字母键 (VK_A, VK_B 等与大写字母的 ASCII 码相同)
    if (wxKey >= 'A' && wxKey <= 'Z') {
        return wxKey;
    }

    // C++代码：处理其他常用键，例如：
    if (wxKey == WXK_RETURN) return VK_RETURN;
    if (wxKey == WXK_ESCAPE) return VK_ESCAPE;
    // TODO: 根据需要添加其他键的映射

    return 0; // 无法识别的键码返回 0
}

//字符串replace
inline wxString ReplaceFirstOccurrence(const wxString& input, const wxString& searchStr, const wxString& replaceStr) {
    wxString result = input;
    if (searchStr.IsEmpty() || replaceStr.IsEmpty())return result;
    size_t pos = result.find(searchStr);
    if (pos == wxString::npos)return result;
    result.Replace(searchStr, replaceStr, false);
    return result;
}

#include <chrono>
#include <iomanip>
#include <sstream>

// 辅助函数：获取当前时间并格式化为 "HH:MM:SS.mmm" 字符串
//使用：_wxLogDebug(wxT("%s [开始] 待测函数运行。"), GetCurrentTimestampWithMs());
inline wxString GetCurrentTimestampWithMs() {
    // 1. 获取当前时间点
    auto now = std::chrono::high_resolution_clock::now();

    // 2. 将其转换为 std::time_t 以便格式化日期和秒
    // 注意：high_resolution_clock 不一定能直接转 time_t，这里我们通过 system_clock 来桥接，
    // 但精确的毫秒或微秒部分我们直接计算。
    auto time_point = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;

    auto timer = std::chrono::system_clock::to_time_t(time_point);
    std::tm bt = *std::localtime(&timer);

    // 3. 格式化并拼接
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S") // 标准时分秒
        << '.'
        << std::setfill('0') << std::setw(3) << ms.count(); // 毫秒部分

    // 4. 返回 wxString
    return wxString::FromUTF8(oss.str().c_str());
}

//样式与位置，压缩、解压
//样式占高4bit，位置占高低位bit，共16bit
inline UINT16 CompressStyleInfo(int style, int pos) {
    int mappedStyle = (style == 0) ? 0 : (style - 39);
    uint16_t highStyle = (mappedStyle & 0x0F) << 12;
    uint16_t lowPos = pos & 0x0FFF;

    return highStyle | lowPos;
}
inline std::pair<int, int> DecompressStyleInfo(UINT16 compressed) {
    int pos = compressed & 0x0FFF;
    int mappedStyle = compressed >> 12;
    int actualStyle;
    if (mappedStyle == 0) {
        actualStyle = 0;
    } else {
        actualStyle = mappedStyle + 39;
    }

    return { actualStyle, pos };
}

inline const char* ChangeSVGColor(char* buff,const char* svgStr,const char* color,int index){
    size_t svgStr_len = strlen(svgStr);
    memcpy(buff, svgStr, svgStr_len + 1);
    memcpy(buff + index, color, 6);
    return (const char*)buff;
}

inline wxString ValidateAndExtractColor(const wxString& full_color_str, const wxString& default_color){
    if (full_color_str.Length() == 7 && full_color_str.StartsWith("#")){
        auto color_code = full_color_str.Mid(1, 6);
        bool is_valid_hex = true;
        for (int i = 0; i < 6; i++){
            if (!std::isxdigit(color_code.GetChar(i))){
                is_valid_hex = false;
                break;
            }
        }
        if (is_valid_hex){
            return color_code;
        }
    }
    return default_color;
}