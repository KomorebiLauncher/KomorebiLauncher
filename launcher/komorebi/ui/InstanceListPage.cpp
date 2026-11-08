// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceListPage.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include "minecraft/MinecraftInstance.h"
#include "InstanceList.h"
#include <DesktopServices.h>
#include <QItemSelectionModel>
#include <QMenu>
#include <QSignalBlocker>

#include "Application.h"
#include "icons/IconList.h"
#include "komorebi/core/InstanceRepository.h"
#include "ui/kit/KPalette.h"
#include "ui/widgets/LauncherBackground.h"

using Komorebi::Core::InstanceInfo;
using Komorebi::Core::InstanceRepository;

namespace {

constexpr int kIconSize = 40;
constexpr int kRowHeight = 68;
constexpr int kMargin = 6;

}  // namespace

class InstanceListModel final : public QAbstractListModel {
   public:
    explicit InstanceListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    void setInstances(QList<InstanceInfo> instances)
    {
        beginResetModel();
        m_instances = std::move(instances);
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : int(m_instances.size()); }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_instances.size())
            return {};
        const auto& instance = m_instances.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                return instance.name;
            case Qt::ToolTipRole:
                return instance.path;
            default:
                return {};
        }
    }

    const InstanceInfo& at(int row) const { return m_instances.at(row); }

   private:
    QList<InstanceInfo> m_instances;
};

class InstanceCardDelegate final : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return QSize(option.rect.width(), kRowHeight);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        // The model has no Q_OBJECT -- it needs no signals of its own -- so this is a plain RTTI cast.
        const auto* model = dynamic_cast<const InstanceListModel*>(index.model());
        if (!model)
            return;
        const auto& instance = model->at(index.row());
        const auto colors = Komorebi::KColors::fromPalette(QApplication::palette());

        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRect card = option.rect.adjusted(kMargin, kMargin / 2, -kMargin, -kMargin / 2);

        QPainterPath path;
        path.addRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), Komorebi::Radius::Surface, Komorebi::Radius::Surface);
        painter->fillPath(path, selected ? colors.wash(26) : QColor(colors.paper.red(), colors.paper.green(), colors.paper.blue(), hovered ? 210 : 190));
        painter->setPen(QPen(selected ? colors.wash(130) : colors.line, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);

        // icon
        const QRect iconRect(card.left() + 14, card.center().y() - kIconSize / 2, kIconSize, kIconSize);
        const QIcon icon = APPLICATION->icons()->getIcon(instance.iconKey);
        if (!icon.isNull())
            icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);

        const int textLeft = iconRect.right() + 14;
        const int textWidth = card.right() - textLeft - 12;

        QFont title = option.font;
        title.setBold(true);
        painter->setFont(title);
        painter->setPen(instance.playable ? colors.ink : colors.inkSoft);
        painter->drawText(QRect(textLeft, card.top() + 12, textWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, instance.name);

        QStringList meta;
        if (!instance.minecraftVersion.isEmpty())
            meta << instance.minecraftVersion;
        if (instance.loader != Komorebi::Core::Loader::Vanilla)
            meta << QStringLiteral("%1 %2").arg(QString::fromLatin1(qPrintable(Komorebi::Core::loaderName(instance.loader))), instance.loaderVersion);
        if (!instance.group.isEmpty())
            meta << instance.group;
        if (!instance.playable)
            meta << QCoreApplication::translate("InstanceListPage", "broken");

        painter->setFont(option.font);
        painter->setPen(colors.inkSoft);
        painter->drawText(QRect(textLeft, card.top() + 34, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter, meta.join(QStringLiteral(" · ")));

        painter->restore();
    }
};

InstanceListPage::InstanceListPage(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);

    m_view = new QListView(this);
    m_view->setObjectName("coreInstanceList");
    m_view->setModel(m_model = new InstanceListModel(this));
    m_view->setItemDelegate(new InstanceCardDelegate(m_view));
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setMouseTracking(true);
    m_view->setUniformItemSizes(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_view, 1);

    m_empty = new QLabel(tr("No instances yet."), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->hide();
    layout->addWidget(m_empty);

    m_repository = Komorebi::Core::makeLegacyRepository(this);
    connect(m_repository.get(), &InstanceRepository::changed, this, &InstanceListPage::reload);

    // A single click is a selection: it says which instance the rest of the window is about.
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& index) {
        if (index.isValid())
            emit selectionChanged(m_model->at(index.row()).id);
    });

    // Double click opens the instance the way a double click does anywhere else in the launcher. The
    // context menu is the window's: it already holds every instance action, and a second menu here
    // would drift away from it the first time an action is added.
    connect(m_view, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        emit instanceActivated(m_model->at(index.row()).id);
    });
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_view->indexAt(pos);
        if (!index.isValid())
            return;
        emit contextMenuRequested(m_model->at(index.row()).id, m_view->viewport()->mapToGlobal(pos));
    });

    reload();
}

InstanceListPage::~InstanceListPage() = default;

void InstanceListPage::reload()
{
    const auto instances = m_repository->instances();
    m_model->setInstances(instances);
    m_empty->setVisible(instances.isEmpty());
    m_view->setVisible(!instances.isEmpty());
}

void InstanceListPage::selectInstance(const QString& id)
{
    if (id.isEmpty() || !m_view->selectionModel())
        return;

    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (m_model->at(row).id != id)
            continue;
        if (m_view->currentIndex().row() == row)
            return;
        // This selection came from the window, not from the player: reporting it back would bounce
        // between the two lists forever.
        const QSignalBlocker blocked(m_view->selectionModel());
        m_view->setCurrentIndex(m_model->index(row));
        return;
    }
}

void InstanceListPage::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    LauncherBackground::paint(this, LauncherBackground::resolve());
}

void showCoreInstanceList(QWidget* parent)
{
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QCoreApplication::translate("InstanceListPage", "Instances (core preview)"));
    dialog->resize(520, 620);

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* page = new InstanceListPage(dialog);
    layout->addWidget(page);

    // The page only knows what was wanted; the shell knows what to do about it.
    const auto find = [](const QString& id) -> MinecraftInstance* {
        auto* instances = APPLICATION->instances();
        return instances ? instances->getInstanceById(id) : nullptr;
    };
    QObject::connect(page, &InstanceListPage::instanceActivated, dialog, [find](const QString& id) {
        if (auto* instance = find(id))
            APPLICATION->showInstanceWindow(instance);
    });
    QObject::connect(page, &InstanceListPage::editRequested, dialog, [find](const QString& id) {
        if (auto* instance = find(id))
            APPLICATION->showInstanceWindow(instance, QStringLiteral("settings"));
    });
    QObject::connect(page, &InstanceListPage::launchRequested, dialog, [find](const QString& id) {
        if (auto* instance = find(id))
            APPLICATION->launch(instance);
    });
    QObject::connect(page, &InstanceListPage::openFolderRequested, dialog, [find](const QString& id) {
        if (auto* instance = find(id))
            DesktopServices::openPath(instance->instanceRoot());
    });

    dialog->show();
}

