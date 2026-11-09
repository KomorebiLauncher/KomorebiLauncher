// SPDX-License-Identifier: GPL-3.0-only
#include "komorebi/ui/NewInstanceDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "komorebi/core/version/VersionManifest.h"
#include "komorebi/services/download/DownloadSource.h"
#include "komorebi/services/install/InstanceCreator.h"

namespace {

const auto kManifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");

struct LoaderChoice {
    QString id;    ///< what the creator calls it; empty is the game alone
    QString label; ///< what the player reads
    QString versionHint;
};

const QList<LoaderChoice>& loaderChoices()
{
    static const QList<LoaderChoice> choices = {
        { QString(), QObject::tr("None (vanilla)"), QString() },
        { QStringLiteral("fabric"), QObject::tr("Fabric"), QObject::tr("leave empty for the newest") },
        { QStringLiteral("quilt"), QObject::tr("Quilt"), QObject::tr("leave empty for the newest") },
        { QStringLiteral("forge"), QObject::tr("Forge"), QObject::tr("required, for example 47.4.0") },
        { QStringLiteral("neoforge"), QObject::tr("NeoForge"), QObject::tr("leave empty for the newest") },
    };
    return choices;
}

}  // namespace

KomorebiNewInstanceDialog::KomorebiNewInstanceDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("New instance"));
    setMinimumWidth(460);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("what to call it"));
    form->addRow(tr("Name"), m_name);

    m_version = new QComboBox(this);
    m_version->setPlaceholderText(tr("reading the version list"));
    m_version->setEnabled(false);
    form->addRow(tr("Minecraft"), m_version);

    m_loader = new QComboBox(this);
    for (const auto& choice : loaderChoices())
        m_loader->addItem(choice.label, choice.id);
    form->addRow(tr("Loader"), m_loader);

    m_loaderVersion = new QLineEdit(this);
    m_loaderVersionLabel = new QLabel(tr("Loader version"), this);
    form->addRow(m_loaderVersionLabel, m_loaderVersion);
    layout->addLayout(form);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    auto* buttons = new QDialogButtonBox(this);
    m_create = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    m_cancel = buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(m_loader, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { loaderChanged(); });
    connect(m_create, &QPushButton::clicked, this, &KomorebiNewInstanceDialog::startCreation);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);

    loaderChanged();
    fetchVersions();
}

KomorebiNewInstanceDialog::~KomorebiNewInstanceDialog() = default;

void KomorebiNewInstanceDialog::fetchVersions()
{
    m_status->setText(tr("Reading the list of Minecraft versions"));
    m_versionAttempt = 0;
    requestVersions();
}

void KomorebiNewInstanceDialog::requestVersions()
{
    // The address the download source names is tried first and the manifest address second: a mirror
    // that drops the connection should not leave the dialog with nothing to choose from.
    QUrl url = QUrl(kManifestUrl);
    if (m_versionAttempt == 0) {
        const auto source =
            Komorebi::Services::downloadSourceFromId(APPLICATION->settings()->get("DownloadSource").toString());
        url = Komorebi::Services::sourceUrl(url, source);
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = APPLICATION->network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() != QNetworkReply::NoError && m_versionAttempt == 0) {
            ++m_versionAttempt;
            reply->deleteLater();
            requestVersions();
            return;
        }
        versionsArrived(reply);
    });
}

void KomorebiNewInstanceDialog::versionsArrived(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_status->setText(tr("Could not read the version list: %1").arg(reply->errorString()));
        m_create->setEnabled(false);
        return;
    }

    const auto document = QJsonDocument::fromJson(reply->readAll()).object();
    QString error;
    const auto manifest = Komorebi::Core::parseVersionManifest(document, &error);
    if (!manifest) {
        m_status->setText(error);
        m_create->setEnabled(false);
        return;
    }

    int newest = -1;
    for (const auto& version : manifest->versions) {
        QString label = version.id;
        if (version.type != QLatin1String("release"))
            label += QStringLiteral(" (%1)").arg(version.type);
        if (version.id == manifest->latestRelease)
            newest = m_version->count();
        m_version->addItem(label, version.id);
    }
    // A list the player has to open before they know what is chosen reads as if nothing were chosen.
    m_version->setCurrentIndex(newest >= 0 ? newest : 0);
    m_version->setEnabled(true);
    m_status->setText(tr("%1 versions. Newest release: %2").arg(manifest->versions.size()).arg(manifest->latestRelease));
}

void KomorebiNewInstanceDialog::loaderChanged()
{
    const QString loader = m_loader->currentData().toString();
    const auto choices = loaderChoices();
    const auto found = std::find_if(choices.begin(), choices.end(), [&loader](const LoaderChoice& choice) {
        return choice.id == loader;
    });
    if (found == choices.end())
        return;

    const bool needsVersion = loader == QLatin1String("forge");
    m_loaderVersionLabel->setVisible(!loader.isEmpty());
    m_loaderVersion->setVisible(!loader.isEmpty());
    m_loaderVersion->setPlaceholderText(found->versionHint);
    m_loaderVersion->setText(QString());
    Q_UNUSED(needsVersion)
}

void KomorebiNewInstanceDialog::startCreation()
{
    const QString name = m_name->text().trimmed();
    const QString version = m_version->currentData().toString();
    const QString loader = m_loader->currentData().toString();
    const QString loaderVersion = m_loaderVersion->text().trimmed();

    if (name.isEmpty()) {
        m_status->setText(tr("The instance needs a name."));
        return;
    }
    if (version.isEmpty()) {
        m_status->setText(tr("Choose the Minecraft version to run."));
        return;
    }
    if (loader == QLatin1String("forge") && loaderVersion.isEmpty()) {
        m_status->setText(tr("Forge needs its own version."));
        return;
    }

    m_create->setEnabled(false);
    m_cancel->setEnabled(false);
    m_progress->setVisible(true);
    m_progress->setRange(0, 0);
    m_status->setText(tr("Preparing"));

    Komorebi::Services::InstanceCreateRequest request;
    request.dataRoot = APPLICATION->dataRoot();
    request.name = name;
    request.minecraftVersion = version;
    request.loader = loader;
    request.source = Komorebi::Services::downloadSourceFromId(APPLICATION->settings()->get("DownloadSource").toString());
    if (loader == QLatin1String("forge"))
        request.forgeVersion = loaderVersion;
    else if (loader == QLatin1String("neoforge"))
        request.neoforgeVersion = loaderVersion;
    else
        request.loaderVersion = loaderVersion;

    m_creator = new Komorebi::Services::InstanceCreator(APPLICATION->network(), this);
    connect(m_creator, &Komorebi::Services::InstanceCreator::status, this, &KomorebiNewInstanceDialog::creationStatus);
    connect(m_creator, &Komorebi::Services::InstanceCreator::progress, this, &KomorebiNewInstanceDialog::creationProgress);
    connect(m_creator, &Komorebi::Services::InstanceCreator::finished, this, &KomorebiNewInstanceDialog::creationFinished);
    connect(m_creator, &Komorebi::Services::InstanceCreator::failed, this, &KomorebiNewInstanceDialog::creationFailed);
    m_creator->start(request);
}

void KomorebiNewInstanceDialog::creationStatus(const QString& text)
{
    m_status->setText(text);
}

void KomorebiNewInstanceDialog::creationProgress(int done, int total)
{
    m_progress->setRange(0, total > 0 ? total : 0);
    m_progress->setValue(done);
}

void KomorebiNewInstanceDialog::creationFinished(const QString& directory)
{
    m_createdDirectory = directory;
    accept();
}

void KomorebiNewInstanceDialog::creationFailed(const QString& reason)
{
    m_status->setText(tr("Could not create the instance: %1").arg(reason));
    m_progress->setVisible(false);
    m_create->setEnabled(true);
    m_cancel->setEnabled(true);
}
