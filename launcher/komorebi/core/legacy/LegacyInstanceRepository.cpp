// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceRepository.h"

#include <QDebug>

#include "Application.h"
#include "BaseInstance.h"
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

/// Reads one instance through the old types. This is the only place that knows about them.
InstanceInfo describe(BaseInstance* instance)
{
    InstanceInfo info;
    if (!instance)
        return info;

    info.id = instance->id();
    info.name = instance->name();
    info.group = instance->group();
    info.iconKey = instance->iconKey();
    info.path = instance->instanceRoot();
    info.playable = !instance->hasVersionBroken();

    if (auto* minecraft = dynamic_cast<MinecraftInstance*>(instance)) {
        if (auto* profile = minecraft->getPackProfile()) {
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
            info.loader = Loader::Vanilla;
            for (const auto& candidate : loaders) {
                const auto version = profile->getComponentVersion(QString::fromLatin1(candidate.uid));
                if (!version.isEmpty()) {
                    info.loader = candidate.loader;
                    info.loaderVersion = version;
                    break;
                }
            }
        }
    }
    return info;
}

class LegacyRepository final : public InstanceRepository {
   public:
    explicit LegacyRepository(QObject* parent) : InstanceRepository(parent) {}

    QList<InstanceInfo> instances() const override
    {
        QList<InstanceInfo> result;
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return result;
        for (auto* instance : app->instances()->getInstances()) {
            auto info = describe(instance);
            if (!info.id.isEmpty())
                result.append(info);
        }
        return result;
    }

    std::optional<InstanceInfo> find(const QString& id) const override
    {
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return std::nullopt;
        auto* instance = app->instances()->getInstanceById(id);
        if (!instance)
            return std::nullopt;
        return describe(instance);
    }

    QStringList groups() const override
    {
        auto* app = APPLICATION_DYN;
        if (!app || !app->instances())
            return {};
        return app->instances()->getGroups();
    }
};

}  // namespace

std::unique_ptr<InstanceRepository> makeLegacyRepository(QObject* parent)
{
    return std::make_unique<LegacyRepository>(parent);
}

}  // namespace Komorebi::Core
