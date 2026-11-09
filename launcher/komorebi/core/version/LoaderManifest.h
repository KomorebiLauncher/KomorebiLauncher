// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  Mod loaders that publish their profile themselves.
 *
 *  Fabric does not ship an installer: its metadata says which loader versions exist for a game version,
 *  and a profile for one of them lists the classes and libraries a launch needs. That is close enough to
 *  a component that converting one is mostly renaming fields -- except the libraries, whose URLs are a
 *  maven base rather than a full address.
 */

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace Komorebi::Core {

/// One loader release Fabric lists for a game version.
struct LoaderRelease {
    QString version;
    bool stable = false;
};

/// Reads Fabric's loader list for one game version (v2/versions/loader/<mc>).
std::optional<QList<LoaderRelease>> parseFabricLoaderList(const QJsonArray& document, QString* error = nullptr);

/**
 * Converts a Fabric loader profile into component metadata.
 *
 * The profile names the main class that starts the game and lists libraries as a maven base plus a
 * coordinate; a component entry wants a full download address, so each library gets one built from the two.
 */
std::optional<QJsonObject> componentFromFabricProfile(const QJsonObject& profile, const QString& loaderVersion,
                                                      QString* error = nullptr);

}  // namespace Komorebi::Core
