// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/LibraryFile.h"

#include <QDir>
#include <QUrl>

namespace Komorebi::Core {

QString libraryFilePath(const LibraryEntry& entry, const QString& librariesDirectory)
{
    if (!entry.path.isEmpty())
        return QDir(librariesDirectory).filePath(entry.path);

    if (!entry.url.isEmpty()) {
        QString path = QUrl(entry.url).path();
        const int maven = path.indexOf(QStringLiteral("/maven/"));
        if (maven >= 0)
            path = path.mid(maven + 7);
        else if (path.startsWith(QLatin1Char('/')))
            path = path.mid(1);
        if (path.endsWith(QLatin1String(".jar")))
            return QDir(librariesDirectory).filePath(path);
    }

    const QString relative = libraryRelativePath(entry.name);
    if (relative.isEmpty())
        return {};
    return QDir(librariesDirectory).filePath(relative);
}

}  // namespace Komorebi::Core
