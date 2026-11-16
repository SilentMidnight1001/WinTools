# WinTools 库文档

**版本：2.0.0（由 1.13.1 更新）**  
**最后更新：2026/11/16**

> **OCR 模块已更换引擎（进行中）**
> `ocr::Ocr` 已由 Tesseract 切换为 **RapidOCR**（PP-OCRv6 small ONNX 模型 + onnxruntime），正在尝试另一个方向；
> 类接口、依赖清单与本文件都还在持续更新中，以代码为准（详见 [OCR 文字识别示例](#ocr文字识别)）。

## 项目目录

- [概述](#概述)
- [快速开始与依赖](#快速开始)
- [模块独立构建](#模块独立构建)
- [鼠标操作示例](#鼠标操作)
- [键盘操作示例](#键盘操作)
- [图像处理示例](#图像处理)
- [OCR 文字识别示例](#ocr文字识别)
- [视频处理示例](#视频处理)
- [音频处理示例](#音频处理)
- [实用工具示例](#实用工具)
- [文件对话框示例](#文件对话框)
- [DLL 调用示例](#dll调用)
- [压缩与解压示例](#压缩与解压)
- [弹窗与窗口控制示例](#弹窗与窗口控制)
- [进程与系统管理示例](#进程与系统管理)
- [PowerShell 与 CMD 调用示例](#powershell-与-cmd-调用)
- [清理与多线程示例](#清理与多线程)
- [完整示例](#完整示例)
- [注意事项](#注意事项)
- **更新追溯**
    - [2026-11-16 · 控制台输入与调试输出迁移到 wtlPrint](#update-20261116)
    - [2026-11-15 · OCR 模块独立构建与产物目录分层](#update-20261115)
    - [2026-11-15 · wtlMediaToolkit 音频播放控制](#update-20261115-audio)
    - [2026-10-31 · 版本 2.0.0：类名统一与 OCR 多目标定位](#update-20261031)
    - [2026-10-15 · OCR 引擎更换为 RapidOCR（进行中）](#update-20261015)
    - [2026-09-20 · 版本 1.12：键鼠模块拆分与独立库统一](#update-20260920)
    - [2026-09-18 · 版本 1.11：文件模块拆分与 FileObject](#update-20260918)
    - [2026-09-13 · 项目更名：WinKeyPress → WinTools](#update-20260913)
    - [2026-09-05 · 版本 1.10：API 命名统一](#update-20260905)
    - [2026-08-28 · 输出与格式化](#update-20260828)

---

<a id="update-20261116"></a>
### 本次更新（2026/11/16）— 控制台输入与调试输出迁移到 wtlPrint

1. **接口与实现迁移**
   - `wtl::useConsoleUtf8()`、`wtl::input()`、`wtl::debugMessage()` 的声明从 `winToolsH.hpp` 移至 `wtlPrintFormat.hpp`。
   - 三个函数的实现从 `winTools.cpp` 移至 `wtlPrint.cpp`。
   - `DebugMessage` 宏随 `debugMessage` 一起移至 `wtlPrintFormat.hpp`。

2. **模块归属**
   - `wtlPrint` / `wtlPrintStatic` 现在同时提供控制台编码、控制台输入、调试输出和原有格式化输出接口。
   - 独立使用这些接口时可只包含 `wtlPrintFormat.hpp`，并链接 `wtlPrint` 或 `wtlPrintStatic`。

3. **兼容性**
   - 函数名、参数、默认值和 `wtl` 命名空间均未改变。
   - `winToolsH.hpp` 继续包含 `wtlPrintFormat.hpp`，原有通过聚合头文件调用的代码无需修改。
   - `vsVersion` 与 `mingwVersion` 两套源码保持同步。

4. **验证**
   - MinGW 与 MSVC 下 `wtlPrint`、`wtlPrintStatic`、`WinTools`、`WinToolsStatic` 均编译通过。
   - 已确认 `wtl::useConsoleUtf8`、`wtl::input`、`wtl::debugMessage` 位于 `wtlPrint` 动态库和静态库中。

---

<a id="update-20261115"></a>
### 本次更新（2026/11/15）— OCR 模块独立构建与产物目录分层

1. **新增独立 OCR 模块**
   - 公开接口从 `winToolsH.hpp` 拆分到 `wtlOcr.hpp`，实现文件 `wtlOcr.cpp` 不再包含聚合头文件。
   - 新增动态库目标 `wtlOcr` 与静态库目标 `wtlOcrStatic`，命名规则与其他 `wtl*` 模块一致。
   - MSVC 输出 `wtlOcr.dll` / `wtlOcr.lib` 与 `wtlOcrStatic.lib`；MinGW 输出 `libwtlOcr.dll` / `libwtlOcr.dll.a` 与 `libwtlOcrStatic.a`。
   - `WinTools` 与 `WinToolsStatic` 继续作为完整聚合库保留；`winToolsH.hpp` 仍会包含 `wtlOcr.hpp`，原有聚合调用方式不受影响。

2. **依赖隔离**
   - RapidOCR 头文件仍只在 `wtlOcr.cpp` 内使用，公共头文件不暴露 OcrLite C API。
   - `wtlOcr` 直接链接 OpenCV、RapidOCR 及所需 Windows 系统库，可单独引用，不必依赖 `WinTools` 聚合库。
   - 构建 `wtlOcr` 时会把 RapidOCR、onnxruntime 和对应 OpenCV DLL 复制到 `wtlOcr.dll` 所在目录。

3. **文件编码**
   - 新增和修改的 OCR 头文件、源文件、`winToolsH.hpp` 与 CMake 配置均保持 UTF-8 BOM，兼容 Visual Studio 编译。

4. **构建产物目录分层**
   - 项目目标不再把 DLL、导入库和静态库输出到构建根目录。
   - `source/src/lib` 下按用途分为 `dll`、`link`、`static` 三个目录。
   - `dll` 存放运行期 DLL，`link` 存放 DLL 对应导入库，`static` 存放 `*Static` 静态库。
   - 第三方 DLL 和导入库由 `WinToolsBinDir` 统一复制到对应目录。

5. **Bug 修复记录**

   | 问题 | 位置 | 处理 | 影响 |
   | --- | --- | --- | --- |
   | MinGW 完整编译失败：`string does not name a type` | `mingwVersion/include/wtlMediaToolkit.hpp` | 删除未使用的私有成员 `string audioPath` 及多余空注释 | 修复 MinGW 编译阻塞；不改变公共接口 |
   | MSVC Release 使用 GCC 参数 `-O3`，产生 `D9002` 警告 | `CMakeLists.txt` | MSVC 改为 `/O2`，MinGW 保留 `-O3` | 消除警告并修正 MSVC Release 优化配置 |
   | VS / MinGW 源码文件名可能不一致 | `vsVersion`、`mingwVersion` | 逐目录比较 `include` 与 `src/source` | 两套目录均为 15 个头文件、15 个源文件，文件名完全一致 |
   | 中文注释存在明显拼写错误 | `vsVersion/include/winToolsH.hpp`、`mingwVersion/include/winToolsH.hpp` | `布偶值` 改为 `布尔值`，`dpi司陪房` 改为 `DPI 缩放` | 仅修改注释，不影响代码行为 |

6. **命名兼容说明**
   - `wtlFiledialog`、`wtlKeyBoard` 为项目已有公开文件名、目标名和类名，本次不重命名，避免破坏现有调用代码。
   - 本次未修改任何 C++ 公共标识符、CMake 目标名或库文件名。
   - 修复后已执行 MinGW 全量构建和 VS2022 Release 全量构建，均通过。

---

<a id="update-20261115-audio"></a>
### 本次更新（2026/11/15）— wtlMediaToolkit 音频播放控制

1. **AudioTools 播放接口**
   - `wtl::AudioTools` 新增 Media Foundation 播放能力：`playAudio()`、`pauseAudio()`、`stopAudio()`。
   - `playAudio()` 在暂停后再次调用时从当前位置继续播放。
   - `stopAudio()` 停止播放并释放播放器资源；`closeAudio()` 作为兼容别名保留。
   - 新增 `getLastError()`，返回最近一次英文错误信息。

2. **构造函数重载**

   | 构造函数 | 说明 |
   | --- | --- |
   | `AudioTools()` | 创建未指定音频路径的播放器，原有音频处理能力保持可用 |
   | `AudioTools(const std::string& audioPath)` | 使用 UTF-8 音频路径创建播放器 |

3. **空路径保护**
   - `playAudio()`、`pauseAudio()`、`stopAudio()` 调用前都会检查音频路径。
   - 路径为空时返回 `false`，通过 `std::cerr` 输出英文原因，同时更新 `getLastError()`。
   - 示例输出：

   ```text
   Cannot play audio: Audio path is empty
   Cannot pause audio: Audio path is empty
   Cannot stop audio: Audio path is empty
   ```

4. **构建依赖**
   - `wtlMediaToolkit`、`wtlMediaToolkitStatic` 和 `WinTools` 增加 Media Foundation 系统库依赖：`mfplay`、`mfplat`、`mfuuid`。
   - 同时链接 `shlwapi`、`uuid` 和 `ole32`。

5. **验证**
   - MinGW 与 MSVC 下 `wtlMediaToolkit`、`wtlMediaToolkitStatic`、`WinTools` 均编译通过。
   - 已使用中文 UTF-8 路径音频完成播放、暂停、继续播放和停止的运行时验证。

---

<a id="update-20261031"></a>
### 本次更新（2026/10/31）— 版本 2.0.0：类名统一与 OCR 多目标定位

> 本版删除了旧的 `*Event` 类名，属于**破坏性接口变更**，因此版本号由 1.13.1 升级为 2.0.0。

1. **公开类名统一为 `Tools`**

   | 旧类名 | 新类名 |
   | --- | --- |
   | `KeyBoardEvent` | `KeyBoardTools` |
   | `MouseEvent` | `MouseTools` |
   | `ImageEvent` | `ImageTools` |
   | `VideoEvent` | `VideoTools` |
   | `AudioEvent` | `AudioTools` |

   键鼠和媒体类统一以 `Tools` 结尾，旧类名不再保留。方法名及其参数未改动，已有代码需要替换类名。

2. **OCR 按文字定位按钮**
   - `ocr::Ocr` 新增 4 个接口：`getButtonRect()`、`getButtonPosition()`、`getButtonRectVec()`、`getButtonPositionVec()`。
   - 单结果接口返回第一个匹配；Vec 接口返回当前虚拟屏幕中全部匹配的重复按钮。
   - Vec 结果按匹配优先级、从上到下、从左到右排序，矩形与中心点顺序一致。
   - 返回屏幕坐标；单结果接口未找到时 `result=false`、坐标为 -1，Vec 接口返回空 vector。

---

<a id="update-20261015"></a>
### 本次更新（2026/10/15）— OCR 引擎更换为 RapidOCR（进行中）

1. **OCR 引擎更换**
   - `ocr::Ocr` 由 Tesseract 改为 RapidOCR（PP-OCRv6 small ONNX 模型 + onnxruntime，走 OcrLite C API）。
   - 旧实现一并移除：`tessdata` 语言包、`setLanguage()` 切换语言、Tesseract/Leptonica 链接；`ocr_tessdata/` 目录不再使用。
   - 这一版是新的尝试方向，识别参数、接口细节和文档仍在持续更新中。

2. **模型目录**
   - 新增 `<工程根>/model/`：`PP-OCRv6_det_small.onnx`、`ch_ppocr_mobile_v2.0_cls_mobile.onnx`、`PP-OCRv6_rec_small.onnx`、`ppocrv6_dict.txt`。
   - 模型只保留在工程根的 `model/`，构建时不再复制到输出目录或 `src/lib`；`RADIOOCR_MODELS_DIR` 指向该目录。
   - 构造函数按「传入目录 → 运行目录 `./model` → 编译期 `RADIOOCR_MODELS_DIR`」查找模型。

3. **CMake：RapidOCR 跨编译器依赖**
   - `RadiOcr_DIR`（默认 `D:/CPPLibrary/radioOCR`）下按编译器取 `mingwVersion` / `vsVersion`，链接 `libRapidOcrOnnxClib.dll.a` 或 `RapidOcrOnnx.lib`。
   - 项目二进制与第三方运行期 DLL 直接输出到构建目录的 `source/src/lib`；`vsVersion` / `mingwVersion` 下只放源码与头文件，不再生成 `bin`。
   - 头文件只在 `wtlOcr.cpp` 内部包含，公共头文件不暴露 RapidOCR 类型。

4. **`ocr::Ocr` 接口**
   - 构造即初始化、析构即释放；`ready()` 判断引擎是否可用，`modelsPath()` 返回实际使用的模型目录。
   - `readImageText()` / `readImageTextLine()` 已按 RapidOCR 实现（自读图片字节 + 行首缩进还原），`outConf` 返回平均置信度（0-100）。

5. **中文路径**
   - 图片路径按 UTF-8 处理（`fs::u8path`），图片由本库读成字节后再交给引擎，中文路径的图片可正常识别。
   - 模型目录必须是纯 ASCII（RapidOCR 内部把窄字符串逐字节转宽字符），中文路径会先尝试相对路径、8.3 短名等价的 ASCII 形式，拿不到就返回未就绪而不是崩溃。

6. **初始化输出**
   - RapidOCR 的 `CrnnNet::initModel` 里有一句无条件的 `printf("total keys size(%lu)\n", ...)`，绕过库内的 `Logger` 开关，任何调用方初始化模型时都会在 stdout 最上面看到它；已从 `RadiOcr_DIR` 的 `vsVersion` / `mingwVersion` 源码中删除并重新编译两份库，重新从上游拉取该库会把它带回来。

---

<a id="update-20260920"></a>
### 本次更新（2026/9/20）— 版本 1.12：键鼠模块拆分与独立库统一

1. **键盘模块文件调整**
   - `wtlKeyboard.hpp/.cpp` 调整为 `wtlKeyBoard.hpp/.cpp`。
   - `winToolsH.hpp` 改为包含 `wtlKeyBoard.hpp`。
   - Windows 文件系统不区分大小写，`wtlKeyboard.hpp` 与 `wtlKeyBoard.hpp` 不能同时存在，键盘头文件统一使用 `wtlKeyBoard.hpp`。

2. **键鼠模块解除聚合库依赖**
   - `wtlKeyBoard.cpp` 与 `wtlMouse.cpp` 不再包含 `winToolsH.hpp`。
   - 模块内部改为直接包含 `wtlKeyBoard.hpp`、`wtlMouse.hpp`、`wtlStringCode.hpp`、`wtlTime.hpp`、`windows.h` 和 `interception.h`。
   - 时间等待改用 `wtl::time::sleep`。
   - 键鼠模块使用 `std::call_once` 和 `std::atexit` 注册独立退出清理，单独链接时不再依赖 `WinTools` 聚合库中的 `autoClear` 与 `exitCheckWork`。

3. **模块动态库与静态库统一**
   - 动态库目标使用模块原名，静态库目标使用 `Static` 后缀。
   - `wtlDict`、`wtlPrint`、`wtlStringCode`、`wtlFileManagement`、`wtlMediaToolkit`、`wtlTime`、`wtlZip`、`wtlOpen`、`wtlRegex`、`wtlKeyBoard`、`wtlMouse` 均提供动态库和静态库目标。
   - `WinTools` 与 `WinToolsStatic` 继续作为完整聚合库保留。

4. **纯头文件目标**
   - `wtlThrowError`、`wtlTypeName` 使用 CMake `INTERFACE` 目标，不生成二进制库。
   - `wtlThrowError.cpp` 仅包含示例代码，不参与独立库构建。

5. **编码与构建**
   - 修改过的 CMake、键鼠头文件和键鼠源文件使用 UTF-8 BOM。
   - MSVC 与 MinGW 下全部模块动态库、静态库以及 `WinTools`、`WinToolsStatic` 均已编译通过。

---

<a id="update-20260918"></a>
### 本次更新（2026/9/18）— 版本 1.11：文件模块拆分与 FileObject

1. **文件管理模块独立**
   - 新增 `include/wtlFileManagement.hpp` 与 `src/source/wtlFileManagement.cpp`。
   - `FileManagement` 类声明从 `winToolsH.hpp` 移出，方法实现从 `winTools.cpp` 移出。
   - `winToolsH.hpp` 继续包含 `wtlFileManagement.hpp`，原本只引用聚合头的代码无需修改。

2. **新增独立构建目标**
   - 新增静态库目标 `wtlFileManagement`。
   - `WinTools` 与 `WinToolsStatic` 均已加入 `wtlFileManagement.cpp`。
   - `wtlFileManagement` 通过 `PUBLIC` 依赖 `wtlStringCode`。
   - MinGW 输出 `libwtlFileManagement.a`，MSVC 输出 `wtlFileManagement.lib`。

3. **新增第三方 FileObject 文件目录对象**
   - `FileObject` 已并入 `wtl` 命名空间，声明位于 `wtlFileManagement.hpp`，实现位于 `wtlFileManagement.cpp`。
   - 支持目录递归加载、目录树输出、全部文件路径获取以及文件名递归搜索。
   - 支持 `EXACT`、`REGEX`、`WHOLE_WORD`、`CONTAINS` 四种匹配模式。
   - 支持区分或不区分大小写的文件名匹配。
   - `FileObject` 当前没有模板成员，全部方法均在 cpp 中实现；后续若增加模板接口，模板定义需保留在头文件中。

4. **模板缩放默认值统一**
   - `getImagePosition` 与 `getImagePositionVec` 的 `templateZoom` 默认值统一为 `1.0`。
   - `templateZoom=1.0` 表示模板图片在 100% 缩放的电脑上采集。
   - 识别时会根据当前系统缩放自动计算模板匹配尺寸。

5. **编码与兼容**
   - `wtlFileManagement.hpp/.cpp` 使用 UTF-8 BOM 保存，避免 MSVC 按系统本地代码页解析中文注释。
   - MinGW 与 VS 两套版本中的 `wtlFileManagement.hpp/.cpp` 内容保持一致。
   - MinGW 和 MSVC 下的 `wtlFileManagement`、`WinTools`、`WinToolsStatic` 均已编译通过。

---

<a id="update-20260913"></a>
### 本次更新（2026/9/13）— 项目更名：WinKeyPress → WinTools

> ⚠️ 本次为**项目名称变更**，库名、头文件、命名空间、静态库目标等同步调整，请按下方说明更新构建脚本与引用代码。

1. **项目更名为 WinTools**  
   原 `WinKeyPress` 正式更名为 `WinTools`，库对外品牌名、文档标题、CMake 工程名同步更新。

2. **头文件与静态库目标同步更名**  
   - 主头文件 `winKeyPressH.hpp` 更名为 `winToolsH.hpp`（建议引用处同步替换）。  
   - 主聚合静态库 `WinKeyPressStatic` 更名为 `WinToolsStatic`：MinGW 输出 `libWinToolsStatic.a`，MSVC 输出 `WinToolsStatic.lib`。  
   - 模块静态库简写命名保持 `wtlDict`、`wtlPrint`、`wtlStringCode`、`wtlZip` 不变。  
   - 共享库自动导出策略保持 `--export-all-symbols` / `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` 不变。

3. **C++ 命名空间统一为 `wtl`**  
   当前公开类、函数、枚举均使用 `wtl` 命名空间，例如 `wtl::MouseTools`、`wtl::ImageTools`、`wtl::FileObject`。

4. **功能无变化**  
   本次仅更名，未新增/删除/修改任何功能接口。

5. **构建环境与依赖版本**  
   详细依赖清单见项目根目录 `read.txt`，关键版本如下：  
   - 构建工具链：CMake ≥ 3.27，C++17；MinGW（x64-mingw-static）或 MSVC（x64-windows，需 `/utf-8`）  
   - OpenCV 4.11.0、Tesseract 5.5.1、Leptonica 1.85.0、zlib 1.3.1  
   - Interception（驱动级键鼠）、FFmpeg（2026-03-18-git-106616f13d，视频/音频处理）  
   - vcpkg 传递依赖：libpng 1.6.50、libjpeg-turbo 3.1.1、libtiff 4.7.0、libwebp 1.6.0、giflib 5.2.2、openjpeg 2.5.3、liblzma 5.8.1 等

---

<a id="update-20260905"></a>
### 本次更新（2026/9/5）— 版本 1.10：接口命名统一

> ⚠️ 本次为**破坏性接口变更**，旧函数名不再可用，请按 `read.txt` 中的映射更新调用代码。

> 版本 1.0 → 1.10：本次**未新增功能**，只统一 API 格式，将原本大写字母开头的 API 改为小驼峰命名。

1. **公开接口函数统一小驼峰**  
   `MouseClick` → `mouseClick`、`GetImagePosition` → `getImagePosition`、  
   `KillProcess` → `killProcess`、`ExitCheckWork` → `exitCheckWork` 等。
2. **类内辅助方法统一 `_` 前缀**  
   `getOneKeyCode(key_name)` → `_getOneKeyCode(key_name)`、  
   `CreateDirectoryRecursive` → `_createDirectoryRecursive` 等。
3. **文件级辅助函数使用 `_` 前缀并加 `static`**  
   `cleanup_check` 改为 static `_cleanupCheck`，不再作为公开接口声明。
4. **清理注册去重**  
   `KeyBoardTools`/`MouseTools` 构造改为调用 `exitCheckWork()`，  
   `exitCheckWork()` 内部用 `autoClear` 防止重复注册退出清理。
5. 双下划线内部符号、构造函数/析构函数、宏、类名、命名空间、变量、枚举不参与改名。
6. 完整旧名到新名清单见项目根目录 `read.txt`。
7. **模块拆分**：新增 `include/wtlZip.hpp`，`ZipDirToBin` 声明从 `winToolsH.hpp` 移出，  
   主头文件通过 `#include <wtlZip.hpp>` 引入，`wtlZip.hpp` 已加入 `WinToolsStatic` 静态库源列表，功能接口不变。
8. **CMake 静态库命名**：`WinToolsLb` 改为 `WinToolsStatic`，MinGW 输出 `libWinToolsStatic.a`，  
   MSVC 输出 `WinToolsStatic.lib`；共享库继续使用 `--export-all-symbols` / `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` 自动导出。
9. **模块静态库简写命名**：新增 `wtlDict`、`wtlPrint`、`wtlStringCode`、`wtlZip` 静态库目标；  
   MinGW 输出 `libwtlDict.a` 等，MSVC 输出 `wtlDict.lib` 等；主聚合静态库保持 `WinToolsStatic`。

<a id="update-20260828"></a>
### 本次更新（2026/8/28）— 输出与格式化

> ⚠️ 含一处**行为变更**，升级前请看第 1 条。

1. **`print`/`println` 多参数自动空格分隔**（对齐 Python 的 `print`）  
   `wtl::println("a","b","c")` 由 `abc` 变为 `a b c`。  
   受影响写法：`println("值: ", x)` 现输出 `值:  x`（两个空格），应改写为 `println("值:", x)`。  
   含 `{}` 的格式化调用不受影响。详见 [输出函数](#十五输出函数)。
2. **含花括号的普通文本不再崩溃**  
   正则 `\d{2,3}`、集合 `{a,b}`、JSON `{"k":1}` 此前会被误判为格式串，  
   导致抛异常终止或静默吞掉内容，现已能自动识别并原样输出。
3. **花括号转义 `}}` 修复**  
   `wtl::format("{{\"k\":{}}}", 1)` 此前抛异常，现正确输出 `{"k":1}`，规则与 Python 的 `str.format` 一致。  
   详见 [花括号转义](#花括号转义)。
4. **显式索引复用可用**  
   `println("{0} 和 {0}", "X")` 由原样输出变为正确输出 `X 和 X`，`{0} {1} {0}` 同理。
5. **修复 `wtl::color::printColor` 无法用字符串字面量调用**  
   模板重载内部错写为 `ColoredText`（实际类名 `__ColoredText__`），  
   此前 `printColor("红色文本", wtl::Color("red"))` 编译不过，现已修正。
6. **MSVC 编译修复**：`CMakeLists.txt` 加入 `NOMINMAX`，避免 `windows.h` 的 `min`/`max` 宏  
   把 `std::max(a,b)` 展开成非法的 `std::(...)`（MSVC 报 C2589）。

---

## 概述
该库为 Windows 平台提供鼠标/键盘模拟、热键注册、剪贴板操作、图像识别、OCR 文字识别、音视频处理、DLL 调用、文件压缩等功能，适用于自动化脚本开发。核心特性包括：
- **跨线程控制**：支持在独立线程中运行监听函数
- **链式操作**：可组合移动、点击、按键等事件
- **低延迟**：直接调用 Windows API 实现快速响应
- **图像识别**：支持通过模板匹配定位屏幕中的图像位置（基于 OpenCV）
- **OCR 文字识别**：已更换为 RapidOCR 引擎（onnxruntime + PP-OCRv6 small 模型），支持中英文识别；旧的 Tesseract 实现已移除，新方向仍在更新中（2026/10/15 更新）
- **驱动级操作**：支持驱动级鼠标键盘模拟（`drivingStage=true`）
- **编码转换**：提供 UTF-8、GBK、Big5 等多编码互转功能
- **窗口控制**：支持窗口置顶、消息弹窗等
- **进程管理**：支持进程终止、开机自启、DLL 注入、进程 PID 查询等功能
- **字典数据结构**：提供类似 Python 的 `Dict` 容器，保持插入顺序，支持 JSON 序列化
- **格式化输出**：支持类似 Python 的 `print` 和 `format` 功能，多参数自动以空格分隔（2026/8/28 更新）
- **正则表达式**：提供 `findAll`、`search`、`sub` 等正则操作（`re` 命名空间）
- **颜色输出**：支持控制台彩色文本输出（`Color` 类）
- **随机数生成**：提供多种随机数生成功能（`random` 命名空间）
- **文件操作**：提供 `FileManagement` 文件管理类和 `Open` 文件读写类
- **文件目录对象**：第三方 `FileObject` 支持递归目录加载、文件名多模式搜索、目录树输出和完整文件路径获取
- **音视频处理**：支持视频截取、合并、添加音频、封面设置、获取视频信息等
- **DLL 调用**：支持动态调用 DLL 函数（支持 `stdcall`/`cdecl`/`fastcall`/`thiscall`）
- **文件压缩**：支持文件夹压缩与解压（ZIP 格式）
- **加密功能**：支持字符串加密解密、二进制资源嵌入
- **控制台控制**：支持控制台隐藏/显示、光标定位、行刷新
- **系统音量控制**：支持设置系统音量
- **进程 PID 查询**：支持根据进程路径获取 PID
- **PowerShell/CMD 调用**：通过 `PowerShellSession` 和 `CommandSession` 类执行命令（2026/8/3 更新）
- **注册表操作**：支持读取注册表键值
- **窗口焦点锁定**：支持锁定窗口焦点
- **调试信息输出**：支持多级别调试日志，带文件名、函数名、行号和颜色输出
- **异常处理**：提供 Python 风格的异常抛出工具（`ThrowError` 类）
- **类型识别**：`typeName` 函数支持识别多种数据类型（包括 `Dict`、`tuple` 等）

**版本 1.10 API 改名介绍**

本版本未添加新功能，仅将原大写开头 API 统一改为小驼峰，例如：

- `AddHotKey` → `addHotKey`
- `MouseClick` → `mouseClick`
- `GetImagePosition` → `getImagePosition`
- `KillProcess` → `killProcess`
- `ExitCheckWork` → `exitCheckWork`
- `setdefault` → `setDefault`
- `findall` → `findAll`
- `randint` → `randInt`
- `askopenfilename` → `askOpenFilename`

其余 API 的完整旧名 → 新名映射见项目根目录 `read.txt`。

---

## 快速开始
### 1. 引入头文件
```cpp
#include "winToolsH.hpp"
using namespace std;
```

### 2. 编译指令
```bash
g++ main.cpp -o app -lUser32 -lOle32 -lgdiplus -lopencv_core -lopencv_highgui -lopencv_imgproc -lShlwapi -lws2_32 -lRapidOcrOnnxClib
```
*注：使用图像识别功能需链接 OpenCV 库。OCR 已更换为 RapidOCR，可单独链接 `wtlOcr` / `wtlOcrStatic`；链接时需要 `libRapidOcrOnnxClib.dll.a`（MSVC 为 `RapidOcrOnnx.lib`），运行时需要 `libRapidOcrOnnxClib.dll` / `RapidOcrOnnx.dll`、`onnxruntime.dll` 以及模型目录 `model/`。链接 `wtlOcr` 时 CMake 会携带 RapidOCR 链接依赖并复制运行期 DLL；链接静态库时最终可执行程序仍需准备这些运行期文件。*

*注：若代码中使用 `std::min`/`std::max`，建议加上 `-DNOMINMAX`（MSVC 为 `/DNOMINMAX`）。  
`windows.h` 会把 `max` 定义成宏，使 `std::max(a,b)` 展开成非法的 `std::(...)`。  
本项目的 `CMakeLists.txt` 已默认定义该宏。（2026/8/28 更新）*

### 3. 本项目依赖与版本

WinTools 当前构建环境使用以下主要依赖：

| 依赖 | 版本 | 用途 |
| --- | --- | --- |
| OpenCV | 4.11.0 | 图像识别、图像处理、视频读取/写入 |
| RapidOCR (RapidOcrOnnx) | 独立构建，本机位于 `D:/CPPLibrary/radioOCR` | OCR 文字识别（PP-OCRv6 small ONNX 模型） |
| onnxruntime | 随 RapidOCR 提供 | OCR 模型推理 |
| zlib | 1.3.1 | 文件夹压缩/解压、文件数据压缩 |
| Interception | 本机版本未标注 | 驱动级鼠标/键盘模拟 |
| FFmpeg | 2026-03-18-git-106616f13d | 音视频剪切/合并/转码等 |

构建工具链：

- CMake：`>= 3.27`
- C++ 标准：C++17
- MinGW：`g++`，版本目录 `WTL_DIR = D:/WinTools/mingwVersion`
- MSVC：Visual Studio C++（VS2019+），版本目录 `WTL_DIR = D:/WinTools/vsVersion`，需 `/utf-8` 编译选项（已自动开启）

> 旧版 OCR（Tesseract + Leptonica）曾通过 vcpkg 引入一批传递依赖（libpng、libjpeg-turbo、libtiff、libwebp、giflib、openjpeg、liblzma，以及 MSVC 分支的 libcurl、libarchive、OpenSSL、zstd、lz4、bzip2）。
> 更换为 RapidOCR 后，这些链接项已从 `CMakeLists.txt` 移除，不再需要（2026/10/15 更新）。

Windows 系统库（无需额外安装，编译器自带）：

- `gdiplus` / `Shcore` / `Windowscodecs`
- `winmm` / `user32`
- `ws2_32` / `crypt32` / `secur32` / `xmllite` / `iphlpapi`
- （`winmm`、`user32` 通过源码内 `#pragma comment(lib,...)` 声明）

运行时资源（随项目分发，非编译链接项）：

- `model/`：OCR 模型目录（RapidOCR / PP-OCRv6 small），含 `PP-OCRv6_det_small.onnx`、`ch_ppocr_mobile_v2.0_cls_mobile.onnx`、`PP-OCRv6_rec_small.onnx`、`ppocrv6_dict.txt`；只保留在工程根，不随构建复制
- `ocr_tessdata/`：旧 Tesseract 版语言数据，更换引擎后已不再使用（保留仅为历史遗留）
- `driver/install-interception.exe`：Interception 驱动安装程序（`/install` 安装，`/uninstall` 卸载）
- `ffmpeg2026/`：FFmpeg 可执行文件（视频/音频处理）

第三方运行期 DLL（构建时由 CMake 复制到 `source/src/lib/dll`）：

- OpenCV（`opencv_world4110.dll` / `libopencv_world4110.dll`、`opencv_videoio_ffmpeg4110_64.dll`）
- `interception.dll`、`zlib.dll` / `libzlib.dll`
- RapidOCR（`RapidOcrOnnx.dll` / `libRapidOcrOnnxClib.dll`）、`onnxruntime.dll`
- 编译器运行时**不在复制范围内**：MinGW 的 `libgcc_s_seh-1.dll` / `libstdc++-6.dll` / `libwinpthread-1.dll` 属于可执行文件自己的事，由开发者自行准备；MSVC 侧对应的是系统级 VC++ 运行库（MSVCP140 / VCRUNTIME140 等，由 vc_redist 提供）

> 注：OpenCV 与 zlib 为源码外单独安装（路径写死在 CMake 中）；OCR 依赖来自 RapidOCR 的独立构建（`RadiOcr_DIR`，其下再分 `mingwVersion` / `vsVersion`，按编译器各取一份）。客户单独编译时，需按上述版本号准备对应库，并把 CMake 中的绝对路径（`OpenCV_DIR` / `Zlib_DIR` / `Interception_DIR` / `RadiOcr_DIR`）改成本机实际路径。

---

## 模块独立构建

模块可以单独链接，不需要引入完整 `WinTools` 聚合库。使用模块头文件即可获得对应接口，例如：

```cpp
#include <wtlKeyBoard.hpp>
#include <wtlMouse.hpp>

wtl::KeyBoardTools keyboard;
wtl::MouseTools mouse;
```

`winToolsH.hpp` 仍会包含全部模块头文件，原有聚合调用方式不受影响。

### 模块文件

| 模块 | 头文件 | 源文件 |
| --- | --- | --- |
| 键盘 | `wtlKeyBoard.hpp` | `wtlKeyBoard.cpp` |
| 鼠标 | `wtlMouse.hpp` | `wtlMouse.cpp` |
| 字典 | `wtlDict.hpp` | `wtlDict.cpp` |
| 控制台输入输出与格式化 | `wtlPrintFormat.hpp` | `wtlPrint.cpp` |
| 字符串编码 | `wtlStringCode.hpp` | `wtlStringCode.cpp` |
| 文件管理 | `wtlFileManagement.hpp` | `wtlFileManagement.cpp` |
| 媒体工具 | `wtlMediaToolkit.hpp` | `wtlMediaToolkit.cpp` |
| OCR 文字识别 | `wtlOcr.hpp` | `wtlOcr.cpp` |
| 时间 | `wtlTime.hpp` | `wtlTime.cpp` |
| 压缩与解压 | `wtlZip.hpp` | `wtlZip.cpp` |
| 文件读写 | `wtlOpen.hpp` | `wtlOpen.cpp` |
| 正则表达式 | `wtlRegex.hpp` | `wtlRegex.cpp` |

### 动态库与静态库目标

| 模块 | 动态库目标 | 静态库目标 |
| --- | --- | --- |
| `wtlDict` | `wtlDict` | `wtlDictStatic` |
| `wtlPrint` | `wtlPrint` | `wtlPrintStatic` |
| `wtlStringCode` | `wtlStringCode` | `wtlStringCodeStatic` |
| `wtlFileManagement` | `wtlFileManagement` | `wtlFileManagementStatic` |
| `wtlMediaToolkit` | `wtlMediaToolkit` | `wtlMediaToolkitStatic` |
| `wtlOcr` | `wtlOcr` | `wtlOcrStatic` |
| `wtlTime` | `wtlTime` | `wtlTimeStatic` |
| `wtlZip` | `wtlZip` | `wtlZipStatic` |
| `wtlOpen` | `wtlOpen` | `wtlOpenStatic` |
| `wtlRegex` | `wtlRegex` | `wtlRegexStatic` |
| `wtlKeyBoard` | `wtlKeyBoard` | `wtlKeyBoardStatic` |
| `wtlMouse` | `wtlMouse` | `wtlMouseStatic` |

完整聚合库目标：

| 类型 | 目标 |
| --- | --- |
| 动态库 | `WinTools` |
| 静态库 | `WinToolsStatic` |

### 输出目录

所有项目二进制和第三方运行期依赖统一位于构建输出的 `source/src/lib`。该目录只包含以下三个子目录：

```text
source/src/lib
├── dll
├── link
└── static
```

| 目录 | 内容 | MinGW 示例 | MSVC 示例 |
| --- | --- | --- | --- |
| `dll` | 运行期动态库及第三方 DLL | `libWinTools.dll`、`libwtlOcr.dll`、`libRapidOcrOnnxClib.dll` | `WinTools.dll`、`wtlOcr.dll`、`RapidOcrOnnx.dll` |
| `link` | DLL 对应导入库及第三方导入库 | `libWinTools.dll.a`、`libwtlOcr.dll.a` | `WinTools.lib`、`wtlOcr.lib` |
| `static` | 名称带 `Static` 的静态库 | `libWinToolsStatic.a`、`libwtlOcrStatic.a` | `WinToolsStatic.lib`、`wtlOcrStatic.lib` |

完整路径：

```text
MinGW: buildMinGW/source/src/lib/{dll,link,static}
MSVC : buildVs/<Config>/source/src/lib/{dll,link,static}
```

CMake 通过目标属性控制输出位置：

```text
动态库 RUNTIME_OUTPUT_DIRECTORY -> dll
动态库 ARCHIVE_OUTPUT_DIRECTORY -> link
静态库 ARCHIVE_OUTPUT_DIRECTORY -> static
```

第三方依赖按相同规则拆分：

- 运行期 DLL 复制到 `dll`，包括 OpenCV、RapidOCR、onnxruntime、interception 和 zlib。
- 导入库复制到 `link`，包括 OpenCV `.dll.a` / `.lib`、RapidOCR `.dll.a` / `.lib`、interception `.lib` 和 zlib `.dll.a` / `.lib`。
- MSVC 链接器生成的 `.exp` 中间文件与对应导入库一同位于 `link`。

`WinToolsBinDir` 负责建立目录、复制第三方依赖，并依赖全部聚合库和模块目标：

```powershell
cmake --build buildMinGW --target WinToolsBinDir
cmake --build buildVs --config Release --target WinToolsBinDir
```

源码和头文件复制到：

```text
MinGW: buildMinGW/source/{include,src}
MSVC : buildVs/<Config>/source/{include,src}
```

`buildMinGW` 和 `buildVs/<Config>` 根目录只保留构建系统文件，不再生成项目 DLL、导入库或静态库。运行程序时仍需保证可执行文件能够找到 `dll` 目录中的动态库。

### CMake 链接

动态链接：

```cmake
target_link_libraries(app PRIVATE wtlOpen)
target_link_libraries(app PRIVATE wtlRegex)
target_link_libraries(app PRIVATE wtlKeyBoard)
target_link_libraries(app PRIVATE wtlMouse)
target_link_libraries(app PRIVATE wtlOcr)
```

静态链接：

```cmake
target_link_libraries(app PRIVATE wtlOpenStatic)
target_link_libraries(app PRIVATE wtlRegexStatic)
target_link_libraries(app PRIVATE wtlKeyBoardStatic)
target_link_libraries(app PRIVATE wtlMouseStatic)
target_link_libraries(app PRIVATE wtlOcrStatic)
```

也可以直接链接完整聚合库：

```cmake
target_link_libraries(app PRIVATE WinTools)
# 或
target_link_libraries(app PRIVATE WinToolsStatic)
```

外部工程直接引用构建产物时，路径对应关系如下：

| 用途 | 路径 |
| --- | --- |
| 头文件 | `source/include` |
| 链接库 | `source/src/lib/link` |
| 运行期 DLL | `source/src/lib/dll` |
| 静态库 | `source/src/lib/static` |

### 纯头文件目标

以下模块不需要链接库，只需加入对应 CMake 目标或直接包含头文件：

```cmake
target_link_libraries(app PRIVATE wtlThrowError wtlTypeName)
```

对应头文件：

- `wtlThrowError.hpp`
- `wtlTypeName.hpp`

### 模块依赖

| 模块 | 内部依赖 | 运行时外部依赖 |
| --- | --- | --- |
| `wtlDict` | `wtlThrowError` | 无 |
| `wtlPrint` | `wtlDictStatic`、`wtlTypeName`、`wtlThrowError` | 无 |
| `wtlStringCode` | 无 | 无 |
| `wtlFileManagement` | `wtlStringCodeStatic` | 无 |
| `wtlOpen` | `wtlStringCodeStatic`、`wtlDictStatic` | 无 |
| `wtlRegex` | `wtlStringCodeStatic` | 无 |
| `wtlZip` | zlib | zlib DLL |
| `wtlMediaToolkit` | `wtlPrintStatic`、`wtlFileManagementStatic`、`wtlRegexStatic` | OpenCV |
| `wtlOcr` | OpenCV、RapidOCR | RapidOcr DLL、onnxruntime.dll、OpenCV DLL、`model/` |
| `wtlKeyBoard` | `wtlStringCodeStatic`、`wtlTimeStatic` | `interception.dll` |
| `wtlMouse` | `wtlTimeStatic` | `interception.dll` |

`wtlMediaToolkit` 同时链接 `gdiplus`、`Shcore`、`Windowscodecs`、`ole32`、
Media Foundation（`mfplay`、`mfplat`、`mfuuid`）、`shlwapi` 和 `uuid`
等 Windows 系统库。

`wtlOcr` 同时链接 `user32`、`gdi32`、`ws2_32`；MSVC 下还会链接 RapidOCR 所需的系统库。动态库构建后会把 RapidOCR、onnxruntime 和对应 OpenCV DLL 复制到输出目录。

### 目标命名变化

原先 `wtlDict`、`wtlPrint` 等名称表示静态库。当前统一规则为：

- 不加后缀的目标表示动态库。
- 加 `Static` 后缀的目标表示静态库，例如 `wtlDictStatic`、`wtlPrintStatic`、`wtlOpenStatic`。

原有使用静态库的 CMake 工程需要将目标名改为对应的 `*Static` 名称。

---

## 鼠标操作

### 初始化
```cpp
wtl::MouseTools mouse;
```

### 方法详解

#### 1. `mouseMoveTo` - 移动鼠标
| 参数    | 类型    | 说明        |
|---------|---------|-------------|
| x       | int     | 目标 X 坐标（像素） |
| y       | int     | 目标 Y 坐标（像素） |
| delay   | float   | 移动延迟（秒） |

**驱动级操作**：添加 `drivingStage=true` 参数启用驱动级模拟

```cpp
mouse.mouseMoveTo(0, 0, 1);                       // 移动到屏幕左上角
mouse.mouseMoveTo(500, 300, 0.5, true);           // 驱动级移动
mouse.mouseMoveTo(100, 200, true, false);         // 驱动级移动（占位参数）
```

#### 2. `mouseClick` - 点击操作
| 参数     | 类型     | 可选值                               | 默认值    |
|----------|----------|--------------------------------------|-----------|
| x, y     | int      | -                                    | 无        |
| clicks   | int      | ≥1                                   | 1         |
| button   | string   | "left", "right", "roll"              | "left"    |
| delay    | float    | 点击间延迟（秒）                      | 0         |

```cpp
// 在(100,200)处右键双击
mouse.mouseClick(100, 200, 2, "right");
mouse.mouseClickDriving(100, 200, 1, "left", 0.1);   // 驱动级点击

// 使用 wtl::MousePosition 结构体
wtl::MousePosition pos = wtl::getMousePosition();
mouse.mouseClick(pos, 2, "left", 0.2);
mouse.mouseClickDriving(pos, 1, "right", 0.1);
```

#### 3. `mouseDown`/`mouseUp` - 按下/释放
| 参数          | 类型   | 可选值                                                       | 默认值   |
|---------------|--------|--------------------------------------------------------------|----------|
| button        | string | "left", "right", "roll", "forward", "backspace"              | "left"   |
| drivingStage  | bool   | 是否启用驱动级模拟                                            | false    |

```cpp
mouse.mouseDown();                         // 按下左键
mouse.mouseDown("right", true);            // 驱动级按下右键
Sleep(1000);
mouse.mouseUp();                           // 释放左键
mouse.mouseUp("right", true);              // 驱动级释放右键
```

#### 4. `mouseRoll` - 滚轮控制
| 参数  | 类型 | 说明           |
|-------|------|----------------|
| move  | int  | 正数向上，负数向下 |

```cpp
mouse.mouseRoll(120);                      // 向上滚动一页
mouse.mouseRoll(-120, true);               // 驱动级滚动
```

#### 5. `mouseSideKeyDown`/`mouseSideKeyUp` - 侧键操作
| 参数     | 类型     | 可选值                       | 默认值  |
|----------|----------|------------------------------|---------|
| button   | string   | "forward", "backspace"       | -       |
| clicks   | int      | ≥1                           | 1       |
| delay    | double   | 点击间延迟（秒）              | 0.0     |

```cpp
mouse.mouseSideKeyPress("forward");                            // 按下并释放前进按钮
mouse.mouseSideKeyDownDriving("backspace", 2, 0.1);            // 驱动级双击后退按钮
mouse.mouseSideKeyUpDriving("forward", 1, 0.05);               // 驱动级释放前进按钮
```

#### 6. `listenMouseEvent` - 事件监听
支持监听鼠标所有按键（左、右、中、侧边按钮）
```cpp
// 需在独立线程运行
thread mouseThread([&](){
    mouse.listenMouseEvent();
});
mouseThread.detach();

// 获取鼠标事件码
int mouseCode = mouse.getMouseCode();
switch(mouseCode) {
    case wtl::mouseCode::WM_LEFTDOWN:      // 1001
        cout << "左键按下" << endl;
        break;
    case wtl::mouseCode::WM_FORWARDOWN:    // 1004
        cout << "前进按钮按下" << endl;
        break;
    case wtl::mouseCode::WM_BACKSPACEUP:   // -1005
        cout << "后退按钮释放" << endl;
        break;
}
```

#### 7. 鼠标事件码枚举
```cpp
namespace wtl::mouseCode {
    // 按下：正数
    WM_LEFTDOWN=1001, WM_ROLLDOWN=1002, WM_RIGHTDOWN=1003,
    WM_FORWARDOWN=1004, WM_BACKSPACEDOWN=1005,
    // 释放：负数
    WM_LEFTUP=-1001, WM_ROLLUP=-1002, WM_RIGHTUP=-1003,
    WM_FORWARDUP=-1004, WM_BACKSPACEUP=-1005
}
```

#### 8. `uninstallMouseEvent` - 注销鼠标监听
```cpp
wtl::MouseTools::uninstallMouseEvent();    // 注销所有鼠标事件，但不退出程序
```

#### 9. `exitMouseEvent` - 退出鼠标监听
```cpp
wtl::MouseTools::exitMouseEvent();         // 停止鼠标监听并清理资源
```

---

## 键盘操作

### 初始化
```cpp
wtl::KeyBoardTools keyboard;
```

### 方法详解

#### 1. `keyDown`/`keyUp` - 单键操作
| 参数 | 类型   | 示例值                       |
|------|--------|------------------------------|
| key  | string | "a", "ctrl", "F1" 等        |

```cpp
keyboard.keyDown("alt");
keyboard.keyDown("tab", true);     // 驱动级按下
keyboard.keyUp("tab");
keyboard.keyUp("alt", true);       // 驱动级释放
```

#### 2. `pressKey` - 按下并释放
```cpp
keyboard.pressKey("enter");        // 模拟回车键
keyboard.pressKey("ctrl", true);   // 驱动级按键
```

#### 3. `pressHotKey` - 组合键
支持任意数量组合键（最多 4 个）
```cpp
keyboard.pressHotKey("win", "r");                      // 打开运行窗口
keyboard.pressHotKey("ctrl", "alt", "delete");         // 三键组合
keyboard.pressHotKeyDriving("ctrl", "shift", "esc");   // 驱动级组合键
```

#### 4. `writeStr` - 字符串输入
```cpp
keyboard.writeStr("admin@123", 0.05);   // 逐字符输入，每字符延迟 0.05 秒
```

#### 5. `copyStr` - 剪贴板操作
```cpp
keyboard.copyStr("待复制的文本");
keyboard.pressHotKey("ctrl", "v");      // 粘贴
string copied = keyboard.getCopyStr();  // 获取剪贴板内容
```

#### 6. `addHotKey` - 注册热键
| 参数            | 类型                         | 说明                 |
|-----------------|------------------------------|----------------------|
| key             | string                       | 格式如 "ctrl shift k" |
| targetVoidFunc  | std::function<void()>        | 触发时执行的函数      |

**支持键名**：a-z, 0-9, f1-f12, ctrl, alt, shift, win, esc, space, enter, tab, backspace, delete, home, end, page_up, page_down, up_arrow, down_arrow, left_arrow, right_arrow, insert, caps, num, 以及 `, [, ], \, ;, ', ,, ., /, -, =

```cpp
keyboard.addHotKey("ctrl+alt+t", [](){
    system("start cmd.exe");   // 打开命令行
});

class MyClass {
public:
    void testFunc() { cout << "类成员函数触发" << endl; }
};
MyClass obj;
keyboard.addHotKey("ctrl+shift+a", std::bind(&MyClass::testFunc, &obj));
```

#### 7. `waitHotKey` - 监听热键
```cpp
thread hotkeyThread([&](){
    keyboard.waitHotKey();     // 阻塞监听
});
hotkeyThread.detach();
```

#### 8. `listenOneKeyEvent` - 单按键监听
```cpp
// 在独立线程中调用监听函数
wtl::KeyBoardTools k;
thread keyThread([&](){
    k.listenOneKeyEvent(true);   // real_time=true 实时响应
});
keyThread.detach();

// 主循环中获取按键码
while (true) {
    long keyCode = k.getOneKeyCode();
    if (keyCode == wtl::keyCode::KEY_ESC_DOWN) {   // 27
        wtl::KeyBoardTools::exitOneKeyEvent();     // 退出监听
        break;
    }
}
```

#### 9. 键盘事件码枚举
```cpp
namespace wtl::keyCode {
    // 按下：正数（虚键码）
    KEY_A_DOWN=65, KEY_B_DOWN=66, ..., KEY_Z_DOWN=90,
    KEY_0_DOWN=48, ..., KEY_9_DOWN=57,
    KEY_CTRL_DOWN=162, KEY_ALT_DOWN=164, KEY_SHIFT_DOWN=160,
    // 释放：负数
    KEY_A_UP=-65, ..., KEY_Z_UP=-90,
    KEY_CTRL_UP=-162, KEY_ALT_UP=-164, KEY_SHIFT_UP=-160
}
```

#### 10. `clearHotKey` - 清除所有注册热键
```cpp
wtl::KeyBoardTools::clearHotKey();
```

#### 11. `uninstallHotKeyEvent` - 注销快捷键
```cpp
wtl::KeyBoardTools::uninstallHotKeyEvent();   // 注销所有快捷键，但不退出程序
```

#### 12. `uninstallOneKeyEvent` - 注销单按键监听
```cpp
wtl::KeyBoardTools::uninstallOneKeyEvent();   // 注销单个按键监听，但不退出程序
```

#### 13. `exitOneKeyEvent` - 退出单按键监听
```cpp
wtl::KeyBoardTools::exitOneKeyEvent();        // 停止单个按键监听并清理资源
```

---

## 图像处理

### `wtl::ImageTools` 类

#### 初始化
```cpp
wtl::ImageTools imgevent;
```

### 一、图像识别

> **模板匹配说明**：屏幕识别函数的**第 3 个参数就是缩放** `templateZoom`，表示模板采集时那台电脑的缩放（`1.5` = 150%），函数按 `本机缩放 / templateZoom` 自动换算；不传则按模板原始尺寸匹配（同机同缩放）。匹配在屏幕灰度图上进行。
>
> **找不到图片时**：默认抛 `wtl::ThrowError` 异常；只有显式传 `notException = true` 时才不抛，返回 `result = false`、`x = y = -1`。
>
> **中文路径**：图片模块的路径统一按 **UTF-8** 传入，内部已支持中文/Unicode 路径，无需再传 `useGbk`。

#### `getImagePosition` - 获取图像在屏幕中的位置
| 参数           | 类型   | 说明                           |
|----------------|--------|--------------------------------|
| filePath       | string | 模板图像路径（PNG/JPEG 等）    |
| threshold      | double | 匹配阈值（0.0~1.0，推荐 0.85） |
| templateZoom   | double | 模板采集时的缩放，1.5=150%（第 3 个参数，不传则原尺寸匹配）|
| notException   | bool   | 找不到时是否抛出异常（默认 false=抛）|

**返回值**:
`ImagePosition` 结构体，包含 `x`（匹配中心 X）、`y`（匹配中心 Y）、`result`（是否匹配成功）

```cpp
wtl::ImageTools imgevent;
ImagePosition img = imgevent.getImagePosition("img.png", 0.85);   // 缩放自动适配，无需传入
if (img.result) {
    wtl::MouseTools m;
    m.mouseClick(img.x, img.y);   // 点击图像中心
}
```

#### `getImagePositionVec` - 获取所有匹配位置
```cpp
auto positions = imgevent.getImagePositionVec("img.png", 0.85, false, 10);
for (auto& pos : positions) {
    cout << "匹配位置: (" << pos.x << ", " << pos.y << ")" << endl;
}
```

#### `getSpecifiedAreaImagePosition` - 指定区域识别
| 参数       | 类型   | 说明                   |
|------------|--------|------------------------|
| x1, y1     | int    | 搜索区域左上角坐标      |
| x2, y2     | int    | 搜索区域右下角坐标      |
| filePath   | string | 模板图像路径            |
| threshold  | double | 匹配阈值               |

```cpp
auto pos = imgevent.getSpecifiedAreaImagePosition(100, 100, 800, 600, "img.png", 0.9);
```

#### `findSimilarImage` - 查找相似图像
```cpp
// 在屏幕中查找
bool found = imgevent.findSimilarImage("img.png", 0.9);
// 在指定区域查找
bool foundInArea = imgevent.findSpecifiedSimilarImage(100, 100, 800, 600, "img.png", 0.9);
// 对比两张图片
bool similar = imgevent.findSimilarImage("img1.png", "img2.png", 0.95);
```

### 二、获取图像矩形位置
#### `getImageRect` - 获取模板矩形位置
| 参数       | 类型   | 说明                   |
|------------|--------|------------------------|
| filePath   | string | 模板图像路径            |
| threshold  | double | 匹配阈值               |

**返回值**:
`ImageRect` 结构体，包含 `topX`, `topY`, `btnX`, `btnY`, `result`

```cpp
ImageRect rect = imgevent.getImageRect("img.png", 0.9);
if (rect.result) {
    cout << "左上: (" << rect.topX << ", " << rect.topY << ")" << endl;
    cout << "右下: (" << rect.btnX << ", " << rect.btnY << ")" << endl;
}
```

#### 从主图定位模板位置
```cpp
ImageRect rect = imgevent.getImageRect("template.png", "screenshot.png", 0.9);
```

#### 缩放参数 `templateZoom`

屏幕识别函数的第 3 个参数就是缩放 `templateZoom`，表示模板采集时那台电脑的缩放（`1.5` = 150%），函数按 `本机缩放 / templateZoom` 自动换算；省略则按模板原始尺寸匹配。也可以用等价的 `...Zoom` 系列函数：

| 普通写法（第 3 个参数为缩放） | 等价 `...Zoom` 写法 |
|------------------------------|---------------------|
| `getImagePosition` | `getImagePositionZoom(filePath, threshold, templateZoom, notException = false)` |
| `getImagePositionVec` | `getImagePositionVecZoom(filePath, threshold, templateZoom, notException = false, maxMatches = 0)` |
| `getSpecifiedAreaImagePosition` | `getSpecifiedAreaImagePositionZoom(x1, y1, x2, y2, filePath, threshold, templateZoom, notException = false)` |
| `findSimilarImage` | `findSimilarImageZoom(filePath, threshold, templateZoom)` |
| `findSpecifiedSimilarImage` | `findSpecifiedSimilarImageZoom(x1, y1, x2, y2, filePath, threshold, templateZoom)` |
| `getImageRect` | `getImageRectZoom(filePath, threshold, templateZoom, notException = false)` |

```cpp
wtl::ImageTools imgevent;
// 模板在 170% 缩放的电脑上采集；找不到时默认抛 wtl::ThrowError
ImagePosition img = imgevent.getImagePosition("btn.png", 0.75, 1.7);

// 找不到时不想抛异常，就用 notException = true（此时 result=false、坐标为 -1）
ImagePosition img2 = imgevent.getImagePosition("btn.png", 0.75, 1.7, true);
if (img2.result) {
    wtl::MouseTools m;
    m.mouseClick(img2.x, img2.y);
}

if (img.result) {
    wtl::MouseTools m;
    m.mouseClick(img.x, img.y);   // 点击图像中心
}
```

### 三、修改图像格式
#### `setImageFormat` - 更改图像格式
```cpp
imgevent.setImageFormat("img.png", "img.jpg");
```

### 四、修改图像尺寸
#### `setImageSize` - 更改图像尺寸
| 参数                   | 类型     | 说明                  |
|------------------------|----------|-----------------------|
| imgPath                | string   | 源图像路径             |
| width                  | float    | 图片宽度               |
| height                 | float    | 图片高度               |
| targetImageSavePath    | string   | 目标存放路径（可选）   |
| outPutName             | string   | 输出图片名称（可选）   |

```cpp
imgevent.setImageSize("test.png", 150, 400, "D:/output/", "resized.png");
```

#### `getImageSize` - 获取图像尺寸
```cpp
ImageSize size = imgevent.getImageSize("img.png");
cout << "尺寸: " << size.x << "x" << size.y << ", 通道: " << size.channels << endl;
```

### 五、截屏功能
#### `captureScreen` - 截取整个屏幕
```cpp
imgevent.captureScreen("screenshot", "./");
```

#### `captureAreaScreen` - 区域截图
```cpp
imgevent.captureAreaScreen("area.png", 654, 142, 2498, 973);
```

### 六、去除水印
#### `removeWatermark` - 去除水印
```cpp
bool success = imgevent.removeWatermark("img.png", "output.png", 100, 100, 300, 200);
auto binData = imgevent.removeWatermarkBin("img.png", 100, 100, 300, 200);
```

### 七、绘制矩形
```cpp
imgevent.drawRectOnImage("img.png", "output.png", 100, 100, 300, 200);
```

---

## OCR 文字识别

> **引擎已更换（进行中）**
> `ocr::Ocr` 已由 Tesseract 更换为 **RapidOCR**（PP-OCRv6 small ONNX 模型 + onnxruntime）。
> 旧的 `tessdata` 路径、语言切换等能力不再适用，接口与本节内容仍在持续更新中，以代码为准。

### `ocr::Ocr` 类

引擎在**构造时自动初始化**、**析构时自动释放**，不需要手动 init / deinit。

独立模块头文件与 CMake 目标：

```cpp
#include <wtlOcr.hpp>
```

```cmake
target_link_libraries(app PRIVATE wtlOcr)
# 或链接静态库
target_link_libraries(app PRIVATE wtlOcrStatic)
```

`WinTools.dll` / `WinToolsStatic.lib` 仍包含 OCR 功能；使用 `winToolsH.hpp` 的旧代码无需修改。

#### 初始化
```cpp
ocr::Ocr engine;                        // 模型目录自动查找
ocr::Ocr engine("D:/radioOCR/models");  // 手动指定模型目录
```

模型目录的查找顺序（目录内需备齐 det/cls/rec 三个 onnx 与 `ppocrv6_dict.txt`）：

1. 构造参数传入的目录
2. 运行目录（当前工作目录）下的 `./model`
3. 编译期写入的 `RADIOOCR_MODELS_DIR`（本工程为 `<工程根>/model`）

#### 方法详解
| 方法                   | 参数             | 返回值          | 说明                                                     |
|------------------------|------------------|-----------------|----------------------------------------------------------|
| `ready()`              | 无               | bool            | 引擎是否初始化成功                                       |
| `modelsPath()`         | 无               | const string&   | 返回实际使用的模型目录                                   |
| `readImageText()`      | imgPath, outConf | string          | 识别整张图片文本，多行以 `\n` 连接；深色主题截图会自动反相（亮字暗底→暗字亮底）并去掉彩色干扰；文本块按 上→下、左→右 排成阅读顺序，行首缩进按检测框还原 |
| `readImageTextLine()`  | imgPath, outConf | vector<string>  | 逐行识别文本，等同于把 `readImageText()` 结果按 `\n` 拆开 |
| `getButtonRect()`      | buttonName       | wtl::ImageRect  | 截取当前虚拟屏幕并按文字查找，返回文字块矩形；忽略空白和 ASCII 大小写 |
| `getButtonPosition()`  | buttonName       | wtl::ImagePosition | 截取当前虚拟屏幕并按文字查找，返回文字块中心点 |
| `getButtonRectVec()`   | buttonName       | vector<wtl::ImageRect> | 返回所有匹配矩形；按匹配优先级、从上到下、从左到右排序 |
| `getButtonPositionVec()` | buttonName     | vector<wtl::ImagePosition> | 返回所有匹配中心点，顺序与 `getButtonRectVec()` 一致 |

`outConf` 为可选输出参数，返回平均置信度（0-100）。

```cpp
// 声明对象即初始化
ocr::Ocr ocr;

if (ocr.ready()) {
    int confidence = 0;
    string text = ocr.readImageText("screenshot.png", &confidence);
    wtl::println("识别结果: {}", text);
    wtl::println("置信度: {}%", confidence);

    auto lines = ocr.readImageTextLine("screenshot.png");
    for (auto& line : lines) {
        wtl::println("行: {}", line);
    }
} else {
    // 模型目录缺失或模型加载失败时会走到这里
    wtl::println("OCR 引擎不可用, 模型目录: {}", ocr.modelsPath());
}
```

#### 按文字定位按钮

```cpp
ocr::Ocr ocr;
if (ocr.ready()) {
    wtl::ImageRect rect = ocr.getButtonRect("登录");
    if (rect.result) {
        wtl::println("按钮矩形: ({}, {}) - ({}, {})",
                     rect.topX, rect.topY, rect.btnX, rect.btnY);
    }

    wtl::ImagePosition pos = ocr.getButtonPosition("登录");
    if (pos.result) {
        wtl::MouseTools mouse;
        mouse.mouseClick(pos.x, pos.y);
    }

    auto rects = ocr.getButtonRectVec("登录");
    for (const auto& item : rects) {
        wtl::println("匹配矩形: ({}, {}) - ({}, {})",
                     item.topX, item.topY, item.btnX, item.btnY);
    }

    auto positions = ocr.getButtonPositionVec("登录");
    for (const auto& item : positions) {
        wtl::println("匹配中心: ({}, {})", item.x, item.y);
    }
}
```

这些方法都返回虚拟屏幕全局坐标。单结果接口取 Vec 结果的第一个；`getButtonPosition()` 和 `getButtonPositionVec()` 内部会调用对应的矩形接口，需要同时取得矩形和中心点时只调用矩形接口，再自行计算中心即可。

其它说明：

- 图片路径按 UTF-8 传入（内部用 `fs::u8path` 处理），中文路径可用；图片由本库读成字节后再交给引擎，不受引擎内部窄字符路径转换的影响
- 模型目录路径需要是纯 ASCII：RapidOCR 内部把窄字符串逐字节转宽字符，非 ASCII 路径必然加载失败；中文路径会先尝试相对路径、8.3 短名等价的 ASCII 形式，拿不到就返回未就绪而不是崩溃
- RapidOCR 的 `CrnnNet::initModel` 里有一句无条件的 `printf("total keys size(%lu)\n", ...)`（绕过库内的 `Logger` 开关，默认就会打到 stdout 最上面），已在 `RadiOcr_DIR` 的 `vsVersion` / `mingwVersion` 源码里删除并重新编译；重新从上游拉取该库会把它带回来
- 同一个 `ocr::Ocr` 对象不要并发调用（引擎内部有共享状态）

---

## 视频处理

### `wtl::VideoTools` 类

#### 初始化
```cpp
wtl::VideoTools video;
```

#### 1. `videoArrivalTargetStop` - 剪切视频
```cpp
video.videoArrivalTargetStop("input.mp4", "output.mp4", 10.0, 30.0);
// useFFMPEG=true, useGbk=true, useGpu=false, show_errors=true
```

#### 2. `compositeVideo` - 合并视频
```cpp
vector<string> videos = {"v1.mp4", "v2.mp4"};
video.compositeVideo(videos, "output.mp4", 30.0);
// useGpu=false
```

#### 3. `addAudioToVideo` - 添加音频
```cpp
video.addAudioToVideo("video.mp4", "audio.mp3", "output.mp4");
// useGpu=false, silent=true
```

#### 4. `removeVideoAudioTrack` - 删除音轨
```cpp
video.removeVideoAudioTrack("video.mp4", "output.mp4");
// useGpu=false
```

#### 5. `setVideoCover` - 设置封面
```cpp
video.setVideoCover("video.mp4", "cover.jpg", "output.mp4");
// useGpu=false
```

#### 6. 获取视频信息
```cpp
long long duration = video.getVideoDuration("video.mp4");   // 毫秒
double fps = video.getVideoFrame("video.mp4");               // 帧率
string codec = video.getVideoCodec("video.mp4");
```

#### 7. `imageFrameBinMergeVideo` - 图片帧合成视频
```cpp
vector<vector<unsigned char>> frames;
// 填充帧数据...
video.imageFrameBinMergeVideo(frames, "output.mp4", 30.0);
```

---

## 音频处理

### `wtl::AudioTools` 类

#### 初始化
```cpp
wtl::AudioTools audio;
wtl::AudioTools player("D:/music/example.mp3"); // UTF-8 音频路径
```

#### 播放控制接口

| 接口 | 返回值 | 说明 |
| --- | --- | --- |
| `playAudio()` | `bool` | 播放音频；暂停后再次调用会从当前位置继续 |
| `pauseAudio()` | `bool` | 暂停当前音频 |
| `stopAudio()` | `bool` | 停止播放并释放播放器资源 |
| `closeAudio()` | `bool` | `stopAudio()` 的兼容别名 |
| `getLastError()` | `std::string` | 获取最近一次英文错误信息 |

完整调用示例：

```cpp
#include <wtlMediaToolkit.hpp>

#include <iostream>

int main()
{
    wtl::AudioTools audio(u8"D:/music/example.mp3");

    if (!audio.playAudio())
    {
        std::cerr << audio.getLastError() << '\n';
        return 1;
    }

    audio.pauseAudio();
    audio.playAudio(); // 从暂停位置继续
    audio.stopAudio();
    return 0;
}
```

#### 1. `playAudio` - 播放或继续
```cpp
if (!player.playAudio()) {
    std::cerr << player.getLastError() << '\n';
}
```

暂停后再次调用 `playAudio()` 会从当前位置继续播放。

#### 2. `pauseAudio` - 暂停
```cpp
player.pauseAudio();
```

#### 3. `stopAudio` - 停止并释放播放器
```cpp
player.stopAudio();
```

`closeAudio()` 是 `stopAudio()` 的兼容别名。播放、暂停、停止前都会检查音频
路径；路径为空时返回 `false`，通过 `std::cerr` 输出英文原因，并可通过
`getLastError()` 获取同一条错误信息。

#### 4. `compositeMusic` - 合并音频
```cpp
vector<string> audios = {"a1.mp3", "a2.wav"};
audio.compositeMusic(audios, "output.mp3");
```

#### 5. `splitAudioTracks` - 提取音轨
```cpp
audio.splitAudioTracks("video.mp4", "audio.mp3");
```

#### 6. `setSystemVolumeModern` - 设置系统音量
```cpp
audio.setSystemVolumeModern(60); // 0-100
```

---

## 实用工具

### 一、屏幕信息获取
#### `getScreenSize` - 获取屏幕分辨率
```cpp
ScreenSize screen = getScreenSize();   // 或 getScreenSize(false) 不输出
cout << "分辨率: " << screen.x << "x" << screen.y << endl;
cout << "缩放: " << screen.screenZoom << endl;
```

#### `wtl::getMousePosition` - 获取鼠标坐标
```cpp
wtl::MousePosition pos = wtl::getMousePosition();
cout << "鼠标坐标: (" << pos.x << ", " << pos.y << ")" << endl;
```

### 二、字符串编码类
#### `wtl::StringCode` - 字符串编码转换
| 函数                    | 参数                     | 类型                | 说明                                    |
|-------------------------|--------------------------|---------------------|-----------------------------------------|
| `gbkToUtf8`             | gbkStr                   | string              | 将 GBK 编码字符串转为 UTF-8             |
| `utf8ToGbk`             | utf8Str                  | string              | 将 UTF-8 编码字符串转为 GBK             |
| `strToWStr`             | str                      | string              | 将字符串转为宽字符                       |
| `strToWStr`（重载）     | str, useGBK              | string, bool        | 根据布尔值转换编码                       |
| `wStrToStr`             | wstr                     | wstring             | 将宽字符转为普通字符串                   |
| `stringChangeEncoding`  | str, targetEncoding      | string, string      | 自动转码到目标编码                       |
| `checkStringCode`       | str                      | string              | 检测字符串编码类型                       |

**支持检测的编码**: utf8bom, utf16le, utf16be, utf32le, utf32be, utf8, ascii, gbk, gb2312, big5, latin1, binary

```cpp
wtl::StringCode scd;
string utf8Str = scd.gbkToUtf8("中文文本");
string gbkStr = scd.utf8ToGbk(utf8Str);
wstring wstr = scd.strToWStr("文本", true);
string encoding = scd.checkStringCode(utf8Str);
string converted = scd.stringChangeEncoding(gbkStr, "utf8");
```

### 三、文件操作类
#### `wtl::FileManagement` - 文件管理类

声明位于 `wtlFileManagement.hpp`，实现位于 `wtlFileManagement.cpp`。该模块也可以单独构建为 `wtlFileManagement` 静态库。

| 函数名        | 参数                                             | 返回值 | 说明                         |
|---------------|--------------------------------------------------|--------|------------------------------|
| `removeFile`  | const std::string& fileName                      | bool   | 删除文件                     |
| `createDir`   | const std::string& fileName                      | bool   | 创建目录                     |
| `removeDir`   | const std::string& dirPath                       | bool   | 递归删除目录                 |
| `writeLog`    | const std::string& str, const std::string& fileName | bool | 以追加模式写日志            |
| `copyDir`     | const std::filesystem::path& sourcePath, const std::filesystem::path& targetPath | bool | 复制文件夹到目标路径 |
| `getFileTree` | const std::string& rootPath                      | std::string | 获取目录树字符串        |

```cpp
wtl::FileManagement f;
f.createDir("../Test");
f.removeFile("../Test/Test.txt");
f.removeDir("D:/Test/test");
f.writeLog("日志内容...", "test.log");
f.copyDir("./", "D:/Test");
std::string tree = f.getFileTree("D:/Test");
```

#### `wtl::FileObject` - 文件目录对象

第三方目录扫描模块。声明位于 `wtlFileManagement.hpp`，非模板方法实现位于 `wtlFileManagement.cpp`。

匹配模式：

| 模式 | 说明 |
|------|------|
| `wtl::fileManagementCode::MatchMode::EXACT` | 文件名完全相等 |
| `wtl::fileManagementCode::MatchMode::REGEX` | 使用正则表达式匹配 |
| `wtl::fileManagementCode::MatchMode::WHOLE_WORD` | 整名匹配 |
| `wtl::fileManagementCode::MatchMode::CONTAINS` | 文件名包含匹配串 |

主要接口：

| 函数 | 说明 |
|------|------|
| `searchFiles(pattern)` | 递归搜索匹配文件名并返回完整路径 |
| `getAllFiles()` | 获取当前目录及所有子目录中的文件完整路径 |
| `getAllEntries()` | 获取当前目录下的文件和文件夹条目 |
| `getTreeString(depth=0)` | 返回目录树字符串 |
| `printTree(depth=0)` | 将目录树输出到标准输出 |
| `loadDirectory()` | 重新加载当前目录并重建子目录对象 |
| `setModel(mode)` / `getModel()` | 设置或读取匹配模式 |
| `setCaseSensitive(cs)` / `isCaseSensitive()` | 设置或读取大小写敏感状态 |
| `translateModelToEnglish(mode)` | 将匹配模式转换为英文描述 |
| `translateFromEnglish(text)` | 将英文描述转换回匹配模式 |

```cpp
wtl::FileObject files(
    "D:/Test",
    wtl::fileManagementCode::MatchMode::CONTAINS,
    false
);

auto matched = files.searchFiles(".txt");
auto allFiles = files.getAllFiles();
auto entries = files.getAllEntries();
std::string tree = files.getTreeString();
files.printTree();
```

### 四、文件 IO 操作
#### `wtl::Open` - 文件读写类
| 函数          | 参数                                                   | 返回值                     | 说明                              |
|---------------|--------------------------------------------------------|----------------------------|-----------------------------------|
| 构造函数      | file_path, file_mode, encoding="auto", useGBK=true, auto_close=true | -                          | 打开文件                          |
| `open`        | file_path, file_mode, encoding="auto", useGBK=true, auto_close=true | void                       | 打开文件                          |
| `isOpen`      | -                                                      | bool                       | 返回文件是否打开                  |
| `read`        | -                                                      | string                     | 读取整个文件（自动识别编码）      |
| `readLine`    | -                                                      | vector<string>             | 逐行读取文件内容                  |
| `readBinary`  | -                                                      | vector<unsigned char>      | 读取整个二进制文件                |
| `readJson`    | -                                                      | Dict<string, JsonValue>    | 读取 JSON 文件                    |
| `readBinaryToString` | -                                               | string                     | 二进制文件转字符串                |
| `write`       | const string &str                                      | void                       | 写入文件                          |
| `writeBinary` | const vector<unsigned char>& data                      | void                       | 写入二进制文件（XOR 加密）        |
| `writeJson`   | Dict<K, V> dicts                                       | void                       | 写入 JSON 文件                    |
| `writeStringToBinary` | const string &str                               | void                       | 字符串转二进制写入文件（XOR 加密）|
| `close`       | -                                                      | void                       | 关闭文件                          |

```cpp
// 写入文件
wtl::Open opt;
opt.open("测试.txt", "w", "auto", true);
opt.write("测试写入");
opt.close();

// 读取文件
wtl::Open ipt;
ipt.open("测试.txt", "r", "auto", true);
string content = ipt.read();
cout << content << endl;

// JSON 读写
wtl::Dict<string, wtl::JsonValue> dict;
dict["name"] = "John";
dict["age"] = 30.0;
wtl::Open jsonFile;
jsonFile.open("data.json", "w");
jsonFile.writeJson(dict);
jsonFile.close();

auto loaded = jsonFile.readJson();
cout << loaded["name"].strVal << endl;
```

### 五、字典数据结构
#### `wtl::Dict` - 保持插入顺序的字典
| 函数/操作符     | 说明                         | 示例                                    |
|-----------------|------------------------------|-----------------------------------------|
| `operator[]`    | 访问或插入键值对             | `dict["key"] = value`                   |
| `insert`        | 插入键值对                   | `dict.insert("key", value)`             |
| `pop`           | 删除键值对                   | `dict.pop("key")`                       |
| `contains`      | 检查键是否存在               | `dict.contains("key")`                  |
| `update`        | 合并另一个字典               | `dict.update(otherDict)`                |
| `setDefault`    | 设置默认值                   | `dict.setDefault("key", defaultValue)`  |
| `items`         | 返回键值对视图               | `for(auto& [k,v]: dict.items())`        |
| `keys`          | 返回键列表                   | `auto keys = dict.keys()`               |
| `values`        | 返回值列表                   | `auto values = dict.values()`           |
| `size`          | 大小                         | `dict.size()`                           |
| `empty`         | 是否为空                     | `dict.empty()`                          |
| `clear`         | 清空                         | `dict.clear()`                          |
| `dumps`         | 转为 JSON 字符串             | `dict.dumps()`                          |
| `loads`（静态） | 从 JSON 字符串解析           | `Dict::loads(jsonStr)`                  |
| `get`           | 安全获取值（不抛异常）       | `dict.get("key", defaultValue)`         |

```cpp
// 创建字典
wtl::Dict<string, int> dict;

// 插入键值对
dict["apple"] = 10;
dict.insert("banana", 20);
dict["orange"] = 15;

// 检查键是否存在
if (dict.contains("apple")) {
    cout << "找到 apple" << endl;
}

// 安全获取值
int value = dict.get("grape", 0);   // 不存在返回 0

// 遍历字典（保持插入顺序）
for (auto& [key, value] : dict.items()) {
    wtl::println("{}: {}", key, value);
}

// JSON 转换
string jsonStr = dict.dumps();
auto loaded = wtl::Dict<string, int>::loads(jsonStr);

// 嵌套字典
wtl::Dict<string, wtl::Dict<string, int>> nested;
wtl::Dict<string, int> inner;
inner["x"] = 1;
inner["y"] = 2;
nested["point"] = inner;
```

### 六、字符串转 JSON
#### `wtl::stringToJson` - JSON 字符串转 Dict
```cpp
string jsonStr = "{\"name\": \"John\", \"age\": 30, \"scores\": [90, 85, 92]}";
auto dict = wtl::stringToJson(jsonStr);
wtl::println("姓名: {}, 年龄: {}", dict["name"].strVal, (int)dict["age"].numVal);
auto scores = dict["scores"].arrVal;
for (auto& score : scores) {
    wtl::println("分数: {}", (int)score.numVal);
}
```

### 七、管理员权限
#### `wtl::isAdmin` - 判断是否为管理员权限
```cpp
if (wtl::isAdmin()) {
    cout << "管理员权限" << endl;
}
```

#### `wtl::openAdmin` - 以管理员身份重新打开
```cpp
if (!wtl::isAdmin()) {
    wtl::openAdmin();   // 以管理员权限重新打开当前程序
}
```

### 八、环境变量与编码设置
#### `wtl::addTempEnv` - 添加临时环境变量
```cpp
wtl::addTempEnv("D:\\Python\\python嵌入式", "PATH");
wtl::addTempEnv(L"D:\\QtImageTools\\python嵌入式", L"PATH");
```

#### `wtl::useConsoleUtf8` - 设置控制台编码为 UTF-8
```cpp
wtl::useConsoleUtf8();
```

#### `wtl::getConsoleEncoding` - 获取控制台编码
```cpp
string encoding = wtl::getConsoleEncoding();
cout << "控制台编码: " << encoding << endl;
```

### 九、文件夹与文件操作
#### `wtl::listDir` - 获取文件夹内文件列表
```cpp
vector<string> files = wtl::listDir("C:/MyFolder");
for (auto& file : files) {
    cout << file << endl;
}
```

#### `wtl::openDir` - 后台打开文件夹
```cpp
wtl::openDir("C:/MyFolder");
```

### 十、排序与时间函数
#### `wtl::sort` - 排序（支持升降序）
```cpp
int numList[] = {5, 2, 8, 1, 9};
auto a = wtl::sort(numList, 5, "down");   // 降序
auto b = wtl::sort(numList, 5, "up");     // 升序

vector<int> nums = {3, 1, 4, 1, 5};
auto sorted = wtl::sort(nums, "up");
```

#### `wtl::sum` - 列表求和
```cpp
vector<int> nums = {1, 2, 3, 4, 5};
int total = wtl::sum(nums);
```

#### `wtl::getNowTime` - 获取当前时间
```cpp
string time = wtl::getNowTime();
cout << "当前时间: " << time << endl;
```

#### `wtl::getRunTime` - 获取运行时间（类似 Python 的 time.time()）
```cpp
double start = wtl::getRunTime();
// 执行操作
double end = wtl::getRunTime();
cout << "耗时: " << (end - start) << "秒" << endl;
```

### 十一、路径获取
#### `wtl::getExePath` - 获取当前 exe 所在路径
```cpp
string path = wtl::getExePath(false);
cout << "当前路径: " << path << endl;
```

#### `wtl::getNowExeName` - 获取当前 exe 名称
```cpp
string name = wtl::getNowExeName();
```

### 十二、延迟函数
#### `wtl::sleep` - 延迟函数
```cpp
wtl::sleep(1.5);   // 延迟 1.5 秒
```

### 十三、类型识别
#### `wtl::typeName` - 获取变量类型名称
```cpp
auto type = wtl::typeName(123);
cout << "类型: " << type << endl;   // 输出: int

wtl::Dict<string, int> dict;
cout << wtl::typeName(dict) << endl;   // 输出: dict

auto tup = std::make_tuple(1, "hello", 3.14);
cout << wtl::typeName(tup) << endl;    // 输出: tuple
```

**支持识别的类型**：string, char, c_str, unsigned_char, wchar_t, wc_str, w_str, int, double, float, long, long_long, short, unsigned_int, unsigned_long, unsigned_short, unsigned_long_long, bool, int_arr, long_arr, long_long_arr, short_arr, unsigned_int_arr, unsigned_short_arr, unsigned_long_arr, unsigned_long_long_arr, double_arr, float_arr, dict, tuple, class, struct, vector_int, vector_char, vector_string, vector_double, vector_float, vector_unsigned_char, vector_unsigned_int, vector_unsigned_long, vector_unsigned_short, vector_unsigned_long_long, vector_short, vector_long, vector_long_long

### 十四、字符串格式化
#### `wtl::string_format` - C 风格格式化
```cpp
string formatted = wtl::string_format("姓名: %s, 年龄: %d", "张三", 25);
```

#### `wtl::format` - C++ 风格格式化
支持容器、字典、map、数组、tuple 等类型
```cpp
vector<int> a = {1, 2, 3};
wtl::print("输出: {}", a);

vector<vector<int>> b = {{1, 2}, {3, 4}};
cout << wtl::format("嵌套数组: {}", b) << endl;

int arr[] = {5, 6, 7};
cout << wtl::format("C 数组: {}", arr) << endl;

std::map<string, int> mp = {{"a", 1}, {"b", 2}};
cout << wtl::format("map: {}", mp) << endl;

wtl::Dict<string, int> dict;
dict["x"] = 10;
dict["y"] = 20;
cout << wtl::format("字典: {}", dict) << endl;

auto tup = std::make_tuple(1, "hello", 3.14);
cout << wtl::format("元组: {}", tup) << endl;   // 输出: (1, "hello", 3.14)
```

#### 格式化说明符
支持类似 Python 的格式化语法：
```cpp
// 浮点数精度控制
wtl::println("{:.2f}", 3.14159);   // 输出: 3.14
wtl::println("{:.4f}", 3.14159);   // 输出: 3.1416

// 显式索引，可倒序、可复用
wtl::println("{0} + {1}", "A", "B");   // 输出: A + B
wtl::println("{1} + {0}", "A", "B");   // 输出: B + A
wtl::println("{0} 和 {0}", "X");       // 输出: X 和 X
```

#### 花括号转义
需要输出字面花括号时用 `{{` 和 `}}`，规则与 Python 的 `str.format` 一致：
```cpp
wtl::format("{{}}");                   // 输出: {}
wtl::format("{{}} 与 {}", "x");        // 输出: {} 与 x
wtl::format("{{\"k\":{}}}", 1);        // 输出: {"k":1}
```

注意：转义只在字符串**走格式化分支**时生效。若串内没有任何有效占位符，
`print`/`println` 会退回按空格拼接输出，此时 `{{}}` 原样保留：
```cpp
wtl::println("{{}}");                  // 输出: {{}}  （无占位符，未格式化）
wtl::println("{{}} 与 {}", "x");       // 输出: {} 与 x（有占位符，走格式化）
wtl::format("{{}}");                   // 输出: {}    （format 总是格式化）
```

含花括号但不构成占位符的普通文本（正则 `\d{2,3}`、集合 `{a,b}`、JSON `{"k":1}`）
会被自动识别并原样输出，无需转义：
```cpp
wtl::println("匹配 \\d{2,3} 的结果", "OK");   // 输出: 匹配 \d{2,3} 的结果 OK
wtl::println("集合 {a,b}", "x");              // 输出: 集合 {a,b} x
```

### 十五、输出函数
#### `wtl::print` / `wtl::println` - 智能输出
```cpp
wtl::print("Hello");                    // 不换行输出
wtl::println("World");                  // 换行输出
vector<int> a = {1, 2, 3};
wtl::println("姓名: {}, 年龄: {}, 数组: {}", "李四", 30, a);

// 多参数自动以空格分隔（与 Python 的 print 一致）
wtl::println("a", "b", "c");            // 输出: a b c
wtl::println("x", 1, 2.5, a);           // 输出: x 1 2.5 [1, 2, 3]

// 注意：第一个参数含 {} 时走格式化分支，参数之间不插入空格
wtl::println("坐标: ({}, {})", 10, 20); // 输出: 坐标: (10, 20)
```

#### 颜色输出
```cpp
// 使用颜色输出
wtl::println(wtl::color::printColor("红色文本", wtl::Color("red")));
wtl::println(wtl::color::printColor("绿色文本", wtl::Color("green")));
wtl::println(wtl::color::printColor("蓝色文本", wtl::Color("blue")));

// RGB 颜色
wtl::println(wtl::color::printColor("自定义颜色", wtl::Color(255, 128, 0)));

// 十六进制颜色
wtl::println(wtl::color::printColor("十六进制颜色", wtl::Color("#FF00FF")));

// 颜色堆栈
wtl::Print::pushColor(wtl::Color("red"));
wtl::print("红色文本");
wtl::Print::pushColor(wtl::Color("green"));
wtl::print("绿色文本");
wtl::Print::popColor();    // 返回到红色
wtl::print("又是红色");
wtl::Print::popColor();    // 返回到默认
```

#### `wtl::Color` 类
支持多种颜色格式：
- RGB 值：`Color(255, 0, 0)` 红色
- 颜色名称：`Color("red")` 红色
- 十六进制：`Color("#FF0000")` 红色

**支持的颜色名称**: red, green, blue, yellow, cyan, magenta, white, black, gray, orange, purple, pink, brown 等

#### `wtl::printUpdateLine` - 刷新行输出
```cpp
for (int i = 0; i < 100; i++) {
    wtl::printUpdateLine("进度: {}%", i);
    wtl::sleep(0.1);
}
```

#### `Print::gotoXy` / `Print::clearLine` - 光标控制
```cpp
wtl::Print p;
p.gotoXy(10, 5);   // 移动光标到 (10,5)
p.clearLine();     // 清空当前行
```

### 十六、随机数功能
#### `random` 命名空间
| 函数       | 参数                     | 返回值                | 说明                         |
|------------|--------------------------|-----------------------|------------------------------|
| `randInt`  | min, max                 | int/long/long long    | 随机整数                     |
| `uniform`  | min, max                 | float/double          | 随机浮点数                   |
| `choice`   | vector<T>                | T                     | 随机选择一个元素             |
| `choices`  | population, weights, k   | vector<T>             | 带权重随机选择多个           |

```cpp
int num = random::randInt(1, 100);
double fnum = random::uniform(0.0, 1.0);
vector<string> fruits = {"apple", "banana", "orange"};
string chosen = random::choice(fruits);
vector<double> weights = {0.1, 0.3, 0.6};
auto chosenFruits = random::choices(fruits, weights, 2);
```

### 十七、正则表达式功能
#### `re` 命名空间
| 函数       | 参数                               | 返回值              | 说明                   |
|------------|------------------------------------|---------------------|------------------------|
| `findAll`  | pattern, text, flags               | vector<string>      | 查找所有匹配           |
| `search`   | pattern, text, flags               | string              | 搜索第一个匹配         |
| `sub`      | pattern, repl, text, flags         | string              | 替换匹配               |
| `sub`      | pattern, callback, text, flags     | string              | 回调函数替换           |
| `escape`   | pattern                            | string              | 转义正则元字符，用于字面匹配 |

**标志**: `re::IGNORECASE` (I), `re::MULTILINE` (M), `re::NOMODEL`

```cpp
auto numbers = re::findAll(R"(\d+)", "abc123def456");
string result = re::sub(R"(\d+)", "NUM", "abc123def456");
string cbResult = re::sub(R"(\d+)",
    [](const std::smatch& m){ return "[" + m.str() + "]"; },
    "abc123def456");
string found = re::search("hello", "HELLO World", re::IGNORECASE);
```

`re::escape` 使用示例：

```cpp
std::string raw = "\\hello()";
std::string literalPattern = re::escape(raw);

// literalPattern 会变成: \\hello\(\)
std::string matched = re::search(literalPattern, "路径是 \\hello() 结束");
```

### 十八、字符串操作
#### `wtl::input` - 控制台输入
声明位于 `wtlPrintFormat.hpp`，实现位于 `wtlPrint.cpp`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `inform_text` | `std::string` | 输入前显示的提示文本；为空时不显示 |
| `color` | `std::string` | 提示文本颜色；默认为空，不应用颜色 |

```cpp
string name = wtl::input("请输入姓名: ");
string command = wtl::input("请输入命令: ", "cyan");
```

#### `wtl::stringToVector` - 字符串转容器
```cpp
string text = "line1\nline2\nline3";
auto lines = wtl::stringToVector(text);
```

#### `wtl::SearchText` - 查找匹配行（Python 风格）

| 方法                | 返回值                            | 说明                         |
|---------------------|-----------------------------------|------------------------------|
| `findLine`          | long                              | 第一个匹配行号，未找到为 -1  |
| `findLines`         | vector<long>                      | 所有匹配行号                 |
| `findLineContent`   | string                            | 第一个匹配行内容             |
| `findLinesContent`  | vector<string>                    | 所有匹配行内容               |
| `findLinesMap`      | Dict<int, string>                 | 行号 -> 行内容映射           |
| `findLinesMapEach`  | vector<Dict<int, string>>         | 每个匹配行一个独立映射       |

```cpp
wtl::SearchText search;
string text = "hello world\nfoo bar\nhello again";

long lineNo = search.findLine(text, "hello");          // 1
auto lineNos = search.findLines(text, "hello");        // {1, 3}

string lineText = search.findLineContent(text, "hello");   // "hello world"
auto lineTexts = search.findLinesContent(text, "hello");   // {"hello world", "hello again"}

auto lineMap = search.findLinesMap(text, "hello");         // {1: "hello world", 3: "hello again"}
auto lineMapEach = search.findLinesMapEach(text, "hello"); // [{1: "hello world"}, {3: "hello again"}]
```

### 十九、加密与资源
#### `wtl::addStringKey` / `wtl::decryptStringKey` - 字符串加密
```cpp
string encrypted = wtl::addStringKey("hello", "mykey");
string decrypted = wtl::decryptStringKey(encrypted, "mykey");
```

#### `wtl::addBinaryResources` - 嵌入二进制数据
```cpp
vector<unsigned char> data = {0x01, 0x02, 0x03};
wtl::addBinaryResources("myapp.exe", data);
auto extracted = wtl::parseEmbeddedBinaryData("myapp.exe");
```

### 二十、控制台控制
#### `wtl::hideConsole` / `wtl::showConsole` - 控制台隐藏/显示
```cpp
wtl::hideConsole();   // 隐藏控制台
wtl::showConsole();   // 显示控制台
```

### 二十一、调试信息输出
#### `wtl::debugMessage` - 调试信息输出
| 参数          | 类型   | 说明                                    |
|---------------|--------|-----------------------------------------|
| message       | string | 调试信息内容                            |
| file          | const char* | 文件名（由 `__FILE__` 宏传入）   |
| func_name     | const char* | 函数名（由 `__func__` 宏传入）   |
| line          | int    | 行号（由 `__LINE__` 宏传入）            |
| debug_model   | string | 调试级别：TRACE, DEBUG, INFO, WARN, ERROR, FATAL（默认 "INFO"） |
| output_color  | string | 输出颜色（默认 "green"）                |

**支持的颜色参数**: red, green, blue, yellow, cyan, magenta, white, black, gray, orange, purple, pink, brown

**调试级别说明**:
| 级别   | 说明                         |
|--------|------------------------------|
| TRACE  | 追踪 - 最详细的调试信息      |
| DEBUG  | 调试 - 开发阶段的详细信息    |
| INFO   | 信息 - 正常运行的记录        |
| WARN   | 警告 - 潜在问题，不影响运行  |
| ERROR  | 错误 - 功能执行失败，程序可继续 |
| FATAL  | 致命 - 严重错误，程序即将退出 |

```cpp
// 使用 debugMessage 函数直接调用
wtl::println(wtl::debugMessage("程序启动成功", __FILE__, __func__, __LINE__, "INFO"));
wtl::println(wtl::debugMessage("警告：磁盘空间不足", __FILE__, __func__, __LINE__, "WARN", "yellow"));
wtl::println(wtl::debugMessage("致命错误", __FILE__, __func__, __LINE__, "FATAL", "red"));

// 使用宏辅助调用（更简洁）
wtl::println(wtl::DebugMessage("程序初始化完成", "INFO", "green"));
wtl::println(wtl::DebugMessage("处理数据中...", "DEBUG", "blue"));
wtl::println(wtl::DebugMessage("连接超时", "ERROR", "red"));
```

**输出格式示例**:
```
[INFO] 程序启动成功 | file.cpp:25 in main()
[WARN] 警告：磁盘空间不足 | file.cpp:30 in processData()
[ERROR] 连接超时 | file.cpp:45 in connectServer()
```

### 二十二、异常处理
#### `wtl::ThrowError` - Python 风格异常抛出
```cpp
// 直接抛出异常（自动获取文件名、行号、函数名）
wtl::ThrowError::showError("文件不存在");

// 输出格式类似 Python traceback:
// Traceback (most recent call last):
//   File "main.cpp", line 42, in main
//     showError("文件不存在")
// Exception: 文件不存在
```

---

## 文件对话框

### `filedialog` 命名空间

#### `askOpenFilename` - 选择单个文件
| 参数      | 类型     | 说明                                          |
|-----------|----------|-----------------------------------------------|
| caption   | string   | 对话框标题（默认 "选择文件"）                  |
| dir       | string   | 初始目录                                      |
| filter    | string   | Qt 风格过滤器，如 "图片 (*.png *.jpg);;所有文件 (*.*)" |

**返回值**: 选中的文件路径，取消返回空字符串

```cpp
string file = filedialog::askOpenFilename("选择图片", "C:/", "图片 (*.png *.jpg);;所有文件 (*.*)");
```

#### `askOpenFilenames` - 选择多个文件
```cpp
vector<string> files = filedialog::askOpenFilenames("选择文件", "C:/", "文本 (*.txt);;所有文件 (*.*)");
```

#### `askDirectory` - 选择文件夹
```cpp
string folder = filedialog::askDirectory("选择文件夹", "C:/");
```

---

## DLL 调用

### `wtl::CallWinDll` 类

#### 初始化
| 参数         | 类型     | 说明                                              |
|--------------|----------|---------------------------------------------------|
| winDllPath   | fs::path | DLL 路径                                          |
| convention   | string   | 调用约定："stdcall"(默认), "cdecl", "fastcall", "thiscall" |

```cpp
wtl::CallWinDll dll("user32.dll", "stdcall");
```

#### `callWinDll` - 调用 DLL 函数
```cpp
// 调用 MessageBoxW
int result = dll.callWinDll<int, HWND, LPCWSTR, LPCWSTR, UINT>(
    "MessageBoxW", nullptr, L"Hello", L"Title", MB_OK);

// 调用 GetSystemMetrics
int screenWidth = dll.callWinDll<int, int>("GetSystemMetrics", SM_CXSCREEN);
```

#### `isOpen` - 检查 DLL 是否加载成功
```cpp
if (dll.isOpen()) {
    // 调用函数
}
```

#### `setCallingConvention` - 切换调用约定
```cpp
dll.setCallingConvention("cdecl");
```

#### `getCallingConvention` - 获取当前调用约定
```cpp
string convention = dll.getCallingConvention();
```

---

## 压缩与解压

### `wtl::ZipDirToBin` 类

#### `compressFolder` - 压缩文件夹
```cpp
wtl::ZipDirToBin zip;
bool success = zip.compressFolder("C:/MyFolder", "archive.bin");
```

#### `decompressFolder` - 解压文件夹
```cpp
wtl::ZipDirToBin zip;
bool success = zip.decompressFolder("archive.bin", "C:/OutputFolder");
```

---

## 弹窗与窗口控制

### 一、消息弹窗
#### `wtl::WMessageBox` - 消息框类
| 函数          | 参数                          | 返回值 | 说明                |
|---------------|-------------------------------|--------|---------------------|
| `showInfo`    | title, content                | void   | 信息提示框          |
| `showWarning` | title, content                | void   | 警告提示框          |
| `showError`   | title, content                | void   | 错误提示框          |
| `askYesNo`    | title, content, model="info"  | bool   | 是/否选择框         |

```cpp
wtl::WMessageBox msg;
msg.showInfo("提示", "操作成功完成!");
msg.showWarning("警告", "磁盘空间不足!");
msg.showError("错误", "文件打开失败!");

bool result = msg.askYesNo("确认", "确定要删除吗?", "warning");
if (result) {
    // 用户选择"是"
}
```

### 二、窗口置顶
#### `wtl::WindowTop` - 窗口置顶控制
| 函数                | 参数                                   | 返回值 | 说明                    |
|---------------------|----------------------------------------|--------|-------------------------|
| 构造函数            | hwndName, pxc=0, pyc=0, wc=0, hc=0   | -      | 通过窗口标题创建        |
| 构造函数            | hwnd, pxc=0, pyc=0, wc=0, hc=0       | -      | 通过窗口句柄创建        |
| `setWindowsTopOn`   | -                                      | void   | 开启窗口置顶            |
| `setWindowsTopOff`  | -                                      | void   | 关闭窗口置顶            |

```cpp
// 通过窗口标题设置置顶
wtl::WindowTop top("计算器", 0, 0, 400, 300);
top.setWindowsTopOn();

// 取消置顶
top.setWindowsTopOff();
```

---

## 进程与系统管理

### 一、进程管理
#### `wtl::killProcess` - 终止进程
| 参数           | 类型     | 说明                     |
|----------------|----------|--------------------------|
| processName    | fs::path | 进程名                   |
| killTree       | bool     | 是否终止进程树           |
| partialMatch   | bool     | 是否部分匹配             |
| pid            | DWORD    | 进程 ID                  |

```cpp
// 终止进程
wtl::killProcess("notepad.exe");
wtl::killProcess("notepad.exe", true, false);   // 终止进程树，精确匹配
wtl::killProcess(1234);                         // 通过 PID 终止
```

#### `wtl::getProcessPid` - 获取进程 PID
| 参数           | 类型     | 说明                           |
|----------------|----------|--------------------------------|
| processPath    | fs::path | 进程可执行文件路径             |
| getAllPidList  | bool     | 是否返回所有匹配 PID           |

```cpp
DWORD pid = wtl::getProcessPid("C:/Windows/notepad.exe");
auto allPids = wtl::getProcessPid("C:/Windows/notepad.exe", true);
```

#### `wtl::startOtherApp` - 启动其他应用程序
```cpp
wtl::startOtherApp("D:\\Programs\\myapp.exe");
wtl::startOtherApp("cmd.exe", true);   // 显示控制台
```

#### `wtl::notRepeatOpenExe` - 防止重复打开
```cpp
if (wtl::notRepeatOpenExe("myapp.exe")) {
    // 已经有实例在运行
    return 0;
}
```

#### `wtl::findExeInPath` - 查找可执行文件路径
```cpp
string path = wtl::findExeInPath("python.exe");
```

### 二、开机自启管理
#### `wtl::setAutoStartUp` - 设置开机自启
| 参数     | 类型     | 说明                       |
|----------|----------|----------------------------|
| appName  | fs::path | 注册表项名称               |
| exePath  | fs::path | 可执行文件路径（可选）     |

```cpp
// 当前程序开机自启
wtl::setAutoStartUp("myapp");
// 指定程序开机自启
wtl::setAutoStartUp("myapp", "C:\\Programs\\myapp.exe");
// 通过命令
wtl::setAutoStartUpCmd("myapp", "C:\\Programs\\myapp.exe");
```

#### `wtl::disableAutoStart` - 禁用开机自启
```cpp
wtl::disableAutoStart("myapp");
```

#### `wtl::getAutoStartExePath` - 获取自启程序路径
```cpp
string path = wtl::getAutoStartExePath("myapp");
```

### 三、注册表操作
#### `wtl::getRegistryPath` - 获取注册表键值
```cpp
string path = wtl::getRegistryPath("Software\\Microsoft\\Windows\\CurrentVersion\\Run", "myapp");
```

### 四、系统音量控制
#### `wtl::setSystemVolumeModern` - 设置系统音量
```cpp
wtl::setSystemVolumeModern(50);   // 设置音量为 50%
```

### 五、DLL 注入
#### `wtl::loadToMemory` - 注入 DLL
```cpp
wtl::loadToMemory(1234, "C:/my.dll");
```

### 六、窗口焦点锁定
#### `wtl::lockWindows` - 锁定窗口焦点
```cpp
// 通过窗口标题
wtl::lockWindows("记事本");
// 通过进程 PID
wtl::lockWindows(1234);
```

### 七、DPI 设置
#### `wtl::getScreenZoom` - 获取屏幕缩放比例
```cpp
float zoom = wtl::getScreenZoom();
```

#### `wtl::setAutoDpi` - 自动调整 DPI 感知
```cpp
wtl::setAutoDpi();   // 程序启动时调用
```

### 八、图标设置
#### `wtl::setExeIcon` - 设置 exe 图标
```cpp
wtl::setExeIcon("myapp.exe", "icon.ico", "./");
```

### 九、资源文件操作
#### `wtl::getWinRcData` - 获取 Windows rc 文件数据
```cpp
auto data = wtl::getWinRcData(101);   // 资源 ID
```

---

## PowerShell 与 CMD 调用

### `wtl::PowerShellSession` 类
用于执行 PowerShell 命令，支持交互式会话管理。

#### 构造函数
| 参数         | 类型   | 说明                     | 默认值 |
|--------------|--------|--------------------------|--------|
| auto_close   | bool   | 析构时是否自动关闭会话   | true   |

#### 方法
| 方法                      | 参数               | 返回值        | 说明                 |
|---------------------------|--------------------|---------------|----------------------|
| `runPowerShellCommand`    | command (string)   | string        | 执行 PowerShell 命令 |

```cpp
// 创建 PowerShell 会话（自动关闭）
wtl::PowerShellSession ps;

// 执行 PowerShell 命令
string output = ps.runPowerShellCommand("Get-Process");
wtl::println("输出: {}", output);

// 执行多条命令
ps.runPowerShellCommand("$a = 10");
string result = ps.runPowerShellCommand("$a * 2");
wtl::println("结果: {}", result);   // 输出: 20

// 手动控制关闭（auto_close=false）
{
    wtl::PowerShellSession ps2(false);
    ps2.runPowerShellCommand("Write-Host 'Hello'");
    // 需要手动调用析构或使用指针管理
}
```

### `wtl::CommandSession` 类
用于执行 CMD 命令，支持交互式会话管理。

#### 构造函数
| 参数         | 类型   | 说明                     | 默认值 |
|--------------|--------|--------------------------|--------|
| auto_close   | bool   | 析构时是否自动关闭会话   | true   |

#### 方法
| 方法              | 参数               | 返回值        | 说明                 |
|-------------------|--------------------|---------------|----------------------|
| `runCommand`      | command (string)   | string        | 执行 CMD 命令        |

```cpp
// 创建 CMD 会话（自动关闭）
wtl::CommandSession cmd;

// 执行 CMD 命令
string output = cmd.runCommand("dir");
wtl::println("目录列表:\n{}", output);

// 执行多条命令
cmd.runCommand("cd C:\\");
cmd.runCommand("echo 当前目录: %cd%");

// 手动控制关闭（auto_close=false）
{
    wtl::CommandSession cmd2(false);
    cmd2.runCommand("ipconfig");
    // 需要手动调用析构或使用指针管理
}
```

**注意事项**：
1. 会话对象在析构时会自动关闭子进程（除非 `auto_close=false`）
2. 如果 `auto_close=false`，需要在适当时机释放对象，否则会输出警告信息
3. 命令执行结果会以字符串形式返回，包含标准输出和标准错误
4. 支持连续执行多条命令，保持同一会话上下文

---

## 清理与多线程

### 自动清理
- 析构时自动释放未松开的按键
- 程序退出时自动清理资源
- 使用 `exitCheckWork()` 注册退出清理

### 清理函数
```cpp
wtl::KeyBoardTools::clearHotKey();                // 清除所有注册热键
wtl::KeyBoardTools::uninstallHotKeyEvent();       // 注销热键监听
wtl::KeyBoardTools::uninstallOneKeyEvent();       // 注销单按键监听
wtl::KeyBoardTools::exitOneKeyEvent();            // 退出单按键监听
wtl::MouseTools::uninstallMouseEvent();      // 注销鼠标监听
wtl::MouseTools::exitMouseEvent();           // 退出鼠标监听
exitCheckWork();                                  // 注册退出时的清理函数
```

### 线程管理
- **必须** 将 `listenMouseEvent` 和 `waitHotKey` 放在独立线程
- 推荐使用 `detach` 避免主线程阻塞：
  ```cpp
  thread t([&](){ keyboard.waitHotKey(); });
  t.detach();
  ```

---

## 完整示例
```cpp
#include <thread>
#include "winToolsH.hpp"

using namespace std;

int main() {
    // 设置控制台编码
    wtl::useConsoleUtf8();

    // 设置 DPI 感知
    wtl::setAutoDpi();

    // 图像识别示例
    wtl::ImageTools imgEvent;
    ImagePosition img = imgEvent.getImagePosition("button.png", 0.9);
    if (img.result) {
        wtl::MouseTools mouse;
        mouse.mouseClick(img.x, img.y);
    }

    // OCR 识别示例 (引擎已更换为 RapidOCR, 声明对象即初始化)
    ocr::Ocr ocr;
    if (ocr.ready()) {
        string text = ocr.readImageText("screenshot.png");
        wtl::println("OCR 结果: {}", text);
    }

    // 字典使用示例
    wtl::Dict<string, int> fruitDict;
    fruitDict["apple"] = 10;
    fruitDict["banana"] = 20;
    fruitDict["orange"] = 15;
    wtl::println("水果字典: {}", fruitDict);

    // 正则表达式示例
    string text = "价格: $123.45 数量: 100";
    auto numbers = re::findAll(R"(\d+\.?\d*)", text);
    wtl::println("找到的数字: {}", numbers);

    // 颜色输出示例
    wtl::println(wtl::color::printColor("成功提示:", wtl::Color("green")), "操作完成!");
    wtl::println(wtl::color::printColor("警告提示:", wtl::Color("yellow")), "请注意磁盘空间!");
    wtl::println(wtl::color::printColor("错误提示:", wtl::Color("red")), "文件打开失败!");

    // 随机数示例
    int randomNum = random::randInt(1, 100);
    wtl::println("随机数: {}", randomNum);

    // 文件操作示例
    wtl::Open file;
    file.open("data.json", "w");
    file.writeJson(fruitDict);
    file.close();

    // 字符串编码转换示例
    wtl::StringCode converter;
    string utf8Text = converter.gbkToUtf8("自动化脚本");

    // 键盘热键示例
    wtl::KeyBoardTools keyboard;
    keyboard.addHotKey("ctrl+alt+l", [](){
        wtl::println("热键触发：锁定系统");
    });

    // 消息弹窗示例
    wtl::WMessageBox msg;
    msg.showInfo("提示", "自动化脚本启动成功!");

    // 调试信息输出示例
    wtl::println(wtl::debugMessage("程序初始化完成", __FILE__, __func__, __LINE__, "INFO"));
    wtl::println(wtl::DebugMessage("数据处理中...", "DEBUG", "blue"));
    wtl::println(wtl::DebugMessage("连接服务器成功", "INFO", "green"));

    // 类型识别示例
    wtl::Dict<string, int> dict;
    cout << "dict 类型: " << wtl::typeName(dict) << endl;   // 输出: dict
    auto tup = std::make_tuple(1, "hello", 3.14);
    cout << "tuple 类型: " << wtl::typeName(tup) << endl;   // 输出: tuple

    // 异常抛出示例
    try {
        wtl::ThrowError::showError("这是一个测试异常");
    } catch (const std::runtime_error& e) {
        wtl::println("捕获异常: {}", e.what());
    }

    // PowerShell 会话示例
    wtl::PowerShellSession ps;
    string psOutput = ps.runPowerShellCommand("Get-Process | Select-Object -First 3");
    wtl::println("PowerShell 输出:\n{}", psOutput);

    // CMD 会话示例
    wtl::CommandSession cmd;
    string cmdOutput = cmd.runCommand("echo Hello World");
    wtl::println("CMD 输出: {}", cmdOutput);

    // 启动监听线程
    thread hotkeyThread([&](){
        keyboard.waitHotKey();
    });
    hotkeyThread.detach();

    // 注册退出清理
    exitCheckWork();

    while (true) {
        wtl::sleep(1);
    }
    return 0;
}
```

---

## 注意事项
1. **管理员权限**：部分操作（如 DLL 注入、进程终止）需以管理员身份运行
2. **按键冲突**：避免重复注册系统级热键（如 `alt+F4`）
3. **图像识别依赖**：需正确安装 OpenCV 库并配置编译环境
4. **OCR 依赖**：OCR 已更换为 RapidOCR（onnxruntime + PP-OCRv6 small），需保证 `model/` 模型目录能被找到；初始化失败时 `ready()` 返回 false，引擎会打印 `[OCR:FATAL]` / `[OCR:ERROR]` 说明原因
5. **编码兼容性**：文件路径建议使用 GBK 编码，内部统一使用 UTF-8
6. **调试建议**：监听函数启动前添加日志输出确认线程状态
7. **DPI 适配**：在高 DPI 屏幕上使用前调用 `wtl::setAutoDpi()`
8. **线程安全**：监听函数需要在独立线程中运行
9. **字典顺序**：`wtl::Dict` 保持插入顺序
10. **颜色输出**：需要支持 ANSI 转义序列的控制台
11. **正则表达式**：需要包含 `<regex>` 头文件
12. **随机数**：需要包含 `<random>` 头文件
13. **文件系统**：需要 C++17 支持（`<filesystem>`）
14. **Windows 版本**：部分 API 可能需要特定 Windows 版本支持
15. **音视频处理**：需要 FFmpeg 支持（可选）
16. **驱动级操作**：驱动级模拟可能需要管理员权限
17. **加密文件**：`writeBinary` 和 `writeStringToBinary` 使用 XOR 加密，需配合 `readBinary` 使用
18. **DLL 调用约定**：默认使用 `stdcall`，可通过 `setCallingConvention` 切换
19. **文件对话框**：使用现代 Windows API（`IFileOpenDialog`），需 Windows Vista+
20. **控制台颜色**：`Color` 类支持 RGB、颜色名称和十六进制格式
21. **进程 PID 查询**：`getProcessPid` 支持通过路径精确匹配或获取所有匹配 PID
22. **调试信息输出**：`debugMessage` 支持 6 种调试级别和颜色输出，配合 `DebugMessage` 宏使用更简洁
23. **PowerShell/CMD 会话**：`PowerShellSession` 和 `CommandSession` 类支持交互式会话管理，析构时自动清理资源（2026/8/3 更新）
24. **类型识别**：`typeName` 函数可识别 `Dict`、`tuple` 等复杂类型
25. **异常处理**：`wtl::ThrowError::showError` 使用 `__builtin_FILE()`、`__builtin_LINE()`、`__builtin_FUNCTION()` 获取调用点信息，支持 MSVC/GCC/Clang
26. **命名空间**：公开模块统一使用 `wtl` 命名空间（`wtl::KeyBoardTools`、`wtl::MouseTools`、`wtl::keyCode`、`wtl::mouseCode`、`wtl::MousePosition`）
27. **多参数分隔（行为变更）**：`print`/`println` 传入多个参数时自动以空格分隔，与 Python 的 `print` 一致。原先为直接拼接，故拼接式写法需去掉字面量尾部空格——`println("值: ", x)` 会输出 `值:  x`，应改写为 `println("值:", x)`。含 `{}` 的格式化调用不受影响；`print` 跨多次调用之间也不会插入空格（2026/8/28 更新）
28. **花括号文本**：含花括号但不构成占位符的普通文本（正则 `\d{2,3}`、集合 `{a,b}`、JSON `{"k":1}`）会被自动识别并原样输出，无需转义。但空花括号 `{}` 与占位符语法完全同形无法区分，`println("void f(){}", "x")` 仍会把 `{}` 当占位符替换掉，此时请让该文本单独一次调用输出（2026/8/28 更新）
29. **转义生效范围**：`{{`/`}}` 转义仅在字符串走格式化分支时生效。串内没有有效占位符时 `print`/`println` 会退回空格拼接，此时 `{{}}` 原样保留；`wtl::format` 总是格式化，故转义始终生效（2026/8/28 更新）
30. **NOMINMAX**：`CMakeLists.txt` 已定义 `NOMINMAX`，禁止 `windows.h` 定义 `min`/`max` 宏。该宏会使 `std::max(a,b)` 展开成非法的 `std::(...)`，在 MSVC 下报 C2589（MinGW 的 `minwindef.h` 用 `#ifndef __cplusplus` 挡住了，故仅 MSVC 受影响）（2026/8/28 更新）
31. **异常统一**：全库异常统一使用 `wtl::ThrowError::showError`（英文消息，抛出 `std::runtime_error`），旧的自定义异常 `ImageNotFoundException` 已移除
32. **项目更名**：`WinKeyPress` 已更名为 `WinTools`，头文件 `winKeyPressH.hpp` 改为 `winToolsH.hpp`，主静态库 `WinKeyPressStatic` 改为 `WinToolsStatic`，命名空间统一为 `wtl`（2026/9/13 更新）

---
