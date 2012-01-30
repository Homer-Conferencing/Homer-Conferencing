/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: Implementation of StreamPositionWidget.h
 * Author:  Thomas Volkert
 * Since:   2011-06-16
 */

#include <Widgets/StreamPositionWidget.h>

#include <QWidget>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

StreamPositionWidget::StreamPositionWidget(QWidget* pParent):
    QWidget(pParent)
{
    initializeGUI();
}

StreamPositionWidget::~StreamPositionWidget()
{
}

///////////////////////////////////////////////////////////////////////////////

void StreamPositionWidget::initializeGUI()
{
    setupUi(this);
}

///////////////////////////////////////////////////////////////////////////////

void StreamPositionWidget::showPosition(int64_t tCurPos, int64_t tEndPos)
{
    int tHour, tMin, tSec;

    tHour = tCurPos / 3600;
    tCurPos %= 3600;
    tMin = tCurPos / 60;
    tCurPos %= 60;
    tSec = tCurPos;
    mLbCurPos->setText(QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0'));

    tHour = tEndPos / 3600;
    tEndPos %= 3600;
    tMin = tEndPos / 60;
    tEndPos %= 60;
    tSec = tEndPos;
    mLbEndPos->setText(QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0'));
}

}} //namespace
