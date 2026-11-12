# wxNotepad2

小巧轻量的 Windows 桌面树状笔记软件，基于 **C++23 / wxWidgets (Scintilla)** 与 **SQLite3 (FTS5)** 构建，单文件绿色运行，无需安装额外运行库。

> 本仓库基于原版 [wxNotepad2](https://codeberg.org/ibulis/wxNotepad2) 维护改进，遵循 **GNU GPLv3** 开源协议。

---

## 直接下载使用

前往 [GitHub Releases](https://github.com/symudo/wxNotepad2/releases/latest) 页面，直接下载 `wxNotepad2.exe` 即可双击运行。

### 数据存储模式

- **默认模式**：配置与数据库保存在 `%APPDATA%\wxNotePad\` 目录（`wxNotePad.ini` 与 `wxNotePad.db`）。
- **便携模式**：在 `wxNotepad2.exe` 同目录下新建一个空的 `wxNotePad.ini` 文件，程序即把所有配置与笔记保存在当前目录。
- **指定数据库模式**：支持命令行参数 `-o`（弹窗选择数据库文件）或 `-d path\to\wxNotePad.db`（直接指定数据库路径）。

---

## 核心功能与改进

- **树状结构与卡片预览**：左侧采用树形目录管理文件夹、笔记、标签、最近笔记与回收站；选中文件夹时右侧显示内容预览卡片。
- **自适应行号显示**：编辑器行号栏根据文档总行数位数、系统 DPI 与缩放比例动态扩展宽度（参考 Notepad++ 显示效果），告别百行以上行号截断。
- **实时防抖保存**：编辑正文或调整文本样式后停笔默认 **1 秒**自动写入 SQLite 数据库并同步清除未保存 `*` 标记（可在`全局设置 -> 实时保存延迟`中自定义为 `200~60000` 毫秒，或设为 `0` 关闭定时实时保存，保留完整 `Ctrl+Z` 撤销历史）；切换节点、隐藏到托盘及退出时立即兜底落盘。
- **全文检索（FTS5）**：支持自定义中文按字分词的全局笔记检索（`Ctrl+Shift+F`）以及当前笔记查找替换（`Ctrl+F`）。
- **轻量富文本样式与大纲导航**：支持加粗、前/背景高亮色、标题与大纲级别样式，并可在工具栏右侧下拉框快速跳转大纲目录。
- **实用编辑效率工具**：
  - 连续输入 4 个 `=` / `*` / `#` / `-` 自动展开分隔线
  - 新行输入算式加 `=` 回车自动求值（基于 `tinyexpr`）
  - 剪切板监听（支持窗口标题黑名单）、自定义链接/命令、网络收藏服务（基于 `mongoose`）、暗色主题与系统托盘常驻
- 更多操作细节与快捷键请参阅项目内的 [wxNotepad2帮助.md](wxNotepad2%E5%B8%AE%E5%8A%A9.md)。

---

## 编译构建说明

### GitHub Actions 自动构建

推送代码到 `main` / `master` 分支或推送 `v*` 版本标签时，[build.yml](.github/workflows/build.yml) 会自动在 `windows-latest` 上使用 MSVC + Ninja + vcpkg（`x64-windows-static` / `/MT` 全静态链接）编译单文件 `wxNotepad2.exe`。

### 本地自行构建（Windows）

前置要求：Visual Studio 2022（C++ 桌面开发）、CMake 3.20+、Ninja 与 vcpkg。

```powershell
cmake -B build -S . -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

cmake --build build --config Release --parallel
```

构建完成后可在 `build\wxNotepad2.exe` 获取可执行文件。

---

## 开源协议

本项目基于 [GNU General Public License v3.0](LICENSE) 开源。
