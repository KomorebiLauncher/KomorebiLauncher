// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Starting an instance with the core instead of the launcher's old launch path.
 *
 *  A launch is not one step: the instance has to be read, the files it is missing have to be fetched,
 *  its natives unpacked, and only then can the command line be built. This runs those in order and
 *  says what it is doing, so a player looking at a slow connection sees a reason rather than a window
 *  that does nothing.
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

#include "komorebi/core/asset/AssetIndex.h"
#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/version/VersionJson.h"
#include "komorebi/services/download/FileDownloader.h"
#include "komorebi/services/download/DownloadSource.h"

class QNetworkAccessManager;

namespace Komorebi::Services {

/// Who is playing: what the game is told about the account.
struct LaunchAccount {
    QString username;
    QString uuid;
    QString accessToken;
    QString userType = QStringLiteral("msa");
    QString clientId;
    QString xuid;
};

/// What the launch needs from the launcher: where everything lives, how much memory, where to fetch.
struct InstanceLaunchOptions {
    QString dataRoot;           ///< holds instances/, meta/, libraries/, assets/
    QString instanceDirectory;  ///< the instance to start
    int minMemoryMb = 512;
    int maxMemoryMb = 2048;
    QStringList extraJvmArguments;
    DownloadSource source = DownloadSource::Official;
};

/// Builds the command line for an already-read version, or explains why it cannot.
std::optional<Core::LaunchPlan> buildPlan(const Core::VersionJson& manifest, const InstanceLaunchOptions& options,
                                          const LaunchAccount& account, QString* error = nullptr);

/// Reads the instance and builds its command line without fetching anything.
std::optional<Core::LaunchPlan> planInstance(const InstanceLaunchOptions& options, const LaunchAccount& account,
                                             QString* error = nullptr);

class InstanceLaunch : public QObject {
    Q_OBJECT

   public:
    explicit InstanceLaunch(QNetworkAccessManager* network, QObject* parent = nullptr);

    /// Starts the sequence; started() or failed() follows, once.
    void start(const InstanceLaunchOptions& options, const LaunchAccount& account);

    void cancel();

    bool isRunning() const { return m_running; }

   signals:
    void status(const QString& message);
    void progress(int done, int total);
    void started(const QString& logPath);
    void failed(const QString& error);

   private:
    void stepLoadInstance();
    void stepDownloadLibraries();
    void stepAssetIndex();
    void stepDownloadAssets();
    void stepExtractNatives();
    void stepLaunch();

    /// Runs the queue and continues with `next` once every file is there.
    void fetch(const QList<FileDownload>& files, const std::function<void()>& next);
    void fail(const QString& reason);
    void finish(const QString& logPath);

    QNetworkAccessManager* m_network = nullptr;
    InstanceLaunchOptions m_options;
    LaunchAccount m_account;
    std::optional<Core::VersionJson> m_manifest;
    std::optional<Core::AssetIndex> m_assetIndex;
    QList<FileDownload> m_pending;
    bool m_running = false;
    bool m_reported = false;
};

}  // namespace Komorebi::Services
