// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QString>
#include <QUrl>

namespace Net {

/**
 * Mirror of the Adoptium (Temurin) release archives.
 *
 * The launcher downloads Java runtimes from Adoptium, whose files live on GitHub releases. In
 * mainland networks that host is frequently unreachable, and unlike Mojang's hosts the mirror is
 * not a rename: the Tsinghua mirror lays the same files out as
 * `/{major}/{jdk|jre}/{arch}/{os}/{file name}`. The file names, however, are byte-identical to
 * the GitHub release assets, so the mirror URL can be derived from the official one alone.
 *
 * Measured 2026-11-07: the files served there are the same ones, verified by full sha256 against
 * the official checksums for JDK 17, JDK 21 and JRE 17. USTC, Aliyun and Huawei Cloud have no
 * Adoptium mirror at all, which is why only this base is used.
 */
inline constexpr auto kAdoptiumMirrorBase = "https://mirrors.tuna.tsinghua.edu.cn/Adoptium";

/**
 * The mirror URL for an official Adoptium download, or an invalid URL when this official URL is
 * not one the mirror can express.
 *
 * Only URLs whose last path segment is a release asset name can be translated: patterns such as
 * `api.adoptium.net/v3/binary/...` carry no file name, so the mirror path cannot be reconstructed
 * without asking the API for it first.
 */
auto adoptiumMirrorUrl(const QUrl& official, const QString& mirrorBase = QLatin1String(kAdoptiumMirrorBase)) -> QUrl;

/**
 * True when `url` points at an Adoptium download that `adoptiumMirrorUrl` may be able to
 * translate. Kept separate so the policy can decide without building a URL.
 */
auto looksLikeAdoptiumDownload(const QUrl& url) -> bool;

}  // namespace Net
