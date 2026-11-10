// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/LibraryFile.h"

#include <QDir>
#include <QUrl>

#include "komorebi/core/Support/ContainedPath.h"

namespace Komorebi::Core {

QString libraryFilePath(const LibraryEntry& entry, const QString& librariesDirectory)
{
    if (!entry.path.isEmpty())
        return containedPath(librariesDirectory, entry.path);

    if (!entry.url.isEmpty()) {
        QString path = QUrl(entry.url).path();
        const int maven = path.indexOf(QStringLiteral("/maven/"));
        if (maven >= 0)
            path = path.mid(maven + 7);
        else if (path.startsWith(QLatin1Char('/')))
            path = path.mid(1);
        if (path.endsWith(QLatin1String(".jar"))) {
            const QString contained = containedPath(librariesDirectory, path);
            if (!contained.isEmpty())
                return contained;
        }
    }

    const QString relative = libraryRelativePath(entry.name);
    if (relative.isEmpty())
        return {};
    return containedPath(librariesDirectory, relative);
}

const ClassifierFile* classifierFile(const LibraryEntry& entry, const Platform& platform)
{
    const QString classifier = entry.nativesClassifier(platform);
    if (classifier.isEmpty())
        return nullptr;
    const auto it = entry.classifiers.constFind(classifier);
    return it == entry.classifiers.constEnd() ? nullptr : &it.value();
}

QString nativeLibraryPath(const LibraryEntry& entry, const Platform& platform, const QString& librariesDirectory)
{
    const QString classifier = entry.nativesClassifier(platform);
    if (classifier.isEmpty())
        return {};

    // The format may name the classifier's file outright, path included; the download and the unpack
    // both have to use that address, or one of them ends up looking where the other did not write.
    if (const auto* file = classifierFile(entry, platform); file && !file->path.isEmpty())
        return containedPath(librariesDirectory, file->path);

    // Otherwise the file is the entry's own coordinate with the classifier appended: a natives jar is
    // named artifact-version-classifier.jar, like any other classified artifact.
    const auto parts = entry.name.split(QLatin1Char(':'));
    if (parts.size() < 3 || !isPlainFileName(classifier))
        return {};
    const QString coordinate = parts.at(0) + QLatin1Char(':') + parts.at(1) + QLatin1Char(':') + parts.at(2)
                               + QLatin1Char(':') + classifier;
    const QString relative = libraryRelativePath(coordinate);
    if (relative.isEmpty())
        return {};
    return containedPath(librariesDirectory, relative);
}

}  // namespace Komorebi::Core
