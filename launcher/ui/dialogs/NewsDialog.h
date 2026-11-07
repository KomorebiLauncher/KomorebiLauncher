#pragma once

#include <QDialog>
#include <QHash>
#include <QImage>

#include "news/NewsEntry.h"

namespace Ui {
class NewsDialog;
}

class NewsDialog : public QDialog {
    Q_OBJECT

   public:
    NewsDialog(QList<NewsEntryPtr> entries, QWidget* parent = nullptr);
    ~NewsDialog();

   public slots:
    void toggleArticleList();

   protected:
    void paintEvent(QPaintEvent* event) override;

   private slots:
    void selectedArticleChanged(const QString& new_title);

   private:
    Ui::NewsDialog* ui;

    QHash<QString, NewsEntryPtr> m_entries;
    bool m_article_list_hidden = false;
    /// Resolved once: the dialog is short-lived and the theme cannot change while it is open.
    QImage m_background;
};
