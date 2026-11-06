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

## 尚未完成的部分

- **Java 运行时下载还没有镜像**：游戏本体、库文件与资源走 BMCLAPI（默认值，可在设置 → API 里改）或等价镜像，但自动下载的 Java 运行时仍取自 Mojang 的 java-runtime 清单与各发行版的官方 API（Adoptium 等），国内网络下可能很慢或失败。这是当前对目标用户影响最大的一处缺口。
- **本地自行编译时没有 CurseForge key**：CI 构建通过仓库 secret 注入 `Launcher_CURSEFORGE_API_KEY`，正式发布的安装包带 key、CurseForge 来源可用（设置 → API 里也能自己填）。本地 `cmake` 默认该值为空，需要手动加 `-D Launcher_CURSEFORGE_API_KEY=<你的 key>`。另外自 2026-07-16 起，CurseForge 的文件下载域名 edge.forgecdn.net 本身也要求携带 x-api-key。
- **截图上传需要玩家自己的 ImgBB API key**：不内置任何图床凭据（Imgur 在中国大陆不可达，且其应用注册不对所有人开放）。首次上传时界面会提示填写自己的 key（<https://api.imgbb.com/> 免费注册即可），保存在本机设置里；不填则上传功能不出现。

## 微软登录与 AppID

微软登录需要一个在 Microsoft 身份平台上注册、并且**被 Mojang 列入白名单**的应用（AppID）。本仓库内置的 AppID 由本组织自行注册，**已于 2026-11-06 通过 Mojang 审核**，正版登录正常可用：

- 用玩家自己的微软账户登录、校验 Java 版版权、读取档案并启动游戏；
- **自己注册一个 AppID 没有用**：白名单逐个应用审核，本仓库的 ID 不能给衍生构建复用 —— 分发衍生构建需要自行申请；
- **离线档案**：自 0.1.6 起，新建离线档案要求账户列表里存在"拥有 Minecraft 的微软账户"。这是一道刻意保留的门槛，用来明确离线档案不是绕过正版验证的途径；微软登录可用之后，它不再妨碍正常使用；
- **第三方验证服务器（authlib-injector）账户**：不经过这套流程，可独立登录、下载与启动。

申请过程与结果记录在仓库的 issue 追踪里。Mojang 保留撤销批准的权利，若内置 AppID 将来失效，登录会报 `Invalid app registration`，届时需要重新申请。

登录窗口同时提供浏览器跳转与设备码两种流程（界面中间以 or 分隔），它们共用同一个 AppID。

## 立场说明

本项目的目的是让玩家在**自己拥有的**游戏上获得更好的国内下载与使用体验，不是提供绕过任何验证的手段。围绕这一点，有几条固定做法，也希望据此判断别人转述的"这个启动器能干什么"是否属实：

- **不帮助绕过 Minecraft 的安全与执法机制**。启动器不伪造会话、不内置破解补丁、不绕过 Realms 或任何要求正版身份的服务器校验，也不提供任何"免登录启动"的开关、参数或测试路径。
- **离线档案有门槛，且不会自动降级**。新建离线档案要求账户列表里已经存在一个"拥有 Minecraft 的微软账户"；登录失败或会话过期时，绝不会回退到离线档案。离线档案只用于单机世界与允许离线玩家的服务器，它不连接 Microsoft、Realms，也拿不到任何 Microsoft 或 Minecraft 令牌。
- **第三方验证服务器登录由玩家自己添加**。这是给那些本身不使用 Mojang 认证的服务器用的登录方式，启动器不预置列表、不推荐、不代为托管，也不在其中夹带任何令牌转发。
- **不托管、不分发游戏文件**。版本、库与资源一律从 Mojang 或镜像的官方地址下载，并按 Mojang 公布的 SHA-1 校验；镜像只替换主机名，不改变文件内容。
- **不使用其他项目的 AppID**。本仓库携带自己的应用登记；衍生构建必须自行申请，复用本仓库的 ID 会把流量算在本项目名下，也会让我们无法为它负责。
- **不接受被描述成"破解启动器"**。若发现有人这样宣传，我们会公开澄清；这类使用方式不在项目支持范围内。

这些做法不是临时姿态：其中"离线档案门槛"与"不存在未认证启动路径"这两条，是 Mojang 审核时明确列出的条件（不得帮助绕过安全与执法机制、不得支持盗版场景），也是本仓库在 0.1.6 就引入、此后一直保留的原因。

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
