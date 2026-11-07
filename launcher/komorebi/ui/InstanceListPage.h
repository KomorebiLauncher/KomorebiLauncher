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
