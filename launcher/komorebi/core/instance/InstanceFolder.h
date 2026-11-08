// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Reading an instance folder.
 *
 *  An instance is a directory with an `mmc-pack.json` naming the components it is built from and an
 *  `instance.cfg` with what the player sees. This reads those two files and nothing else: the point of
 *  the core is to know what an instance is without asking the launcher's old classes.
 */

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace Komorebi::Core {

/// One component an instance is built from: a loader, the game, or a library the loader needs.
struct ComponentRef {
    QString uid;
    QString version;
};

struct InstanceFolder {
    QString id;         ///< the directory name, which is the instance's id
    QString directory;  ///< absolute path
    QString name;       ///< what the player sees, from instance.cfg
    QList<ComponentRef> components;

    std::optional<ComponentRef> component(const QString& uid) const;

    /// The game version this instance asks for, empty when the ref is not there.
    QString minecraftVersion() const;

    /**
     * The mod loader the instance uses, if any.
     *
     * The core can start a vanilla instance today; a loader has a component manifest of its own that
     * it still has to learn to read, and saying so is better than starting the game without the mods.
     */
    QString loaderUid() const;

    static std::optional<InstanceFolder> read(const QString& directory, QString* error = nullptr);
};

}  // namespace Komorebi::Core
