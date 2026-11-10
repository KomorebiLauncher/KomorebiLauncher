// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/download/InstanceFiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/launch/LaunchPlan.h"
#include "komorebi/core/Support/ContainedPath.h"
#include "komorebi/core/version/ComponentSet.h"
#include "komorebi/core/version/LibraryFile.h"

#include "BuildConfig.h"

namespace Komorebi::Services {

namespace {

QByteArray hashOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file))
        return {};
    return hash.result();
}

/// Whether the file is there and what the manifest says about it holds.
bool fileIsGood(const QString& path, const QByteArray& sha1, qint64 expectedSize)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return false;
    if (!sha1.isEmpty())
        return hashOf(path) == sha1;
    if (expectedSize >= 0)
        return info.size() == expectedSize;
    return info.size() > 0;
}

/// The client jar the manifest names, resolved through the libraries directory.
QString clientJarFor(const Core::VersionJson& manifest, const QString& dataRoot)
{
    if (manifest.mainJarName.isEmpty())
        return {};
    const auto relative = Core::libraryRelativePath(manifest.mainJarName);
    if (relative.isEmpty())
        return {};
    return Core::containedPath(QDir(dataRoot).filePath(QStringLiteral("libraries")), relative);
}

}  // namespace

QList<FileDownload> missingInstanceFiles(const QString& dataRoot, const QString& instanceDirectory, QString* error)
{
    const auto folder = Core::InstanceFolder::read(instanceDirectory, error);
    if (!folder)
        return {};

    const auto components = Core::loadComponents(QDir(dataRoot).filePath(QStringLiteral("meta")), folder->components, error);
    if (!components)
        return {};
    const auto manifest = Core::mergeComponents(*components, error);
    if (!manifest)
        return {};

    const QDir data(dataRoot);
    const QDir libraries(data.filePath(QStringLiteral("libraries")));
    const auto platform = Core::Platform::current();

    QList<FileDownload> missing;
    for (const auto& library : manifest->libraries) {
        if (!library.appliesTo(platform))
            continue;
        if (library.url.isEmpty())
            continue;

        const QString target = Core::libraryFilePath(library, libraries.path());
        if (target.isEmpty())
            continue;
        const QByteArray sha1 = library.sha1.isEmpty() ? QByteArray() : QByteArray::fromHex(library.sha1.toLatin1());
        const qint64 size = library.size > 0 ? library.size : -1;
        if (fileIsGood(target, sha1, size))
            continue;
        missing.append(FileDownload{ QUrl(library.url), target, sha1, size });
    }

    // A natives jar is a classifier of the library it belongs to, not an artifact of its own: without
    // it the launch has nothing to unpack into the instance's natives/, and the game stops at the
    // first call into a graphics or sound library.
    for (const auto& library : manifest->libraries) {
        if (!library.appliesTo(platform))
            continue;
        const auto* classifier = Core::classifierFile(library, platform);
        if (!classifier)
            continue;

        const QString target = Core::nativeLibraryPath(library, platform, libraries.path());
        if (target.isEmpty())
            continue;
        const QByteArray sha1 = classifier->sha1.isEmpty() ? QByteArray() : QByteArray::fromHex(classifier->sha1.toLatin1());
        const qint64 size = classifier->size > 0 ? classifier->size : -1;
        if (fileIsGood(target, sha1, size))
            continue;

        // The format normally gives the classifier its own address; when it does not, the file is the
        // one the vanilla library host keeps under the path its own name spells out.
        QUrl url(classifier->url);
        if (!url.isValid())
            url = QUrl(BuildConfig.LIBRARY_BASE + libraries.relativeFilePath(target));
        if (!url.isValid())
            continue;
        missing.append(FileDownload{ url, target, sha1, size });
    }

    // Maven files are the installer and tools a wrapper-style loader reads from disk; they are named
    // separately from the libraries but still have to be fetched.
    for (const auto& file : manifest->mavenFiles) {
        if (file.url.isEmpty())
            continue;
        const QString target = Core::libraryFilePath(file, libraries.path());
        if (target.isEmpty())
            continue;
        const QByteArray sha1 = file.sha1.isEmpty() ? QByteArray() : QByteArray::fromHex(file.sha1.toLatin1());
        const qint64 size = file.size > 0 ? file.size : -1;
        if (fileIsGood(target, sha1, size))
            continue;
        missing.append(FileDownload{ QUrl(file.url), target, sha1, size });
    }

    // The game's own jar is named by the manifest rather than listed among the libraries.
    const auto client = manifest->downloads.value(QStringLiteral("client")).toObject();
    const QString clientJar = clientJarFor(*manifest, dataRoot);
    if (!clientJar.isEmpty()) {
        const QByteArray sha1 = QByteArray::fromHex(client.value(QStringLiteral("sha1")).toString().toLatin1());
        const qint64 size = static_cast<qint64>(client.value(QStringLiteral("size")).toDouble(-1));
        if (!fileIsGood(clientJar, sha1, size)) {
            const QUrl url(client.value(QStringLiteral("url")).toString());
            if (url.isValid())
                missing.append(FileDownload{ url, clientJar, sha1, size });
        }
    }

    return missing;
}

}  // namespace Komorebi::Services
