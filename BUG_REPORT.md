# WTFLauncher 缺陷审查报告

| 项目 | 内容 |
| --- | --- |
| 审查对象 | `D:\Project\Github\WTFLauncher`（PrismLauncher 的二次开发分支，C++23 / Qt ≥ 6.8 / CMake + Ninja） |
| 审查方式 | 静态审查：逐文件通读源码，并用仓库内单元测试所表达的预期行为做交叉验证 |
| 审查日期 | 2026-10-25 |
| 结论 | 5 项高危、14 项中危、13 项低危可证缺陷；另有 6 项需运行验证、3 项经复核排除的误报 |

## 0. 方法与限制

- **未编译、未运行测试**：本机 shell 无法启动（`pwsh` 每次返回 `0xC0000142`，DLL 初始化失败），`cmake`/`ninja`/`ctest` 均不可用。
- **无法与上游逐行比对**：网络被策略拦截（`web_fetch` 返回 "resolves to a non-public IP"），且仓库内没有 `.git`，无法用提交历史界定"哪些是本分支新写"。
- 因此每条结论都以仓库内实际代码为依据，给出 **文件:行号 + 代码片段**；凡依赖运行才能确认的推断单独列在 §5。对疑似继承自上游的问题，均在条目中注明。

审查范围（按文件修改时间排序，末尾约 40 个文件是本分支的实际工作区）：内存建议与启动链、Java 选择/安装、账号认证（Yggdrasil / authlib-injector）、OptiFine 安装、镜像下载、服务器状态页、崩溃分析。

---

## 1. 结论摘要

| # | 级别 | 位置 | 问题 | 后果 |
| --- | --- | --- | --- | --- |
| **H1** | 高·安全 | `minecraft/auth/AccountData.cpp:351` | 第三方账号**密码明文**写入 `accounts.json` | 凭据泄露且无法吊销 |
| **H2** | 高 | `ui/pages/instance/ServersPage.cpp:493` | ping 结果按**行号**回填 | 列表一变，状态/MOTD 张冠李戴 |
| **H3** | 高 | `ui/pages/instance/McClient.cpp:23` | VarInt 写入用错了位掩码 | 协议版本 763 实际发出 640；长度 ≥128 的字符串被写成 0 |
| **H4** | 高 | `ui/pages/instance/ServerPingTask.*`、`McClient.cpp` | ping 无超时、不可 abort | 一台不回包、但接受 TCP 的服务器会让状态列永久卡住，且刷新无法取消旧查询 |
| **H5** | 高 | `java/LocalJavaScanTask.cpp:106` | 用 arch-first 的 `operator>` 冒充"版本降序" | 实例内同时有 32/64 位 Java 时选到 32 位；多 major 时选到旧的 |
| **M1** | 中 | `modplatform/authlibinjector/AuthlibInjectorInstallTask.cpp:148` | 固定的共享临时文件名 | 并发安装互相覆盖，可能装上**别人服务器的**认证地址 |
| **M2** | 中 | `icons/IconList.cpp:177,200,218` | 图标 key 生成规则前后不一致 | 子目录图标互相覆盖；图标文件改动永不刷新 |
| **M3** | 中 | `modplatform/optifine/OptiFineInstallTask.cpp:294-311` | `harvest()` 挂两个信号 + 在自己信号里 `delete` QProcess | 重复安装、二次完成信号、调试版断言崩溃 |
| **M4** | 中 | `modplatform/optifine/OptiFineInstallTask.cpp:230,265` | 对已启动的加载任务再次 `start()` | 触发 `Task::start()` 的 `ASSERT_NEVER`，调试版直接 abort |
| **M5** | 中 | `java/JavaChecker.cpp:85-88` | 找不到 `JavaCheck.jar` 时不发终态 | 新增的"用实例内 Java"路径会让**启动永久挂起** |
| **M6** | 中·安全 | `ui/dialogs/YggdrasilLoginDialog.cpp:125-129` | 允许任意 `http://` 认证服务器 | 明文密码经网络传输 |
| **M7** | 中 | `minecraft/auth/AccountList.cpp:124-141` | Yggdrasil 账号按全局 `profileId` 判重 | 同一玩家名在两个第三方服务器上会互相覆盖、账号丢失 |
| **M8** | 中 | `net/NetJob.cpp:71-82` + `net/MirrorPolicy.h:89` | 重试不重置镜像序列 | NetJob 的 3 次重试只会重打最后一个候选源 |
| **M9** | 中 | `net/MirrorPolicy.cpp:103` vs `ui/pages/instance/VersionPage.cpp:451` | 空 `MirrorBaseUrl` 两处语义相反 | "只走镜像"被静默降级为"只走官方"，而无提示 |
| **M10** | 中 | `ui/pages/instance/ServerStatus.cpp:76-79` | "有响应但没带 players"被判离线 | 状态页同时显示 `Offline` 与版本/延迟，自相矛盾 |
| **M11** | 中 | `JavaCommon.cpp:133` | "测试"按钮把 `maxMem` 当 `minMem` 传 | 用 `-Xms=max -Xmx=max` 测试，低内存机器上误报"参数坏" |
| **M12** | 中 | `java/LocalJavaScanTask.cpp:35-51` | 只扫一层目录 | 找不到 Mojang 官方 runtime（布局是四层） |
| **M13** | 中 | `ui/pages/instance/McResolver.cpp:39-42` | SRV 记录直接取第一条 | 忽略 RFC 2782 的 priority/weight，可能连到备用/不可用节点 |
| **M14** | 中 | `ui/pages/instance/McClient.cpp:123-140` | 不校验响应长度 | 负数长度导致解析空包；超大长度可无限吃内存 |
| L1 | 低 | `ui/widgets/JavaSettingsWidget.cpp:386-398` | 阈值提示分支顺序把 `min>max` 挡死 | 用户看不到真正的配置错误 |
| L2 | 低 | `HardwareInfo.cpp:323-339` | FreeBSD `hw.physmem` 是字节却按 KiB 换算 | 总内存报大 1024 倍，内存建议失去上限保护 |
| L3 | 低 | `minecraft/auth/Parsers.cpp:445-447` | 注释与代码对 token 年龄的假设相反 | Yggdrasil token 12 小时内不刷新 |
| L4 | 低 | `minecraft/auth/AccountList.cpp:314`、`java/JavaInstallList.cpp:98` | `row > count()` 的 off-by-one | `row == count()` 时越界（release 下 UB） |
| L5 | 低 | `net/Request.h:156` | `setUrl()` 不重建镜像序列 | 头文件声明的不变式被破坏 |
| L6 | 低 | `net/Request.cpp:475-477` | `m_errorResponse` 跨镜像累积 | 报错内容来自别的镜像，误导排障 |
| L7 | 低 | `net/MirrorPolicy.h:85-87` | 空序列 `current()` 抛异常 / `remaining()` 下溢 | 新增调用方一个疏忽就崩 |
| L8 | 低 | `java/JavaChecker.cpp:162` | 只把退出码 1 当错误 | 其它非零退出码被当作正常输出解析 |
| L9 | 低 | `modplatform/optifine/OptiFineInstallTask.cpp:195-208` | OptiFine / client jar 下载不校验哈希；`filename` 直接拼路径 | 镜像被劫持时可装任意 jar；路径可逃逸 scratch |
| L10 | 低 | `java/LocalJavaScanTask.cpp:58` | 未 `setAbortable`、重启不清空结果 | 扫描期间启动不可取消；复用对象会残留旧结果 |
| L11 | 低 | `ui/widgets/MirrorListEditor.cpp:88-99` | 手改配置的镜像项不做校验也不提示 | UI 显示启用、实际被静默跳过 |
| L12 | 低 | `minecraft/auth/steps/YggdrasilStep.cpp:49-53` | 带 query 的地址会把 endpoint 拼进 query | 登录 404，且错误提示指向错误原因 |
| L13 | 低 | `ui/widgets/JavaSettingsWidget.cpp:329-344` | 模组数只数顶层、只在加载时算一次 | 内存建议经常过期 |

---

## 2. 高危问题

### H1 第三方账号密码以明文落盘（安全）

**位置**：`launcher/minecraft/auth/AccountData.cpp:311-316`（读）、`:347-355`（写）、`MinecraftAccount.cpp:92-99`、`AccountList.cpp:545-571`、`YggdrasilLoginDialog.cpp:170-175`

```cpp
// AccountData.cpp:347
} else if (type == AccountType::Yggdrasil) {
    output["type"] = "Yggdrasil";
    output["auth-server"] = authServerUrl;
    output["login"] = loginName;
    output["password"] = password;          // <-- 明文
    ...
}

// AccountList.cpp:545
for (MinecraftAccountPtr account : m_accounts) {
    QJsonObject accountObj = account->saveToJson();   // 内含上面的 password
    ...
}
file.write(doc.toJson());                            // :571
```

**为什么是缺陷**

刷新第三方账号本来只需要 `accessToken` + `clientToken`（`AccountData` 已有 `yggdrasilToken`，`Parsers::parseYggdrasilResponse` 也已把 `clientToken` 存进 `extra`），这里却把**用户密码**写进 `accounts.json`：

1. 该文件是普通文件，会被 OneDrive/坚果云/备份工具同步，也常被用户打包进求助用的 zip；
2. `AccountList.cpp:572` 只设了 `ReadOwner|WriteOwner`（同机其他用户），挡不住同步、备份、提权进程、崩溃转储；
3. 令牌可以吊销，**密码不能**；
4. 登录对话框（`YggdrasilLoginDialog`）没有"记住密码"选项，用户不会预期密码被保存。

**建议**：不持久化密码；refresh 失败时重新弹登录框，或把长期凭据放进系统钥匙串（DPAPI / Keychain / libsecret）。无论如何都要在迁移时清空已落盘的旧字段。

---

### H2 服务器状态按"行号"回填，列表一变就张冠李戴

**位置**：`launcher/ui/pages/instance/ServersPage.cpp:471-504`

```cpp
int row = 0;
for (Server& server : m_servers) {
    server.m_status = {};
    ...
    auto* task = new ServerPingTask(target.address, target.port);
    m_currentQueryTask->addTask(Task::Ptr(task));

    connect(task, &Task::finished, this, [this, task, row]() {   // 捕获的是行号快照
        if (row >= m_servers.size()) {                            // 只查上界
            return;
        }
        m_servers[row].m_status = task->m_outputStatus;
        emit dataChanged(index(row, 0), index(row, COLUMN_COUNT - 1));
    });
    row++;
}
```

**为什么是缺陷**：ping 是异步的（并发 + SRV 解析），而这期间模型有三种改动路径：`addEmptyRow()`（`:222`）、`removeServers()`（`:237`）、`moveUp/moveDown`（`:259`/`:274`）、以及 `load()`（`:439`，`servers.dat` 被游戏或别的进程改写后整表 `swap`）。只要发生一次，`m_servers[row]` 就变成另一台服务器：状态、人数、MOTD、延迟全部错位（被腾出的那行永远停在 `...`）。用户据此点"加入服务器"就会进错服。

**建议**：回填时按稳定标识（`m_address`，或给 `Server` 加 `QUuid m_id`）`find_if` 定位；找不到就丢弃。或在查询期间复用已有的 `m_locked` 锁住编辑。

---

### H3 VarInt 写入用错了位掩码（Minecraft 协议）

**位置**：`launcher/ui/pages/instance/McClient.cpp:16-30`（`writeVarInt`），使用点 `:105-113`

```cpp
constexpr auto g_varIntValueMask = 0x7F;
constexpr auto g_varIntContinue  = 0x80;

void writeVarInt(QByteArray& data, int value)
{
    while ((value & ~g_varIntValueMask) != 0) {   // 判断是否还有高位：正确
        data.append(static_cast<uint8_t>((value & ~g_varIntValueMask) | g_varIntContinue));  // 错：应为 value & 0x7F
        value >>= 7;
    }
    data.append(static_cast<uint8_t>(value));
}
```

**为什么是缺陷**：续延字节应当是 `(value & 0x7F) | 0x80`，这里却是 `(value & ~0x7F) | 0x80`——取的是**高 25 位**。按 wiki.vg 的 VarInt 规则逐个算：

| 期望值 | 正确编码 | 实际编码 | 服务器解出 |
| --- | --- | --- | --- |
| 763（协议版本，`:106`） | `FB 05` | `80 05` | **640** |
| 128（字符串长度） | `80 01` | `00 01` | **0**（首字节没有续延位，解码器直接停止） |
| 1 / 0 | `01` / `00` | `01` / `00` | 正确 |

所以**每次 ping 的握手包协议版本都是错的**（763→640）；此外服务端地址超过 127 字符（`writeString`，`:76`）或响应长度 ≥128 时，长度前缀会被写成 0。`readVarInt`（`:44-65`）的掩码是对的，问题只在写方向。现有 `tests/ServerStatus_test.cpp` 只测解析，不覆盖 `McClient`，所以测试发现不了。

顺带一个相关问题：`writeString()`（`:74-78`）用 `value.size()`（UTF-16 码元数）写长度前缀，却用 `value.toUtf8()` 追加字节。纯 ASCII 域名下两者相同，非 ASCII 域名时长度与实际字节数不符（这类域名本该先 punycode 化）。

**建议**：改成 `(value & g_varIntValueMask) | g_varIntContinue`，并补一个 `McClient` 的编码单测（763 → `FB 05`）。

---

### H4 服务器 ping 无超时、不可取消

**位置**：`ui/pages/instance/ServerPingTask.h:9-22`、`ServerPingTask.cpp:10-41`、`McClient.cpp:86-100`

```cpp
class ServerPingTask : public Task {
   public:
    explicit ServerPingTask(QString domain, int port) : Task(), m_domain(domain), m_port(port) {}
    ...
   protected:
    virtual void executeTask() override;      // 没有 abort()/canAbort()，也没有 QTimer
};
```

**为什么是缺陷**：

1. `Task::canAbort()` 默认返回 `m_can_abort == false`（`Task.h:117,219`），`Task::abort()`（`Task.h:166-172`）在 `!canAbort()` 时**不发出任何信号**。于是 `ServersPage::queryServersStatus()` 里的 `m_currentQueryTask->abort()` 必然失败，`ConcurrentTask::abort()` 走到 `emitFailed(tr("Failed to abort all running tasks."))`（`ConcurrentTask.cpp:82-94`）——每次点"刷新"都会产生一次失败信号，而旧查询继续跑。
2. `McClient::getStatusData()` 没有超时定时器。如果服务器**接受 TCP 但不回包**（防护设备、假死进程、半开连接），`readyRead` 永不触发，`ServerPingTask` 永不结束，该行永远停在 `...`，`ConcurrentTask` 也永不完成；反复刷新还会叠加更多悬挂连接。

**建议**：`ServerPingTask` 增加 `setAbortable(true)` 并实现 `abort()`（关 socket、`emitAborted()`）；给 `McClient` 加一个总超时（例如 10s，`QTimer::singleShot`）→ `emitFail(tr("Timed out"))`。

---

### H5 本地 Java 的"最新优先"排序实际按 CPU 架构排序

**位置**：`java/LocalJavaScanTask.cpp:104-106`、`java/JavaInstall.cpp:24-46`、`minecraft/launch/AutoInstallJava.cpp:113-118`

```cpp
// LocalJavaScanTask.cpp:104
// Newest first: an instance folder may hold several runtimes and the highest usable one is
// the better default.
std::sort(m_installs.begin(), m_installs.end(), [](const JavaInstallPtr& a, const JavaInstallPtr& b) { return *a > *b; });
```

```cpp
// JavaInstall.cpp:24
bool JavaInstall::operator<(const JavaInstall& rhs) const
{
    auto archCompare = StringUtils::naturalCompare(arch, rhs.arch, Qt::CaseInsensitive);
    if (archCompare != 0)
        return archCompare < 0;               // 先比 arch！
    if (id < rhs.id) { ... }
    ...
}
bool JavaInstall::operator>(const JavaInstall& rhs) const { return (!operator<(rhs)) && (!operator==(rhs)); }
```

**为什么是缺陷**：`operator<` 的第一关键字是 `arch`（来自 `JavaChecker::Result::realPlatform`，如 `x86`/`amd64`/`x86_64`），版本号只在 arch 相同时才参与比较；`operator>` 又是 `!(<) && !(==)`。所以 `*a > *b` 实际是"按 arch 字符串降序"，`LocalJavaScanTask` 注释里的 "Newest first" 并不成立。

两个具体后果：

- `"x86"` 字典序大于 `"amd64"`，降序后 **32 位排在 64 位前面**；`AutoInstallJava::chooseLocalJava()` 里 `Java::pickInstall()` 取第一个兼容项，于是选中 32 位 Java（无法使用 >2048 MiB 堆），且不像自动下载分支那样有"检测到 32 位"的提示（`:142-144`）。
- 当 `getCompatibleJavaMajors()` 含多个 major（如 `[21,17]`）时，可能选中 17 而不是 21。

同仓库 `JavaInstallList.cpp:137-150` 的注释已经明确写过"JavaInstall 自身的排序把架构放在最前，反向排序会把 32 位排在 64 位之上"，新代码却又直接用了一次。

**建议**：复用 `sortJavas` 的语义（先 `id` 降序，再 `is_64bit` 优先），或让 `pickInstall` 显式按 `requiredMajors` 顺序 + 位宽排序。

---

## 3. 中危问题

### M1 authlib-injector 组件文件使用固定的共享临时文件名

**位置**：`modplatform/authlibinjector/AuthlibInjectorInstallTask.cpp:136-167`

```cpp
auto staging = FS::PathCombine(QDir::tempPath(), QStringLiteral("wtf-authlib-injector-component.json"));
QSaveFile out(staging);
...
if (packProfile->getComponent(componentUid) && !packProfile->remove(componentUid)) { ... }
if (!packProfile->installComponents({ staging })) { ... }
QFile::remove(staging);
```

**为什么是缺陷**：`QDir::tempPath()` 全机共享、文件名固定。两个启动器窗口（或同机两个实例）同时给**不同**服务器安装时：A 写完文件 → B 覆盖成自己的 `serverUrl` → A 才执行 `installComponents({staging})`，于是 A 的实例里装上了 **B 的认证服务器地址**，`-javaagent` 会把玩家凭据导向错误的服务器。此外还有固定可预测路径带来的本地竞态。对比同分支 OptiFine 的实现（`OptiFineInstallTask::executeTask` 用私有 `QTemporaryDir`），这里是唯一没有隔离的。

**建议**：改用 `QTemporaryDir`/唯一文件名，并保证成败路径都清理。

---

### M2 图标列表：key 生成规则前后不一致

**位置**：`icons/IconList.cpp:176-179`、`:198-200`、`:212-235`、`:85-92`

```cpp
// 删除/新增分支：去掉目录、去掉扩展名
QString key = QFileInfo(relativePath).completeBaseName();

// 文件内容变化：保留目录 + 扩展名
QString key = m_dir.relativeFilePath(checkfile.absoluteFilePath());
int idx = getIconIndex(key);       // m_nameIndex.find(key) 精确匹配 -> 永远 -1
if (idx == -1)
    return;

// 排序里的子目录判断
bool aIsSubdir = a.m_key.contains(QDir::separator());
```

**为什么是缺陷**：

1. `fileChanged()` 永远命中不到：注册的 key 是 `grass`，查询的是 `grass.png`（子目录图标还会带 `sub/`）。结果是**换了图标文件，界面不刷新**。
2. 子目录图标与根目录同名图标 key 相同（都是 `completeBaseName()`），`addIcon()`（`:395`）发现 key 已存在就 `replace()`，后遍历到的把前一张顶掉；删除其中一个还会把另一个的 `FileBased` 类型摘掉。
3. `sortIconList()` 的"根目录图标排前面"是死代码——`completeBaseName()` 里不可能出现分隔符；Windows 上即便有，`QDir::separator()` 是 `\` 而 key 是 `/`。

**建议**：统一用"相对路径去扩展名"作 key（根目录就是 `completeBaseName()`），三处复用同一个函数；`sortIconList()` 改用 `QLatin1Char('/')`。

---

### M3 OptiFine 安装：`harvest()` 挂两个信号，并在 QProcess 自己的信号里 `delete` 它

**位置**：`modplatform/optifine/OptiFineInstallTask.cpp:294-311`、`:46-56`

```cpp
connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this] { harvest(); });
connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) { failWith(...); return; }
    harvest();                        // Crashed/Timedout/... 也会走到
});
...
void InstallTask::harvest()
{
    delete m_process;                 // 在它自己发出的 finished 信号栈里删除 sender
    m_process = nullptr;
    ...
    packProfile->installComponents({ staging });
    emitSucceeded();                  // 或 failWith()->emitFailed()
}
```

**为什么是缺陷**：

1. **二次收割**：QProcess 崩溃时 Qt 先发 `errorOccurred(Crashed)` 再发 `finished(_, CrashExit)`，`harvest()` 会跑两遍——第二遍重新读 json、重新删拷全部库文件、再 `remove()+installComponents()`。
2. **二次完成信号**：第二遍结尾再次 `emitSucceeded()/emitFailed()`；`Task::emitSucceeded()` 开头是 `if (ASSERT_NEVER(!isRunning())) ...`（`tasks/Task.cpp:148-154`），`ASSERT_NEVER` = `(Q_ASSERT(cond == false), cond)`（`AssertHelpers.h:24`）→ **调试构建 abort**，发布构建只打 critical 日志后返回（状态可能"先成功后失败"）。
3. **在信号里删除 sender**：`harvest()` 从 `m_process` 的 `finished` 槽同步调用，此处应 `deleteLater()`。
4. **abort 后仍收割**：`abort()`（`:46-56`）`kill()` 后立刻 `emitAborted()`，被 kill 的进程随后仍发 `finished` → `harvest()` 继续把组件装进实例，并对已结束的任务再发完成信号（同上，调试版断言）。用户点了"取消"，实例里却可能多出半成品 OptiFine。

**建议**：加 `m_harvested` 一次性标志；`errorOccurred` 只处理 `FailedToStart`；`delete m_process` → `deleteLater()`；`abort()` 置位后 `harvest()` 直接返回。

---

### M4 OptiFine 安装：对已启动的加载任务再次 `start()`

**位置**：`modplatform/optifine/OptiFineInstallTask.cpp:229-231,265` + `java/JavaInstallList.cpp:54-76`

```cpp
// JavaInstallList.cpp
Task::Ptr JavaInstallList::getLoadTask(bool forceReload)
{
    Q_UNUSED(forceReload)
    load();                    // load() 内部已 m_load_task->start()
    return getCurrentTask();
}
void JavaInstallList::load()
{
    if (m_status != Status::InProgress) {
        m_status = Status::InProgress;
        m_load_task.reset(new JavaListLoadTask(this, m_only_managed_versions));
        m_load_task->start();          // 这里已经启动
    }
}
```
```cpp
// OptiFineInstallTask.cpp:230,265
m_current = javas->getLoadTask();      // 返回时已是 Running
connect(m_current.get(), &Task::finished, this, ...);
m_current->start();                    // 第二次启动
```

**为什么是缺陷**：`Task::start()` 在 `State::Running` 分支执行 `ASSERT_NEVER(isRunning())`（`tasks/Task.cpp:107-111`），即 **调试构建点"安装 OptiFine"直接断言 abort**，发布构建静默无效。同仓库 `AutoInstallJava::resolveJava()`（`:215-217`）的写法是对的（`if (!m_current_task->isRunning()) start()`），说明这里是漏改。

**建议**：删除 `m_current->start();`，或加同样的 `isRunning()` 守卫。

---

### M5 找不到 `JavaCheck.jar` 时 JavaChecker 不发终态 → 启动永久挂起

**位置**：`java/JavaChecker.cpp:81-88`、`java/LocalJavaScanTask.cpp:76-84`

```cpp
void JavaChecker::executeTask()
{
    QString checkerJar = JavaUtils::getJavaCheckPath();
    if (checkerJar.isEmpty()) {
        qDebug() << "Java checker library could not be found. Please check your installation.";
        return;                      // 既没有 emitFailed 也没有 emitSucceeded，killTimer 也没启动
    }
```

**为什么是缺陷**：`LocalJavaScanTask` 用 `ConcurrentTask` 包住每个 `JavaChecker`，而 `ConcurrentTask` 只有收到子任务的 succeeded/failed/aborted 才会结束（`ConcurrentTask.cpp:111-149`）。不终态的 checker 让 `scanFinished()` 永不执行 → `AutoInstallJava` 停在 "Looking for Java inside the instance" → `LaunchTask` 永远 Running，用户只能杀进程。

这是 upstream 遗留的提前 return，但本分支新增的"使用实例内 Java"把影响从"Java 列表加载卡住"升级为**启动卡死**，且没有任何超时兜底。

**建议**：该分支改 `emitFailed(tr("Java checker not found"));`；`LocalJavaScanTask` 增加总超时。

---

### M6 任意 `http://` 认证服务器都会收到明文密码（安全）

**位置**：`ui/dialogs/YggdrasilLoginDialog.cpp:125-129`、`minecraft/auth/steps/YggdrasilStep.cpp:66-67`

```cpp
if (!url.isValid() || url.host().isEmpty() || (scheme != QLatin1String("https") && scheme != QLatin1String("http"))) { ... }
...
body[QStringLiteral("username")] = data.loginName;
body[QStringLiteral("password")] = data.password;
```

校验只要求 scheme 是 http(s)，随后把用户名+密码 POST 过去。同网段攻击者/代理可直接截获。测试（`tests/YggdrasilAuth_test.cpp:210`）用 `http://127.0.0.1:8000` 说明本意只是本地开发服务器。

**建议**：仅 loopback/`localhost` 允许 http，其余强制 https；或对非本地 http 明确警告并要求确认。

---

### M7 Yggdrasil 账号按全局 `profileId` 判重，跨服务器同名账号互相覆盖

**位置**：`minecraft/auth/AccountList.cpp:124-141`、`:496-500`

```cpp
// override/replace existing account with the same profileId
auto profileId = account->profileId();
if (profileId.size()) {
    auto existingAccount = findAccountByProfileId(profileId);
    if (existingAccount != -1) {
        ...
        m_accounts[existingAccount] = account;      // 原地替换并断开旧账号信号
```

**为什么是缺陷**：MSA 的 profileId 是 Mojang 全局唯一的，用它判重没问题；但 Yggdrasil 的 UUID 由各认证服务器自行分配，很多服务器用"玩家名派生离线 UUID"，于是**同名玩家在两个第三方服务器上会得到同一个 UUID**：后添加的账号原地替换先前的，旧服务器的地址/登录名/密码随之丢失；`loadV3()` 则静默丢弃第二个；`InstanceAccountId` 也只按 profileId 查找，实例绑定会指错账号。

**建议**：MSA 用 `profileId`；Yggdrasil 用 `authServerUrl + profileId`（或 `+ loginName`）作身份键。

---

### M8 NetJob 重试不重置镜像序列

**位置**：`net/NetJob.cpp:71-82`、`net/Request.cpp:99-104,360-379`、`net/MirrorPolicy.h:89-96`

```cpp
// NetJob.cpp:73 — 失败请求重新入队（同一个 Request 对象）
m_failed.removeIf([this](QHash<Task*, Task::Ptr>::iterator task) {
    if (static_cast<Net::Request*>(task->get())->replyStatusCode() == 404) return false;
    m_done.remove(task->get());
    m_queue.enqueue(*task);
    return true;
});
```
```cpp
// MirrorPolicy.h:89 — index 只能单调递增，全仓库没有 rewind
auto advance() -> bool { if (m_index + 1 >= m_urls.size()) return false; ++m_index; return true; }
```

**为什么是缺陷**：`m_sources` 只在构造时生成一次，`tryNextSource()` 失败后 `m_index` 已停在最后一个候选；`Task::start()` 重跑 `executeTask()` 时用的是这个最后的 `m_url`，立即 `advance()` 返回 false。于是"重试 3 次"只会重打**最后一个**镜像/候选，`OfficialThenMirror` 下官方主机永久失去重试机会；Maven 主机有 4 个候选时同理。

**建议**：给 `SourceSequence` 加 `rewind()`，在"重新开始"的入口（覆盖 `Request::start()` 或用独立标志区分 `tryNextSource` 内部复用）复位到首个候选并同步 `m_url`。

---

### M9 空 `MirrorBaseUrl` 在两处含义相反

**位置**：`net/MirrorPolicy.cpp:103-105,141-143` vs `ui/pages/instance/VersionPage.cpp:451-453`

```cpp
// MirrorPolicy.cpp:103
auto base = validatedBase(isOwnService ? servicesMirrorBase : mirrorBase);
if (!base.isValid()) return {};            // 空 -> 没有镜像候选
...
if (mirrors.empty()) return { url };       // 静默回落官方

// VersionPage.cpp:451
QString mirrorBase = APPLICATION->settings()->get("MirrorBaseUrl").toString().trimmed();
if (mirrorBase.isEmpty())
    mirrorBase = QStringLiteral("https://bmclapi2.bangbang93.com");   // 空 -> 默认镜像
```

**为什么是缺陷**：用户清空输入框（以为回到默认）并选 "Mirror only" 时，网络层认为"没有镜像"→ 静默降级成"只走官方"，而同一次会话里 OptiFine / authlib-injector 的下载仍旧发往 BMCLAPI。行为不一致且无提示。

**建议**：把默认值收敛到一处（`Application` 暴露 helper，或 `MirrorPolicy::candidates()` 对空值套用同一默认），或明确"空=禁用"并删掉 VersionPage 的回填。

---

### M10 "有响应但没带 players"被当成离线

**位置**：`ui/pages/instance/ServerStatus.cpp:71-83`、`ServerStatus.h:18-26`、`ServersPage.cpp:340-368`

```cpp
struct ServerStatus {
    int online = -1;
    int max = -1;
    bool isOnline() const { return online >= 0; }
};
...
auto players = data.value("players").toObject();
if (!players.isEmpty()) {
    status.online = static_cast<int>(players.value("online").toDouble(-1));
    status.max = static_cast<int>(players.value("max").toDouble(-1));
}
```

**为什么是缺陷**：`players` 虽是惯例字段但并非必需（部分代理、隐藏玩家数配置会省略）。此时 ping 成功、版本/延迟/MOTD 都拿到了，但 `online` 保持 `-1` → `isOnline()` false → 第 2 列显示 `Offline`，而第 3、4 列同时显示版本和延迟，UI 自相矛盾。

**建议**：增加 `bool answered`（或让 `readServerStatus` 在无 players 时置 `online = 0`），`isOnline()` 改为判断"是否收到了完整响应"。

---

### M11 "测试"按钮把 `maxMem` 当 `minMem` 传

**位置**：`JavaCommon.cpp:126-136`、`JavaCommon.h:43`

```cpp
checker.reset(new JavaChecker(m_path, m_args, m_maxMem, m_maxMem, result.javaVersion.requiresPermGen() ? m_permGen : 0, 0));
//                                          ^^^^^^^^  minMem     ^^^^^^^^ maxMem
```

`TestCheck::m_minMem`（`JavaCommon.h:43`）从头到尾没有被读过（可 `grep m_minMem` 验证）。于是"测试"用 `-Xms<max> -Xmx<max>` 启动 JVM：在设置了大堆、可用内存不足的机器上 JVM 直接启动失败，弹 "The specified Java binary didn't work with the arguments you provided"，把用户真实可用的参数误判为坏参数。

**建议**：传 `m_minMem`。

---

### M12 本地 Java 扫描只下一层，找不到官方 runtime

**位置**：`java/LocalJavaScanTask.cpp:27-56`

```cpp
const QStringList runtimeRoots = {
    FS::PathCombine(instanceRoot, QStringLiteral("runtime")),
    FS::PathCombine(instanceRoot, QStringLiteral(".minecraft"), QStringLiteral("runtime")),
};
for (const auto& runtimeRoot : runtimeRoots) {
    ...
    appendIfPresent(root.absolutePath());                            // runtime/bin/java
    const auto entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& child : children) { appendIfPresent(...); }     // runtime/<one-level>/bin/java
}
```

**为什么是缺陷**：Mojang 官方启动器的布局是四层：`.minecraft/runtime/java-runtime-gamma/windows-x64/java-runtime-gamma/bin/javaw.exe`，而这里特意加了 `.minecraft/runtime` 却只扫一层，永远找不到。旁证：同仓库 `JavaUtils::getMinecraftJavaBundle()`（`JavaUtils.cpp:586-605`）用队列**递归**下探直到发现 `bin`，说明别处知道这棵树是多层的。本分支自己下载的 runtime 是一层（`<javaPath>/<name>/bin`），所以只覆盖了自家产物。

**建议**：与 `getMinecraftJavaBundle` 一样递归（限深度、跳过 `lib`），并让 `LocalJavaScan_test` 覆盖官方布局。

---

### M13 SRV 记录直接取第一条

**位置**：`ui/pages/instance/McResolver.cpp:32-42`

```cpp
auto records = lookup->serviceRecords();
if (records.isEmpty()) { pingWithDomainA(domain, port); return; }
const auto& firstRecord = records.at(0);      // 忽略 priority / weight
QString newDomain = firstRecord.target();
int newPort = firstRecord.port();
pingWithDomainA(newDomain, newPort);
```

**为什么是缺陷**：RFC 2782 要求先按 `priority` 升序、同优先级内按 `weight` 加权选择；`QDnsLookup` 返回的是 DNS 顺序，`at(0)` 可能是备份节点。另外 `target == "."` 表示"该服务不可用"，此时应回落到原域名或直接报错，而这里会把 `"."` 当主机名去做 A 查询，得到误导性的 "A record lookup failed"。

**建议**：按 priority/weight 排序选择；`target` 为 `"."` 时回退到原始域名。

---

### M14 不校验响应包长度

**位置**：`ui/pages/instance/McClient.cpp:116-141`

```cpp
m_resp.append(m_socket.readAll());
if (m_responseReadState == ResponseReadState::Waiting && m_resp.size() >= 5) {
    auto res = readVarInt(m_resp);
    ...
    m_wantedRespLength = *res;         // 没有范围检查
}
if (m_responseReadState == ResponseReadState::GotLength && m_resp.size() >= m_wantedRespLength) { ... }
```

**为什么是缺陷**：`readVarInt` 返回**有符号** `int`，`FF FF FF FF 0F` 会解成 `-1`，于是 `size() >= -1` 立即成立、`parseResponse()` 在空/半截缓冲区上跑，报一个与真实原因无关的解析错误；反过来，一个巨大的长度会让 `m_resp` 无上限增长（叠加 H4 的无超时，可造成内存耗尽）。

**建议**：拒绝 `length < 0` 或 `> 上限`（例如 2 MiB），并把 `readVarInt` 改成无符号语义。

---

## 4. 低危与健壮性问题

| # | 位置 | 说明 | 建议 |
| --- | --- | --- | --- |
| L1 | `ui/widgets/JavaSettingsWidget.cpp:386-398` | `maxMem >= sysMiB` / `>90%` 两个分支排在 `maxMem < minMem` 前面，导致"最大值小于最小值"这条提示在最需要时看不到；`loadSettings()`（`:172-178`）还会**静默对调** min/max 并在下次保存时写回。 | 把一致性检查提到最前；对调时给可见提示。 |
| L2 | `HardwareInfo.cpp:323-339` | `sysctl hw.physmem` 返回的是**字节**，代码按 KiB 处理（`out = mem / 1024`），结果 `totalRamMiB()` 大 1024 倍。下游 `MemoryAdvice` 的 3/4 上限与"留 2 GiB"上限因此全部失效（4 GB 机器 + 150 模组会建议 8192 MiB）。 | `/1024/1024`；顺手把 `:239` 的 `toUInt` 换成 `toULongLong`（>4 TiB 溢出）。疑似上游遗留，但被新代码放大。 |
| L3 | `minecraft/auth/Parsers.cpp:445-447` | 注释写"按已签发一天处理"，代码却把 `issueInstant` 设为**现在**，于是 `shouldRefresh()` 认为还有 24 h，12 h 内永不刷新 Yggdrasil token。 | 两者取其一并改一致。 |
| L4 | `minecraft/auth/AccountList.cpp:314`、`java/JavaInstallList.cpp:98` | `if (index.row() > count())` 应为 `>=`；`row == count()` 时 `at(count())`/`m_vlist[count()]` 越界（QList 断言 / release UB）。 | 改 `>=`（upstream 遗留）。 |
| L5 | `net/Request.h:156` | `setUrl()` 只改 `m_url`，不重建 `m_sources`/`m_originalUrl`，破坏头文件写明的"m_url 永远是候选之一"；`SkinManageDialog` 的 `makeFile(QUrl(), path)` + `setUrl()` 就落在这个状态。 | `setUrl()` 内重建序列，或删掉该接口。 |
| L6 | `net/Request.cpp:475-477` | `m_errorResponse` 跨源只 append 不清空，最终报错展示的是**别的**镜像的响应体。 | 每次新尝试/切换源时 `clear()`。 |
| L7 | `net/MirrorPolicy.h:85-87` | 空序列时 `current()` 用 `at()` 抛 `std::out_of_range`，`remaining()` 的 `size_t` 减法下溢成 `SIZE_MAX`。目前有 `isEmpty()` 保护，但接口本身不安全。 | 返回 `std::optional<QUrl>`；`remaining()` 空时返回 0。 |
| L8 | `java/JavaChecker.cpp:162` | `if (status == CrashExit \|\| exitcode == 1)`：退出码 2/3… 会被当作正常输出解析；若失败前已打印三个属性，还会被标成 `Valid` 并被自动选中。 | 改 `exitcode != 0`。 |
| L9 | `modplatform/optifine/OptiFineInstallTask.cpp:195-208` | client jar 与 OptiFine jar 下载均未校验（版本 JSON 里本来有 sha1/size；authlib-injector 那边就校验了 sha256）；`filename` 直接 `FS::PathCombine(m_scratchRoot, filename)`，镜像返回 `../../x.jar` 会写出 scratch。 | 加 `ChecksumValidator`；只取 `QFileInfo(filename).fileName()`。 |
| L10 | `java/LocalJavaScanTask.cpp:58-62` | 未 `setAbortable(true)`（启动阶段无法取消，见 H4 同类问题）；`executeTask()` 不清 `m_results/m_installs`，对象被复用会残留旧条目（`:108` 只清前者）。 | 加 `setAbortable` 与 `clear()`。 |
| L11 | `ui/widgets/MirrorListEditor.cpp:88-99` | 只有 "Add" 按钮走 `isUsablePrefix()`；从配置文件读入的条目（`setEntries`）与保存（`APIPage::applySettings`）不做校验，运行时又被 `validatedBase()` 静默跳过——UI 显示启用、实际从不生效。 | 读入时标记无效项，或在保存时统一过滤并提示。 |
| L12 | `minecraft/auth/steps/YggdrasilStep.cpp:49-53` | 字符串直接拼 `base + "/authserver/" + operation`：地址带 `?x=1` 时 endpoint 会落进 query（`#frag` 时落进 fragment），登录 404，而错误提示还会误导用户"地址多了 authserver"。 | 用 `QUrl` 解析后清空 query/fragment，基于 `path()` 追加。 |
| L13 | `ui/widgets/JavaSettingsWidget.cpp:329-344` | `modCount()` 只数 `mods/`、`coremods/` 顶层的 `*.jar/*.zip`，且仅在 `loadSettings()` 调一次；装完模组后建议值不更新。 | 用 `ModFolderModel` 计数，或在页面显示时重算。 |

其它小问题：

- `minecraft/auth/steps/YggdrasilStep.cpp:28-32`：refresh 时若 `extra["clientToken"]` 缺失会**现场生成新 token**，严格的服务器会返回 403 → 账号被判 `Expired`。缺失时应直接走完整登录。
- `minecraft/launch/EnsureAvailableMemory.cpp:28-48`：macOS 分支 `switch` 无 `default`，未知压力等级会弹出**空白**对话框（`LowMemWarning` 打开时）。
- `ui/pages/instance/ServersPage.cpp:804-808`：`m_model->at(currentServer)->m_address` 无空指针检查（目前靠按钮禁用兜住）。
- `ui/pages/instance/ServersPage.cpp:507-512`：`m_watcher` 只监听目录，`fileChanged()` 是空实现，`servers.dat` 的变更靠目录 mtime 间接感知。
- `modplatform/optifine/OptiFineInstallTask.cpp:219-226`：选"版本号最大的 ≥8 Java"来跑安装器；1.12 及更早的 OptiFine 安装器是按 Java 8 写的，用 Java 21 跑可能失败。
- `ui/dialogs/YggdrasilLoginDialog.cpp:142-168`：多角色账号会用同一份密码**连续登录两次**（先空白选择、再按角色重登），第一次的 token 被丢弃，服务器端可能被限流。

---

## 5. 需运行验证 / 未证实

1. **`Request::downloadFinished()` 在自己的 `finished` 槽里 `m_reply.reset()`**（`Request.cpp:423` → `:201`）：在 QNetworkReply 发 `finished` 的过程中 delete 它。上游 redirect 路径（`:355`）也是同样写法且长期可用，属既有风险，不建议单独改动——除非能复现崩溃。
2. **`MetaCacheSink::initCache()` 的 `CacheHit` 与镜像回退的相互作用**：条目"未过期"时 `init()` 直接返回 `CacheHit`，`executeTask()` 会 `emitSucceeded()` 而**不下载**。若某条目在一次失败尝试后被标成未过期，回退重试会"假成功"。需要用"缓存条目存在但文件被截断"的用例验证。
3. **`MinecraftAccount::authFailed()` 直接解引用 `m_currentTask`**（`MinecraftAccount.cpp:168`）：正常路径下信号只来自活的任务，但 `login()` 在 `Q_ASSERT` 之后无条件 `reset()` 旧任务（`:129-131`），release 下并发登录可能把旧任务的失败打到新任务上。需 UI 复现路径。
4. **`CrashAnalysis` 的 `UnsupportedClassVersionError` 分支**（`CrashAnalysis.cpp:112-126`）：老式 `Unsupported major.minor version 52.0` 走 `std::swap(wanted, present)`；若日志是"现代格式但缺 `versions up to`"的截断消息，会让"需要的 Java"和"在用的 Java"说反。建议用真实日志核对。
5. **`Request` 的镜像改写无条件套用到 POST/PUT/DELETE**（`Request.cpp:99-104`）：现有调用点里没有发往白名单主机（Mojang/Maven）的带凭据写请求，故暂无泄露路径；但这是设计上的隐患。
6. **`JavaChecker::finished()` 里 `m_process.reset()`**（`:150`）同样是在进程自己的 `finished` 槽中删除 sender；Qt 常见实现下可用，但与 M3 属同类风险。

---

## 6. 经复核排除的误报（避免重复排查）

以下几处初看像缺陷，逐行核对后**不是**问题，记录在此以免后续重复投入：

1. **`JavaInstallList::getLoadTask()` 可能返回 nullptr → `m_current->start()` 空指针崩溃**：不成立。`load()` 在 `m_status != InProgress` 时总会新建并启动任务，随后 `getCurrentTask()` 因 `InProgress` 返回非空；`updateListData()` 是先把 `m_status` 置 `Done` 再 `reset()`，不存在"InProgress 但任务为空"的窗口。真正的问题只是重复 `start()`（见 M4）。
2. **authlib-injector 的 jar 落在"进程 CWD"而不是实例目录，属于 bug**：不成立。`Application.cpp:445` 启动时 `QDir::setCurrent(dataPath)`，`Library::storagePrefix()` 的 `libraries/` 与 `HttpMetaCache` 的 `addBase("libraries", QDir("libraries").absolutePath())`（`Application.cpp:1060`）都解析到**启动器数据目录下的 `libraries/`**，与普通库的下载位置一致，安装与启动两次解析也一致。代价只是实例不自包含（与上游普通库相同）。
3. **`shared_qobject_ptr::reset()` 在信号回调里释放发送者造成 use-after-free**：不成立。`QObjectPtr.h:24` 用 `QObject::deleteLater` 作为 deleter（`LocalJavaScanTask`、`AutoInstallJava`、`JavaInstallList` 的 `reset()` 都是延后删除）。仍需注意这些对象可能活过当前事件循环迭代。

---

## 7. 测试覆盖缺口

仓库为新增功能配了不少扎实的单测（`MemoryAdvice`、`JavaSelection`、`MirrorPolicy`、`MirrorFallback`、`OptiFineList`、`AuthlibInjector`、`YggdrasilAuth`、`AccountUsage`、`ServerStatus`、`CrashAnalysis`、`LocalJavaScan`），本报告很多结论正是靠"代码 vs 用例预期"互相印证得出的。但**缺陷正集中在没有测试的文件上**：

| 无测试的文件 | 相关缺陷 |
| --- | --- |
| `OptiFineInstallTask` | M3、M4、L9 |
| `AuthlibInjectorInstallTask` | M1 |
| `ServersPage` / `ServerPingTask` / `McClient` / `McResolver` | H2、H3、H4、M13、M14 |
| `JavaSettingsWidget` / `JavaCommon` | L1、M11 |
| `IconList` | M2 |
| `AccountData::saveState()` | H1 |
| `LocalJavaScanTask` 的排序与 `<root>/runtime` 布局 | H5、M12 |

建议优先补的用例：

1. `McClient` 编码：`writeVarInt(763) == {0xFB,0x05}`、`writeString(128 字节) == {0x80,0x01,...}`。
2. `ServersPage`：3 台服务器 + 查询未完成时 `addEmptyRow(1)`，断言状态回填到正确的服务器（把 ping 抽象成可注入的假任务）。
3. `OptiFineInstallTask` / `AuthlibInjectorInstallTask`：把进程/网络替换成假实现，断言 `harvest()` 只跑一次、abort 后不再 emit 成功、组件文件路径唯一。
4. `LocalJavaScanTask`：混入 `x86` + `amd64` 同一 major，断言选中 64 位；再覆盖 Mojang 四层 runtime 布局。
5. `AccountData`：`saveState()` 的 key 白名单断言（防止再次把密码写进去）。

---

## 8. 修复优先级建议

| 顺序 | 内容 | 理由 |
| --- | --- | --- |
| 1 | H1（明文密码）、M6（明文 http 传密码） | 直接威胁用户凭据，且泄露不可逆 |
| 2 | H3（VarInt）、H4（ping 无超时/不可取消）、M1（authlib 并发装错服务器） | 协议错误、界面/线程卡死、装错认证地址 |
| 3 | H2（状态错位）、H5（选错 Java）、M3/M4（OptiFine 安装断言与重复收割）、M5（启动挂起） | 主要功能错误或调试版崩溃 |
| 4 | M7–M14、L1–L13、§4 的小项 | 数据正确性、可维护性 |
| 5 | §7 的测试补齐 | 把"行号/固定路径/密码/位掩码"这四类问题用用例钉住 |

> 说明：本报告为静态审查结果，未链接或运行任何测试。修复前建议先在**调试构建**下复现 M3/M4（预期触发 `Task` 的 `ASSERT_NEVER`），再在发布构建下验证协议与状态页行为。

---

## 9. 修复记录（2026-10-25，按 §8 顺序执行，§7 未做）

> **未编译、未运行测试**：本机 shell 仍不可用（`pwsh` → `0xC0000142`），下面每一次改动都是逐行复核过的静态修改，但**必须**先跑一次构建与 `ctest` 才能算完成。
>
> **2026-10-25 补充**：上述环境限制已解除，构建与测试已实际执行，结果见 §10。其中 §10.1 记录了一处只有编译器才能发现的遗漏（`ServerPingTask.h` 缺 `executeTask()` 声明），已修复。

### 9.1 第一优先级：凭据

| 编号 | 改动 | 文件 |
| --- | --- | --- |
| H1 | `saveState()` 不再写 `password`；`resumeStateFromV3()` 不再读该字段；`loadV3()` 检测到旧文件里还有 `password` 时立即重写账号文件（清洗）；`password` 成员加注释说明"仅内存"；`LaunchController::reauthenticateAccount()` 在密码为空时用 `QInputDialog` 询问一次再登录。 | `minecraft/auth/AccountData.cpp`、`AccountData.h`、`AccountList.cpp`、`LaunchController.cpp` |
| H1（测试） | `test_AccountRoundTrip` 由"密码能往返"改为"密码不被持久化"。这是行为约定变更，不改测试就无法与修复共存。 | `tests/YggdrasilAuth_test.cpp` |
| M6 | `YggdrasilLoginDialog::serverProblem()`：非回环主机禁止 `http://`（仅 `localhost`/`127.0.0.1`/`::1`/`*.localhost` 允许），其余要求 https。 | `ui/dialogs/YggdrasilLoginDialog.cpp` |

### 9.2 第二优先级：协议、超时、临时文件

| 编号 | 改动 | 文件 |
| --- | --- | --- |
| H3 | `writeVarInt()` 改用 `value & 0x7F`；`writeString()` 的长度前缀改用 UTF-8 字节数。 | `ui/pages/instance/McClient.cpp` |
| H4 | `ServerPingTask` 重写：`setAbortable(true)`、`abort()` 实现、15 s 总超时；`McClient`/`McResolver` 以 `ServerPingTask` 为父对象，取消时随任务析构并关闭 socket。 | `ServerPingTask.h/.cpp` |
| M14 | `McClient` 校验响应长度（负数/超过 2 MiB 直接失败）；新增 `m_finished` 防止重复上报与继续读包。 | `McClient.cpp`、`McClient.h` |
| M1 | authlib-injector 的组件文件改用 `QTemporaryDir`，不再用共享临时目录里的固定文件名。 | `AuthlibInjectorInstallTask.cpp` |

### 9.3 第三优先级：状态页、Java 选择、OptiFine 安装、启动挂起

| 编号 | 改动 | 文件 |
| --- | --- | --- |
| H2 | `Server` 增加 `m_id`；模型增加 `m_nextId` 与 `m_queryGeneration`；ping 回填改为"代数 + 身份"双重校验，`load()` 与每次刷新都会推进代数，过期结果直接丢弃。 | `ui/pages/instance/ServersPage.cpp` |
| H5 | 本地 Java 排序改为"主版本降序 → 64 位优先 → 路径"，不再复用 `JavaInstall::operator>`（它以 arch 为首关键字）。 | `java/LocalJavaScanTask.cpp` |
| M3 | `harvest()` 加 `m_harvested` 一次性标志与 `m_aborted` 检查；`m_process` 改为 `deleteLater()`；`abort()` 先置 `m_aborted`。 | `OptiFineInstallTask.cpp/.h` |
| M4 | 删除 `resolveJava()` 里对已在运行的加载任务的第二次 `start()`，并补空指针检查。 | `OptiFineInstallTask.cpp` |
| M5 | `JavaChecker` 找不到 `JavaCheck.jar` 时发出 `Errored` 结果并 `emitFailed()`，不再"既不成功也不失败"。 | `java/JavaChecker.cpp` |
| M5（配套） | 反过来的风险一并处理：`LocalJavaScanTask::abort()` 先置自身为已取消再取消子任务；`scanFinished()` 在已取消时直接返回；`JavaChecker` 新增 `canAbort()/abort()`；`JavaListLoadTask::javaCheckerFinished()` 加 `isRunning()` 守卫。 | `LocalJavaScanTask.cpp/.h`、`JavaChecker.cpp/.h`、`JavaInstallList.cpp` |

### 9.4 第四优先级：其余中危与低危

| 编号 | 改动 | 文件 |
| --- | --- | --- |
| M2 | 图标 key 统一为"相对路径去扩展名"（含子目录）；`sortIconList()` 改用 `/` 判断；`getIconIndex()` 对旧格式（仅文件名）保留唯一匹配回退。 | `icons/IconList.cpp` |
| M7 | 账号身份改为 `(类型, profileId, Yggdrasil 认证服务器)`：不同服务器的同名/同 UUID 账号不再互相覆盖。 | `minecraft/auth/AccountList.cpp` |
| M8 | `SourceSequence` 增加 `rewind()`；`Request::start()` 在重试时复位候选序列、重定向/重试计数与错误体。 | `net/MirrorPolicy.h`、`net/Request.cpp/.h` |
| M9 | 统一为"空地址 = 没有镜像"（`tests/MirrorPolicy_test.cpp` 明确约定该语义）：`VersionPage` 不再偷偷回填 BMCLAPI；OptiFine 列表任务与 authlib-injector 安装任务在没有镜像地址时给出明确提示。 | `VersionPage.cpp`、`OptiFineListTask.cpp`、`AuthlibInjectorInstallTask.cpp` |
| M10 | `ServerStatus` 增加 `answered`；`readServerStatus()` 置位；状态页在"有回应但没有 players 块"时显示 `?` 而不是 `Offline`。`isOnline()` 语义保持不变（现有测试依赖它）。 | `ServerStatus.h/.cpp`、`ServersPage.cpp` |
| M11 | "测试"按钮把 `minMem` 传给 `JavaChecker`（原先两次都传 `maxMem`）。 | `JavaCommon.cpp` |
| M12 | 本地 runtime 扫描改为有界递归（深度 4），可找到官方启动器的 `<component>/<platform>/<component>/bin` 布局。 | `java/LocalJavaScanTask.cpp/.h` |
| M13 | SRV 记录按 priority 升序、weight 降序选择；`target == "."` 时回落到原域名。 | `ui/pages/instance/McResolver.cpp` |
| L1 | 内存提示把"最大值小于最小值"提到最前判断。 | `ui/widgets/JavaSettingsWidget.cpp` |
| L2 | FreeBSD/OpenBSD 的 `hw.physmem` 按字节换算 MiB；Linux `/proc/meminfo` 改用 `toULongLong`。 | `HardwareInfo.cpp` |
| L3 | Yggdrasil token 的 `issueInstant` 改为"一天前"，与注释一致（下次启动会续期一次）。 | `minecraft/auth/Parsers.cpp` |
| L4 | `AccountList::data()`、`JavaInstallList::data()` 的 `> count()` 改为 `>= count()` 并检查负数行。 | 两个文件 |
| L5 | `Request::setUrl()` 同步重建 `m_originalUrl` 与候选序列。 | `net/Request.h` |
| L6 | 切换镜像源时清空 `m_errorResponse`。 | `net/Request.cpp` |
| L7 | `SourceSequence::current()/remaining()` 在空序列上返回空值/0，不再抛异常或下溢。 | `net/MirrorPolicy.h` |
| L8 | `JavaChecker` 把任何非零退出码都当作 `Errored`。 | `java/JavaChecker.cpp` |
| L9 | OptiFine 下载：Minecraft client jar 校验 sha1；临时文件名只取 `fileName()`，不再接受镜像给的路径。 | `OptiFineInstallTask.cpp/.h` |
| L10 | `LocalJavaScanTask` 增加 `setAbortable()`、重启时清空结果。 | `java/LocalJavaScanTask.cpp/.h` |
| L11 | 镜像列表编辑器对"读入但不可用"的条目标红并加提示。 | `ui/widgets/MirrorListEditor.cpp` |
| L12 | `YggdrasilStep::endpoint()` 改用 `QUrl` 拼接，先清空 query/fragment。 | `minecraft/auth/steps/YggdrasilStep.cpp` |
| L13 | 内存建议在页面 `showEvent` 时重算。 | `ui/widgets/JavaSettingsWidget.cpp/.h` |
| §4 | Yggdrasil refresh 不再凭空生成 `clientToken`（缺失时省略，只有交互式登录才新建）；macOS 内存压力未知等级不再弹空白框；"加入服务器"补空指针检查。 | `YggdrasilStep.cpp`、`EnsureAvailableMemory.cpp`、`ServersPage.cpp` |

### 9.5 有意未做

| 项 | 原因 |
| --- | --- |
| §5 的 6 条"未证实" | 需要运行时证据，不能在静态审查阶段改动。 |
| §7 的测试补齐 | 按要求跳过。只改了一个因行为变更而失效的既有断言（H1）。 |
| `JavaSettingsWidget::loadSettings()` 的 min/max 静默对调 | 只在配置损坏时触发，且已被 L1 的可见提示覆盖；改动会影响正常路径。 |
| OptiFine 安装器选用 Java 8 优先 | 需要产品决策（不同 OptiFine 世代要求不同 Java），不宜由审查者单方面改变行为。 |
| 多角色账号的二次登录 | 需要改交互流程（先选角色再登录），属于功能改造。 |
| `servers.dat` 的单文件监听 | 改监听对象会影响 `dirChanged` 的语义，风险大于收益。 |

### 9.6 行为变化（需要告知用户 / 写进更新说明）

1. **第三方账号密码不再保存**：已经保存过的会被自动清除；token 失效时会弹一次密码框。
2. **Yggdrasil token 每次启动续期一次**：换来的是"不会拿着过期 token 进游戏"。
3. **清空镜像地址 = 关闭镜像**：OptiFine 安装与 authlib-injector 安装会提示需要先填地址，而不再静默使用 BMCLAPI。
4. **非回环 `http://` 认证服务器被拒**：同机测试服务器仍可用 `http://127.0.0.1`。
5. **服务端 ping 超时 15 秒**，且可以取消；玩家数列在"服务器有回应但未给出人数"时显示 `?`。
6. **图标按"子目录/文件名"索引**：旧版本保存的根目录图标关联不受影响，子目录图标有唯一匹配回退。

### 9.7 建议的验证步骤

```powershell
cmake --build build            # 先确认能编过（本环境无法执行）
ctest --test-dir build -R "MemoryAdvice|JavaSelection|MirrorPolicy|MirrorFallback|YggdrasilAuth|AccountUsage|ServerStatus|OptiFineList|AuthlibInjector|LocalJavaScan|CrashAnalysis" --output-on-failure
```

再手动过一遍：服务器页（点刷新两次、ping 期间增删服务器）、安装 OptiFine（Debug 构建）、添加第三方账号（密码不再落盘：查看 `accounts.json`）、把镜像地址清空后安装 OptiFine（应给出提示）。

---

## 10. 构建与测试验证（2026-10-25）

前几节"未编译"的限制已解除：本次在**原有 `build/` 缓存**上实际编译并运行了全部单测。

| 项 | 值 |
| --- | --- |
| 编译器 | MSVC 14.44.35207（VS 2022 BuildTools，`vcvars64`） |
| Qt | 6.11.2 `msvc2022_64` |
| 生成器 | CMake + Ninja Multi-Config（`build/`，vcpkg 依赖已装） |
| 编译选项 | 启动器目标带 `/W4 /WX`（警告即错误） |
| 结果 | Debug 135/135、Release 611/611 编译通过；`ctest` 33/34 通过（Debug 与 Release 一致） |

### 10.1 编译期发现的缺陷（§9 遗漏）

`ServerPingTask.h` 在重写时只留下了 `canAbort()` / `abort()`，**漏掉了 `executeTask()` 的覆盖声明**（对应的定义在 `ServerPingTask.cpp:18` 是完整的）。首次 `cmake --build build --config Debug` 因此失败：

```
ServerPingTask.cpp(18): error C2509: "executeTask": 成员函数没有在"ServerPingTask"中声明
ServersPage.cpp(516): error C2259: "ServerPingTask": 无法实例化抽象类（Task::executeTask 是纯虚）
```

修复：在 `ServerPingTask.h` 的 `protected:` 段补 `void executeTask() override;`。之后 Debug 与 Release 全量构建通过，`/WX` 下无警告。

这是静态审查的固有盲区——§9 的其余改动虽然逐行复核过，但"头文件声明与定义不匹配"只有编译器会指出来，因此 §9 这个"未编译"的前置条件当时确实成立。

### 10.2 单元测试结果

`ctest --test-dir build -C Debug|Release --output-on-failure`：**33/34 通过**，Debug 与 Release 结果一致。

§9.7 点名要跑的一组全部通过：

| 用例 | 覆盖的修复 |
| --- | --- |
| `MemoryAdvice` | 内存建议（L2 的下游） |
| `JavaSelection`、`LocalJavaScan` | H5 排序、M12 四层 runtime 布局 |
| `MirrorPolicy`、`MirrorFallback` | M8 重试复位、M9 空镜像语义、L5/L6/L7 |
| `YggdrasilAuth` | H1（密码不落盘）、M7 账号身份、L12 endpoint 拼接 |
| `AccountUsage` | M10 之外的账号/实例绑定 |
| `ServerStatus` | M10 `answered` 语义（`isOnline()` 保持兼容） |
| `OptiFineList`、`AuthlibInjector` | M9 空镜像提示、M1 临时目录 |
| `CrashAnalysis` | §5 第 4 条的既有行为 |

其余 22 个既有用例（`FileSystem` 除外）同样通过。

**唯一失败：`FileSystem`**（6 个子用例，全部是符号链接）。失败原因与代码无关：

```
Failed to link files: "A required privilege is not held by the client."
Error code: 1314
```

本机未启用开发者模式，`SeCreateSymbolicLinkPrivilege` 为 `Disabled`；用例自带的 `runas` 提权在非交互会话下同样返回 1314。`test_path_depth`、`test_path_trunc` 等非链接用例均通过。

### 10.3 本机跑测试的三个坑（不在本分支代码里，但会让结果失真）

1. **DLL 搜索路径**：直接用 `ctest` 会让 34 个用例全部 `0xC0000135`（DLL 未找到）。需要把 `D:\Qt\6.11.2\msvc2022_64\bin` 放进 `PATH`（CMake 没有为测试设置 `ENVIRONMENT`）。
2. **`%TEMP%` 不可用**：本机默认临时目录下 `QTemporaryDir::isValid()` 返回 false，导致 `LocalJavaScan`（4 例）、`MirrorFallback::test_FileUrlIsFetchable`、`ResourceFolderModel`（3 例）**假失败**。`ResourceFolderModel` 还会因为 `tmp.path()` 为空而退化成扫描当前工作目录，报出 `model.size() 47 != 1` 这种莫名其妙的断言。把 `TMP`/`TEMP` 指向工作区内目录（如 `build/test-tmp`）后全部通过。
3. **上次运行的残留物**：`build/tests/` 里遗留的 `test_folder`、`pack.mcmeta`（目录）、`supercoolmod.jar`、`test_SaveLoad*.ini` 会污染 `FileSystem` 与 `ResourceFolderModel`——`pack.mcmeta` 那个目录会让符号链接用例报 183（已存在）。跑之前应先清理。

### 10.4 结论

- §9 的修复**能编译、不破坏既有单测**，这一条现在有证据；此前的"未编译"风险已关闭。
- §7 的测试仍未补，因此 H2（按身份回填）、H4（超时/取消）、M3/M4（`ASSERT_NEVER`）、M5（缺 jar 不终态）、M13/M14 这些**只靠手测**的路径依然没有自动覆盖——Debug 构建下真实触发一次 `Task` 断言，仍是 §9.7 手工清单里的第一项。
- 建议把 10.3 的三条写进 CI 或测试文档，否则同样的假失败会反复出现。

