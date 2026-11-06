// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Komorebi Launcher - Minecraft Launcher
 *  Copyright (C) 2026 KomorebiLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "ImgbbAPI.h"

#include <QFile>
#include <QHttpMultiPart>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <expected>

#include "Application.h"
#include "Json.h"
#include "net/RPCSink.h"
#include "net/RawHeaderProxy.h"
#include "settings/SettingsObject.h"

#include <QDebug>
#include <QHttpPart>
#include <QList>

namespace Imgbb {

auto isConfigured() -> bool
{
    auto* app = APPLICATION_DYN;
    if (!app)
        return false;
    return !app->settings()->get(kApiKeySetting).toString().trimmed().isEmpty();
}

std::pair<Net::Request::Ptr, QString*> makeUpload(ScreenShot::Ptr shot)
{
    auto getPayload = [shot] -> std::expected<QHttpMultiPart*, QString> {
        auto* file = new QFile(shot->m_file.absoluteFilePath());
        if (!file->open(QFile::ReadOnly)) {
            qWarning() << "Could not open file" << shot->m_file.absoluteFilePath() << "for reading:" << file->errorString();
            file->deleteLater();
            return std::unexpected(
                QObject::tr("Could not open file %1 for reading: %2").arg(shot->m_file.absoluteFilePath(), file->errorString()));
        }

        // ImgBB takes the file itself under the `image` field; sending the bytes rather than a
        // base64 string keeps the payload a third of the size and under their 32 MB cap longer.
        QHttpPart filePart;
        filePart.setBodyDevice(file);
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
        filePart.setHeader(QNetworkRequest::ContentDispositionHeader, R"(form-data; name="image"; filename=")" + file->fileName() + "\"");

        auto* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        file->setParent(multipart);
        multipart->append(filePart);
        return multipart;
    };

    auto parseFunc = [shot](const QByteArray& response) -> Net::RPC::Sink<QString>::ParseResult {
        auto doc = Json::requireDocument(response);
        if (!doc) {
            qDebug() << "imgbb server did not reply with JSON" << doc.error();
            return std::unexpected("Invalid json reply");
        }
        auto object = doc->object();
        // ImgBB reports failure inside a 200 reply, so the flag has to be checked instead of the
        // status code; the error text is worth surfacing because it names a bad key or a full quota.
        if (!object.value("success").toBool()) {
            const auto error = object.value("error").toObject().value("message").toString();
            qDebug() << "Screenshot upload not successful:" << doc->toJson();
            return std::unexpected(error.isEmpty() ? QStringLiteral("Screenshot was not uploaded successfully") : error);
        }
        const auto data = object.value("data").toObject();
        auto url = data.value("url").toString();
        if (url.isEmpty())
            url = data.value("display_url").toString();
        shot->m_url = url;
        return url;
    };

    auto endpoint = QString::fromLatin1(kUploadEndpoint);
    if (auto* app = APPLICATION_DYN) {
        const auto key = app->settings()->get(kApiKeySetting).toString().trimmed();
        if (!key.isEmpty()) {
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("key"), key);
            endpoint += QLatin1Char('?') + query.toString(QUrl::FullyEncoded);
        }
    }

    auto dl = Net::Request::makeCustomRequest({ .method = Net::HttpMethod::Post, .url = QUrl(endpoint), .data = getPayload });
    auto sink = std::make_unique<Net::RPC::Sink<QString>>(std::move(parseFunc));
    auto* result = sink->result();
    dl->setSink(std::move(sink));
    dl->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(QList<Net::HeaderPair>{ { .headerName = "Accept",
                                                                                       .headerValue = "application/json" } }));
    return { dl, result };
}

}  // namespace Imgbb
