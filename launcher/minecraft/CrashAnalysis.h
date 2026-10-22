// SPDX-License-Identifier: GPL-3.0-only
/*
 *  WTF Launcher - Minecraft Launcher
 *  Copyright (C) 2026 WTFLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

/** One thing the log says went wrong, phrased so a player can act on it. */
struct CrashFinding {
    QString title;
    QString detail;
    QString suggestion;
};

/** Reads a game log and names the failures this launcher recognizes. */
class CrashAnalysis : public QObject {
    Q_OBJECT

   public:
    static auto analyze(const QStringList& lines) -> QList<CrashFinding>;

    /** The Java release a class file version was compiled for, or an empty string if it is not a known one. */
    static auto javaReleaseForClassVersion(int classVersion) -> QString;
};
