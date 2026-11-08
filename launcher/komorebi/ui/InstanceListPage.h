// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The instance list, written against the core's repository rather than the launcher's own list.
 *  Read-only for now: it shows what the repository says and nothing else, so the interface itself
 *  is what gets exercised.
 */

#pragma once

#include <memory>

#include <QPoint>
#include <QWidget>

namespace Komorebi::Core {
class InstanceRepository;
}

class QListView;
class InstanceListModel;
class QLabel;

class InstanceListPage : public QWidget {
    Q_OBJECT

   public:
    explicit InstanceListPage(QWidget* parent = nullptr);
    ~InstanceListPage() override;

    /// Re-reads the repository; called on show and when it reports a change.
    void reload();

    /// Marks the card for this instance, without reporting it back as a new selection.
    void selectInstance(const QString& id);

   signals:
    /// The player asked for something that belongs to the shell, not to the list: the page says
    /// what was wanted and the window decides what that means.
    void instanceActivated(const QString& id);
    void launchRequested(const QString& id);
    void editRequested(const QString& id);
    void openFolderRequested(const QString& id);
    /// The player asked for the context menu; the window owns the actions it should contain.
    void contextMenuRequested(const QString& id, const QPoint& globalPos);

    /// The card under the cursor changed; the window keeps the toolbar and the details in step.
    void selectionChanged(const QString& id);

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    std::unique_ptr<Komorebi::Core::InstanceRepository> m_repository;
    InstanceListModel* m_model = nullptr;
    QListView* m_view = nullptr;
    QLabel* m_empty = nullptr;
};

/// Opens the page in a window of its own, for comparing it with the list that is still in use.
void showCoreInstanceList(QWidget* parent);
