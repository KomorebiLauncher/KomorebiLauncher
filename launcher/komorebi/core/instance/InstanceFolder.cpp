// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/core/instance/InstanceFolder.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QSet>

namespace Komorebi::Core {

namespace {

/// Components that are not mod loaders: the game itself, its Java runtime and the graphics library.
QSet<QString> nonLoaderComponents()
{
    return { QStringLiteral("net.minecraft"), QStringLiteral("net.minecraft.java"), QStringLiteral("org.lwjgl"),
             QStringLiteral("org.lwjgl3") };
}

QString readInstanceName(const QString& directory, const QString& fallback)
{
    QFile file(QDir(directory).filePath(QStringLiteral("instance.cfg")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return fallback;

    const auto text = QString::fromUtf8(file.readAll());
    for (const auto& rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(QLatin1String("name=")))
            continue;
        const QString name = line.mid(5).trimmed();
        if (!name.isEmpty())
            return name;
    }
    return fallback;
}

}  // namespace

std::optional<ComponentRef> InstanceFolder::component(const QString& uid) const
{
    for (const auto& ref : components) {
        if (ref.uid == uid)
            return ref;
    }
    return std::nullopt;
}

QString InstanceFolder::minecraftVersion() const
{
    const auto ref = component(QStringLiteral("net.minecraft"));
    return ref ? ref->version : QString();
}

QString InstanceFolder::loaderUid() const
{
    const auto known = nonLoaderComponents();
    for (const auto& ref : components) {
        if (!known.contains(ref.uid))
            return ref.uid;
    }
    return {};
}

std::optional<InstanceFolder> InstanceFolder::read(const QString& directory, QString* error)
{
    const auto fail = [error](const QString& reason) -> std::optional<InstanceFolder> {
        if (error)
            *error = reason;
        return std::nullopt;
    };

    if (directory.isEmpty())
        return fail(QStringLiteral("no instance directory was given"));

    const QDir dir(directory);
    if (!dir.exists())
        return fail(QStringLiteral("%1 is not a directory").arg(directory));

    QFile pack(dir.filePath(QStringLiteral("mmc-pack.json")));
    if (!pack.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("%1 has no mmc-pack.json").arg(directory));

    QJsonParseError parseError{};
    const auto document = QJsonDocument::fromJson(pack.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("%1/mmc-pack.json is not readable: %2").arg(directory, parseError.errorString()));

    InstanceFolder folder;
    folder.directory = dir.absolutePath();
    folder.id = dir.dirName();
    folder.name = readInstanceName(folder.directory, folder.id);

    for (const auto& entry : document.object().value(QStringLiteral("components")).toArray()) {
        const auto object = entry.toObject();
        ComponentRef ref;
        ref.uid = object.value(QStringLiteral("uid")).toString();
        ref.version = object.value(QStringLiteral("version")).toString();
        if (!ref.uid.isEmpty() && !ref.version.isEmpty())
            folder.components.append(ref);
    }

    if (folder.components.isEmpty())
        return fail(QStringLiteral("%1/mmc-pack.json lists no components").arg(directory));

    return folder;
}

}  // namespace Komorebi::Core
