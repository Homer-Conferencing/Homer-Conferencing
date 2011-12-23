/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Purpose: packet statistic dock widget
 * Author:  Thomas Volkert
 * Since:   2009-04-20
 */

#ifndef _OVERVIEW_DATA_STREAMS_WIDGET_
#define _OVERVIEW_DATA_STREAMS_WIDGET_

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>

#include <PacketStatistic.h>

#include <ui_OverviewDataStreamsWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define VIEW_DATA_STREAMS_UPDATE_PERIOD                         250 // each 250 ms an update

///////////////////////////////////////////////////////////////////////////////

class OverviewDataStreamsWidget :
    public QDockWidget,
    public Ui_OverviewDataStreamsWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewDataStreamsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow);

    /// The destructor.
    virtual ~OverviewDataStreamsWidget();

public slots:
    void SetVisible(bool pVisible);

private slots:
    void TwVideoCustomContextMenuEvent(const QPoint &pPos);
    void TwAudioCustomContextMenuEvent(const QPoint &pPos);

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    void ResetCurrentVideoStatistic();
    void ResetCurrentAudioStatistic();
    void initializeGUI();
    void UpdateView();
    void SaveStatistic();
    QString Int2String(int64_t pSize);
    void FillCellText(QTableWidget *pTable, int pRow, int pCol, QString pText);
    void FillRow(QTableWidget *pTable, int pRow, Homer::Monitor::PacketStatistic *pStats);

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

