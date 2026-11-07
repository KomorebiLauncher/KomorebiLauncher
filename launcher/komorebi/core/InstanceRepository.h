// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *
 *  The core's instance repository. The interface is ours; for now the implementation behind it is
 *  the one the launcher already had, so a new page can be written against the interface before
 *  anything is migrated, and the implementation can then be replaced without touching the page.
 */

#pragma once

#include <memory>
#include <optional>

#include <QList>
#include <QObject>
#include <QStringList>

#include "Instance.h"

namespace Komorebi::Core {

class InstanceRepository : public QObject {
    Q_OBJECT

   public:
    using QObject::QObject;
    ~InstanceRepository() override = default;

    /// Every instance, in the order the launcher would show them.
    virtual QList<InstanceInfo> instances() const = 0;

    virtual std::optional<InstanceInfo> find(const QString& id) const = 0;

    /// Groups in the order they were created; an instance may name one of these or nothing.
    virtual QStringList groups() const = 0;

   signals:
    /// The list changed: an instance was added, removed, renamed or moved between groups.
    void changed();
};

/// The repository backed by the launcher's existing instance list.
std::unique_ptr<InstanceRepository> makeLegacyRepository(QObject* parent = nullptr);

}  // namespace Komorebi::Core
