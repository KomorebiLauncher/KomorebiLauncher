// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Tayou <git@tayou.org>
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */
#include "ITheme.h"
#include <QDir>
#include <QStyleFactory>
#include "Application.h"
#include "settings/SettingsObject.h"
#include "HintOverrideProxyStyle.h"
#include "rainbow.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMenu>
#include <QToolTip>
#include <QPalette>

namespace {

/** The opposite shade of the given text colour, used for the halo behind it. */
auto haloFor(const QColor& text) -> QColor
{
    const int luma = (299 * text.red() + 587 * text.green() + 114 * text.blue()) / 1000;
    return luma < 128 ? QColor(255, 255, 255, 230) : QColor(0, 0, 0, 210);
}

/**
 * Gives every label a faint outline in the opposite shade.
 *
 * A stylesheet has no text-shadow, so the halo has to come from an effect. The filter sits on the
 * application and acts when a label is first shown, which covers labels built lazily by pages far
 * from here.
 */
/**
 * Gives popup windows a light palette.
 *
 * Menus, drop-down lists and tooltips are separate top-level windows whose colours come from the
 * palette, not from the stylesheet; the theme's dark palette is what made them look black no matter
 * what the sheet said.
 */
class PopupPaletteFilter final : public QObject
{
  public:
    explicit PopupPaletteFilter(QObject* parent) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Show) {
            const bool isPopup = qobject_cast<QMenu*>(object) || qobject_cast<QToolTip*>(object) ||
                                 (object->inherits("QComboBoxPrivateContainer"));
            if (isPopup) {
                auto palette = QApplication::palette();
                palette.setColor(QPalette::Window, QColor(0xFA, 0xF7, 0xF0));
                palette.setColor(QPalette::Base, QColor(0xFA, 0xF7, 0xF0));
                palette.setColor(QPalette::WindowText, QColor(0x24, 0x2E, 0x4C));
                palette.setColor(QPalette::Text, QColor(0x24, 0x2E, 0x4C));
                palette.setColor(QPalette::Button, QColor(0xFA, 0xF7, 0xF0));
                palette.setColor(QPalette::ButtonText, QColor(0x24, 0x2E, 0x4C));
                palette.setColor(QPalette::Highlight, QColor(0xEF, 0xDC, 0xD8));
                palette.setColor(QPalette::HighlightedText, QColor(0x24, 0x2E, 0x4C));
                if (auto* widget = qobject_cast<QWidget*>(object))
                    widget->setPalette(palette);
            }
        }
        return QObject::eventFilter(object, event);
    }
};
class TextOutlineFilter final : public QObject
{
  public:
    explicit TextOutlineFilter(QObject* parent) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Show) {
            if (auto* label = qobject_cast<QLabel*>(object); label && !label->graphicsEffect()) {
                auto* halo = new QGraphicsDropShadowEffect(label);
                halo->setBlurRadius(4);
                halo->setOffset(0, 0);
                halo->setColor(haloFor(label->palette().color(QPalette::WindowText)));
                label->setGraphicsEffect(halo);
            }
        }
        return QObject::eventFilter(object, event);
    }
};

}  // namespace
void ITheme::apply(bool)
{
    APPLICATION->setStyleSheet(QString());
    QApplication::setStyle(new HintOverrideProxyStyle(QStyleFactory::create(qtTheme())));
    QApplication::setPalette(colorScheme());
    auto sheet = appStyleSheet();

    // The text colour cannot come from a stylesheet, so it is appended here when one was chosen.
    // The bare "*" selector wins over the theme's own rules by being later in the same sheet.
    const auto textColor = APPLICATION->settings()->get("ThemeTextColor").toString();
    if (!textColor.isEmpty())
        sheet += QStringLiteral("\n* { color: %1; }\n").arg(textColor);

    APPLICATION->setStyleSheet(sheet);
    QDir::setSearchPaths("theme", searchPaths());

    static PopupPaletteFilter* popupFilter = nullptr;
    if (!popupFilter) {
        popupFilter = new PopupPaletteFilter(APPLICATION);
        APPLICATION->installEventFilter(popupFilter);
    }

    if (APPLICATION->settings()->get("ThemeTextOutline").toBool()) {
        static TextOutlineFilter* outlineFilter = nullptr;
        if (!outlineFilter) {
            outlineFilter = new TextOutlineFilter(APPLICATION);
            APPLICATION->installEventFilter(outlineFilter);
        }
    }
}

QPalette ITheme::fadeInactive(QPalette in, qreal bias, QColor color)
{
    auto blend = [&in, bias, color](QPalette::ColorRole role) {
        QColor from = in.color(QPalette::Active, role);
        QColor blended = Rainbow::mix(from, color, bias);
        in.setColor(QPalette::Disabled, role, blended);
    };
    blend(QPalette::Window);
    blend(QPalette::WindowText);
    blend(QPalette::Base);
    blend(QPalette::AlternateBase);
    blend(QPalette::ToolTipBase);
    blend(QPalette::ToolTipText);
    blend(QPalette::Text);
    blend(QPalette::Button);
    blend(QPalette::ButtonText);
    blend(QPalette::BrightText);
    blend(QPalette::Link);
    blend(QPalette::Highlight);
    blend(QPalette::HighlightedText);
    return in;
}

LogColors ITheme::defaultLogColors(const QPalette& palette)
{
    LogColors result;

    const QColor& bg = palette.color(QPalette::Base);
    const QColor& fg = palette.color(QPalette::Text);

    auto blend = [bg, fg](QColor color) {
        if (Rainbow::luma(fg) > Rainbow::luma(bg)) {
            // for dark color schemes, produce a fitting color first
            color = Rainbow::tint(fg, color, 0.5);
        }
        // adapt contrast
        return Rainbow::mix(fg, color, 1);
    };

    result.background[MessageLevel::Fatal] = Qt::black;

    result.foreground[MessageLevel::Launcher] = blend(QColor("purple"));
    result.foreground[MessageLevel::Debug] = blend(QColor("green"));
    result.foreground[MessageLevel::Warning] = blend(QColor("orange"));
    result.foreground[MessageLevel::Error] = blend(QColor("red"));
    result.foreground[MessageLevel::Fatal] = blend(QColor("red"));

    return result;
}
