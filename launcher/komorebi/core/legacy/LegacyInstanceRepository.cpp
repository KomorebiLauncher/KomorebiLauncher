// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/InstanceRepository.h"

#include <QDebug>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

namespace Komorebi::Core {

QString loaderName(Loader loader)
{
    switch (loader) {
        case Loader::Vanilla:
            return QStringLiteral("vanilla");
        case Loader::Forge:
            return QStringLiteral("forge");
        case Loader::NeoForge:
            return QStringLiteral("neoforge");
        case Loader::Fabric:
            return QStringLiteral("fabric");
        case Loader::Quilt:
            return QStringLiteral("quilt");
        case Loader::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

namespace {

/**
 * Reads one instance through the old types.
 *
 * This is the only place in the new layer that knows they exist: everything above works with
 * InstanceInfo, so the day the implementation is replaced this file is what goes away.
 */
InstanceInfo describe(MinecraftInstance* instance, const QString& group)
{
    InstanceInfo info;
    if (!instance)
        return info;

    info.id = instance->id();
    info.name = instance->name();
    info.group = group;
    info.iconKey = instance->iconKey();
    info.path = instance->instanceRoot();
    info.playable = instance->canLaunch();

    if (auto* profile = instance->getPackProfile()) {
        info.minecraftVersion = profile->getComponentVersion(QStringLiteral("net.minecraft"));

        const struct {
            const char* uid;
            Loader loader;
        } loaders[] = {
            { "net.minecraftforge", Loader::Forge },
            { "net.neoforged", Loader::NeoForge },
            { "net.fabricmc.fabric-loader", Loader::Fabric },
            { "org.quiltmc.quilt-loader", Loader::Quilt },
        };
        for (const auto& candidate : loaders) {
            const auto version = profile->getComponentVersion(QString::fromLatin1(candidate.uid));
            if (!version.isEmpty()) {
                info.loader = candidate.loader;
                info.loaderVersion = version;
                break;
            }
        }
    }
    return info;
}

class LegacyRepository final : public InstanceRepository {
   public:
    explicit LegacyRepository(QObject* parent) : InstanceRepository(parent)
    {
        // The list behind this repository is the launcher's own, and it is the one that knows when an
        // instance was added, removed, renamed or moved. Without hearing from it the new page would
        // show what existed the moment the window opened, for as long as the window stays open.
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return;
        auto* list = app->instances();
        connect(list, &QAbstractItemModel::modelReset, this, &LegacyRepository::notifyChanged);
        connect(list, &QAbstractItemModel::rowsInserted, this, &LegacyRepository::notifyChanged);
        connect(list, &QAbstractItemModel::rowsRemoved, this, &LegacyRepository::notifyChanged);
        connect(list, &QAbstractItemModel::dataChanged, this, &LegacyRepository::notifyChanged);
        connect(list, &QAbstractItemModel::layoutChanged, this, &LegacyRepository::notifyChanged);
    }

    QList<InstanceInfo> instances() const override
    {
        QList<InstanceInfo> result;
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return result;

        auto* list = app->instances();
        for (int index = 0; index < list->count(); ++index) {
            auto* instance = list->at(index);
            if (!instance)
                continue;
            result.append(describe(instance, list->getInstanceGroup(instance->id())));
        }
        return result;
    }

    std::optional<InstanceInfo> find(const QString& id) const override
    {
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return std::nullopt;

        auto* list = app->instances();
        auto* instance = list->getInstanceById(id);
        if (!instance)
            return std::nullopt;
        return describe(instance, list->getInstanceGroup(id));
    }

    QStringList groups() const override
    {
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return {};
        return app->instances()->getGroups();
    }

   private:
    void notifyChanged() { emit changed(); }
};

}  // namespace

std::unique_ptr<InstanceRepository> makeLegacyRepository(QObject* parent)
{
    return std::make_unique<LegacyRepository>(parent);
}

}  // namespace Komorebi::Core
