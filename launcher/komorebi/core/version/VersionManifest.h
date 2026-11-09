// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The list of game versions, and turning one into the metadata this launcher keeps.
 *
 *  Mojang publishes a manifest naming every version, and then one document per version describing its
 *  libraries, arguments and client jar. This launcher keeps each version as a component instead: the
 *  same information, named the way the launcher's component system expects, so a version created here
 *  looks to the rest of the launcher exactly like one it downloaded itself.
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace Komorebi::Core {

/// One entry of the version manifest.
struct RemoteVersion {
    QString id;
    QString type;         ///< release, snapshot, old_beta, old_alpha
    QString url;          ///< the version document
    QString sha1;         ///< of the version document
    QString releaseTime;  ///< ISO 8601
};

struct VersionManifest {
    QString latestRelease;
    QString latestSnapshot;
    QList<RemoteVersion> versions;
};

/// Reads Mojang's version_manifest_v2.json.
std::optional<VersionManifest> parseVersionManifest(const QJsonObject& document, QString* error = nullptr);

/**
 * Converts a Mojang version document into the component metadata kept under meta/net.minecraft/<id>.json.
 *
 * The two formats carry the same facts; the differences are the ones that matter to a launcher: the
 * component format names the client jar through mainJar, keeps the game's arguments as one legacy line,
 * and lists the Java version as the launcher-side pair of a major and a runtime name.
 */
std::optional<QJsonObject> componentFromMojangVersion(const QJsonObject& version, QString* error = nullptr);

}  // namespace Komorebi::Core
