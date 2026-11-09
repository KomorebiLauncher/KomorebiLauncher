// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/legacy/CoreLaunchAdapter.h"

#include <QHash>
#include <QPointer>

#include "Application.h"
#include "QObjectPtr.h"
#include "launch/LogModel.h"
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
                    const std::function<void(int exitCode)>& onFinished, const std::function<void()>& onAborted)
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

    QObject::connect(task, &Services::InstanceLaunch::status, APPLICATION, [model](const QString& text) {
        model->append(MessageLevel::Launcher, text);
    });
    QObject::connect(task, &Services::InstanceLaunch::output, APPLICATION, [model, censor](QString line) {
        for (auto it = censor.begin(); it != censor.end(); ++it)
            line.replace(it.key(), it.value());
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
                     [onFinished, forget, model](int exitCode) {
                         model->append(MessageLevel::Launcher,
                                       QObject::tr("The game exited with code %1.").arg(exitCode));
                         forget();
                         if (onFinished)
                             onFinished(exitCode);
                     });
    task->start(options, who);
    return true;
}

}  // namespace Komorebi::Legacy
