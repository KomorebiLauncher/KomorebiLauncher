// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The core's view of a version manifest.
 *
 *  A version is a JSON document that may inherit from another one, which the launcher has to read,
 *  merge and act on: the game's main class, the libraries to put on the class path, the arguments to
 *  pass. This is our own reading of the format, not a wrapper around the launcher's old one, so the
 *  launch core can be moved over a piece at a time and compared against the old behaviour.
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace Komorebi::Core {

/// The platform a manifest is being read for. Only the two fields the format's rules look at.
struct Platform {
    QString os;    ///< windows, osx, linux
    QString arch;  ///< x86_64, x86, arm64

    /// The platform this build of the launcher runs on.
    static Platform current();
};

/// One condition in a library's or argument's rule list.
struct Rule {
    QString action;  ///< "allow" or "disallow"
    QString osName;  ///< empty when the rule does not care
    QString osArch;  ///< empty when the rule does not care
};

/// One library entry, as the manifest lists it.
struct LibraryEntry {
    QString name;  ///< group:artifact:version[:classifier]
    QString path;  ///< relative path under libraries/, or empty when the manifest gives none
    QString url;
    QString sha1;
    qint64 size = 0;

    QList<Rule> rules;
    QJsonObject natives;        ///< platform name to classifier, e.g. windows: natives-windows
    QStringList extractExclude;  ///< entries the natives unpacker has to leave out

    /**
     * Whether this entry applies on the platform.
     *
     * The format's rule list is last-match-wins: an entry is allowed when nothing says otherwise,
     * and each rule that matches the platform overrides the answer with its own action.
     */
    bool appliesTo(const Platform& platform) const;

    /// The natives classifier for the platform, or empty when this entry is not a natives jar.
    QString nativesClassifier(const Platform& platform) const;
};

/// A parsed version manifest.
struct VersionJson {
    QString id;
    QString inheritsFrom;
    QString mainClass;
    QString type;

    QString assets;             ///< old name of the asset index ("legacy", "pre-1.6")
    QString assetIndexId;       ///< assetIndex.id
    QJsonObject assetIndex;     ///< the whole assetIndex object, for its url and checksum
    QJsonObject downloads;      ///< client/server objects, kept raw
    QString mainJarName;        ///< component metadata names the client jar it wants, as a coordinate

    int javaMajor = 0;          ///< javaVersion.majorVersion, 0 when the manifest does not say
    QString javaComponent;      ///< javaVersion.component, the runtime Mojang publishes

    QList<LibraryEntry> libraries;
    /// Libraries a loader needs on disk but not on the class path (its installer, for instance).
    QList<LibraryEntry> mavenFiles;
    QStringList gameArguments;  ///< plain strings from arguments.game
    QStringList jvmArguments;   ///< plain strings from arguments.jvm
    QStringList legacyArguments;  ///< minecraftArguments, split on spaces, for old versions

    /// Parses a manifest. Returns an empty value and fills `error` when the document is not one.
    static std::optional<VersionJson> parse(const QJsonObject& object, QString* error = nullptr);
};

/**
 * Merges a version with the versions it inherits, child first.
 *
 * The child wins where a value is a single thing (main class, asset index, java version);
 * libraries are added child first and de-duplicated; arguments accumulate from the base upwards, so
 * the base arguments come before the ones a loader adds. Old-style `minecraftArguments` are a single
 * string: the child's replace the parent's, because a loader rewrites the whole line.
 */
std::optional<VersionJson> mergeChain(const QList<VersionJson>& childToParent, QString* error = nullptr);

/// "group:artifact:version[:classifier]" to "group/artifact/version/artifact-version[-classifier].jar".
QString libraryRelativePath(const QString& name);

/// Where the entry's jar lives, using the manifest's path when it gives one.
QString libraryPath(const LibraryEntry& entry);

}  // namespace Komorebi::Core
