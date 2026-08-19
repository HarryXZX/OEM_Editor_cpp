# OEM Editor（C++ 版）

一个用于编辑 **Windows OEM 信息** 的轻量级桌面工具。提供图形界面（GUI）与命令行（CLI）两种用法，支持从注册表读写、配置文件导入/导出以及备份恢复。

## 功能说明

- **图形界面编辑**：可视化修改系统 OEM 信息，字段包括：
  - 制造商（Manufacturer）
  - 型号（Model）
  - 支持电话（SupportPhone）
  - 支持网址（SupportURL）
  - 服务时间（SupportHours）
- **注册表读写**：OEM 信息存储于
  `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\OEMInformation`，
  保存后需重启「设置」应用查看更改。
- **自动提权（UAC）**：若未以管理员运行，程序会请求以管理员身份重新启动。
- **配置文件导入/导出**：通过 `导出...` / `导入...` 按钮，以 INI 格式（`[OEMInformation]` 段）读写 `.oeminfo` / `.oembak` 文件。
- **备份自动恢复**：启动时若检测到程序目录下的 `oeminfo.oembak` 备份，会弹窗询问是否导入恢复（恢复成功后可选择删除或保留备份）。
- **未保存修改检测**：关闭或取消时若有未保存改动，会提示保存/放弃。
- **自定义字体**：内嵌「钉钉进步体」（DingTalk JinBuTi）资源，用于界面显示。
- **命令行模式**（无参数则启动 GUI）：

  | 命令 | 说明 |
  | --- | --- |
  | `<exe> restore [配置文件]` | 从指定文件静默恢复 OEM 信息（省略路径则使用当前目录 `oeminfo.oembak`） |
  | `<exe> backup <输出文件>` | 将当前 OEM 信息静默导出到指定文件 |
  | `<exe> edit Key=Value [...]` | 直接修改注册表键值，可用键：`Manufacturer`、`Model`、`SupportPhone`、`SupportURL`、`SupportHours` |
  | `<exe> help` | 显示命令行用法 |

  CLI 示例：

  ```bat
  OEM_Editor.exe edit Manufacturer=ACME Model=X1
  OEM_Editor.exe backup oeminfo.oeminfo
  OEM_Editor.exe restore oeminfo.oembak
  ```

## 编译方法

项目提供两种编译脚本。`.exe` 已在 `.gitignore` 排除，建议自行编译或从发布渠道获取。

### 方式一：MSVC（Visual Studio 2022）

需要安装 **Visual Studio 2022 + 使用 C++ 的桌面开发** 工作负载。双击或运行：

```bat
build.bat
```

脚本通过 `vswhere` 定位 VS 安装，自动选择 `vcvars64.bat` / `vcvars32.bat`，
使用 `cl /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE` 编译 `OEM_Editor.cpp`，
链接 `comctl32.lib shlwapi.lib`。

> 提示：该模式下需将 `OEM_Editor.exe`、`DingTalk JinBuTi.ttf`、`OEM_icon.ico`
> 放在同一目录使用。

### 方式二：MinGW-w64（完全独立）

需要 **MinGW-w64**（`g++` 与 `windres`），并把其 `bin` 目录加入 `PATH`。运行：

```bat
compile.bat
```

脚本先用 `windres` 把 `OEM_Editor.rc`（内嵌图标与字体）编译为 `OEM_Editor_res.o`，
再用 `g++ -static ... -mwindows` 链接资源生成完全独立的 `OEM_Editor.exe`，
无需任何外部文件即可运行。

## 文件说明

| 文件 | 说明 |
| --- | --- |
| `OEM_Editor.cpp` | 主程序源码（GUI + CLI 全部逻辑） |
| `OEM_Editor.rc` | 资源脚本，内嵌应用图标与字体（FONT） |
| `OEM_Editor_res.o` | `windres` 编译出的资源目标文件（MinGW 构建用） |
| `OEM_Editor.exe` | 编译生成的可执行文件（被 `.gitignore` 忽略） |
| `OEM_icon.ico` | 程序图标 |
| `DingTalk JinBuTi.ttf` | 内嵌自定义字体（钉钉进步体） |
| `build.bat` | MSVC 编译脚本 |
| `compile.bat` | MinGW-w64 编译脚本 |
| `DEVELOPMENT.md` | 开发相关说明文档 |
| `oemedit` | 从网盘拉取并运行最新 `OEM_Editor_cpp.exe` 的辅助脚本（PowerShell） |
| `oeminfo.oeminfo` / `oeminfo2.oeminfo` | 示例导出配置文件 |
| `.gitignore` | 忽略编译产物（`.exe` / `.obj` / `.o` / `.dll` / `.pdb` 等） |

## 使用说明

1. 以管理员身份运行 `OEM_Editor.exe`（GUI 模式会自动请求提权）。
2. 填写或修改 OEM 字段，点击「应用」保存，或「确定」保存并退出。
3. 需要批量/自动化时，使用上面的命令行模式。

> 所有写注册表操作均需管理员权限。修改 OEM 信息只影响系统属性中展示的内容，不影响硬件本身。

---
© HYCX Studio
