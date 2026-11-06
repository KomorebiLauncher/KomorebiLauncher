// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "PackManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

namespace Domestic {

namespace {

auto trimmedString(const QJsonObject& object, const char* key) -> QString
{
    return object.value(QLatin1String(key)).toString().trimmed();
}

/**
 * First non-empty string among several keys of the same object.
 *
 * The formats in the wild disagree about spelling (`loader` / `loaderType`, `gameVersion` /
 * `version`), so the same question is asked of every key a real pack has been seen to use.
 */
auto firstOf(const QJsonObject& object, const QStringList& keys) -> QString
{
    for (const auto& key : keys) {
        auto value = object.value(key).toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

auto normaliseLoader(const QString& raw) -> QString
{
    auto value = raw.toLower();
    if (value == QLatin1String("fabric") || value == QLatin1String("net.fabricmc"))
        return QStringLiteral("fabric");
    if (value == QLatin1String("quilt") || value == QLatin1String("org.quiltmc"))
        return QStringLiteral("quilt");
    if (value == QLatin1String("forge") || value == QLatin1String("net.minecraftforge"))
        return QStringLiteral("forge");
    if (value == QLatin1String("neoforge") || value == QLatin1String("net.neoforged"))
        return QStringLiteral("neoforge");
    return {};
}

/**
 * The game version, which each of the three published descriptions of this format puts somewhere
 * else: at the root, under `launch`, or under `launchInfo`.
 */
auto mcbbsLaunchSection(const QJsonObject& root) -> QJsonObject
{
    for (const auto* key : { "launch", "launchInfo" }) {
        const auto value = root.value(QLatin1String(key));
        if (value.isObject())
            return value.toObject();
    }
    return {};
}

void readMcbbsFiles(const QJsonArray& array, PackDescription& out)
{
    for (const auto& value : array) {
        if (!value.isObject())
            continue;
        const auto entry = value.toObject();

        RemoteFile file;
        file.path = firstOf(entry, { QStringLiteral("path"), QStringLiteral("fileName"), QStringLiteral("filename") });
        file.url = firstOf(entry, { QStringLiteral("url"), QStringLiteral("downloadUrl") });
        file.sha1 = firstOf(entry, { QStringLiteral("sha1"), QStringLiteral("hash") }).remove(QStringLiteral("sha1:"));
        file.size = static_cast<qint64>(entry.value(QLatin1String("size")).toDouble(0));

        const auto hashes = entry.value(QLatin1String("hashes"));
        if (hashes.isObject()) {
            const auto sha1 = hashes.toObject().value(QLatin1String("sha1")).toString();
            if (!sha1.isEmpty())
                file.sha1 = sha1;
        }

        file.curseforgeFileId = entry.value(QLatin1String("fileID")).toInt(0);
        file.curseforgeProjectId = entry.value(QLatin1String("projectID")).toInt(0);

        // A pack that ships its own copy of a file has nothing to download, so an entry without a
        // path is skipped rather than turned into a request for the empty string.
        if (file.path.isEmpty())
            continue;
        if (file.url.isEmpty() && file.curseforgeFileId == 0) {
            out.warnings.append(QObject::tr("Skipped a pack entry with no download source: %1").arg(file.path));
            continue;
        }

        out.files.append(file);
    }
}

}  // namespace

auto overridesPrefixFor(PackFormat format) -> QString
{
    switch (format) {
        case PackFormat::HMCL:
            return QStringLiteral("minecraft/");
        case PackFormat::Mcbbs:
            return QStringLiteral("overrides/");
        case PackFormat::Unknown:
            break;
    }
    return {};
}

auto detectFormat(const QStringList& entryNames) -> PackFormat
{
    bool hasMcbbsManifest = false;
    bool hasHmclManifest = false;
    bool hasHmclInstance = false;

    for (const auto& name : entryNames) {
        const auto lower = name.toLower();
        if (lower.endsWith(QLatin1String("mcbbs.packmeta")))
            hasMcbbsManifest = true;
        else if (lower.endsWith(QLatin1String("modpack.json")))
            hasHmclManifest = true;
        else if (lower.endsWith(QLatin1String("minecraft/pack.json")))
            hasHmclInstance = true;
    }

    // An MCBBS pack can also carry a manifest.json for the CurseForge layout; the packmeta wins
    // because it is the file that only this format has.
    if (hasMcbbsManifest)
        return PackFormat::Mcbbs;
    if (hasHmclManifest && hasHmclInstance)
        return PackFormat::HMCL;
    return PackFormat::Unknown;
}

auto loaderFromLibraries(const QStringList& mavenCoordinates, QString& loader, QString& version) -> bool
{
    for (const auto& coordinate : mavenCoordinates) {
        const auto parts = coordinate.split(QLatin1Char(':'));
        if (parts.size() < 3)
            continue;

        const auto group = parts.at(0).toLower();
        const auto artifact = parts.at(1).toLower();
        auto raw = parts.at(2);

        if (group == QLatin1String("net.fabricmc") && artifact == QLatin1String("fabric-loader")) {
            loader = QStringLiteral("fabric");
            version = raw;
            return true;
        }
        if (group == QLatin1String("org.quiltmc") && artifact == QLatin1String("quilt-loader")) {
            loader = QStringLiteral("quilt");
            version = raw;
            return true;
        }
        if (group == QLatin1String("net.minecraftforge") && artifact == QLatin1String("forge")) {
            // Forge coordinates carry the game version in front: 1.20.1-47.2.0.
            const auto dash = raw.indexOf(QLatin1Char('-'));
            loader = QStringLiteral("forge");
            version = dash >= 0 ? raw.mid(dash + 1) : raw;
            return true;
        }
        if (group == QLatin1String("net.neoforged") && artifact == QLatin1String("neoforge")) {
            loader = QStringLiteral("neoforge");
            version = raw;
            return true;
        }
    }
    return false;
}

auto parseMcbbs(const QByteArray& manifestJson, PackDescription& out, QString* error) -> bool
{
    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(manifestJson, &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error)
            *error = QObject::tr("The MCBBS manifest is not valid JSON: %1").arg(parseError.errorString());
        return false;
    }

    const auto root = document.object();
    out = {};
    out.format = PackFormat::Mcbbs;
    out.overridesPrefix = overridesPrefixFor(PackFormat::Mcbbs);

    const auto manifestType = root.value(QLatin1String("manifestType")).toString();
    if (!manifestType.isEmpty() && manifestType != QLatin1String("minecraftModpack")) {
        out.warnings.append(QObject::tr("The manifest declares the type \"%1\", not minecraftModpack.").arg(manifestType));
    }

    out.name = trimmedString(root, "name");
    out.version = trimmedString(root, "version");
    out.author = trimmedString(root, "author");
    out.description = trimmedString(root, "description");

    const auto launch = mcbbsLaunchSection(root);

    out.gameVersion = firstOf(root, { QStringLiteral("gameVersion"), QStringLiteral("minecraftVersion") });
    if (out.gameVersion.isEmpty())
        out.gameVersion = firstOf(launch, { QStringLiteral("gameVersion"), QStringLiteral("minecraftVersion") });

    out.loader = normaliseLoader(firstOf(root, { QStringLiteral("loader"), QStringLiteral("loaderType") }));
    out.loaderVersion = firstOf(root, { QStringLiteral("loaderVersion") });
    if (out.loader.isEmpty()) {
        out.loader = normaliseLoader(firstOf(launch, { QStringLiteral("loader"), QStringLiteral("loaderType") }));
        out.loaderVersion = firstOf(launch, { QStringLiteral("loaderVersion") });
    }

    const auto files = root.value(QLatin1String("files"));
    if (files.isArray())
        readMcbbsFiles(files.toArray(), out);

    if (out.name.isEmpty())
        out.name = out.version;
    if (out.name.isEmpty())
        out.name = QObject::tr("Imported pack");

    if (out.gameVersion.isEmpty()) {
        if (error)
            *error = QObject::tr("The MCBBS manifest does not name a Minecraft version.");
        return false;
    }
    return true;
}

auto parseHmcl(const QByteArray& modpackJson, const QByteArray& packJson, PackDescription& out, QString* error) -> bool
{
    QJsonParseError parseError{};
    const auto modpackDocument = QJsonDocument::fromJson(modpackJson, &parseError);
    if (modpackDocument.isNull() || !modpackDocument.isObject()) {
        if (error)
            *error = QObject::tr("The HMCL modpack manifest is not valid JSON: %1").arg(parseError.errorString());
        return false;
    }

    out = {};
    out.format = PackFormat::HMCL;
    out.overridesPrefix = overridesPrefixFor(PackFormat::HMCL);

    const auto modpack = modpackDocument.object();
    out.name = trimmedString(modpack, "name");
    out.version = trimmedString(modpack, "version");
    out.author = trimmedString(modpack, "author");
    out.description = trimmedString(modpack, "description");

    // The instance manifest is what actually names the version; a missing or broken one is not
    // fatal on its own, because some exports ship the version inside minecraft/versions/ instead.
    const auto packDocument = QJsonDocument::fromJson(packJson, &parseError);
    if (!packDocument.isNull() && packDocument.isObject()) {
        const auto pack = packDocument.object();
        out.gameVersion = firstOf(pack, { QStringLiteral("jar"), QStringLiteral("gameVersion"), QStringLiteral("id") });

        QStringList coordinates;
        const auto libraries = pack.value(QLatin1String("libraries"));
        if (libraries.isArray()) {
            for (const auto& value : libraries.toArray()) {
                const auto name = value.toObject().value(QLatin1String("name")).toString();
                if (!name.isEmpty())
                    coordinates.append(name);
            }
        }
        loaderFromLibraries(coordinates, out.loader, out.loaderVersion);
    } else if (!packJson.isEmpty()) {
        out.warnings.append(QObject::tr("minecraft/pack.json could not be read; the game version will be taken from the pack contents."));
    }

    if (out.name.isEmpty())
        out.name = out.version;
    if (out.name.isEmpty())
        out.name = QObject::tr("Imported pack");

    return true;
}

}  // namespace Domestic
