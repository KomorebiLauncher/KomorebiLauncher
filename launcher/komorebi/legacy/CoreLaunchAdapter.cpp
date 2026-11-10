// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include <QHash>
#include <QPointer>

#include <QJsonDocument>
#include <QJsonObject>

#include <QFileInfo>

#include "Application.h"
#include "Commandline.h"
#include "QObjectPtr.h"
#include "launch/LogModel.h"
#include "minecraft/CrashAnalysis.h"
#include "logs/LogParser.h"
#include "BaseInstance.h"

#include "komorebi/services/auth/MicrosoftAuth.h"
#include "komorebi/services/auth/TokenVault.h"
#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/launch/InstanceLaunch.h"

namespace Komorebi::Legacy {

namespace {

using StartedCallback = std::function<void(const QString& logPath)>;
using FailedCallback = std::function<void(const QString& error)>;
using FinishedCallback = std::function<void(int exitCode)>;
using AbortedCallback = std::function<void()>;
using StatusCallback = std::function<void(const QString& status)>;

/// The JVM arguments the launcher always adds, plus the ones the instance names.
///
/// This deliberately avoids MinecraftInstance::javaArguments(): that one needs the instance's components
/// to have been resolved, and a freshly created instance has not been through that. Everything here comes
/// from settings, which is what the launch is allowed to depend on.
QStringList jvmArgumentsFor(MinecraftInstance* instance)
{
    QStringList arguments{ QStringLiteral("-Duser.language=en") };
    arguments += Commandline::splitArgs(instance->settings()->get("JvmArgs").toString());

    const auto nativeLibrary = [instance](const QString& useSetting, const QString& customSetting, const QString& detected) {
        if (!instance->settings()->get(useSetting).toBool())
            return QString();
        const QString custom = instance->settings()->get(customSetting).toString();
        const QString path = custom.isEmpty() ? detected : custom;
        return QFileInfo(path).isFile() ? QFileInfo(path).absoluteFilePath() : QString();
    };
    const QString openal = nativeLibrary(QStringLiteral("UseNativeOpenAL"), QStringLiteral("CustomOpenALPath"),
                                         APPLICATION->m_detectedOpenALPath);
    const QString glfw = nativeLibrary(QStringLiteral("UseNativeGLFW"), QStringLiteral("CustomGLFWPath"),
                                       APPLICATION->m_detectedGLFWPath);
    const QString sdl = nativeLibrary(QStringLiteral("UseNativeSDL"), QStringLiteral("CustomSDLPath"),
                                      APPLICATION->m_detectedSDLPath);
    if (!openal.isEmpty())
        arguments.append(QStringLiteral("-Dorg.lwjgl.openal.libname=") + openal);
    if (!glfw.isEmpty())
        arguments.append(QStringLiteral("-Dorg.lwjgl.glfw.libname=") + glfw);
    if (!sdl.isEmpty())
        arguments.append(QStringLiteral("-Dorg.lwjgl.sdl.libname=") + sdl);

    return arguments;
}

/// The launches this run started, so the launcher can stop one without owning its process.
QHash<QString, QPointer<Services::InstanceLaunch>> s_launches;

/// What the game is told about the account: one shape from two sources.
Services::LaunchAccount accountFromPrism(const MinecraftAccountPtr& account)
{
    Services::LaunchAccount who;
    who.username = account->profileName();
    who.uuid = account->profileId();
    who.accessToken = account->accessToken();
    return who;
}

Services::LaunchAccount accountFromCore(const Services::AuthSession& session)
{
    Services::LaunchAccount who;
    who.username = session.username;
    who.uuid = session.uuid;
    who.accessToken = session.accessToken;
    return who;
}

/// Everything a launch does once the account question is answered.
bool beginLaunch(MinecraftInstance* instance, const Services::LaunchAccount& who,
                 const StartedCallback& onStarted, const FailedCallback& onFailed, const FinishedCallback& onFinished,
                 const AbortedCallback& onAborted, const StatusCallback& onStatus)
{
    Services::InstanceLaunchOptions options;
    options.dataRoot = APPLICATION->dataRoot();
    options.instanceDirectory = instance->instanceRoot();
    options.minMemoryMb = instance->settings()->get("MinMemAlloc").toInt();
    options.maxMemoryMb = instance->settings()->get("MaxMemAlloc").toInt();
    options.source = Services::downloadSourceFromId(APPLICATION->settings()->get("DownloadSource").toString());

    /*
     * The instance's own settings. These are what the old path read too: the Java it names, the JVM
     * arguments (which carry the agents a loader or an auth server adds), environment overrides, and a
     * server or world to join once the game starts.
     */
    options.extraJvmArguments = jvmArgumentsFor(instance);

    if (instance->settings()->get("OverrideJavaLocation").toBool()) {
        const QString javaPath = instance->settings()->get("JavaPath").toString();
        if (!javaPath.isEmpty())
            options.javaPathOverride = javaPath;
    }

    const auto envDocument = QJsonDocument::fromJson(instance->settings()->get("Env").toString().toUtf8());
    if (envDocument.isObject()) {
        const auto envObject = envDocument.object();
        for (auto it = envObject.begin(); it != envObject.end(); ++it)
            options.environment.insert(it.key(), it.value().toString());
    }

    if (instance->settings()->get("JoinServerOnLaunch").toBool()) {
        const QString address = instance->settings()->get("JoinServerOnLaunchAddress").toString().trimmed();
        if (!address.isEmpty()) {
            // "host" or "host:port"; a bare host joins the default port.
            const int colon = address.lastIndexOf(QLatin1Char(':'));
            bool portOk = false;
            const int port = colon > 0 ? address.mid(colon + 1).toInt(&portOk) : 0;
            options.serverAddress = portOk ? address.left(colon) : address;
            options.serverPort = portOk ? port : 25565;
        } else {
            const QString world = instance->settings()->get("JoinWorldOnLaunch").toString().trimmed();
            if (!world.isEmpty())
                options.worldToJoin = world;
        }
    }

    auto* task = new Services::InstanceLaunch(APPLICATION->network(), APPLICATION);
    const QString id = instance->id();
    s_launches.insert(id, task);
    const auto forget = [id] { s_launches.remove(id); };

    auto model = makeShared<LogModel>();
    model->setMaxLines(getConsoleMaxLines(instance->settings()));
    model->setStopOnOverflow(shouldStopOnConsoleOverflow(instance->settings()));
    instance->setCoreLogModel(model);

    // The launcher's own message about the access token and profile must not end up in a log the
    // player can copy out; the game prints them in some of its messages.
    QMap<QString, QString> censor;
    if (!who.accessToken.isEmpty() && who.accessToken != QLatin1String("0"))
        censor.insert(who.accessToken, QStringLiteral("<ACCESS TOKEN>"));
    if (!who.uuid.isEmpty())
        censor.insert(who.uuid, QStringLiteral("<PROFILE ID>"));

    QObject::connect(task, &Services::InstanceLaunch::status, APPLICATION, [model, onStatus](const QString& text) {
        // A launch that appears to do nothing is a launch whose last step can be read in the log.
        qDebug().noquote() << "core launch:" << text;
        model->append(MessageLevel::Launcher, text);
        if (onStatus)
            onStatus(text);
    });
    // CloseAfterLaunch hides the launcher once the game is really running and brings it back when the
    // game ends; the old path watched for the same line in the output.
    const bool closeAfterLaunch = instance->settings()->get("CloseAfterLaunch").toBool();
    auto windowsClosed = std::make_shared<bool>(false);

    QObject::connect(task, &Services::InstanceLaunch::output, APPLICATION,
                     [model, censor, closeAfterLaunch, windowsClosed](QString line) {
                         for (auto it = censor.begin(); it != censor.end(); ++it)
                             line.replace(it.key(), it.value());

                         if (closeAfterLaunch && !*windowsClosed && line.contains(QStringLiteral("Setting user"))) {
                             *windowsClosed = true;
                             APPLICATION->closeAllWindows();
                         }
                         model->append(LogParser::guessLevel(line, model->previousLevel()), line);
                     });
    QObject::connect(task, &Services::InstanceLaunch::failed, APPLICATION, [model](const QString& reason) {
        model->append(MessageLevel::Error, reason);
    });

    QObject::connect(task, &Services::InstanceLaunch::started, APPLICATION, [onStarted](const QString& logPath) {
        if (onStarted)
            onStarted(logPath);
    });
    QObject::connect(task, &Services::InstanceLaunch::failed, APPLICATION, [onFailed, forget](const QString& reason) {
        qWarning().noquote() << "core launch failed:" << reason;
        forget();
        if (onFailed)
            onFailed(reason);
    });
    QObject::connect(task, &Services::InstanceLaunch::aborted, APPLICATION, [onAborted, forget] {
        forget();
        if (onAborted)
            onAborted();
    });
    QObject::connect(task, &Services::InstanceLaunch::gameFinished, APPLICATION,
                     [onFinished, forget, model, windowsClosed](int exitCode) {
                         if (*windowsClosed)
                             APPLICATION->showMainWindow();
                         model->append(MessageLevel::Launcher,
                                       QObject::tr("The game exited with code %1.").arg(exitCode));

                         if (exitCode != 0) {
                             // The same reading the old launch path appends: what the launcher worked out
                             // has to live in the log, because that is what gets copied and posted.
                             QStringList lines;
                             for (int row = 0; row < model->rowCount(); ++row)
                                 lines << model->data(model->index(row), Qt::DisplayRole).toString();
                             for (const auto& finding : CrashAnalysis::analyze(lines)) {
                                 model->append(MessageLevel::Launcher, QObject::tr("Diagnosis: %1").arg(finding.title));
                                 model->append(MessageLevel::Launcher, QObject::tr("Suggestion: %1").arg(finding.suggestion));
                             }
                         }

                         forget();
                         if (onFinished)
                             onFinished(exitCode);
                     });
    // The launch holds the process and the callbacks above; once it has ended nothing else refers to
    // it, and a launcher that starts the game several times in one session would keep every one of
    // them alive until it exits. Deferred, because these signals are emitted from inside it.
    QObject::connect(task, &Services::InstanceLaunch::failed, task, &QObject::deleteLater);
    QObject::connect(task, &Services::InstanceLaunch::aborted, task, &QObject::deleteLater);
    QObject::connect(task, &Services::InstanceLaunch::gameFinished, task, &QObject::deleteLater);
    task->start(options, who);
    return true;
}

}  // namespace

bool killInstance(const QString& instanceId)
{
    const auto task = s_launches.value(instanceId);
    if (!task)
        return false;
    task->kill();
    return true;
}

bool launchInstance(MinecraftInstance* instance, const MinecraftAccountPtr& account,
                    const Services::AuthSession& coreAccount, const StartedCallback& onStarted,
                    const FailedCallback& onFailed, QString* error, const FinishedCallback& onFinished,
                    const AbortedCallback& onAborted, const StatusCallback& onStatus)
{
    if (!instance) {
        if (error)
            *error = QStringLiteral("no instance was selected");
        return false;
    }
    if (!account && coreAccount.uuid.isEmpty()) {
        if (error)
            *error = QStringLiteral("no account is signed in");
        return false;
    }

    // The core's account, when one is configured for launching; the launcher's own otherwise.
    const bool useCore = !coreAccount.uuid.isEmpty();
    if (!useCore)
        return beginLaunch(instance, accountFromPrism(account), onStarted, onFailed, onFinished, onAborted, onStatus);

    const Services::LaunchAccount who = accountFromCore(coreAccount);
    if (!who.accessToken.isEmpty() && !coreAccount.expired())
        return beginLaunch(instance, who, onStarted, onFailed, onFinished, onAborted, onStatus);

    /*
     * A stored session keeps only the refresh token, so the first launch of the day renews it here.
     * That is one HTTPS round trip, not a login: it runs while the window says what it is doing, and
     * the game is only started once the token is in hand.
     */
    if (onStatus)
        onStatus(QObject::tr("Renewing the Komorebi account's sign-in…"));
    qInfo().noquote() << "core launch: renewing the Komorebi account" << coreAccount.uuid;

    auto* auth = new Services::MicrosoftAuth(APPLICATION->network(), APPLICATION);
    auth->setClientId(APPLICATION->getMSAClientID());

    QObject::connect(auth, &Services::MicrosoftAuth::signedIn, APPLICATION, [=] {
        const Services::AuthSession renewed = auth->session();
        QString ignored;
        Services::TokenVault::forDataRoot(APPLICATION->dataRoot()).upsert(renewed, &ignored);
        auth->deleteLater();
        qInfo().noquote() << "core launch: the Komorebi account was renewed as" << renewed.username;
        beginLaunch(instance, accountFromCore(renewed), onStarted, onFailed, onFinished, onAborted, onStatus);
    });
    QObject::connect(auth, &Services::MicrosoftAuth::failed, APPLICATION, [=](const QString& reason) {
        auth->deleteLater();
        qWarning().noquote() << "core launch: the Komorebi account could not be renewed:" << reason;
        // A broken stored sign-in should not make the launcher stop working: the account the launcher
        // already has is still there, and the player can fix the Komorebi one from its dialog.
        if (!account) {
            if (onFailed)
                onFailed(reason);
            return;
        }
        if (onStatus)
            onStatus(QObject::tr("The Komorebi account could not be renewed (%1); starting with the launcher's own account.").arg(reason));
        beginLaunch(instance, accountFromPrism(account), onStarted, onFailed, onFinished, onAborted, onStatus);
    });

    auth->refresh(coreAccount.refreshToken);
    return true;
}

}  // namespace Komorebi::Legacy
