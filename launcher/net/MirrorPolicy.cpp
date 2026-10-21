// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MirrorPolicy.h"

#include <QStringList>
#include <algorithm>

#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "settings/SettingsObject.h"
#endif

namespace Net {

auto parseMirrorList(const QString& text) -> QStringList
{
    QStringList entries;
    for (const auto& line : text.split(QLatin1Char('\n'))) {
        auto entry = line.trimmed();
        if (entry.isEmpty() || entry.startsWith(QLatin1Char('#')))
            continue;
        entries.append(entry);
    }
    return entries;
}

namespace {

/** Hosts whose objects are mirrored at the mirror root, with the path unchanged. */
const QStringList kMojangObjectHosts = {
    QStringLiteral("piston-meta.mojang.com"),
    QStringLiteral("piston-data.mojang.com"),
    QStringLiteral("launcher.mojang.com"),
    QStringLiteral("launchermeta.mojang.com"),
};

/** Services this project runs, which a Mojang content mirror does not carry. */
const QStringList kOwnServiceHosts = {
    QStringLiteral("meta.prismlauncher.org"),
    QStringLiteral("wtflauncher.github.io"),
};

/**
 * Hosts whose files all live in one maven tree.
 *
 * Measured on 2026-10-21: the same prefix answers a Mojang library, a Forge library, a Maven
 * Central artifact and the Forge installer jar, so one list of prefixes covers loaders and
 * libraries together.
 */
const QStringList kMavenHosts = {
    QStringLiteral("libraries.minecraft.net"),
    QStringLiteral("maven.minecraftforge.net"),
    QStringLiteral("maven.neoforged.net"),
    QStringLiteral("maven.fabricmc.net"),
    QStringLiteral("repo1.maven.org"),
};

/**
 * Deliberately not static: a function-local static would freeze the first base the launcher ever
 * sees and silently keep using it after the user edits the setting. Tolerant parsing accepts
 * stray whitespace from a settings field, so the scheme and host are checked here instead -
 * "not a url" parses to something with a host.
 */
auto validatedBase(const QString& raw) -> QUrl
{
    const QUrl base(raw.trimmed(), QUrl::TolerantMode);
    const auto scheme = base.scheme().toLower();
    if (!base.isValid() || base.host().isEmpty() || (scheme != QLatin1String("https") && scheme != QLatin1String("http")))
        return {};
    return base;
}

auto withPath(const QUrl& source, const QUrl& base, const QString& prefix) -> QUrl
{
    QUrl out = base;
    auto path = base.path();
    if (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    out.setPath(path + prefix + source.path());
    if (source.hasQuery())
        out.setQuery(source.query());
    return out;
}

}  // namespace

auto MirrorPolicy::mirrorUrls(const QUrl& url, const QString& mirrorBase, const QString& servicesMirrorBase, const QStringList& mavenMirrors)
    -> std::vector<QUrl>
{
    if (!url.isValid() || !url.host().size())
        return {};

    const auto host = url.host().toLower();
    const bool isOwnService = kOwnServiceHosts.contains(host);
    auto base = validatedBase(isOwnService ? servicesMirrorBase : mirrorBase);
    if (!base.isValid())
        return {};

    std::vector<QUrl> out;

    if (isOwnService) {
        // A verbatim mirror keeps the whole path, including the /i18n prefix of the translation site.
        out.push_back(withPath(url, base, {}));
    } else if (kMojangObjectHosts.contains(host)) {
        out.push_back(withPath(url, base, {}));
    } else if (host == QLatin1String("resources.download.minecraft.net")) {
        out.push_back(withPath(url, base, QStringLiteral("/assets")));
    } else if (kMavenHosts.contains(host)) {
        for (const auto& entry : mavenMirrors) {
            auto prefix = validatedBase(entry);
            if (prefix.isValid())
                out.push_back(withPath(url, prefix, {}));
        }
        // Both prefixes are served by BMCLAPI and either can rate-limit, so keep both in order.
        out.push_back(withPath(url, base, QStringLiteral("/maven")));
        out.push_back(withPath(url, base, QStringLiteral("/libraries")));
    }

    std::vector<QUrl> unique;
    for (const auto& candidate : out) {
        if (std::find(unique.begin(), unique.end(), candidate) == unique.end())
            unique.push_back(candidate);
    }
    return unique;
}

auto MirrorPolicy::candidatesFor(const QUrl& url, MirrorPolicyKind kind, const QString& mirrorBase, const QString& servicesMirrorBase,
                                 const QStringList& mavenMirrors) -> std::vector<QUrl>
{
    if (kind == MirrorPolicyKind::OfficialOnly)
        return { url };

    auto mirrors = mirrorUrls(url, mirrorBase, servicesMirrorBase, mavenMirrors);
    if (mirrors.empty())
        return { url };

    switch (kind) {
        case MirrorPolicyKind::MirrorOnly:
            return mirrors;
        case MirrorPolicyKind::MirrorThenOfficial:
            mirrors.push_back(url);
            return mirrors;
        case MirrorPolicyKind::OfficialThenMirror:
        default: {
            std::vector<QUrl> out{ url };
            out.insert(out.end(), mirrors.begin(), mirrors.end());
            return out;
        }
    }
}

auto MirrorPolicy::candidates(const QUrl& url) -> std::vector<QUrl>
{
#if defined(LAUNCHER_APPLICATION)
    // APPLICATION_DYN, not APPLICATION: the unit tests run against a plain QCoreApplication, and
    // casting that to Application would hand back a pointer that is neither null nor valid.
    auto* app = APPLICATION_DYN;
    if (!app)
        return { url };
    auto* settings = app->settings();
    auto kind = static_cast<MirrorPolicyKind>(settings->get("MirrorPolicy").toInt());
    return candidatesFor(url, kind, settings->get("MirrorBaseUrl").toString(), settings->get("ServicesMirrorBaseUrl").toString(),
                         parseMirrorList(settings->get("MavenMirrors").toString()));
#else
    return { url };
#endif
}

}  // namespace Net
