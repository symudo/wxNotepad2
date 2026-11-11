#pragma once

#include <wx/string.h>
#include <wx/font.h>
#include <wx/fontutil.h> // 【修复：引入 MSW 平台特定头文件，确保 wxNativeFontInfo 完整定义】
#include <windows.h>      // 需要 LOGFONTW 定义
#include <dwrite.h>
#include <objbase.h>      // 需要 CoInitializeEx, CoUninitialize
#include <string>
#include <algorithm>      // 可能需要 std::min
#include <wx/log.h>

//由于一个已知的bug：wxFont只能使用字体显示名称作为Get/Set参数，而wxSTC使用字体英文名称作为参数
//因此，需要在这里将 wxFont 的本地化名称转换为 DirectWrite 获取的标准英文名称

// C++ 辅助宏和模板
// -------------------------------------------------------------------

// 简单的 COM 智能指针 (RAII)，用于自动释放 DirectWrite 接口
template <class T>
class ComPtr{
public:
    ComPtr(): ptr_(nullptr){}
    ComPtr(T* p): ptr_(p){ if (ptr_) ptr_->AddRef(); }
    ~ComPtr(){ if (ptr_) ptr_->Release(); }

    T* operator->() const{ return ptr_; }
    T** operator&(){
        if (ptr_) ptr_->Release();
        ptr_ = nullptr;
        return &ptr_;
    }
    operator bool() const{ return ptr_ != nullptr; }

private:
    T* ptr_;
    ComPtr(const ComPtr&) = delete;
    void operator=(const ComPtr&) = delete;
};

// -------------------------------------------------------------------
// DirectWrite Factory RAII 管理器 (单例)
// -------------------------------------------------------------------

class DirectWriteFactoryRAII{
public:
    // 获取单例实例 (懒惰初始化)
    static IDWriteFactory* Get(){
        if (s_pDWriteFactory == nullptr){
            Initialize();
        }
        return s_pDWriteFactory;
    }

private:
    static IDWriteFactory* s_pDWriteFactory;
    static bool s_comInitialized;

    // 静态初始化函数
    static void Initialize(){
        if (!s_comInitialized){
            HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            if (SUCCEEDED(hr)){
                s_comInitialized = true;
            } else if (hr != S_FALSE){
                wxLogError(wxT("COM 初始化失败 (0x%X)。"), hr);
            }
        }

        if (s_comInitialized && s_pDWriteFactory == nullptr){
            HRESULT hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                (IUnknown**)&s_pDWriteFactory
            );
            if (FAILED(hr)){
                wxLogError(wxT("DirectWrite Factory 创建失败 (0x%X)。"), hr);
            }
        }
    }

    // 静态析构函数，负责清理 COM 和 DWrite Factory
    struct Cleanup{
        ~Cleanup(){
            if (s_pDWriteFactory){
                s_pDWriteFactory->Release();
                s_pDWriteFactory = nullptr;
            }
            if (s_comInitialized){
                CoUninitialize();
            }
        }
    };
    static Cleanup cleanup;

    DirectWriteFactoryRAII() = delete;
};

// C++17: 使用 inline 变量实现在头文件中的静态成员定义
inline IDWriteFactory* DirectWriteFactoryRAII::s_pDWriteFactory = nullptr;
inline bool DirectWriteFactoryRAII::s_comInitialized = false;
inline DirectWriteFactoryRAII::Cleanup DirectWriteFactoryRAII::cleanup;


// -------------------------------------------------------------------
// 核心接口：字体名称转换
// -------------------------------------------------------------------

/**
 * @brief 将 wxFont 对象的本地化名称替换为 DirectWrite 获取的标准英文名称。
 *
 * @param font 输入的 wxFont 对象的引用。
 * @return wxFont 返回一个新的 wxFont 对象，其中 FaceName 已更新为标准英文名；
 * 如果输入已经是英文名或转换失败，则返回原始 wxFont 的拷贝。
 */
inline wxFont ConvertLocalizedFontToEnglish(const wxFont& font){
    wxString localizedName = font.GetFaceName();

    // 如果已经是英文名（ASCII 字符），则无需处理，直接返回原始字体的拷贝
    if (localizedName.IsAscii()){
        return font;
    }

    IDWriteFactory* pFactory = DirectWriteFactoryRAII::Get();
    if (pFactory == nullptr){
        return font;
    }

    ComPtr<IDWriteFontCollection> pFontCollection;
    HRESULT hr = pFactory->GetSystemFontCollection(&pFontCollection);

    if (FAILED(hr) || !pFontCollection){
        return font;
    }

    // 1. 查找字体家族索引
    UINT32 index = 0;
    BOOL exists = FALSE;
    hr = pFontCollection->FindFamilyName(localizedName.wc_str(), &index, &exists);

    if (FAILED(hr) || !exists){
        return font;
    }

    // 2. 获取字体家族对象
    ComPtr<IDWriteFontFamily> pFontFamily;
    hr = pFontCollection->GetFontFamily(index, &pFontFamily);

    if (FAILED(hr) || !pFontFamily){
        return font;
    }

    // 3. 获取字体家族的名称集合
    ComPtr<IDWriteLocalizedStrings> pFamilyNames;
    hr = pFontFamily->GetFamilyNames(&pFamilyNames);

    if (FAILED(hr) || !pFamilyNames){
        return font;
    }

    // 4. 查找英文名称 ("en-us")
    UINT32 nameIndex = 0;
    BOOL found = FALSE;
    hr = pFamilyNames->FindLocaleName(L"en-us", &nameIndex, &found);

    if (FAILED(hr) || !found){
        nameIndex = 0;
    }

    // 5. 提取名称字符串
    UINT32 length = 0;
    hr = pFamilyNames->GetStringLength(nameIndex, &length);

    if (SUCCEEDED(hr)){
        std::wstring wsName(length, L'\0');
        hr = pFamilyNames->GetString(nameIndex, &wsName[0], length + 1);

        if (SUCCEEDED(hr)){
            wxString standardEnglishName(wsName.c_str());

            // 6. **【修复：使用兼容的 wxWidgets 方法修改 FaceName】**
            // 这一步依赖于 wxNativeFontInfo 构造函数可以接受 wxFont。
            // 必须使用一个新的 wxNativeFontInfo 实例来替换 FaceName。

            // a) 获取原始字体的所有信息（除了 FaceName）
            const wxNativeFontInfo* originalInfo = font.GetNativeFontInfo();

            // b) 创建一个新的 wxNativeFontInfo 实例，并使用新的 FaceName
            // wxNativeFontInfo 的构造函数可以接受 LOGFONTW 的指针（在 MSW 上）
            // 但最安全的方式是使用 wxFont 的各种属性创建新的 wxNativeFontInfo 
            // 
            // *最好的办法是使用一个 wxFont 的拷贝，然后用其公共属性进行修改*

            // 由于 wxFont 没有公共方法 SetFaceName，我们只能通过 wxNativeFontInfo 来间接修改。

            // 在 Windows 上，wxNativeFontInfo 内部通常包含 LOGFONTW 结构体。
            // 我们可以利用 wxNativeFontInfo 的平台相关特性来完成修改。

            // 【方法：使用 wxNativeFontInfo 拷贝，然后修改其内部 LOGFONT 字段】
            // wxNativeFontInfo::GetNativeFontInfo() 返回的指针，在 wxMSW 中通常是指向 LOGFONTW 的。

            // 1. 创建一个 wxNativeFontInfo 的拷贝
            wxNativeFontInfo newNativeInfo = *originalInfo;

            // 2. 将其转换为 LOGFONTW 指针（这是 MSW 平台依赖的，但非常高效且常见）
            // 我们不能直接调用 ToLogFont，但我们可以将 wxNativeFontInfo 强制转换为 LOGFONTW 指针进行修改
            // 确保使用 WCHAR 版本 LOGFONTW，因为 wxWidgets 在 Windows 上使用 Unicode
            LOGFONTW* lf = (LOGFONTW*)(const_cast<wxNativeFontInfo*>(&newNativeInfo));

            // 3. 修改 lfFaceName 字段
            wcsncpy(lf->lfFaceName, standardEnglishName.wc_str(), LF_FACESIZE);
            lf->lfFaceName[LF_FACESIZE - 1] = L'\0';

            // 4. 从修改后的 newNativeInfo 创建新的 wxFont
            return wxFont(newNativeInfo);
        }
    }

    // 失败或提取名称失败，返回原始字体
    return font;
}

// -------------------------------------------------------------------
// 核心接口：字体名称反向转换
// -------------------------------------------------------------------

/**
 * @brief 将字体的标准英文名称转换为当前系统环境下的本地化显示名称。
 * * @param englishName 输入的字体英文名称。
 * @return wxString 返回找到的本地化名称；如果转换失败或本地化名称与英文名相同，则返回原始英文名。
 */
inline wxString ConvertEnglishFontToLocalized(const wxString& englishName) {
    // 如果输入不是 ASCII 字符（理论上输入应该是英文名，但作为保险）
    if (!englishName.IsAscii()) {
        return englishName;
    }

    IDWriteFactory* pFactory = DirectWriteFactoryRAII::Get();
    if (pFactory == nullptr) {
        return englishName;
    }

    ComPtr<IDWriteFontCollection> pFontCollection;
    HRESULT hr = pFactory->GetSystemFontCollection(&pFontCollection);

    if (FAILED(hr) || !pFontCollection) {
        return englishName;
    }

    // 1. 查找字体家族索引：使用英文名查找
    UINT32 index = 0;
    BOOL exists = FALSE;
    // 使用英文名查找字体家族
    hr = pFontCollection->FindFamilyName(englishName.wc_str(), &index, &exists);

    if (FAILED(hr) || !exists) {
        return englishName; // 未找到该英文名对应的字体家族
    }

    // 2. 获取字体家族对象
    ComPtr<IDWriteFontFamily> pFontFamily;
    hr = pFontCollection->GetFontFamily(index, &pFontFamily);

    if (FAILED(hr) || !pFontFamily) {
        return englishName;
    }

    // 3. 获取字体家族的名称集合
    ComPtr<IDWriteLocalizedStrings> pFamilyNames;
    hr = pFontFamily->GetFamilyNames(&pFamilyNames);

    if (FAILED(hr) || !pFamilyNames) {
        return englishName;
    }

    // 4. **查找本地化名称**
    // 理想情况下，我们应该查找当前 UI 线程的语言环境名称，例如 "zh-cn"。
    // DirectWrite 提供了一个 FindLocaleName 方法，可以查找本地化名称。

    // 【方法一：查找当前线程的 UI 语言】
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = { 0 };
    // GetUserDefaultLocaleName 适用于 C++17 之后的标准语言环境查找，但在 wxWidgets/旧版本 VS 中可能略有不同
    // 另一种更可靠的方式是使用 GetUserDefaultUILanguage 或 GetSystemDefaultUILanguage 
    // 并将其转换为 BCP 47 格式 (如 "zh-CN")。

    // 简单高效的方式：直接使用 FindLocaleName，让 DirectWrite 查找最匹配当前系统的名称。

    UINT32 nameIndex = 0;
    BOOL found = FALSE;

    // DirectWrite 会根据系统 UI 语言进行自动匹配
    // 查找空字符串 "" 会返回最合适的名称（通常是本地化名称或英文 PostScript 名称）
    hr = pFamilyNames->FindLocaleName(L"", &nameIndex, &found);

    // 如果上述方法失败或找不到，则尝试查找 "zh-cn" (假设目标是中文环境)
    if (FAILED(hr) || !found) {
        hr = pFamilyNames->FindLocaleName(L"zh-cn", &nameIndex, &found);
        if (FAILED(hr) || !found) {
            // 否则，回退到第一个名称 (索引 0)
            nameIndex = 0;
        }
    }


    // 5. 提取名称字符串
    UINT32 length = 0;
    hr = pFamilyNames->GetStringLength(nameIndex, &length);

    if (SUCCEEDED(hr)) {
        // length + 1 是为了包含 null 终止符
        std::wstring wsName(length, L'\0');
        hr = pFamilyNames->GetString(nameIndex, &wsName[0], length + 1);

        if (SUCCEEDED(hr)) {
            wxString localizedDisplayName(wsName.c_str());

            // 确保找到的名称不是原始的英文名 (防止返回 "Arial" for "Arial")
            if (localizedDisplayName.IsSameAs(englishName, false)) {
                return englishName;
            }

            return localizedDisplayName;
        }
    }

    // 失败或提取名称失败，返回原始英文名
    return englishName;
}