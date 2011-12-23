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
 * Purpose: network communication statistic and control dock widget
 * Author:  Thomas Volkert
 * Since:   2011-11-16
 */

#ifndef _OVERVIEW_NETWORK_STREAMS_WIDGET_
#define _OVERVIEW_NETWORK_STREAMS_WIDGET_

#include <HBSocket.h>

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>
#include <QCheckBox>

#include <PacketStatistic.h>

#include <ui_OverviewNetworkStreamsWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define VIEW_NETWORK_STREAMS_UPDATE_PERIOD                         250 // each 250 ms an update

///////////////////////////////////////////////////////////////////////////////

class TableEntryLossless:
    public QWidget
{
    Q_OBJECT;
public:

    TableEntryLossless(QWidget *pParent = NULL);

    void AssignSocket(Socket *pSocket);

private slots:
    void BoxToggled(bool pChecked = false);

private:
    Socket      *mSocket;
    QWidget     *mParent;
    QCheckBox   *mCheckBox;
};

///////////////////////////////////////////////////////////////////////////////

class TableEntryDelay:
    public QWidget
{
    Q_OBJECT;
public:

    TableEntryDelay(QWidget *pParent = NULL);

    void AssignSocket(Socket *pSocket);

private slots:
    void EditFinished();

private:
    Socket      *mSocket;
    QWidget     *mParent;
    QLineEdit   *mLineEdit;
};

///////////////////////////////////////////////////////////////////////////////

class TableEntryDatarate:
    public QWidget
{
    Q_OBJECT;
public:

    TableEntryDatarate(QWidget *pParent = NULL);

    void AssignSocket(Socket *pSocket);

private slots:
    void EditFinished();

private:
    Socket      *mSocket;
    QWidget     *mParent;
    QLineEdit   *mLineEdit;
};

///////////////////////////////////////////////////////////////////////////////

class OverviewNetworkStreamsWidget :
    public QDockWidget,
    public Ui_OverviewNetworkStreamsWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewNetworkStreamsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow);

    /// The destructor.
    virtual ~OverviewNetworkStreamsWidget();

public slots:
    void SetVisible(bool pVisible);

private slots:
    void TwOutgoingStreamsCustomContextMenuEvent(const QPoint &pPos);

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    void initializeGUI();
    void UpdateView();
    void SaveValues();
    QString Int2String(int64_t pSize);
    void FillCellText(int pRow, int pCol, QString pText);
    void FillRow(int pRow, Homer::Base::Socket *pSocket);

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

