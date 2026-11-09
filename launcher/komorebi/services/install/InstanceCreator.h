// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Making a new instance from a Minecraft version.
 *
 *  Creating one is a short sequence with a lot of network in it: read the version list, fetch the chosen
 *  version's document, keep it in the launcher's metadata directory, write the instance folder, and fetch
 *  the files the instance needs so its first start does not have to. Each step reports what it is doing;
 *  a failure leaves the instance half-made and says why rather than pretending otherwise.
 */

#pragma once

#include <QObject>
#include <QString>

#include <functional>

#include "komorebi/services/download/DownloadSource.h"

class QNetworkAccessManager;

namespace Komorebi::Services {

struct InstanceCreateRequest {
    QString dataRoot;
    QString name;              ///< what the player calls the instance
    QString minecraftVersion;  ///< the version id from the version list
    DownloadSource source = DownloadSource::Official;
    /// Where the version list is; the default is Mojang's, and a test points it at itself.
    QString versionManifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
    /// Fetch the libraries and client jar now; off is for tests and for callers that start the game
    /// immediately, since the launch fetches whatever is missing anyway.
    bool ensureFiles = true;
};

class InstanceCreator : public QObject {
    Q_OBJECT

   public:
    explicit InstanceCreator(QNetworkAccessManager* network, QObject* parent = nullptr);

    void start(const InstanceCreateRequest& request);

   signals:
    void status(const QString& message);
    void progress(int done, int total);
    void finished(const QString& instanceDirectory);
    void failed(const QString& error);

   private:
    void fetchManifest();
    void fetchVersion(const QString& url, const QString& sha1);
    void writeInstance(const QJsonObject& component);
    void fetchLibraries();
    void fetchAssets();
    void fetch(const QString& url, const QString& target, const QString& sha1, const std::function<void()>& next);
    void fail(const QString& reason);
    void finish();

    QNetworkAccessManager* m_network = nullptr;
    InstanceCreateRequest m_request;
    QString m_manifestPath;
    QString m_versionsRoot;
    QString m_versionId;
    QString m_instanceDirectory;
    QString m_assetIndexId;
    bool m_running = false;
    bool m_reported = false;
};

}  // namespace Komorebi::Services
