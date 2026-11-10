// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/version/ComponentSet.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

#include "komorebi/core/Support/ContainedPath.h"

namespace Komorebi::Core {

namespace {

/// "natives-windows" and the name are what make two entries different libraries.
QString libraryKey(const LibraryEntry& library)
{
    return library.name + QLatin1Char('|') + library.natives.value(QStringLiteral("windows")).toString();
}

}  // namespace

std::optional<QList<LoadedComponent>> loadComponents(const QString& metaRoot, const QList<ComponentRef>& components,
                                                     QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<QList<LoadedComponent>> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (metaRoot.isEmpty())
        return fail(QStringLiteral("no metadata directory was given"));
    if (components.isEmpty())
        return fail(QStringLiteral("the instance names no components"));

    QList<LoadedComponent> loaded;
    for (const auto& ref : components) {
        // Both halves of the reference name directories and files below meta/, and both are read from
        // the instance's mmc-pack.json.
        if (!isPlainFileName(ref.uid) || !isPlainFileName(ref.version))
            return fail(QStringLiteral("%1 %2 is not a component reference").arg(ref.uid, ref.version));
        const QString path = QDir(metaRoot).filePath(ref.uid + QLatin1Char('/') + ref.version + QStringLiteral(".json"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return fail(QStringLiteral("no metadata for component %1 %2 (%3)").arg(ref.uid, ref.version, path));

        QJsonParseError parseError{};
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return fail(QStringLiteral("%1 is not readable: %2").arg(path, parseError.errorString()));

        QString manifestError;
        auto manifest = VersionJson::parse(document.object(), &manifestError);
        if (!manifest)
            return fail(QStringLiteral("%1: %2").arg(path, manifestError));

        loaded.append(LoadedComponent{ ref.uid, ref.version, std::move(*manifest) });
    }
    return loaded;
}

std::optional<VersionJson> mergeComponents(const QList<LoadedComponent>& components, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<VersionJson> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    const LoadedComponent* game = nullptr;
    for (const auto& component : components) {
        if (component.uid == QLatin1String("net.minecraft")) {
            game = &component;
            break;
        }
    }
    if (!game)
        return fail(QStringLiteral("the component set has no net.minecraft to launch"));

    VersionJson merged = game->manifest;

    QStringList seenLibraries;
    QList<LibraryEntry> libraries;
    QList<LibraryEntry> mavenFiles;
    QStringList jvmArguments;
    for (const auto& component : components) {
        // A loader replaces the way the game starts: its main class and, when it still writes one, the
        // whole old-style argument line. That is what makes Forge's wrapper the thing that runs.
        if (component.uid != QLatin1String("net.minecraft")) {
            if (!component.manifest.mainClass.isEmpty())
                merged.mainClass = component.manifest.mainClass;
            // A loader may write a complete command line of its own (the wrapper-style components do,
            // repeating the game's arguments) or only the arguments it adds. A complete line replaces
            // the game's; anything shorter is appended after it.
            const auto& loaderArguments = component.manifest.legacyArguments;
            const bool completeLine = loaderArguments.contains(QStringLiteral("--username"))
                                      || loaderArguments.contains(QStringLiteral("--gameDir"));
            if (completeLine)
                merged.legacyArguments = loaderArguments;
            else
                merged.legacyArguments << loaderArguments;
            mavenFiles << component.manifest.mavenFiles;
        }

        jvmArguments << component.manifest.jvmArguments;
        for (const auto& library : component.manifest.libraries) {
            const QString key = libraryKey(library);
            if (seenLibraries.contains(key))
                continue;
            seenLibraries << key;
            libraries.append(library);
        }
    }

    merged.libraries = libraries;
    merged.mavenFiles = mavenFiles;
    merged.jvmArguments = jvmArguments;
    merged.inheritsFrom.clear();
    return merged;
}

}  // namespace Komorebi::Core
