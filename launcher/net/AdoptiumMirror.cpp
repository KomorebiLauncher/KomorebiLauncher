// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "AdoptiumMirror.h"

#include <QRegularExpression>

namespace Net {

namespace {

/**
 * Release asset names Adoptium publishes, for example
 * `OpenJDK21U-jdk_x64_windows_hotspot_21.0.13_9.zip`.
 *
 * The major version, image type, architecture and operating system are all encoded in the name,
 * which is what makes the mirror path derivable. `.msi` and `.pkg` installers are deliberately
 * not listed: the mirror carries only the archives the launcher unpacks.
 */
const QRegularExpression& archiveNamePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^OpenJDK(\\d+)U-(jdk|jre)_([A-Za-z0-9]+)_([A-Za-z]+)_hotspot_.*\\.(?:zip|tar\\.gz)$"));
    return pattern;
}

auto lastPathSegment(const QUrl& url) -> QString
{
    const auto path = url.path();
    const auto slash = path.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? path.mid(slash + 1) : path;
}

}  // namespace

auto looksLikeAdoptiumDownload(const QUrl& url) -> bool
{
    if (!url.isValid())
        return false;
    const auto host = url.host().toLower();
    if (host == QLatin1String("api.adoptium.net"))
        return true;
    // GitHub serves release assets from github.com and redirects to objects.githubusercontent.com;
    // only the printed link is mirrored, so the redirect target does not need handling.
    if (host == QLatin1String("github.com") || host.endsWith(QLatin1String(".github.com")))
        return url.path().contains(QLatin1String("/adoptium/"), Qt::CaseInsensitive);
    return false;
}

auto adoptiumMirrorUrl(const QUrl& official, const QString& mirrorBase) -> QUrl
{
    if (!looksLikeAdoptiumDownload(official))
        return {};

    const auto trimmedBase = mirrorBase.trimmed();
    if (trimmedBase.isEmpty())
        return {};

    const auto name = lastPathSegment(official);
    const auto match = archiveNamePattern().match(name);
    if (!match.hasMatch())
        return {};

    const auto major = match.captured(1);
    const auto image = match.captured(2);
    const auto arch = match.captured(3);
    const auto os = match.captured(4);

    QUrl out(trimmedBase);
    if (!out.isValid() || out.host().isEmpty())
        return {};
    out.setPath(out.path().replace(QRegularExpression(QStringLiteral("/+$")), {}) + QLatin1Char('/') + major + QLatin1Char('/') + image +
                QLatin1Char('/') + arch + QLatin1Char('/') + os + QLatin1Char('/') + name);
    return out;
}

}  // namespace Net
