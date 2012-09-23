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
 * Author:  Thomas Volkert
 * Since:   2011-01-30
 */

#ifndef _OVERVIEW_FILE_TRANSFERS_WIDGET_
#define _OVERVIEW_FILE_TRANSFERS_WIDGET_

#include <QDockWidget>
#include <QTimerEvent>
#include <QMutex>
#include <QList>

#include <PacketStatistic.h>
#include <FileTransfersManager.h>
#include <ui_OverviewFileTransfersWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class OverviewFileTransfersWidget :
    public QDockWidget,
    public FileTransfersManagerObserver,
    public Ui_OverviewFileTransfersWidget
{
    struct FileTransferEntry{
        bool     Outgoing;
        uint64_t Id;
        QString  Peer;
        QString  FileName;
        uint64_t FileTransferredSize;
        uint64_t FileSize;
        int      GuiId;
    };
    Q_OBJECT;
public:
    /// The default constructor
    OverviewFileTransfersWidget(QAction *pAssignedAction, QMainWindow* pMainWindow);

    /// The destructor.
    virtual ~OverviewFileTransfersWidget();

    void Init();

    /* event call backs */
    virtual void handleFileTransfersManagerEventTransferBeginRequest(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize);
    virtual void handleFileTransfersManagerEventTransferBegin(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize);
    virtual void handleFileTransfersManagerEventTransferData(uint64_t pId, uint64_t pTransferredSize);

private slots:
    void AddEntryDialog();
    void DelEntryDialog();

public slots:
    void SetVisible(bool pVisible);

private:
    virtual void customEvent(QEvent* pEvent);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    void AddTransferEntry(FileTransferEntry pEntry);
    void initializeGUI();
    void UpdateView();
    void FillCellText(QTableWidget *pTable, int pRow, int pCol, QString pText);
    void FillRow(QTableWidget *pTable, int pRow, const FileTransferEntry &pEntry);

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    int                 mTimerId;
    /* ask for file transfers */
    QList<FileTransferEntry> mAskFileTransfers;
    QMutex              mAskFileTransfersMutex;
    /* known file transfers */
    QList<FileTransferEntry> mFileTransfers;
    QMutex              mFileTransfersMutex;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

