# Komorebi Launcher

面向需要国内下载环境的 Minecraft 玩家的启动器，fork 自 Prism Launcher。
A Minecraft launcher forked from Prism Launcher, aimed at players who need China-oriented download sources.

官网 https://komorebilauncher.github.io/ ｜ 下载 https://github.com/KomorebiLauncher/KomorebiLauncher/releases

GPL-3.0-only，见 LICENSE 。

> **NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT.**
> 非 Minecraft 官方产品，与 Mojang 、 Microsoft 均无隶属关系，也未获其批准或背书。本仓库不分发任何游戏文件：版本、资源与库文件一律从 Mojang 或镜像源的官方地址下载。

## 当前能力

- 实例隔离：每个实例独立的 .minecraft 目录、独立设置与独立图标
- 组件系统：Minecraft 本体与 Fabric 、 Forge 、 NeoForge 、 Quilt 等加载器按组件装拆
- 整合包来源：Modrinth 、 CurseForge 、 ATLauncher 、 FTB 、 Legacy FTB 、 Technic ，以及导入 .mrpack 、 MultiMC 与 PolyMC 导出的压缩包
- 模组与整合包更新检查：对已安装的 Mod 逐个比对来源站点的版本
- 模组、资源包、光影包、数据包、存档、截图在实例内直接管理，可从 Modrinth 搜索并安装
- 皮肤上传与披风选择
- Java 运行库自动检测与自动下载（按 Minecraft 与加载器的版本要求选择）
- 账户：微软登录（使用本 fork 自己注册的应用，提供浏览器跳转与设备码两种流程）、第三方验证服务器账户；离线档案需先有一个拥有 Minecraft 的微软账户
- 界面语言：简体中文完成度 100% ，翻译由本组织自建（见下）
- 自动更新：从本仓库的 GitHub Releases 检查并安装更新

## 尚未配置的部分

- CurseForge 的 API key 只属于原项目，衍生构建不得复用，因此本仓库不带 CurseForge API key 。在填入自己的 key 之前（设置 → API ），CurseForge 的模组与整合包来源不可用。另外自 2026-07-16 起，CurseForge 的文件下载域名 edge.forgecdn.net 本身也要求携带 x-api-key 。
- 截图上传到 Imgur 同样未配置。
- 国内镜像下载源与 Java 镜像正在开发中。

## 微软登录与 AppID（当前状态：不可用）

微软登录需要一个在 Microsoft 身份平台上注册、并且**被 Mojang 列入白名单**的应用（AppID）。本仓库内置的 AppID 由本组织自行注册，Mojang 的审核已连续四次未通过，因此：

- 微软账号可以完成 Microsoft 登录与 Xbox 授权，但换取 Minecraft 令牌时会被拒绝，服务端返回 `Invalid app registration`（`api.minecraftservices.com` 上的新旧两个端点都会被拒，与登录方式无关）；
- **自己注册一个 AppID 没有用**：白名单是逐个应用审核的，个人注册的应用不会因此获批；
- **离线档案也受影响**：自 0.1.6 起，新建离线档案要求账户列表里存在"拥有 Minecraft 的微软账户"，而微软账户当前无法登录，所以在批准之前**新建离线档案同样不可用**；
- 目前**只有第三方验证服务器（authlib-injector）账户可用**：它们不经过这套流程，可以正常登录、下载与启动。

审核进展记录在仓库的 issue 追踪里。通过之前，需要正版登录或离线单机的用户请使用官方启动器。

登录窗口同时提供浏览器跳转与设备码两种流程（界面中间以 or 分隔），它们共用同一个 AppID。

## 依赖的上游服务

翻译源由本组织自建，其余元数据仍使用 Prism Launcher 提供的公开服务：

- https://meta.prismlauncher.org/v1/ ：版本与加载器元数据
- https://files.prismlauncher.org/fmllibs/ ：1.5.2 及更早版本的 Forge 依赖库
- https://komorebilauncher.github.io/i18n/ ：语言包，仓库为 KomorebiLauncher/i18n

## 构建

Windows 下实测可用的配置：Visual Studio 2022 Build Tools （ MSVC 14.44 ） 、 CMake 4.4 、 Ninja 、 Qt 6.11.2 win64_msvc2022_64 （需要模块 qtimageformats 与 qtnetworkauth ） 、 Azul Zulu JDK 17 。

JDK 必须是 17 ： libraries/launcher 以 -source 7 编译，新版 JDK 已拒绝该选项。

先初始化子模块，再在开发者命令行环境里配置与构建：

    git submodule update --init --recursive
    cmake --preset windows_msvc -DVCPKG_HOST_TRIPLET=x64-windows -DVCPKG_TARGET_TRIPLET=x64-windows
    cmake --build --preset windows_msvc --config Debug
    ctest --preset windows_msvc --build-config Debug

vcpkg 会在首次配置时从源码编译 libarchive 、 cmark 、 libqrencode 、 tomlplusplus 等依赖，需要十几分钟到几十分钟。

### 本机构建失败的常见原因

以下两类失败与代码无关，都是环境问题，已在 2026-10-27 的机器上复现并绕过：

1. **vcpkg 重新安装依赖时 `cl` 报 `D8050`，或 `vcpkg has crashed ... pwsh.exe`**。诱因是 PATH 里 Windows Store 版 `pwsh` 的别名损坏（该路径一访问就报 "cannot be accessed by the system"），以及 PATH 过长。做法是给构建用一条精简 PATH，并让 vcpkg 只用系统工具、把临时目录放进仓库：

       set PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0;C:\Program Files\Git\cmd;C:\Users\<你>\AppData\Local\Microsoft\WinGet\Links;C:\Program Files\WindowsApps\Microsoft.PowerShell_7.6.9.0_x64__8wekyb3d8bbwe
       call "...\vcvars64.bat"
       set VCPKG_FORCE_SYSTEM_BINARIES=1
       set VCPKG_BINARY_SOURCES=clear
       set TMP=D:\Project\Github\KomorebiLauncher\build\vtmp
       set TEMP=%TMP%
       cmake\vcpkg\vcpkg.exe install --triplet x64-windows --x-manifest-root=. --x-install-root=build\vcpkg_installed --x-buildtrees-root=build\vbt --x-packages-root=build\vpkg

   一次性装完后，`cmake -B build` 会跳过安装步骤。

2. **测试全部以 `0xC0000135` 失败，或 `QTemporaryDir::isValid()` 为 false**。前者是 Qt 的 `bin` 不在 PATH 上，后者是本机默认 `%TEMP%` 不可用；跑 `ctest` 时把 Qt `bin` 加进 PATH，并把 `TMP`/`TEMP` 指到仓库内目录即可。

Windows Store 版的 `pwsh` 别名本身损坏时，`C:\Program Files\WindowsApps\Microsoft.PowerShell_*\pwsh.exe` 仍可直接执行，上面的 PATH 用的就是它。

## 仓库结构与上游同步

默认分支 main 的历史是压缩后的快照，对应上游 Prism Launcher 的提交 77f2bee593eb9e0cdeef530e0e8686524b68d9d9 （2026-10-19）。完整上游历史与逐行 blame 由上游仓库自己维护，本仓库不保留副本。

需要引入上游改动时，把 https://github.com/PrismLauncher/PrismLauncher 加为 remote 后 fetch ，再 cherry-pick 到 main ；cherry-pick 不要求两个分支有共同祖先，因此跨仓库同样可用。

## 许可与出处

本程序是 Prism Launcher 的衍生作品，而 Prism Launcher 本身衍生自 PolyMC 与 MultiMC 。

- 代码许可为 GPL-3.0-only ，上游的版权声明与许可证文本保留在源文件头与 LICENSE 中
- program_info 目录里的标志与部分美术资源采用 CC BY-SA 4.0 ，版权归 Prism Launcher Contributors ，见 program_info/LICENSE ；本仓库新增的应用图标由 program_info/gen_icons.py 生成，采用 CC0-1.0
- 本组织与 Prism Launcher 、 PolyMC 、 MultiMC 均无隶属关系，也未获得任何一方的背书
- 帮助与 Wiki 链接指向上游文档，因为界面与操作方式与之一致
