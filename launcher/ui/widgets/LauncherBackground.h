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
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QWidget>

#include <algorithm>

#include "Application.h"
#include "settings/SettingsObject.h"

/**
 * The artwork the launcher draws behind its windows.
 *
 * One place, because every window that paints it has to look it up the same way: the path from the
 * settings first, then the picture the active theme ships next to its stylesheet -- and then the
 * same scaling rule and the same dimming, whichever window is asking.
 *
 * A window that does not paint it shows whatever the stylesheet left behind -- and a stylesheet
 * that says `background: transparent` on a top-level window leaves a black surface, not the desktop.
 */
namespace LauncherBackground {

/** The picture to paint, or a null image when there is none. Decoded once per path. */
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
    if (path.isEmpty())
        return {};

    // Decoding on every paint would stutter while a window is resized; keying on the path means a
    // changed setting is still picked up without a restart.
    static QHash<QString, QImage> cache;
    auto it = cache.constFind(path);
    if (it != cache.constEnd())
        return it.value();
    if (cache.size() > 4)
        cache.clear();
    return cache.insert(path, QImage(path)).value();
}

/**
 * Draws the picture over the whole widget.
 *
 * Cover or fit is decided by the same rule everywhere: `cover` and `fit` are literal, anything else
 * picks from the shape of the picture, because a tall picture cropped to fill a wide window reads
 * as the background being broken rather than as a choice.
 */
inline void paint(QWidget* target, const QImage& image)
{
    if (image.isNull() || !target)
        return;

    const QSize size = target->size();
    if (size.isEmpty() || image.height() <= 0)
        return;

    auto* app = APPLICATION_DYN;
    if (!app)
        return;

    const auto mode = app->settings()->get("MainWindowBackgroundMode").toString().toLower();
    bool fillWindow = true;
    if (mode == QLatin1String("cover")) {
        fillWindow = true;
    } else if (mode == QLatin1String("fit")) {
        fillWindow = false;
    } else {
        const double imageAspect = static_cast<double>(image.width()) / image.height();
        const double windowAspect = static_cast<double>(size.width()) / size.height();
        const double difference = std::abs(imageAspect - windowAspect) / std::max(imageAspect, windowAspect);
        fillWindow = difference < 0.18;
    }

    QPainter painter(target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const qreal ratio = target->devicePixelRatioF();
    const auto targetSize = size * ratio;
    const auto scaled =
        image.scaled(targetSize, fillWindow ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // The colour behind the bands: dark enough not to compete with the picture, neutral enough not
    // to tint it.
    painter.fillRect(target->rect(), QColor(18, 22, 28));
    painter.drawImage(QPointF((size.width() - scaled.width() / ratio) / 2.0, (size.height() - scaled.height() / ratio) / 2.0), scaled);

    const qreal dim = std::clamp(app->settings()->get("MainWindowBackgroundDim").toDouble(), 0.0, 1.0);
    if (dim > 0.0)
        painter.fillRect(target->rect(), QColor(0, 0, 0, qRound(dim * 255)));
}

}  // namespace LauncherBackground
