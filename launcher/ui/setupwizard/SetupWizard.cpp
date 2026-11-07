#include "SetupWizard.h"

#include "JavaWizardPage.h"
#include "LanguageWizardPage.h"

#include <Application.h>
#include <FileSystem.h>
#include "translations/TranslationsModel.h"

#include <BuildConfig.h>
#include "settings/SettingsObject.h"

#include <QAbstractButton>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent)
{
    setObjectName(QStringLiteral("SetupWizard"));
    resize(620, 660);
    setMinimumSize(300, 400);
    // make it ugly everywhere to avoid variability in theming
    setWizardStyle(QWizard::ClassicStyle);
    setOptions(QWizard::NoCancelButton | QWizard::IndependentPages | QWizard::HaveCustomButton1);

    retranslate();

    connect(this, &QWizard::currentIdChanged, this, &SetupWizard::pageChanged);
}

void SetupWizard::retranslate()
{
    setButtonText(QWizard::NextButton, tr("&Next >"));
    setButtonText(QWizard::BackButton, tr("< &Back"));
    setButtonText(QWizard::FinishButton, tr("&Finish"));
    setButtonText(QWizard::CustomButton1, tr("&Refresh"));
    setWindowTitle(tr("%1 Quick Setup").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
}

BaseWizardPage* SetupWizard::getBasePage(int id)
{
    if (id == -1)
        return nullptr;
    auto pagePtr = page(id);
    if (!pagePtr)
        return nullptr;
    return dynamic_cast<BaseWizardPage*>(pagePtr);
}

BaseWizardPage* SetupWizard::getCurrentBasePage()
{
    return getBasePage(currentId());
}

void SetupWizard::pageChanged(int id)
{
    auto basePagePtr = getBasePage(id);
    if (!basePagePtr) {
        return;
    }
    if (basePagePtr->wantsRefreshButton()) {
        setButtonLayout({ QWizard::CustomButton1, QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
        auto customButton = button(QWizard::CustomButton1);
        connect(customButton, &QAbstractButton::clicked, this, [this]() {
            auto basePagePtr = getCurrentBasePage();
            if (basePagePtr) {
                basePagePtr->refresh();
            }
        });
    } else {
        setButtonLayout({ QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
    }
}

void SetupWizard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWizard::changeEvent(event);
}

SetupWizard::~SetupWizard() {}

void SetupWizard::paintEvent(QPaintEvent* event)
{
    QWizard::paintEvent(event);

    // Same rule as the main window: an explicit picture first, then the one the theme ships.
    auto path = APPLICATION->settings()->get("MainWindowBackground").toString();
    if (path.isEmpty()) {
        const auto theme = APPLICATION->settings()->get("ApplicationTheme").toString();
        if (!theme.isEmpty()) {
            const auto candidate = QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("themes/%1/resources/background.png").arg(theme));
            if (QFileInfo::exists(candidate))
                path = QDir::fromNativeSeparators(candidate);
        }
    }
    if (path.isEmpty())
        return;

    const QImage image(path);
    if (image.isNull())
        return;

    const auto mode = APPLICATION->settings()->get("MainWindowBackgroundMode").toString().toLower();
    bool fill = true;
    if (mode == QLatin1String("fit"))
        fill = false;
    else if (mode != QLatin1String("cover") && height() > 0 && image.height() > 0) {
        const double imageAspect = static_cast<double>(image.width()) / image.height();
        const double windowAspect = static_cast<double>(width()) / height();
        fill = std::abs(imageAspect - windowAspect) / std::max(imageAspect, windowAspect) < 0.18;
    }

    const auto target = size() * devicePixelRatioF();
    const auto scaled = image.scaled(target, fill ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Wizard pages fill their own background by default, which would hide the picture.
    if (auto* page = currentPage()) {
        page->setAutoFillBackground(false);
        page->setAttribute(Qt::WA_NoSystemBackground, true);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(245, 239, 227));
    painter.drawImage(QPointF((width() - scaled.width() / devicePixelRatioF()) / 2.0,
                              (height() - scaled.height() / devicePixelRatioF()) / 2.0),
                      scaled);
}