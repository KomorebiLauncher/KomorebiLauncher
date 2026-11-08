// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  What an instance still needs on disk before it can start.
 *
 *  A launch is only as good as the files under it: a missing library or client jar is a game that
 *  crashes with a stack trace nobody can read. The launcher's metadata already says where each file
 *  comes from and what it should hash to, so this reads that and returns the ones that are absent or
 *  wrong -- the ones a download has to produce, not the ones it can skip.
 */

#pragma once

#include <QList>
#include <QString>

#include "komorebi/services/download/FileDownloader.h"

namespace Komorebi::Services {

/**
 * The files the instance is missing, as the manifests describe them.
 *
 * Files already present and matching their hash (or their size, when the manifest gives no hash) are
 * left out: a second launch should not re-download a library just because the launcher can.
 */
QList<FileDownload> missingInstanceFiles(const QString& dataRoot, const QString& instanceDirectory, QString* error = nullptr);

}  // namespace Komorebi::Services
