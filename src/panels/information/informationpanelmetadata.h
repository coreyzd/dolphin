/*
 * SPDX-FileCopyrightText: 2026 Corey Holt
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INFORMATIONPANELMETADATA_H
#define INFORMATIONPANELMETADATA_H

#include "config-dolphin.h"

/**
 * Selects the widget that shows the details of file items in the
 * Information Panel: Baloo's when Dolphin is built with Baloo,
 * otherwise Dolphin's own basic one.
 */
#if HAVE_BALOO

#include <Baloo/FileMetaDataWidget>

namespace InformationPanelMetaData
{
using Widget = Baloo::FileMetaDataWidget;
using DateFormats = Baloo::DateFormats;
using ConfigurationMode = Baloo::ConfigurationMode;
}

#else

#include "basicfilemetadatawidget.h"

namespace InformationPanelMetaData
{
using Widget = BasicFileMetaDataWidget;
using DateFormats = BasicFileMetaDataWidget::DateFormats;
using ConfigurationMode = BasicFileMetaDataWidget::ConfigurationMode;
}

#endif

#endif // INFORMATIONPANELMETADATA_H
