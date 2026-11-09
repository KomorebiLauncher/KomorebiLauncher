// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/launch/Natives.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "komorebi/core/version/LibraryFile.h"

namespace Komorebi::Core {

namespace {

const auto kMarkerName = QStringLiteral(".komorebi-natives.json");

/// How the format names the systems, and what a natives jar's name carries.
QString nativesToken(const Platform& platform)
{
    if (platform.os == QLatin1String("windows"))
        return QStringLiteral("natives-windows");
    if (platform.os == QLatin1String("osx"))
        return QStringLiteral("natives-osx");
    return QStringLiteral("natives-linux");
}

/// Whether the artifact name says this jar holds natives for the platform, and no other architecture.
bool namesNativesFor(const LibraryEntry& entry, const Platform& platform)
{
    const auto parts = entry.name.split(QLatin1Char(':'));
    if (parts.size() < 2)
        return false;
    const QString artifact = parts.at(1);

    if (!artifact.contains(nativesToken(platform)))
        return false;
    // A 32-bit or ARM jar carries that in its name; the platform rules should already have excluded
    // it, and this keeps a wrongly written rule from unpacking it anyway.
    if (artifact.contains(QLatin1String("-arm64")) && platform.arch != QLatin1String("arm64"))
        return false;
    if (artifact.contains(QLatin1String("-x86")) && !artifact.contains(QLatin1String("-x86_64"))
        && platform.arch != QLatin1String("x86"))
        return false;
    return true;
}

/// The marker that says which jars were unpacked, so a launch does not do it again for nothing.
QByteArray markerFor(const QList<NativeJar>& jars)
{
    QJsonArray entries;
    for (const auto& jar : jars) {
        const QFileInfo info(jar.path);
        entries.append(QJsonObject{ { QStringLiteral("path"), jar.path },
                                    { QStringLiteral("size"), static_cast<double>(info.size()) },
                                    { QStringLiteral("mtime"), static_cast<double>(info.lastModified().toMSecsSinceEpoch()) } });
    }
    return QJsonDocument(QJsonObject{ { QStringLiteral("jars"), entries } }).toJson(QJsonDocument::Compact);
}

bool entryIsExcluded(const QString& name, const QStringList& excludes)
{
    // A jar's own metadata is never a native library; neither is anything the manifest excludes.
    if (name.startsWith(QLatin1String("META-INF/")) || name == QLatin1String("META-INF"))
        return true;
    for (const auto& exclude : excludes) {
        if (name.startsWith(exclude))
            return true;
    }
    return false;
}

bool unpackArchive(const NativeJar& jar, const QString& nativesDirectory, QString* error)
{
    struct archive* archive = archive_read_new();
    archive_read_support_filter_all(archive);
    archive_read_support_format_zip(archive);

    const QByteArray jarPath = QFile::encodeName(jar.path);
    if (archive_read_open_filename(archive, jarPath.constData(), 64 * 1024) != ARCHIVE_OK) {
        if (error)
            *error = QObject::tr("could not open %1: %2").arg(jar.path, QString::fromLocal8Bit(archive_error_string(archive)));
        archive_read_free(archive);
        return false;
    }

    bool ok = true;
    archive_entry* entry = nullptr;
    while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
        const QString name = QFile::decodeName(archive_entry_pathname(entry));
        const auto mode = archive_entry_filetype(entry);
        if (name.isEmpty())
            continue;
        if (mode == AE_IFDIR || entryIsExcluded(name, jar.exclude))
            continue;
        // A path from an archive is not trusted to stay inside the directory it is unpacked into.
        if (name.startsWith(QLatin1Char('/')) || name.contains(QLatin1String("..")))
            continue;

        const QString target = QDir(nativesDirectory).filePath(name);
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
            if (error)
                *error = QObject::tr("could not create the folder for %1").arg(target);
            ok = false;
            break;
        }

        QFile file(target);
        if (!file.open(QIODevice::WriteOnly)) {
            if (error)
                *error = QObject::tr("could not write %1: %2").arg(target, file.errorString());
            ok = false;
            break;
        }

        const void* buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (archive_read_data_block(archive, &buffer, &size, &offset) == ARCHIVE_OK) {
            if (file.write(static_cast<const char*>(buffer), static_cast<qint64>(size)) != static_cast<qint64>(size)) {
                if (error)
                    *error = QObject::tr("could not write %1: %2").arg(target, file.errorString());
                ok = false;
                break;
            }
        }
        if (!ok)
            break;
    }

    archive_read_close(archive);
    archive_read_free(archive);
    return ok;
}

}  // namespace

QList<NativeJar> nativeJars(const VersionJson& merged, const QString& librariesDirectory, const Platform& platform)
{
    QList<NativeJar> jars;
    for (const auto& library : merged.libraries) {
        if (!library.appliesTo(platform) || !namesNativesFor(library, platform))
            continue;

        NativeJar jar;
        jar.path = libraryFilePath(library, librariesDirectory);
        jar.exclude = library.extractExclude;
        jars.append(jar);
    }
    return jars;
}

bool extractNatives(const QList<NativeJar>& jars, const QString& nativesDirectory, QString* error)
{
    if (jars.isEmpty())
        return true;

    const QDir natives(nativesDirectory);
    if (!QDir().mkpath(nativesDirectory)) {
        if (error)
            *error = QObject::tr("could not create %1").arg(nativesDirectory);
        return false;
    }

    const QByteArray marker = markerFor(jars);
    QFile markerFile(natives.filePath(kMarkerName));
    if (markerFile.open(QIODevice::ReadOnly) && markerFile.readAll() == marker)
        return true;

    for (const auto& jar : jars) {
        if (!QFileInfo::exists(jar.path)) {
            if (error)
                *error = QObject::tr("the natives jar %1 is missing").arg(jar.path);
            return false;
        }
        if (!unpackArchive(jar, nativesDirectory, error))
            return false;
    }

    if (markerFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        markerFile.write(marker);
    return true;
}

}  // namespace Komorebi::Core
