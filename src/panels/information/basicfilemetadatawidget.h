/*
 * SPDX-FileCopyrightText: 2026 Corey Holt
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BASICFILEMETADATAWIDGET_H
#define BASICFILEMETADATAWIDGET_H

#include <KFileItem>

#include <QDateTime>
#include <QFutureWatcher>
#include <QList>
#include <QLocale>
#include <QPair>
#include <QString>
#include <QWidget>

class QFormLayout;

/**
 * @brief Shows the details of file items in the Information Panel when
 *        Dolphin is built without Baloo (e.g. on Windows).
 *
 * This is a small stand-in for Baloo::FileMetaDataWidget. It offers the
 * part of that widget's interface the Information Panel uses, so that the
 * panel code is the same with and without Baloo.
 *
 * The basic details (type, size, dates, ...) are taken from the KFileItem.
 * For a single local file, further details (image dimensions, audio tags,
 * ...) are read with KFileMetaData in a worker thread and appended once
 * they are available.
 *
 * Tags, ratings, comments and choosing which details are shown need
 * Baloo's widget and are not offered here.
 */
class BasicFileMetaDataWidget : public QWidget
{
    Q_OBJECT

public:
    /** Mirrors Baloo::DateFormats. */
    enum class DateFormats {
        LongFormat = QLocale::LongFormat,
        ShortFormat = QLocale::ShortFormat,
    };

    /** Mirrors Baloo::ConfigurationMode. */
    enum class ConfigurationMode {
        ReStart = 0,
        Accept,
        Cancel,
    };

    using MetaDataList = QList<QPair<QString, QString>>;

    explicit BasicFileMetaDataWidget(QWidget *parent = nullptr);
    ~BasicFileMetaDataWidget() override;

    void setItems(const KFileItemList &items);
    KFileItemList items() const;

    void setDateFormat(DateFormats format);
    DateFormats dateFormat() const;

    /** Choosing which details are shown is not supported: does nothing. */
    void setConfigurationMode(ConfigurationMode mode);

Q_SIGNALS:
    /** Present for interface compatibility with Baloo::FileMetaDataWidget. */
    void urlActivated(const QUrl &url);

private:
    void rebuild();
    void addRow(const QString &label, const QString &value);
    void addSingleItemRows(const KFileItem &item);
    void addMultipleItemsRows(const KFileItemList &items);
    void startExtraction(const KFileItem &item);
    void slotExtractionFinished();
    QString formatDateTime(const QDateTime &dateTime) const;

private:
    KFileItemList m_items;
    DateFormats m_dateFormat;
    QFormLayout *m_layout;

    QFutureWatcher<MetaDataList> *m_extractionWatcher;
};

#endif // BASICFILEMETADATAWIDGET_H
