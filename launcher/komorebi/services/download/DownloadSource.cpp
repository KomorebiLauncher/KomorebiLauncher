// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/download/DownloadSource.h"

namespace Komorebi::Services {

namespace {

const QString kBangbangHost = QStringLiteral("bmclapi2.bangbang93.com");

struct MirrorRule {
    QString host;
    QString pathPrefix;      ///< empty when the mirror serves the host's paths unchanged
    QString dropPathPrefix;  ///< a repository path the mirror leaves out, dropped before the prefix
};

const QList<MirrorRule>& mirrorRules()
{
    // BMCLAPI keeps maven files under /maven and Mojang's assets under /assets, and passes piston
    // paths through unchanged. These are the hosts the launcher's version manifests name.
    static const QList<MirrorRule> rules = {
        { QStringLiteral("libraries.minecraft.net"), QStringLiteral("/maven") },
        { QStringLiteral("maven.minecraftforge.net"), QStringLiteral("/maven") },
        { QStringLiteral("maven.fabricmc.net"), QStringLiteral("/maven") },
        { QStringLiteral("maven.neoforged.net"), QStringLiteral("/maven"), QStringLiteral("/releases") },
        { QStringLiteral("maven.neoforged.net"), QStringLiteral("/maven"), QStringLiteral("/snapshots") },
        { QStringLiteral("resources.download.minecraft.net"), QStringLiteral("/assets") },
        { QStringLiteral("meta.fabricmc.net"), QStringLiteral("/fabric-meta") },
        { QStringLiteral("piston-meta.mojang.com"), QString() },
        { QStringLiteral("piston-data.mojang.com"), QString() },
        { QStringLiteral("launcher.mojang.com"), QString() },
        { QStringLiteral("launchermeta.mojang.com"), QString() },
    };
    return rules;
}

}  // namespace

QList<DownloadSourceInfo> downloadSources()
{
    return {
        { DownloadSource::Official, QStringLiteral("official"), QStringLiteral("Official (Mojang)") },
        { DownloadSource::Bmclapi, QStringLiteral("bmclapi"), QStringLiteral("BMCLAPI") },
    };
}

QString downloadSourceName(DownloadSource source)
{
    for (const auto& info : downloadSources()) {
        if (info.source == source)
            return info.name;
    }
    return {};
}

DownloadSource downloadSourceFromId(const QString& id)
{
    for (const auto& info : downloadSources()) {
        if (info.id == id)
            return info.source;
    }
    return DownloadSource::Official;
}

QUrl sourceUrl(const QUrl& url, DownloadSource source)
{
    if (!url.isValid() || source == DownloadSource::Official)
        return url;

    for (const auto& rule : mirrorRules()) {
        if (rule.host != url.host())
            continue;

        QString path = url.path();
        if (!rule.dropPathPrefix.isEmpty()) {
            if (!path.startsWith(rule.dropPathPrefix))
                continue;
            path = path.mid(rule.dropPathPrefix.size());
        }

        QUrl mirrored = url;
        mirrored.setScheme(QStringLiteral("https"));
        mirrored.setHost(kBangbangHost);
        mirrored.setPath(rule.pathPrefix + path);
        return mirrored;
    }
    return url;
}

}  // namespace Komorebi::Services
