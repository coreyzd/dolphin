/*
 * SPDX-FileCopyrightText: 2026 Corey Holt
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "basicfilemetadatawidget.h"

#include <KFileMetaData/Extractor>
#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>
#include <KFormat>
#include <KLocalizedString>
#include <KStringHandler>

#include <QDateTime>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QtConcurrent/QtConcurrentRun>

namespace
{
/**
 * Reads the metadata of the local file \a path with the KFileMetaData
 * extractors that are available for \a mimeType. Runs in a worker thread.
 */
BasicFileMetaDataWidget::MetaDataList extractMetaData(const QString &path, const QString &mimeType)
{
    BasicFileMetaDataWidget::MetaDataList list;

    const KFileMetaData::ExtractorCollection collection;
    const auto extractors = collection.fetchExtractors(mimeType);
    if (extractors.isEmpty()) {
        return list;
    }

    KFileMetaData::SimpleExtractionResult result(path, mimeType, KFileMetaData::ExtractionResult::ExtractMetaData);
    for (KFileMetaData::Extractor *extractor : extractors) {
        extractor->extract(&result);
    }

    // A property can have several values (e.g. several artists):
    // show them in one row.
    const auto properties = result.properties();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const KFileMetaData::PropertyInfo info(it.key());
        const QString label = info.displayName();
        const QString value = info.formatAsDisplayString(it.value());
        if (label.isEmpty() || value.isEmpty()) {
            continue;
        }

        if (!list.isEmpty() && list.last().first == label) {
            list.last().second += QLatin1String(", ") + value;
        } else {
            list.append({label, value});
        }
    }

    return list;
}
}

BasicFileMetaDataWidget::BasicFileMetaDataWidget(QWidget *parent)
    : QWidget(parent)
    , m_items()
    , m_dateFormat(DateFormats::LongFormat)
    , m_layout(nullptr)
    , m_extractionWatcher(nullptr)
{
    m_layout = new QFormLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    m_layout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
}

BasicFileMetaDataWidget::~BasicFileMetaDataWidget()
{
    // A still running extraction finishes on its own in the thread pool;
    // its result is dropped together with the watcher.
}

void BasicFileMetaDataWidget::setItems(const KFileItemList &items)
{
    m_items = items;
    rebuild();
}

KFileItemList BasicFileMetaDataWidget::items() const
{
    return m_items;
}

void BasicFileMetaDataWidget::setDateFormat(DateFormats format)
{
    if (m_dateFormat == format) {
        return;
    }

    m_dateFormat = format;
    rebuild();
}

BasicFileMetaDataWidget::DateFormats BasicFileMetaDataWidget::dateFormat() const
{
    return m_dateFormat;
}

void BasicFileMetaDataWidget::setConfigurationMode(ConfigurationMode mode)
{
    Q_UNUSED(mode)
}

void BasicFileMetaDataWidget::rebuild()
{
    // Drop the result of an extraction that was started for the previous items.
    if (m_extractionWatcher) {
        m_extractionWatcher->disconnect(this);
        m_extractionWatcher->deleteLater();
        m_extractionWatcher = nullptr;
    }

    while (m_layout->rowCount() > 0) {
        m_layout->removeRow(0);
    }

    if (m_items.count() == 1) {
        const KFileItem item = m_items.first();
        if (!item.isNull()) {
            addSingleItemRows(item);
            startExtraction(item);
        }
    } else if (m_items.count() > 1) {
        addMultipleItemsRows(m_items);
    }
}

void BasicFileMetaDataWidget::addRow(const QString &label, const QString &value)
{
    if (value.isEmpty()) {
        return;
    }

    QLabel *labelWidget = new QLabel(i18nc("@label a detail of a file, e.g. 'Size:'", "%1:", label), this);
    labelWidget->setForegroundRole(QPalette::PlaceholderText);

    QLabel *valueWidget = new QLabel(value, this);
    valueWidget->setTextFormat(Qt::PlainText);
    valueWidget->setWordWrap(true);
    valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueWidget->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    // The width is given by the panel. Keep the height-for-width flag that
    // QLabel sets for wrapped text: without it the layout reserves the height
    // of the label's size hint, which is two lines for most values.
    QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    valueWidget->setSizePolicy(policy);

    m_layout->addRow(labelWidget, valueWidget);
}

void BasicFileMetaDataWidget::addSingleItemRows(const KFileItem &item)
{
    addRow(i18nc("@label", "Type"), item.mimeComment());

    if (item.isDir()) {
        if (item.isLocalFile() && !item.isSlow()) {
            const QDir dir(item.localPath());
            const int count = dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).count();
            addRow(i18nc("@label", "Size"), i18ncp("@item:intext", "%1 item", "%1 items", count));
        }
    } else {
        addRow(i18nc("@label", "Size"), KFormat().formatByteSize(item.size()));
    }

    addRow(i18nc("@label", "Modified"), formatDateTime(item.time(KFileItem::ModificationTime)));
    addRow(i18nc("@label", "Created"), formatDateTime(item.time(KFileItem::CreationTime)));
    addRow(i18nc("@label", "Accessed"), formatDateTime(item.time(KFileItem::AccessTime)));

    if (item.isLink()) {
        // Paths have no spaces to wrap at: allow wrapping at the separators.
        addRow(i18nc("@label", "Link to"), KStringHandler::preProcessWrap(item.linkDest()));
    }

#ifndef Q_OS_WIN
    addRow(i18nc("@label", "Owner"), item.user());
    addRow(i18nc("@label", "Group"), item.group());
    addRow(i18nc("@label", "Permissions"), item.permissionsString());
#endif
}

void BasicFileMetaDataWidget::addMultipleItemsRows(const KFileItemList &items)
{
    int folderCount = 0;
    int fileCount = 0;
    KIO::filesize_t totalSize = 0;

    for (const KFileItem &item : items) {
        if (item.isDir()) {
            ++folderCount;
        } else {
            ++fileCount;
            totalSize += item.size();
        }
    }

    if (folderCount > 0) {
        addRow(i18nc("@label", "Folders"), QString::number(folderCount));
    }
    if (fileCount > 0) {
        addRow(i18nc("@label", "Files"), QString::number(fileCount));
        addRow(i18nc("@label", "Total size"), KFormat().formatByteSize(totalSize));
    }
}

void BasicFileMetaDataWidget::startExtraction(const KFileItem &item)
{
    // Only read the content of files that are cheap to reach.
    if (item.isDir() || !item.isLocalFile() || item.isSlow()) {
        return;
    }

    const QString path = item.localPath();
    const QString mimeType = item.mimetype();
    if (path.isEmpty() || mimeType.isEmpty()) {
        return;
    }

    m_extractionWatcher = new QFutureWatcher<MetaDataList>(this);
    connect(m_extractionWatcher, &QFutureWatcherBase::finished, this, &BasicFileMetaDataWidget::slotExtractionFinished);
    m_extractionWatcher->setFuture(QtConcurrent::run(extractMetaData, path, mimeType));
}

void BasicFileMetaDataWidget::slotExtractionFinished()
{
    if (!m_extractionWatcher || sender() != m_extractionWatcher) {
        return;
    }

    const MetaDataList list = m_extractionWatcher->result();
    m_extractionWatcher->deleteLater();
    m_extractionWatcher = nullptr;

    for (const auto &entry : list) {
        addRow(entry.first, entry.second);
    }
}

QString BasicFileMetaDataWidget::formatDateTime(const QDateTime &dateTime) const
{
    if (!dateTime.isValid()) {
        return QString();
    }

    const QLocale locale;
    return locale.toString(dateTime, m_dateFormat == DateFormats::ShortFormat ? QLocale::ShortFormat : QLocale::LongFormat);
}

#include "moc_basicfilemetadatawidget.cpp"
