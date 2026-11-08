// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/launch/InstanceLaunch.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>

#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/version/ComponentSet.h"
#include "komorebi/services/java/JavaRuntime.h"

namespace Komorebi::Services {

namespace {

/// The client jar a component manifest asks for, resolved through the libraries directory.
QString clientJarFor(const Core::VersionJson& manifest, const QString& dataRoot)
{
    if (manifest.mainJarName.isEmpty())
        return {};
    const auto relative = Core::libraryRelativePath(manifest.mainJarName);
    if (relative.isEmpty())
        return {};
    return QDir(dataRoot).filePath(QStringLiteral("libraries/") + relative);
}

}  // namespace

std::optional<Core::LaunchPlan> planInstance(const InstanceLaunchOptions& options, const LaunchAccount& account,
                                             QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<Core::LaunchPlan> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    const auto folder = Core::InstanceFolder::read(options.instanceDirectory, error);
    if (!folder)
        return std::nullopt;

    const auto components = Core::loadComponents(QDir(options.dataRoot).filePath(QStringLiteral("meta")), folder->components, error);
    if (!components)
        return std::nullopt;
    const auto manifest = Core::mergeComponents(*components, error);
    if (!manifest)
        return std::nullopt;

    const int required = manifest->javaMajor > 0 ? manifest->javaMajor : 8;
    const auto java = Services::choose(Services::discover(options.dataRoot), required);
    if (!java)
        return fail(QStringLiteral("no Java %1 was found on this machine").arg(required));

    const QString natives = QDir(options.instanceDirectory).filePath(QStringLiteral("natives"));
    QDir().mkpath(natives);

    Core::LaunchOptions launch;
    launch.javaPath = java->path;
    launch.gameDirectory = options.instanceDirectory;
    launch.assetsDirectory = QDir(options.dataRoot).filePath(QStringLiteral("assets"));
    launch.librariesDirectory = QDir(options.dataRoot).filePath(QStringLiteral("libraries"));
    launch.nativesDirectory = natives;
    launch.clientJarPath = clientJarFor(*manifest, options.dataRoot);
    launch.versionName = manifest->id;
    launch.assetIndexName = manifest->assetIndexId.isEmpty() ? manifest->assets : manifest->assetIndexId;
    launch.username = account.username;
    launch.uuid = account.uuid;
    launch.accessToken = account.accessToken;
    launch.userType = account.userType;
    launch.clientId = account.clientId;
    launch.xuid = account.xuid;
    launch.minMemoryMb = options.minMemoryMb;
    launch.maxMemoryMb = options.maxMemoryMb;
    launch.extraJvmArguments = options.extraJvmArguments;

    auto plan = Core::buildLaunchPlan(*manifest, launch);
    if (!plan.ok()) {
        if (error)
            *error = plan.problems.join(QStringLiteral("; "));
        return std::nullopt;
    }
    return plan;
}

bool launchInstance(const InstanceLaunchOptions& options, const LaunchAccount& account, QString* error)
{
    const auto plan = planInstance(options, account, error);
    if (!plan)
        return false;

    // The game outlives this call: give its output a file rather than a pipe nobody reads, and keep
    // the handle with the process so it is closed when the game exits.
    const QDir logs(QDir(options.instanceDirectory).filePath(QStringLiteral("logs")));
    QDir().mkpath(logs.path());
    const QString logPath = logs.filePath(QStringLiteral("core-launch.log"));

    auto* game = new QProcess();
    game->setProgram(plan->program);
    game->setArguments(plan->arguments);
    game->setWorkingDirectory(plan->workingDirectory);
    game->setProcessChannelMode(QProcess::MergedChannels);
    game->setStandardOutputFile(logPath, QIODevice::Append);
    game->setStandardErrorFile(logPath, QIODevice::Append);
    QObject::connect(game, &QProcess::finished, game, &QObject::deleteLater);
    QObject::connect(game, &QProcess::errorOccurred, game, [game, error](QProcess::ProcessError processError) {
        if (error)
            *error = game->errorString();
        if (processError == QProcess::FailedToStart)
            game->deleteLater();
    });
    game->start();
    if (!game->waitForStarted(10000)) {
        if (error)
            *error = QStringLiteral("java did not start: %1").arg(game->errorString());
        delete game;
        return false;
    }
    return true;
}

}  // namespace Komorebi::Services
