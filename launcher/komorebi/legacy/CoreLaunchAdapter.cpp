// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include <QHash>
#include <QPointer>

#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "QObjectPtr.h"
#include "launch/LogModel.h"
#include "minecraft/CrashAnalysis.h"
#include "logs/LogParser.h"
#include "BaseInstance.h"

#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/launch/InstanceLaunch.h"

namespace Komorebi::Legacy {

namespace {

/// The launches this run started, so the launcher can stop one without owning its process.
QHash<QString, QPointer<Services::InstanceLaunch>> s_launches;

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
                    const std::function<void(const QString& logPath)>& onStarted,
                    const std::function<void(const QString& error)>& onFailed, QString* error,
                    const std::function<void(int exitCode)>& onFinished, const std::function<void()>& onAborted,
                    const std::function<void(const QString& status)>& onStatus)
{
    if (!instance) {
        if (error)
            *error = QStringLiteral("no instance was selected");
        return false;
    }
    if (!account) {
        if (error)
            *error = QStringLiteral("no account is signed in");
        return false;
    }

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
    // javaArguments() is extraArguments() plus the flags the launcher always adds (the game language
    // pin among them), which is what the old path handed to java.
    options.extraJvmArguments = instance->javaArguments();

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

    Services::LaunchAccount who;
    who.username = account->profileName();
    who.uuid = account->profileId();
    who.accessToken = account->accessToken();

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
    task->start(options, who);
    return true;
}

}  // namespace Komorebi::Legacy
