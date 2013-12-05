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
 * Purpose: file transfers dock widget
 * Since:   2011-08-14
 */

#ifndef _OVERVIEW_ERRORS_WIDGET_
#define _OVERVIEW_ERRORS_WIDGET_

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>

#include <LogSink.h>

#include <PacketStatistic.h>

#include <ui_OverviewErrorsWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define VIEW_ERROR_LOG_UPDATE_PERIOD                        1000 // each 1000 ms an update

///////////////////////////////////////////////////////////////////////////////

class OverviewErrorsWidget :
    public QDockWidget,
    public Ui_OverviewErrorsWidget,
    public LogSink
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewErrorsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow);

    /// The destructor.
    virtual ~OverviewErrorsWidget();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

public slots:
    void SetVisible(bool pVisible);

private slots:
    void ErrorLogCustomContextMenuEvent(const QPoint &pPos);

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    void initializeGUI();
    void UpdateView();
    void SaveLog();
    //QString Int2String(int pSize);
    //void FillRow(QTableWidget *pTable, int pRow, Homer::Monitor::PacketStatistic *pStats);

    QMutex              mLogBufferMutex;
    QString             mLogBuffer;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
    bool                mNewLogMessageReceived;
    bool                mAutoUpdate;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

