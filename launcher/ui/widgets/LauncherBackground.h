// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QWidget>

#include <algorithm>

#include "Application.h"
#include "settings/SettingsObject.h"

/**
 * The artwork the launcher draws behind its windows.
 *
 * It lives in one place because every window that paints it has to look it up the same way: the
 * path from the settings first, then the picture the active theme ships next to its stylesheet.
 *
 * A window that does not paint it shows whatever the stylesheet left behind -- and a stylesheet
 * that says `background: transparent` on a top-level window leaves a black surface, not the
 * desktop.
 */
namespace LauncherBackground {

/** The picture to paint, or a null image when there is none. */
inline QImage resolve()
{
    auto* app = APPLICATION_DYN;
    if (!app)
        return {};

    auto path = app->settings()->get("MainWindowBackground").toString();
    if (path.isEmpty()) {
        const auto theme = app->settings()->get("ApplicationTheme").toString();
        if (!theme.isEmpty()) {
            const auto candidate = QDir(app->dataRoot()).filePath(QStringLiteral("themes/%1/resources/background.png").arg(theme));
            if (QFileInfo::exists(candidate))
                path = QDir::fromNativeSeparators(candidate);
        }
    }
    return path.isEmpty() ? QImage() : QImage(path);
}

/** Draws the picture over the whole widget, cover-scaled and dimmed as configured. */
inline void paint(QWidget* target, const QImage& image)
{
    if (image.isNull() || !target)
        return;

    QPainter painter(target);
    const QSize size = target->size();
    // Cover rather than letterbox, so the picture does not end in bars.
    const auto scaled = image.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect source((scaled.width() - size.width()) / 2, (scaled.height() - size.height()) / 2, size.width(), size.height());
    painter.drawImage(target->rect(), scaled, source);

    // Darken it, or a bright picture makes the text unreadable.
    const qreal dim = std::clamp(APPLICATION->settings()->get("MainWindowBackgroundDim").toDouble(), 0.0, 1.0);
    if (dim > 0.0)
        painter.fillRect(target->rect(), QColor(0, 0, 0, qRound(dim * 255)));
}

}  // namespace LauncherBackground
