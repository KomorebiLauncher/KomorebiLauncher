// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Checking a path that a manifest supplied before it is used to read or write a file.
 *
 *  The strings under `libraries/`, `assets/`, `meta/` and `versions/` do not come from this launcher:
 *  a library's `path`, an asset's key and a component's uid/version are read out of a JSON document
 *  that may have been written by a loader, a pack author or a mirror. A value of
 *  "../../../startup/link" would otherwise be opened, written and unpacked outside the directory it
 *  is supposed to live in -- without a single question asked on the way.
 *
 *  The check is deliberately blunt: a path is either a plain relative name that stays below the
 *  directory it is appended to, or it is refused and the caller reports it. Nothing in the formats
 *  this launcher reads needs an absolute path, a drive letter or a `..`, so a value that has one is
 *  wrong rather than merely unusual.
 */

#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

namespace Komorebi::Core {

/// Whether `name` is a single path component: not "." or "..", no separator, no drive or stream colon.
/// Used for values that name one directory, such as a version id, a component uid or an asset index id.
inline bool isPlainFileName(const QString& name)
{
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
        return false;
    return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\')) && !name.contains(QLatin1Char(':'));
}

/// Whether `relative` stays below the directory it is appended to: not absolute, no drive letter or
/// UNC share, and no ".." component. A "." component and repeated separators are allowed.
inline bool isContainedRelativePath(const QString& relative)
{
    if (relative.isEmpty())
        return false;

    QString normalised = relative;
    normalised.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // "C:file" and "//server/share/file" are addresses, not names below a directory.
    if (normalised.startsWith(QLatin1Char('/')) || normalised.contains(QLatin1Char(':')))
        return false;
    for (const auto& segment : normalised.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (segment == QLatin1String(".."))
            return false;
    }
    return true;
}

/// The path `relative` names below `basePath`, or an empty string when it would leave it.
inline QString containedPath(const QString& basePath, const QString& relative)
{
    if (!isContainedRelativePath(relative))
        return {};

    QString normalised = relative;
    normalised.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir(basePath).filePath(normalised);
}

}  // namespace Komorebi::Core
