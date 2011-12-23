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
 * Purpose: file transfers dock widget
 * Author:  Thomas Volkert
 * Since:   2011-01-30
 */

#ifndef _OVERVIEW_FILE_TRANSFERS_WIDGET_
#define _OVERVIEW_FILE_TRANSFERS_WIDGET_

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>

#include <PacketStatistic.h>

#include <ui_OverviewFileTransfersWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class OverviewFileTransfersWidget :
    public QDockWidget,
    public Ui_OverviewFileTransfersWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewFileTransfersWidget(QAction *pAssignedAction, QMainWindow* pMainWindow);

    /// The destructor.
    virtual ~OverviewFileTransfersWidget();

public slots:
    void SetVisible(bool pVisible);

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    void initializeGUI();
    void UpdateView();
    QString Int2String(int pSize);
    //void FillRow(QTableWidget *pTable, int pRow, Homer::Monitor::PacketStatistic *pStats);

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

