// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Where a download comes from.
 *
 *  Every URL the launcher uses points at Mojang or at a loader's own maven. A mirror serves the same
 *  bytes under a different address, and which one to use is a preference, not a property of the file:
 *  the manifest keeps its official URL and this rewrites it at fetch time.
 */

#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace Komorebi::Services {

enum class DownloadSource {
    Official,
    Bmclapi,
};

struct DownloadSourceInfo {
    DownloadSource source;
    QString id;    ///< what a settings file stores
    QString name;  ///< what a settings page shows
};

QList<DownloadSourceInfo> downloadSources();

QString downloadSourceName(DownloadSource source);

/// Reads the id a settings file stores, falling back to the official source for anything unknown.
DownloadSource downloadSourceFromId(const QString& id);

/**
 * Rewrites a file's URL onto the chosen source.
 *
 * Hosts the source does not mirror are returned unchanged: a loader's own maven or the launcher's
 * metadata have no mirror entry, and pretending they do would turn a working download into a 404.
 */
QUrl sourceUrl(const QUrl& url, DownloadSource source);

}  // namespace Komorebi::Services
