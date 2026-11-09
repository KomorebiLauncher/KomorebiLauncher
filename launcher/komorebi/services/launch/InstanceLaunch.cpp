// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/launch/InstanceLaunch.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QProcess>

#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/asset/AssetLayout.h"
#include "komorebi/core/launch/Natives.h"
#include "komorebi/core/version/ComponentSet.h"
#include "komorebi/services/download/AssetFiles.h"
#include "komorebi/services/download/DownloadQueue.h"
#include "komorebi/services/download/InstanceFiles.h"
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

std::optional<Core::LaunchPlan> buildPlan(const Core::VersionJson& manifest, const InstanceLaunchOptions& options,
                                          const LaunchAccount& account, QString* error)
{
    const int required = manifest.javaMajor > 0 ? manifest.javaMajor : 8;
    const auto java = choose(discover(options.dataRoot), required);
    if (!java) {
        if (error)
            *error = QObject::tr("no Java %1 was found on this machine").arg(required);
        return std::nullopt;
    }

    const QString natives = QDir(options.instanceDirectory).filePath(QStringLiteral("natives"));
    QDir().mkpath(natives);

    Core::LaunchOptions launch;
    launch.javaPath = java->path;
    launch.gameDirectory = options.instanceDirectory;
    launch.assetsDirectory = QDir(options.dataRoot).filePath(QStringLiteral("assets"));
    launch.librariesDirectory = QDir(options.dataRoot).filePath(QStringLiteral("libraries"));
    launch.nativesDirectory = natives;
    launch.clientJarPath = clientJarFor(manifest, options.dataRoot);
    launch.versionName = manifest.id;
    launch.assetIndexName = manifest.assetIndexId.isEmpty() ? manifest.assets : manifest.assetIndexId;
    launch.username = account.username;
    launch.uuid = account.uuid;
    launch.accessToken = account.accessToken;
    launch.userType = account.userType;
    launch.clientId = account.clientId;
    launch.xuid = account.xuid;
    launch.minMemoryMb = options.minMemoryMb;
    launch.maxMemoryMb = options.maxMemoryMb;
    launch.extraJvmArguments = options.extraJvmArguments;

    auto plan = Core::buildLaunchPlan(manifest, launch);
    if (!plan.ok()) {
        if (error)
            *error = plan.problems.join(QStringLiteral("; "));
        return std::nullopt;
    }
    return plan;
}

std::optional<Core::LaunchPlan> planInstance(const InstanceLaunchOptions& options, const LaunchAccount& account,
                                             QString* error)
{
    const auto folder = Core::InstanceFolder::read(options.instanceDirectory, error);
    if (!folder)
        return std::nullopt;

    const auto components =
        Core::loadComponents(QDir(options.dataRoot).filePath(QStringLiteral("meta")), folder->components, error);
    if (!components)
        return std::nullopt;
    const auto manifest = Core::mergeComponents(*components, error);
    if (!manifest)
        return std::nullopt;

    return buildPlan(*manifest, options, account, error);
}

InstanceLaunch::InstanceLaunch(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network) {}

void InstanceLaunch::start(const InstanceLaunchOptions& options, const LaunchAccount& account)
{
    if (m_running)
        return;
    m_options = options;
    m_account = account;
    m_running = true;
    m_reported = false;
    m_manifest.reset();
    m_assetIndex.reset();
    m_pending.clear();
    stepLoadInstance();
}

void InstanceLaunch::cancel()
{
    if (!m_running || m_reported)
        return;
    m_running = false;
    m_reported = true;
    for (auto* queue : findChildren<DownloadQueue*>()) {
        queue->disconnect(this);
        queue->cancel();
        queue->deleteLater();
    }
    emit aborted();
}

void InstanceLaunch::kill()
{
    if (m_game) {
        if (m_game->state() != QProcess::NotRunning) {
            m_game->terminate();
            if (!m_game->waitForFinished(5000))
                m_game->kill();
        }
        return;
    }
    cancel();
}

qint64 InstanceLaunch::gameProcessId() const
{
    return m_game ? m_game->processId() : 0;
}

void InstanceLaunch::fail(const QString& reason)
{
    if (m_reported)
        return;
    m_running = false;
    m_reported = true;
    emit failed(reason);
}

void InstanceLaunch::stepLoadInstance()
{
    emit status(tr("Reading the instance"));

    QString error;
    const auto folder = Core::InstanceFolder::read(m_options.instanceDirectory, &error);
    if (!folder)
        return fail(error);

    const auto components =
        Core::loadComponents(QDir(m_options.dataRoot).filePath(QStringLiteral("meta")), folder->components, &error);
    if (!components)
        return fail(error);

    m_manifest = Core::mergeComponents(*components, &error);
    if (!m_manifest)
        return fail(error);

    stepDownloadLibraries();
}

void InstanceLaunch::stepDownloadLibraries()
{
    emit status(tr("Checking the files the instance needs"));

    QString error;
    const auto missing = missingInstanceFiles(m_options.dataRoot, m_options.instanceDirectory, &error);
    if (!error.isEmpty())
        return fail(error);

    fetch(missing, [this] { stepAssetIndex(); });
}

void InstanceLaunch::stepAssetIndex()
{
    if (m_manifest->assetIndexId.isEmpty()) {
        // Versions from before the asset index existed have nothing to fetch here.
        return stepExtractNatives();
    }

    const QString path =
        QDir(m_options.dataRoot).filePath(QStringLiteral("assets/indexes/") + m_manifest->assetIndexId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
        QString error;
        m_assetIndex = Core::loadAssetIndex(m_options.dataRoot, m_manifest->assetIndexId, &error);
        if (!m_assetIndex)
            return fail(error);
        return stepDownloadAssets();
    }

    emit status(tr("Fetching the asset index"));
    fetch(missingAssetIndexFile(*m_manifest, m_options.dataRoot), [this] {
        QString error;
        m_assetIndex = Core::loadAssetIndex(m_options.dataRoot, m_manifest->assetIndexId, &error);
        if (!m_assetIndex)
            return fail(error);
        stepDownloadAssets();
    });
}

void InstanceLaunch::stepDownloadAssets()
{
    if (!m_assetIndex)
        return stepExtractNatives();

    emit status(tr("Checking the assets the game uses"));
    const auto missing = missingAssetObjects(*m_assetIndex, m_options.dataRoot);
    fetch(missing, [this] { stepLegacyAssets(); });
}

void InstanceLaunch::stepLegacyAssets()
{
    if (!m_assetIndex)
        return stepExtractNatives();

    const auto directory = Core::legacyAssetDirectory(*m_assetIndex, m_options.dataRoot, m_options.instanceDirectory);
    if (directory.isEmpty())
        return stepExtractNatives();

    emit status(tr("Copying the assets where this version looks for them"));
    QString error;
    if (!Core::materialiseLegacyAssets(*m_assetIndex, m_options.dataRoot, m_options.instanceDirectory, &error))
        return fail(error);

    stepExtractNatives();
}

void InstanceLaunch::stepExtractNatives()
{
    emit status(tr("Unpacking the native libraries"));

    const auto platform = Core::Platform::current();
    const auto jars = Core::nativeJars(*m_manifest, QDir(m_options.dataRoot).filePath(QStringLiteral("libraries")), platform);
    QString error;
    const QString natives = QDir(m_options.instanceDirectory).filePath(QStringLiteral("natives"));
    if (!Core::extractNatives(jars, natives, &error))
        return fail(error);

    stepLaunch();
}

void InstanceLaunch::stepLaunch()
{
    emit status(tr("Starting the game"));

    QString error;
    const auto plan = buildPlan(*m_manifest, m_options, m_account, &error);
    if (!plan)
        return fail(error);

    const QDir logs(QDir(m_options.instanceDirectory).filePath(QStringLiteral("logs")));
    QDir().mkpath(logs.path());
    const QString logPath = logs.filePath(QStringLiteral("core-launch.log"));

    auto* game = new QProcess(this);
    m_game = game;
    game->setProgram(plan->program);
    game->setArguments(plan->arguments);
    game->setWorkingDirectory(plan->workingDirectory);
    game->setProcessChannelMode(QProcess::MergedChannels);

    // The output is read here rather than redirected straight to a file, so the same lines can go to
    // the launcher as well: they are what the instance's console page shows while the game runs.
    m_logFile = new QFile(logPath, this);
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_logFile->deleteLater();
        m_logFile = nullptr;
    }
    m_lineBuffer.clear();

    connect(game, &QProcess::readyRead, this, [this, game] {
        const QByteArray data = game->readAll();
        if (m_logFile)
            m_logFile->write(data);

        m_lineBuffer += QString::fromUtf8(data);
        int newline = m_lineBuffer.indexOf(QLatin1Char('\n'));
        while (newline >= 0) {
            QString line = m_lineBuffer.left(newline);
            m_lineBuffer.remove(0, newline + 1);
            if (line.endsWith(QLatin1Char('\r')))
                line.chop(1);
            emit output(line);
            newline = m_lineBuffer.indexOf(QLatin1Char('\n'));
        }
    });

    connect(game, &QProcess::finished, this, [this, game](int exitCode, QProcess::ExitStatus) {
        if (!m_lineBuffer.isEmpty()) {
            emit output(m_lineBuffer);
            m_lineBuffer.clear();
        }
        if (m_logFile) {
            m_logFile->close();
            m_logFile->deleteLater();
            m_logFile = nullptr;
        }
        m_game = nullptr;
        game->deleteLater();
        emit gameFinished(exitCode);
    });
    connect(game, &QProcess::errorOccurred, game, [this, game](QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart)
            fail(tr("java did not start: %1").arg(game->errorString()));
    });

    game->start();
    if (!game->waitForStarted(10000)) {
        m_game = nullptr;
        game->deleteLater();
        return fail(tr("java did not start: %1").arg(game->errorString()));
    }

    // The game now owns its log file; the launcher only needed to say where it is.
    finish(logPath);
}

void InstanceLaunch::finish(const QString& logPath)
{
    if (m_reported)
        return;
    m_running = false;
    m_reported = true;
    emit started(logPath);
}

void InstanceLaunch::fetch(const QList<FileDownload>& files, const std::function<void()>& next)
{
    if (files.isEmpty()) {
        QMetaObject::invokeMethod(
            this, [next] { next(); }, Qt::QueuedConnection);
        return;
    }

    m_pending = files;
    auto* queue = new DownloadQueue(m_network, this);
    connect(queue, &DownloadQueue::progress, this, &InstanceLaunch::progress);
    connect(queue, &DownloadQueue::finished, this, [this, queue, next](bool ok, const QStringList& failures) {
        queue->deleteLater();
        if (!m_running)
            return;
        if (!ok) {
            const QString reason = failures.isEmpty() ? tr("the download failed") : failures.join(QStringLiteral("; "));
            return fail(reason);
        }
        next();
    });
    queue->start(files, m_options.source);
}

}  // namespace Komorebi::Services
