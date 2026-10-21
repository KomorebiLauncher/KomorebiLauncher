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

#include <QDialog>

#include "modplatform/optifine/OptiFineList.h"

class QListWidget;

/** Picks one of the OptiFine builds published for the instance's Minecraft version. */
class OptiFineInstallDialog final : public QDialog {
    Q_OBJECT

   public:
    OptiFineInstallDialog(QList<OptiFine::Build> builds, const QString& mcVersion, QWidget* parent = nullptr);

    auto selectedBuild() const -> const OptiFine::Build&;

   private:
    QList<OptiFine::Build> m_builds;  ///< newest first, in the same order as the list widget
    QListWidget* m_list;
};
