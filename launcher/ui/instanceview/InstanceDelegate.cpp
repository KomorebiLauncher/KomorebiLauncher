// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "InstanceDelegate.h"

#include "ui/kit/KPalette.h"
#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <QIcon>
#include <QPoint>
#include <QTextEdit>
#include "BaseInstance.h"
#include "InstanceList.h"

namespace {
// Geometry of an instance card, in the item's own coordinates. The plate covers the whole item, so
// everything drawn on it has to keep clear of the rounded top edge -- the icon used to start at the
// very top and was painted over the border, which read as the icon being stuck through the card.
constexpr int cardInset = 3;      // plate distance from the item rect
constexpr int cardPadTop = 10;    // plate's top edge to the icon
constexpr int iconSize = 48;
constexpr int iconTextGap = 6;    // icon to the label
constexpr int cardPadBottom = 12;
constexpr int compactWidth = 120;  // narrower than this and the label is dropped
}  // namespace

// Origin: Qt
static void viewItemTextLayout(QTextLayout& textLayout, int lineWidth, qreal& height, qreal& widthUsed)
{
    height = 0;
    widthUsed = 0;
    textLayout.beginLayout();
    QString str = textLayout.text();
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        if (line.textLength() == 0)
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
    }
    textLayout.endLayout();
}

ListViewDelegate::ListViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void drawSelectionRect(QPainter* painter, const QStyleOptionViewItem& option)
{
    // Each instance is a card rather than a row: a translucent plate with a soft outline, and the
    // selected one gets a warm wash plus a bar on its left edge. Drawn in the item delegate because
    // the plate has to follow the row's own size, which a stylesheet cannot express.
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    QRect card = option.rect.adjusted(cardInset, cardInset, -cardInset, -cardInset);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // The card follows the control kit: paper, a hairline, the surface radius, and the plum wash
    // for hover and selection rather than colours of its own.
    // The application palette, not the view's: the stylesheet makes the view's own surface
    // transparent, and a transparent Window colour reads as black.
    const Komorebi::KColors colors = Komorebi::KColors::fromPalette(QApplication::palette());
    const QColor paint(colors.paper.red(), colors.paper.green(), colors.paper.blue(), selected ? 230 : (hovered ? 205 : 190));
    const QColor border = selected ? colors.wash(130) : colors.line;

    painter->setPen(QPen(border, 1));
    painter->setBrush(selected ? colors.wash(26) : paint);
    painter->drawRoundedRect(card, Komorebi::Radius::Surface, Komorebi::Radius::Surface);

    if (selected) {
        QPainterPath bar;
        bar.addRoundedRect(QRectF(card.left() + 1, card.top() + 12, 3, card.height() - 24), 1.5, 1.5);
        painter->fillPath(bar, colors.plum);
    }

    painter->restore();
}

void drawBadges(QPainter* painter, const QStyleOptionViewItem& option, BaseInstance* instance, QIcon::Mode mode, QIcon::State state)
{
    QList<QString> pixmaps;
    if (instance->isRunning()) {
        pixmaps.append("status-running");
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        pixmaps.append("status-bad");
    }

    static const int itemSide = 24;
    static const int spacing = 1;
    const int itemsPerRow = qMax(1, qFloor(double(option.rect.width() + spacing) / double(itemSide + spacing)));
    const int rows = qCeil((double)pixmaps.size() / (double)itemsPerRow);
    QListIterator<QString> it(pixmaps);
    painter->translate(option.rect.topLeft() + QPoint(-cardInset, cardInset));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < itemsPerRow; ++x) {
            if (!it.hasNext()) {
                return;
            }
            // FIXME: inject this.
            auto icon = QIcon::fromTheme(it.next());
            // opt.icon.paint(painter, iconbox, Qt::AlignCenter, mode, state);
            const QPixmap pixmap;
            // itemSide
            QRect badgeRect(option.rect.width() - x * itemSide + qMax(x - 1, 0) * spacing - itemSide,
                            y * itemSide + qMax(y - 1, 0) * spacing, itemSide, itemSide);
            icon.paint(painter, badgeRect, Qt::AlignCenter, mode, state);
        }
    }
    painter->translate(-(option.rect.topLeft() + QPoint(-cardInset, cardInset)));
}

static QSize viewItemTextSize(const QStyleOptionViewItem* option)
{
    QStyle* style = option->widget ? option->widget->style() : QApplication::style();
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(option->font);
    textLayout.setText(option->text);
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, option, option->widget) + 1;
    QRect bounds(0, 0, 100 - 2 * textMargin, 600);
    qreal height = 0, widthUsed = 0;
    viewItemTextLayout(textLayout, bounds.width(), height, widthUsed);
    const QSize size(qCeil(widthUsed), qCeil(height));
    return QSize(size.width() + 2 * textMargin, size.height());
}

void ListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    painter->save();
    painter->setClipRect(opt.rect);

    opt.features |= QStyleOptionViewItem::WrapText;
    opt.text = index.data().toString();
    opt.textElideMode = Qt::ElideRight;
    opt.displayAlignment = Qt::AlignTop | Qt::AlignHCenter;

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, opt.widget) + 1;

    // Everything on the plate starts below its rounded top edge: the icon first, the label under
    // it, with the same breathing room at the bottom.
    const int iconTop = opt.rect.top() + cardInset + cardPadTop;
    QRect iconbox(opt.rect.left(), iconTop, opt.rect.width(), iconSize);

    QRect textRect = opt.rect;
    textRect.setTop(iconTop + iconSize + iconTextGap);
    textRect.adjust(textMargin, 0, -textMargin, -cardInset - cardPadBottom);

    // draw background
    drawSelectionRect(painter, opt);

    // icon mode and state, also used for badges
    QIcon::Mode mode = QIcon::Normal;
    if (!(opt.state & QStyle::State_Enabled))
        mode = QIcon::Disabled;
    else if (opt.state & QStyle::State_Selected)
        mode = QIcon::Selected;
    QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

    // draw the icon
    opt.icon.paint(painter, iconbox, Qt::AlignCenter, mode, state);

    // In a rail there is no room beside the icon for a label, and a squeezed one is worse than
    // none: the card becomes the icon and the name lives in the tooltip.
    const bool compact = opt.rect.width() < compactWidth;

    if (!compact) {
        // set the text colors
        QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
            cg = QPalette::Inactive;
        // The card carries a light plate, so the text stays dark in both states -- a white label on
        // a pale selected card would be unreadable.
        painter->setPen(opt.palette.color(cg, QPalette::Text));

        // draw the text
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textOption.setTextDirection(opt.direction);
        textOption.setAlignment(QStyle::visualAlignment(opt.direction, opt.displayAlignment));
        QTextLayout textLayout;
        textLayout.setTextOption(textOption);
        textLayout.setFont(opt.font);
        textLayout.setText(opt.text);

        qreal width, height;
        viewItemTextLayout(textLayout, textRect.width(), height, width);

        const int lineCount = textLayout.lineCount();

        const QRect layoutRect =
            QStyle::alignedRect(opt.direction, opt.displayAlignment, QSize(textRect.width(), int(height)), textRect);
        const QPointF position = layoutRect.topLeft();
        for (int i = 0; i < lineCount; ++i) {
            const QTextLine line = textLayout.lineAt(i);
            line.draw(painter, position);
        }
    }

    // FIXME: this really has no business of being here. Make generic.
    auto instance = (BaseInstance*)index.data(InstanceList::InstancePointerRole).value<void*>();
    if (instance) {
        if (!compact) {
            drawBadges(painter, opt, instance, mode, state);
        } else {
            // A 24px badge would sit on top of the icon in a card this narrow, so the state is a
            // dot in the corner instead.
            QColor dot;
            if (instance->isRunning())
                dot = QColor(0x3F, 0xA0, 0x62);
            else if (instance->hasCrashed() || instance->hasVersionBroken())
                dot = QColor(0xB0, 0x3A, 0x3A);

            if (dot.isValid()) {
                const QRect card = opt.rect.adjusted(cardInset, cardInset, -cardInset, -cardInset);
                painter->setPen(Qt::NoPen);
                painter->setBrush(dot);
                painter->drawEllipse(QPointF(card.right() - 7, card.bottom() - 7), 4, 4);
            }
        }
    }

    painter->restore();
}

QSize ListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.features |= QStyleOptionViewItem::WrapText;
    opt.text = index.data().toString();
    opt.textElideMode = Qt::ElideRight;
    opt.displayAlignment = Qt::AlignTop | Qt::AlignHCenter;

    // The width follows the list, not a constant: a card that is 100px wide cannot show a name and
    // was what made the list look like a column of slivers. The floor is low enough for the rail,
    // where the card is meant to be a bare icon.
    const int available = option.widget ? option.widget->width() : 240;
    const int width = qMax(48, available - 16);

    // The plate covers the item, and everything inside it is placed from the same offsets the
    // painter uses, so the height stays in step with what is actually drawn.
    int height = 2 * cardInset + cardPadTop + iconSize + cardPadBottom;
    if (width >= compactWidth) {
        height += iconTextGap;
        height += viewItemTextSize(&opt).height();
    }
    return QSize(width, height);
}

class NoReturnTextEdit : public QTextEdit {
    Q_OBJECT
   public:
    explicit NoReturnTextEdit(QWidget* parent) : QTextEdit(parent)
    {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    }
    bool event(QEvent* event) override
    {
        auto eventType = event->type();
        if (eventType == QEvent::KeyPress || eventType == QEvent::KeyRelease) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && eventType == QEvent::KeyPress) {
                emit editingDone();
                return true;
            }
            if (key == Qt::Key_Tab) {
                return true;
            }
        }
        return QTextEdit::event(event);
    }
   signals:
    void editingDone();
};

void ListViewDelegate::updateEditorGeometry(QWidget* editor,
                                            const QStyleOptionViewItem& option,
                                            [[maybe_unused]] const QModelIndex& index) const
{
    // Same box the painter uses for the label, so the editor appears exactly where the text was --
    // and inside the card: the label box sits on the seam between the icon and the bottom padding,
    // so using it directly put part of the editor outside the card and over whatever is below.
    const int top = option.rect.top() + cardInset + cardPadTop + iconSize + iconTextGap;
    const int bottom = option.rect.bottom() - cardInset - cardPadBottom;
    const int height = qMax(22, bottom - top);
    const QRect textRect(option.rect.left() + cardInset + 2, top, option.rect.width() - 2 * (cardInset + 2), height);
    editor->setGeometry(textRect);
}

void ListViewDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto text = index.data(Qt::EditRole).toString();
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    realEditor->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    realEditor->append(text);
    realEditor->selectAll();
    realEditor->document()->clearUndoRedoStacks();
}

void ListViewDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    QString text = realEditor->toPlainText();
    text.replace(QChar('\n'), QChar(' '));
    text = text.trimmed();
    // Prevent instance names longer than 128 chars
    text.truncate(128);
    if (text.size() != 0) {
        const auto before = model->data(index).toString();
        model->setData(index, text);
        emit textChanged(before, text);
    }
}

QWidget* ListViewDelegate::createEditor(QWidget* parent,
                                        [[maybe_unused]] const QStyleOptionViewItem& option,
                                        [[maybe_unused]] const QModelIndex& index) const
{
    auto editor = new NoReturnTextEdit(parent);
    connect(editor, &NoReturnTextEdit::editingDone, this, &ListViewDelegate::editingDone);
    return editor;
}

void ListViewDelegate::editingDone()
{
    NoReturnTextEdit* editor = qobject_cast<NoReturnTextEdit*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

#include "InstanceDelegate.moc"
