// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The core's view of an instance: a value, not a widget and not a QObject. Everything the rest of
 *  the launcher needs to show or start an instance is here, and nothing that belongs to the old
 *  implementation leaks through.
 */

#pragma once

#include <QString>
#include <QStringList>

namespace Komorebi::Core {

/// What turns a Minecraft version into a playable profile.
enum class Loader {
    Vanilla,
    Forge,
    NeoForge,
    Fabric,
    Quilt,
    Unknown,
};

QString loaderName(Loader loader);

/// One instance, as the core knows it.
struct InstanceInfo {
    QString id;                ///< stable identifier, the directory name today
    QString name;              ///< what the player sees
    QString group;             ///< folder, empty for the ungrouped
    QString iconKey;           ///< the icon the player picked, or the default
    QString minecraftVersion;  ///< e.g. 1.20.1, empty when the version is not known
    QString loaderVersion;     ///< empty for vanilla or when the loader does not say
    Loader loader = Loader::Vanilla;
    QString path;              ///< the instance directory
    bool playable = false;     ///< false when the version is broken or missing
};

}  // namespace Komorebi::Core
