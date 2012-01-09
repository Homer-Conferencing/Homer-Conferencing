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
 * Purpose: process statistic dock widget
 * Author:  Thomas Volkert
 * Since:   2009-05-17
 */

#ifndef _OVERVIEW_THREADS_WIDGET_
#define _OVERVIEW_THREADS_WIDGET_

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>

#include <ProcessStatistic.h>

#include <ui_OverviewThreadsWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class OverviewThreadsWidget :
    public QDockWidget,
    public Ui_OverviewThreadsWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewThreadsWidget(QAction *pAssignedAction, QMainWindow *pMainWindow);

    /// The destructor.
    virtual ~OverviewThreadsWidget();

public slots:
    void SetVisible(bool pVisible);

private:
    void initializeGUI();
    void UpdateView();
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    void FillCellText(int pRow, int pCol, QString pText);
    void FillRow(int pRow, Homer::Monitor::ProcessStatistic *pStats);

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
    int                 mSelectedRow;
    bool                mSummarizeCpuCores;
    int                 mCpuCores;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

