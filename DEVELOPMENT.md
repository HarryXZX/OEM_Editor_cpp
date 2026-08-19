# OEM 信息编辑器（C++ 版） — 开发文档

> **文件位置**: `E:\HYTools\Project\OEM_Editor_cpp\`
> **编译器**: MinGW-w64 GCC (g++)
> **目标平台**: Windows 10/11 x64
> **字符集**: Unicode (UTF-16 LE)
> **最新版本**: v2.3 (2025-07-22)

---

## 1. 项目概述

Windows 原生 GUI 程序，用于编辑系统 OEM 信息（显示在设置"关于"页面中）。存储在注册表 `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\OEMInformation`。

界面模仿系统属性对话框。支持 GUI 编辑和命令行静默操作。

### 核心功能

| 功能 | 说明 |
|------|------|
| **读取 OEM 信息** | 启动时从注册表加载 |
| **保存到注册表** | 需管理员权限 |
| **导出配置** | "另存为"对话框，两个过滤器类型，用户自选路径和文件名 |
| **导入配置** | 文件选择对话框，支持 .oeminfo / .oembak |
| **启动检测备份** | 自动检测 oeminfo.oembak 并询问是否恢复；恢复成功后原地变换窗口询问是否删除备份 |
| **未保存修改检测** | 关闭窗口时询问保存 |
| **帮助按钮** | 右上角 ? 按钮，弹窗展示命令行用法（文件名动态显示） |
| **命令行操作** | restore / backup / edit / help 参数，无 GUI 静默执行，语言感知 |
| **动态 exe 名** | 所有提示中使用真实文件名，重命名 exe 后自动适应 |

---

## 2. 文件清单

```
OEM_Editor_cpp/
├── OEM_Editor.cpp      # 主源代码（单文件架构）
├── OEM_Editor.rc       # 资源文件（图标 + 嵌入字体）
├── OEM_Editor.exe      # 编译输出
├── OEM_Editor_res.o    # 编译后的资源对象文件
├── OEM_icon.ico        # 程序图标
└── DEVELOPMENT.md      # 本开发文档
```

---

## 3. 编译方法

```powershell
cd E:\HYTools\Project\OEM_Editor_cpp
windres OEM_Editor.rc OEM_Editor_res.o
g++ -std=c++11 -O2 -static -mwindows -municode -o OEM_Editor.exe OEM_Editor.cpp OEM_Editor_res.o -lcomctl32 -lshlwapi -lgdi32 -lcomdlg32
```

**注意**: `-mwindows` 会抑制控制台输出。命令行模式通过 `AttachConsole` / `AllocConsole` + `WriteConsoleW` 实现控制台输出（绕过 C 运行库编码转换，避免中文乱码）。退出前调用 `CmdSyncCursor()` 同步控制台光标位置，确保 cmd 提示符出现在输出末尾下方。

---

## 4. 程序架构

### 4.1 启动流程

```
wWinMain(lpCmdLine)
  │
  ├─ CommandLineToArgvW(GetCommandLineW()) 解析参数
  │   ├─ restore [file] → EnsureConsole() → CmdRestore() → 退出
  │   │                   省略路径默认 exe 同目录 oeminfo.oembak
  │   ├─ backup <file>  → EnsureConsole() → CmdBackup()  → 退出
  │   ├─ edit K=V [...] → EnsureConsole() → CmdEdit()    → 退出
  │   ├─ help/--help/-?  → EnsureConsole() → 显示帮助    → 退出
  │   │                   中文系统→中文 / 其他→英文
  │   ├─ 未知命令        → EnsureConsole() → 提示用 help → 退出 (exit 1)
  │   └─ 无参数 → 继续 GUI（不分配控制台）
  │
  ├─ UAC 提权检测（仅影响 GUI 路径）
  ├─ InitCommonControlsEx()
  ├─ LoadCustomFont() → CreateFonts()
  ├─ InitExeName()    → 缓存真实文件名到 g_szExeName
  ├─ CheckBackupFile() → oeminfo.oembak 存在则弹恢复对话框
  ├─ 创建主窗口 → 消息循环
  └─ 退出 + 清理
```

### 4.2 主窗口布局 (480 x 310)

```
┌──────────────────────────────────────────────────────────┐
│  OEM 信息编辑器                                    [?]   │  ← 帮助按钮(右上角)
│──────────────────────────────────────────────────────────│  ← 凹线分隔
│  制造商 (M):     [__________________________________]    │
│  型号 (O):       [__________________________________]    │
│  支持电话 (P):   [__________________________________]    │
│  支持网址 (U):   [__________________________________]    │
│  服务时间 (H):   [__________________________________]    │
│                                                          │
│  [导入...]  [导出...]                                    │  ← 左对齐
│                              [取消]  [应用]  [确定]       │  ← 右对齐
│  HYCX Studio. ALL RIGHTS WASTED                           │
└──────────────────────────────────────────────────────────┘
```

---

## 5. 命令行用法

**注意**: 以下示例中 `{exe名}` 为占位符，程序运行时会自动替换为真实文件名（如 `OEM_Editor.exe`、`oemedit.exe` 等）。

**语言感知**: `help` 命令根据系统 UI 语言自动切换中文/英文输出。
- `GetUserDefaultUILanguage()` → `PRIMARYLANGID()` → `LANG_CHINESE`（0x0004）→ 中文
- 其他 → 英文

**动态文件名**: 所有命令行提示（help、usage、错误信息）和 GUI 帮助弹窗中使用 `g_szExeName` 显示真实 exe 文件名。

### restore — 恢复配置

```cmd
{exe名} restore [配置文件路径]
```

从指定的 .oeminfo / .oembak 文件读取配置，直接写入注册表。**省略路径则默认使用 exe 同目录下的 `oeminfo.oembak`**。

相对路径解析为 exe 所在目录（非当前工作目录），由 `ResolvePath()` 函数处理。

```cmd
:: 示例
{exe名} restore                          :: 默认使用 {exe目录}\oeminfo.oembak
{exe名} restore D:\backup\oeminfo.oembak :: 绝对路径
{exe名} restore mybackup.oembak           :: {exe目录}\mybackup.oembak
```

### backup — 导出配置

```cmd
{exe名} backup <输出文件路径>
```

读取当前注册表 OEM 信息，导出为 INI 格式文件到指定路径。

```cmd
:: 示例
{exe名} backup D:\backup\my_pc.oeminfo
{exe名} backup E:\HYTools\oeminfo.oembak
```

### edit — 修改指定注册表项

```cmd
{exe名} edit <注册表键名>=<值> [键名=值 ...]
```

直接修改注册表中的指定 OEM 字段。键名与注册表值名一致。

**有效键名：**
| 键名 | 说明 |
|------|------|
| `Manufacturer` | 制造商 |
| `Model` | 型号 |
| `SupportPhone` | 支持电话 |
| `SupportURL` | 支持网址 |
| `SupportHours` | 服务时间 |

```cmd
:: 示例：修改制造商和型号
{exe名} edit Manufacturer=ACME Model=X1

:: 示例：设置所有字段（多参数）
{exe名} edit Manufacturer=ACME Model=Widget SupportPhone=800-123-4567 SupportURL=https://example.com SupportHours=24/7
```

### help — 显示帮助信息

```cmd
{exe名} help
{exe名} --help
{exe名} -?
```

显示命令行用法说明。中文系统输出中文，其他语言输出英文。

**中文输出示例** (`LANG_CHINESE`):
```
OEM_Editor.exe 命令行用法 - HYCX Studio

OEM_Editor.exe restore [配置文件]
  从指定文件恢复 OEM 信息（省略路径则使用当前目录 oeminfo.oembak）

OEM_Editor.exe backup <输出文件>
  导出当前 OEM 信息到指定文件

OEM_Editor.exe edit Key=Value [Key=Value ...]
  修改注册表指定键值
  可用键: Manufacturer, Model,
         SupportPhone, SupportURL, SupportHours

  示例:
    OEM_Editor.exe edit Manufacturer=ACME Model=X1
    OEM_Editor.exe edit Manufacturer=ACME Model=Widget SupportPhone=800-123-4567

OEM_Editor.exe
  启动 GUI 编辑器

需要管理员权限。
```

**英文输出示例** (非 `LANG_CHINESE`):
```
oemedit.exe Command Line Usage - HYCX Studio

oemedit.exe restore [config_file]
  Restore OEM info (default: oeminfo.oembak in current dir)

oemedit.exe backup <output_file>
  Export current OEM info to file

oemedit.exe edit Key=Value [Key=Value ...]
  Modify registry keys directly
  Valid keys: Manufacturer, Model,
             SupportPhone, SupportURL, SupportHours

  Examples:
    oemedit.exe edit Manufacturer=ACME Model=X1
    oemedit.exe edit Manufacturer=ACME Model=Widget SupportPhone=800-123-4567

oemedit.exe
  Launch GUI editor

Requires administrator privileges.
```

### 无参数

直接启动 GUI 编辑器。

### 未知命令

```
未知命令: foobar
使用 {exe名} help 查看帮助
```
exit code 1，不显示完整帮助。

### 错误码

| 码 | 含义 |
|----|------|
| 0 | 成功 |
| 1 | 参数错误 / 文件格式无效 / 无有效修改 |
| 2 | 文件不存在 / 注册表打开失败 |
| 3 | 导入失败（缺少必需字段） |
| 4 | 写入注册表失败 |

---

## 6. 导出功能（GUI 另存为对话框）

点击"导出..."按钮时弹出系统"另存为"对话框：

**两个过滤器：**
| 过滤器 | 描述 | 默认后缀 |
|--------|------|----------|
| 1 | OEM 配置信息 (*.oeminfo) | .oeminfo |
| 2 | 自动化配置信息 (*.oembak) | .oembak |

- 默认选中过滤器 1，默认文件名 `oeminfo`
- 用户可自由选择存放位置和文件名
- 选择过滤器 2 后默认后缀自动变为 .oembak（适合一键恢复检测）
- 勾选了 `OFN_OVERWRITEPROMPT`（覆盖前询问）
- **无"所有文件"选项** — 用户只能在两种 OEM 格式间选择

---

## 7. INI 文件格式

两种扩展名相同格式：

```ini
[OEMInformation]
Manufacturer=ACME
Model=Widget X1
SupportPhone=800-123-4567
SupportURL=https://example.com
SupportHours=周一至周五 9:00-18:00
```

| 扩展名 | 用途 | 启动自动检测 |
|--------|------|:---:|
| `.oeminfo` | 常规导出/导入 | 否 |
| `.oembak` | 自动化备份/恢复 | 是（程序目录下） |

---

## 8. 关键函数

### 动态文件名 & 路径

| 函数/变量 | 说明 |
|-----------|------|
| `g_szExeName[MAX_PATH]` | 全局变量，缓存真实 exe 文件名，默认 `oemedit.exe` |
| `InitExeName()` | `GetModuleFileNameW` + `wcsrchr` 提取文件名，wWinMain 启动时调用 |
| `ResolvePath(path)` | 相对路径（无盘符/根目录）自动拼接 exe 目录；绝对路径直接返回 |

### 注册表操作

| 函数 | 说明 |
|------|------|
| `LoadOemInfo()` | 从 HKLM 加载 |
| `SaveOemInfo(info)` | 写入 HKLM |
| `RegReadString` | 读取注册表字符串值 |

### INI 文件操作

| 函数 | 说明 |
|------|------|
| `ExportOemInfoToFile(info, filepath)` | 导出到指定完整路径 |
| `ExportOemInfo(info, filename)` | 导出到程序目录（兼容旧接口） |
| `ImportOemInfo(filepath, info)` | 从文件导入 |

### 命令行

| 函数 | 说明 |
|------|------|
| `EnsureConsole()` | 分配控制台：先 `AttachConsole(ATTACH_PARENT_PROCESS)`，失败则 `AllocConsole()`，获取 `g_hConOut` handle |
| `CmdPrintf(fmt, ...)` | 可变参数控制台输出：`vswprintf` 格式化 → `WriteConsoleW` 写入（避免编码问题） |
| `CmdSyncCursor()` | 退出前同步控制台光标到输出末尾下一行 |
| `CmdPrint(str)` | 控制台单行输出（无格式化参数）
| `CmdEdit(argc, argv)` | 处理 edit 参数（循环从 i=0 开始，argc 检查 `< 1`） |
| `CmdRestore(filepath)` | 处理 restore 参数（`ResolvePath` 解析路径） |
| `CmdBackup(filepath)` | 处理 backup 参数 |

### 修改追踪

| 函数 | 说明 |
|------|------|
| `IsModified()` | 对比当前输入框 vs 原始值 |
| `UpdateOrigSnapshot()` | 保存后重置快照 |
| `DoSave(hWnd)` | 保存 + 更新快照 |
| `GetCurrentInfo()` | 从输入框收集数据 |

---

## 9. UI 着色

| 元素 | RGB |
|------|-----|
| 窗口背景 | (245, 245, 248) |
| 标题 | (30, 30, 60) |
| 页脚 | (150, 150, 150) |
| 确定按钮 | (76, 175, 80) 绿 |
| 应用按钮 | (66, 133, 244) 蓝 |
| 取消按钮 | (200, 200, 200) 灰 |
| 导出按钮 | (255, 152, 0) 橙 |
| 导入按钮 | (156, 39, 176) 紫 |
| 帮助按钮 | (120, 120, 130) 深灰 |

---

## 10. 按钮行为

| 按钮 | 行为 |
|------|------|
| **确定** | 保存注册表 → 退出 |
| **应用** | 保存注册表 → 弹成功提示 → 不退出 |
| **取消** | 有修改？询问放弃 → 退出 |
| **导入** | 文件选择对话框 → 填入输入框 |
| **导出** | 另存为对话框 → 写文件 |
| **?** | `swprintf` 动态生成帮助文本 → MessageBox 弹窗（文件名用 `g_szExeName`） |

**WM_CLOSE（点 X）：** 有修改 → 是/否/取消；无修改 → 直接关闭。

---

## 11. 控件 ID

### 主窗口

| ID | 值 | 类型 |
|----|-----|------|
| LABEL_TITLE | 101 | STATIC |
| LABEL_MFR ~ LABEL_HOURS | 102-110 | STATIC |
| EDIT_MFR ~ EDIT_HOURS | 103-111 | EDIT |
| LABEL_FOOTER | 112 | STATIC |
| BTN_IMPORT | 201 | BUTTON |
| BTN_EXPORT | 202 | BUTTON |
| BTN_CANCEL | 203 | BUTTON |
| BTN_APPLY | 204 | BUTTON |
| BTN_OK | 205 | BUTTON |
| BTN_HELP | 206 | BUTTON |

---

## 12. 给 AI 复现的要点

1. **纯 Win32 API**，单文件 `.cpp`，无框架
2. **类系统属性对话框**：标题左对齐 + 凹线 + 标签(快捷键) + 两行按钮
3. **导出用 `GetSaveFileNameW`**：两个过滤器，**无"所有文件"**
4. **导入用 `GetOpenFileNameW`**：一个组合过滤器，**无"所有文件"**
5. **修改检测**：`g_orig*` 快照 vs 当前输入框
6. **命令行**：`CommandLineToArgvW(GetCommandLineW())` 解析（**必须用 GetCommandLineW()，不能用 lpCmdLine**），`_wcsicmp` 匹配子命令，`LocalFree(argv)` 释放
7. **命令行 edit**：`CmdEdit(argc-2, argv+2)`，循环 **从 i=0 开始**（不是 i=1），argc 检查 **`< 1`**（不是 `< 2`），直接 `RegSetValueExW` 写入
8. **命令行 restore**：**省略路径默认 `{exe目录}\oeminfo.oembak`**，`ResolvePath()` 将相对路径解析为 exe 目录
9. **命令行控制台输出**: `EnsureConsole()` 先 `AttachConsole`，失败则 `AllocConsole`。输出用 `WriteConsoleW`（不用 `printf`/`wprintf`，避免代码页转换导致中文乱码）。格式化字符串中 `wchar_t*` 必须用 `%ls`（不是 `%s`）。退出前 `CmdSyncCursor()` 调用 `SetConsoleCursorPosition` 同步光标。
10. **help 语言感知**：`GetUserDefaultUILanguage()` → `PRIMARYLANGID()` → `LANG_CHINESE` 判断；中文→中文输出，其他→英文输出
11. **未知命令**：不显示完整帮助，只提示"使用 {exe名} help 查看帮助"
12. **动态 exe 名**：`InitExeName()` 启动时 `GetModuleFileNameW` 获取，`g_szExeName` 全局缓存；所有 `CmdPrintf` 和帮助弹窗均用 `%ls` + `g_szExeName`（`%ls` 对应 `wchar_t*`，`%s` 对应 `char*`，混用会导致截断）；**swprintf 参数数量必须与占位符个数一致**
13. **字体嵌入**：`.rc` FONT 资源 → `AddFontResourceExW(FR_PRIVATE)`
14. **备份恢复对话框**：独立窗口类 + 独立消息循环。使用 `SetPropW` 存储备份路径，`GWLP_USERDATA` 存储阶段状态（`RESTORE_PHASE_ASK/OK/FAIL`）。恢复成功后**原地变换窗口内容**（不弹 MessageBox）：标题→"恢复成功"，按钮→"删除备份"/"保留备份"。选删除则 `DeleteFileW` 删除备份文件。
15. **CreateWindowExW 按钮 ID**：`(HMENU)(INT_PTR)IDC_BTN_*` 双重转换
16. **L"..." 内禁止嵌入 ASCII 双引号**（编译器会截断字符串字面量）

---

## 13. 路径解析规则

命令行中**相对路径**解析为 **exe 所在目录**（非当前工作目录），由 `ResolvePath()` 实现：

```
ResolvePath 判断逻辑：
  含盘符 (如 D:\...)     → 绝对路径，直接返回
  含 \\ 或 \             → 绝对路径，直接返回
  其他                   → GetModuleFileNameW 取 exe 目录 + 拼接
```

示例：
| 输入 | ResolvePath 结果 |
|------|-------------------|
| `oeminfo.oembak` | `{exe目录}\oeminfo.oembak` |
| `mybackup.oembak` | `{exe目录}\mybackup.oembak` |
| `D:\backup\oem.oembak` | `D:\backup\oem.oembak`（不变） |

---

## 14. 测试记录

| 测试 | 命令 | 结果 |
|------|------|------|
| backup | `{exe名} backup D:\...\_test.oembak` | ✅ 成功导出 |
| restore (有路径) | `{exe名} restore D:\...\_test.oembak` | ✅ 成功恢复所有5项值 |
| restore (无路径) | `{exe名} restore` | ✅ 默认使用 exe 同目录 oeminfo.oembak |
| restore (文件不存在) | `{exe名} restore nonexistent.xyz` | ✅ exit 2 |
| edit (单参数) | `{exe名} edit Manufacturer=TEST999` | ✅ 成功写入 |
| edit (多参数) | `{exe名} edit Manufacturer=TEST Model=TEST ...` | ✅ 成功写入5个键值 |
| help | `{exe名} help` | ✅ exit 0，中文输出 |
| help (英文) | 语言 ID 0x0409 测试 | ✅ 英文输出正常 |
| --help / -? | `{exe名} --help` | ✅ exit 0 |
| unknown command | `{exe名} foobar` | ✅ exit 1，提示用 help 查看 |
| 帮助弹窗 ? 按钮 | GUI 点击 | ✅ 文件名动态显示（已修复 (null) bug） |
| OEM 恢复 | restore 后注册表值已还原 | ✅ Manufacturer=Lenovo Model=Legion Y7000P IRX9 等 |

---

## 15. 许可

HYCX Studio. ALL RIGHTS WASTED (C) HYCX Studio
