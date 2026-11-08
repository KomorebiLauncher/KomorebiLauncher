// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  A set of components read as one version.
 *
 *  The launcher keeps each part of an instance as its own component: the game, the graphics library,
 *  a mod loader. A Mojang-style manifest with `inheritsFrom` is not how they relate -- they are added
 *  together -- so the core reads each one and folds them into the single manifest a launch needs.
 */

#pragma once

#include <QList>
#include <QString>

#include <optional>

#include "komorebi/core/instance/InstanceFolder.h"
#include "komorebi/core/version/VersionJson.h"

namespace Komorebi::Core {

/// One component together with the manifest read for it.
struct LoadedComponent {
    QString uid;
    QString version;
    VersionJson manifest;
};

/**
 * Reads the manifest of every component from the metadata directory: <meta>/<uid>/<version>.json.
 *
 * Fails naming the component that could not be read rather than merging a partial set, because a
 * launch without, say, the graphics library crashes in the game instead of here.
 */
std::optional<QList<LoadedComponent>> loadComponents(const QString& metaRoot, const QList<ComponentRef>& components,
                                                     QString* error = nullptr);

/**
 * Folds components into one version.
 *
 * The game's fields (main class, asset index, Java version, its own arguments and jar) come from
 * `net.minecraft`; libraries and extra JVM arguments come from every component, in the order given,
 * without duplicates.
 */
std::optional<VersionJson> mergeComponents(const QList<LoadedComponent>& components, QString* error = nullptr);

}  // namespace Komorebi::Core
