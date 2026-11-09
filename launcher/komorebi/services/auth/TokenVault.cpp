// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/services/auth/TokenVault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

#if defined(Q_OS_WIN)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    include <wincrypt.h>
#endif

namespace Komorebi::Services {

namespace {

/// The refresh token, sealed to this user. Empty in, empty out.
QString seal(const QString& secret)
{
    if (secret.isEmpty())
        return {};
#if defined(Q_OS_WIN)
    QByteArray plain = secret.toUtf8();
    DATA_BLOB input{ static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(plain.data()) };
    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"KomorebiLauncher", nullptr, nullptr, nullptr, 0, &output))
        return {};
    const QByteArray sealed(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return QString::fromLatin1(sealed.toBase64());
#else
    // No keychain backend yet; encoded is all this file offers off Windows.
    return QStringLiteral("plain:") + QString::fromLatin1(secret.toUtf8().toBase64());
#endif
}

QString unseal(const QString& sealed)
{
    if (sealed.isEmpty())
        return {};
    if (sealed.startsWith(QLatin1String("plain:")))
        return QString::fromUtf8(QByteArray::fromBase64(sealed.mid(6).toLatin1()));
#if defined(Q_OS_WIN)
    const QByteArray bytes = QByteArray::fromBase64(sealed.toLatin1());
    DATA_BLOB input{ static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(const_cast<char*>(bytes.constData())) };
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output))
        return {};
    const QString secret = QString::fromUtf8(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return secret;
#else
    return {};
#endif
}

}  // namespace

TokenVault::TokenVault(QString path) : m_path(std::move(path))
{
}

TokenVault TokenVault::forDataRoot(const QString& dataRoot)
{
    return TokenVault(QDir(dataRoot).filePath(QStringLiteral("komorebi-accounts.json")));
}

QList<AuthSession> TokenVault::load(QString* error) const
{
    QList<AuthSession> accounts;

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("could not open %1").arg(m_path);
        return accounts;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error)
            *error = QStringLiteral("%1 is not a readable accounts file").arg(m_path);
        return accounts;
    }

    for (const auto& entry : document.object().value(QStringLiteral("accounts")).toArray()) {
        const QJsonObject object = entry.toObject();
        AuthSession account;
        account.username = object.value(QStringLiteral("name")).toString();
        account.uuid = object.value(QStringLiteral("uuid")).toString();
        account.xuid = object.value(QStringLiteral("xuid")).toString();
        account.refreshToken = unseal(object.value(QStringLiteral("refreshToken")).toString());
        if (account.uuid.isEmpty())
            continue;
        accounts.append(account);
    }
    return accounts;
}

bool TokenVault::save(const QList<AuthSession>& accounts, QString* error) const
{
    QJsonArray list;
    for (const auto& account : accounts) {
        const QString sealed = seal(account.refreshToken);
        if (!account.refreshToken.isEmpty() && sealed.isEmpty()) {
            if (error)
                *error = QStringLiteral("the refresh token of %1 could not be sealed").arg(account.username);
            return false;
        }

        QJsonObject object;
        object.insert(QStringLiteral("name"), account.username);
        object.insert(QStringLiteral("uuid"), account.uuid);
        object.insert(QStringLiteral("xuid"), account.xuid);
        object.insert(QStringLiteral("refreshToken"), sealed);
        list.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("accounts"), list);

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("could not write %1").arg(m_path);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("could not finish writing %1").arg(m_path);
        return false;
    }
    return true;
}

bool TokenVault::upsert(const AuthSession& account, QString* error) const
{
    QList<AuthSession> accounts = load();
    for (auto& existing : accounts) {
        if (existing.uuid == account.uuid) {
            existing = account;
            return save(accounts, error);
        }
    }
    accounts.append(account);
    return save(accounts, error);
}

bool TokenVault::remove(const QString& uuid, QString* error) const
{
    QList<AuthSession> accounts = load();
    for (int index = accounts.size() - 1; index >= 0; --index) {
        if (accounts.at(index).uuid == uuid)
            accounts.removeAt(index);
    }
    return save(accounts, error);
}

}  // namespace Komorebi::Services
