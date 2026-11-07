# 三平台编译器差异

CI 在三个平台上都以 `-Werror` 编译，但 **Clang 的警告集合比 GCC / MSVC 大**，因此同一份代码
可能只在 macOS 上失败。以下是已经遇到过的、以及可能遇到的差异。

## 已遇到

| 警告 | 触发 | 影响平台 |
| --- | --- | --- |
| `-Wunused-private-field` | 头文件里声明了私有成员但从未使用 | 仅 Clang（macOS） |
| `-Wno-sfinae-incomplete` 相关 | 需要显式关闭 | 已统一关闭 |

处理：`launcher/CMakeLists.txt` 里对 `Launcher_logic` 与主程序目标加了 `-Wno-unused-private-field`。
**这是兜底，不是许可** —— 删掉用途时仍应删掉成员。

## 可能遇到

| 警告 | 说明 |
| --- | --- |
| `-Wshadow` | 局部变量遮蔽成员，Clang 报得更细 |
| `-Wunused-const-variable` | 文件内未使用的常量 |
| `-Wunreachable-code` | 死代码分支 |

## 排查顺序

1. 先看失败步骤：`Run build` 是编译，`Package (macOS)` 是签名/打包，两者完全不同的方向；
2. 编译失败时先怀疑**类型与声明**（`qobject_cast<T*>` 需要 `Q_OBJECT`、成员函数是否真的存在）；
3. 只有 macOS 失败时，优先怀疑 Clang 独有警告，而不是环境问题。

## 打包链路（macOS）

已知两个不稳定点，均已在 `.github/actions/package/macos/action.yml` 中处理：

| 问题 | 原因 | 处理 |
| --- | --- | --- |
| `code object is not signed at all`（指向 `Contents/MacOS/jars/*.jar`） | 只签主二进制、`--deep` 不覆盖嵌套 jar | 先逐个签 `jars/`，再签整个 `.app`，最后 `codesign --verify` |
| `hdiutil: create failed - Resource busy` | 上一次尝试残留的挂载点 | 创建前强制 `detach`，失败重试 3 次 |