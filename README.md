# WTF Launcher

面向需要国内下载环境的 Minecraft 玩家的启动器，fork 自 Prism Launcher。
A Minecraft launcher forked from Prism Launcher, aimed at players who need China-oriented download sources.

官网 https://wtflauncher.github.io/ ｜ 下载 https://github.com/WTFLauncher/WTFLauncher/releases

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

## 微软登录与 AppID

微软登录需要一个在 Microsoft 身份平台上注册的应用（下称 AppID）。本仓库内置的 AppID 由本组织自行注册，**目前尚未获得 Xbox Live 的授权**，所以 Microsoft 有可能在授权环节直接拒绝登录；这不是账号问题，也不会影响其他功能。

被拒时，登录窗口会在错误信息里说明原因，并给出下一步。可用的做法是**自备 AppID**：

1. 在 https://portal.azure.com 注册一个应用，平台选「移动和桌面应用程序」；
2. 重定向 URI 填 `http://localhost`；
3. 在「身份验证」中允许公共客户端流（Allow public client flows）；
4. 把「应用程序(客户端) ID」填进 设置 → API → MSA Client ID ，重启启动器后生效。

登录窗口同时提供浏览器跳转与设备码两种流程（界面中间以 or 分隔），但它们共用同一个 AppID，所以被拒绝时两种方式都会被拒绝。离线档案与第三方验证服务器（authlib-injector）不经过这套流程，不受影响。

## 依赖的上游服务

翻译源由本组织自建，其余元数据仍使用 Prism Launcher 提供的公开服务：

- https://meta.prismlauncher.org/v1/ ：版本与加载器元数据
- https://files.prismlauncher.org/fmllibs/ ：1.5.2 及更早版本的 Forge 依赖库
- https://wtflauncher.github.io/i18n/ ：语言包，仓库为 WTFLauncher/i18n

## 构建

Windows 下实测可用的配置：Visual Studio 2022 Build Tools （ MSVC 14.44 ） 、 CMake 4.4 、 Ninja 、 Qt 6.11.2 win64_msvc2022_64 （需要模块 qtimageformats 与 qtnetworkauth ） 、 Azul Zulu JDK 17 。

JDK 必须是 17 ： libraries/launcher 以 -source 7 编译，新版 JDK 已拒绝该选项。

先初始化子模块，再在开发者命令行环境里配置与构建：

    git submodule update --init --recursive
    cmake --preset windows_msvc -DVCPKG_HOST_TRIPLET=x64-windows -DVCPKG_TARGET_TRIPLET=x64-windows
    cmake --build --preset windows_msvc --config Debug
    ctest --preset windows_msvc --build-config Debug

vcpkg 会在首次配置时从源码编译 libarchive 、 cmark 、 libqrencode 、 tomlplusplus 等依赖，需要十几分钟到几十分钟。

## 仓库结构与上游同步

默认分支 main 的历史是压缩后的快照，对应上游 Prism Launcher 的提交 77f2bee593eb9e0cdeef530e0e8686524b68d9d9 （2026-10-19）。完整上游历史与逐行 blame 由上游仓库自己维护，本仓库不保留副本。

需要引入上游改动时，把 https://github.com/PrismLauncher/PrismLauncher 加为 remote 后 fetch ，再 cherry-pick 到 main ；cherry-pick 不要求两个分支有共同祖先，因此跨仓库同样可用。

## 许可与出处

本程序是 Prism Launcher 的衍生作品，而 Prism Launcher 本身衍生自 PolyMC 与 MultiMC 。

- 代码许可为 GPL-3.0-only ，上游的版权声明与许可证文本保留在源文件头与 LICENSE 中
- program_info 目录里的标志与部分美术资源采用 CC BY-SA 4.0 ，版权归 Prism Launcher Contributors ，见 program_info/LICENSE ；本仓库新增的应用图标由 program_info/gen_icons.py 生成，采用 CC0-1.0
- 本组织与 Prism Launcher 、 PolyMC 、 MultiMC 均无隶属关系，也未获得任何一方的背书
- 帮助与 Wiki 链接指向上游文档，因为界面与操作方式与之一致
