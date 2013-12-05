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
 * Purpose: Implementation of OverviewFileTransfersWidget.h
 * Since:   2011-01-30
 */

#include <Dialogs/AddNetworkSinkDialog.h>
#include <Widgets/OverviewFileTransfersWidget.h>
#include <PacketStatisticService.h>
#include <Configuration.h>
#include <FileTransfersManager.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QFileDialog>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>
#include <QFileDialog>

namespace Homer { namespace Gui {

using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

OverviewFileTransfersWidget::OverviewFileTransfersWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mTimerId = -1;

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::LeftDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(false);
    }
    connect(mTbAdd, SIGNAL(clicked()), this, SLOT(AddEntryDialog()));
    connect(mTbDel, SIGNAL(clicked()), this, SLOT(DelEntryDialog()));
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    SetVisible(CONF.GetVisibilityFileTransfersWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityFileTransfersWidget());

    Init();
}

OverviewFileTransfersWidget::~OverviewFileTransfersWidget()
{
    FTMAN.DeleteObserver(this);
    CONF.SetVisibilityFileTransfersWidget(isVisible());
}

///////////////////////////////////////////////////////////////////////////////

void OverviewFileTransfersWidget::initializeGUI()
{
    setupUi(this);

    // hide id column
    mTwFiles->setColumnHidden(4, true);
    mTwFiles->sortItems(4);
    mTwFiles->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
//    for (int i = 0; i < 2; i++)
//        mTwFiles->horizontalHeader()->resizeSection(i, mTwFiles->horizontalHeader()->sectionSize(i) * 2);

    UpdateView();
}

void OverviewFileTransfersWidget::Init()
{
    QString tHost = "0.0.0.0"; //TODO: allow configuration by user

    Requirements *tRequs = new Requirements();
    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
    RequirementTransmitLossless *tReqLossLess = new RequirementTransmitLossless();
    RequirementTargetPort *tReqPort = new RequirementTargetPort(FTM_DEFAULT_PORT); //TODO: allow configuration by user

    tRequs->add(tReqChunks);
    tRequs->add(tReqLossLess);

    // add local port
    tRequs->add(tReqPort);

    FTMAN.Init(tHost.toStdString(), tRequs);
    FTMAN.AddObserver(this);

    //TODO: remove the following if the feature is complete
    #ifdef RELEASE_VERSION
        hide();
    #endif

}

void OverviewFileTransfersWidget::handleFileTransfersManagerEventTransferBeginRequest(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize)
{
    FileTransferEntry tEntry;
    tEntry.FileName = QString(pFileName.c_str());
    tEntry.FileSize = pFileSize;
    tEntry.Id = pId;
    tEntry.Peer = QString(pPeerName.c_str());

    mAskFileTransfersMutex.lock();
    mAskFileTransfers.push_back(tEntry);
    //fire user defined Qt event only one time
    if (mAskFileTransfers.size() < 2)
        QApplication::postEvent(this, new QEvent(QEvent::User));
    mAskFileTransfersMutex.unlock();
}

void OverviewFileTransfersWidget::handleFileTransfersManagerEventTransferBegin(uint64_t pId, std::string pPeerName, std::string pFileName, uint64_t pFileSize)
{
    FileTransferEntry tEntry;
    tEntry.FileName = QString(pFileName.c_str());
    tEntry.FileSize = pFileSize;
    tEntry.Id = pId;
    tEntry.Peer = QString(pPeerName.c_str());
    tEntry.FileTransferredSize = 0;
    tEntry.Outgoing = (FTMAN.IsFromLocalhost(pId));

    // RECEIVING A FILE: add entry to file transfer database
    AddTransferEntry(tEntry);
}

void OverviewFileTransfersWidget::handleFileTransfersManagerEventTransferData(uint64_t pId, uint64_t pTransferredSize)
{
    QList<FileTransferEntry>::iterator tIt;
    bool tFound = false;

    mFileTransfersMutex.lock();

    for(tIt = mFileTransfers.begin(); tIt != mFileTransfers.end(); tIt++)
    {
        if (tIt->Id == pId)
        {
            tFound = true;
            tIt->FileTransferredSize = pTransferredSize;
            //LOG(LOG_VERBOSE, "Updating entry: %"PRIu64" transferred, %"PRIu64" overall", pTransferredSize, tEntry.FileSize);
            break;
        }
    }

    mFileTransfersMutex.unlock();

    //fire user defined Qt event
    if (tFound)
        QApplication::postEvent(this, new QEvent(QEvent::User));
}

void OverviewFileTransfersWidget::AddTransferEntry(FileTransferEntry pEntry)
{
    mFileTransfersMutex.lock();
    pEntry.GuiId = mFileTransfers.size();
    mFileTransfers.push_back(pEntry);
    //fire user defined Qt event
    QApplication::postEvent(this, new QEvent(QEvent::User));
    mFileTransfersMutex.unlock();
}

void OverviewFileTransfersWidget::customEvent(QEvent* pEvent)
{
    FileTransferEntry tEntry;

    if (pEvent->type() != QEvent::User)
    {
        LOG(LOG_WARN, "Unexpected event type");
        return;
    }

    bool tShouldShowAskFileTransfersDialog;

    do
    {
        tShouldShowAskFileTransfersDialog = false;
        mAskFileTransfersMutex.lock();
        if (mAskFileTransfers.size() > 0)
        {
            tShouldShowAskFileTransfersDialog = true;
            tEntry = mAskFileTransfers.first();
            mAskFileTransfers.pop_front();
        }
        mAskFileTransfersMutex.unlock();

        if (tShouldShowAskFileTransfersDialog)
        {
            QString tFileName = QFileDialog::getSaveFileName(parentWidget(), "Store file " + tEntry.FileName + "(" + QString("%1").arg(tEntry.FileSize) + " bytes) from " + tEntry.Peer + " to.. ",
                                                                    CONF.GetDataDirectory() + "/" + tEntry.FileName,
                                                                    "",
                                                                    NULL,
                                                                    CONF_NATIVE_DIALOGS);

            if (!tFileName.isEmpty())
            {
                CONF.SetDataDirectory(tFileName.left(tFileName.lastIndexOf('/')));
                FTMAN.AcknowledgeTransfer(tEntry.Id, tFileName.toStdString());
            }
        }
    }while(tShouldShowAskFileTransfersDialog);

    UpdateView();
}

void OverviewFileTransfersWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewFileTransfersWidget::SetVisible(bool pVisible)
{
	CONF.SetVisibilityFileTransfersWidget(pVisible);
    if (pVisible)
    {
        move(mWinPos);
        show();
        // update GUI elements every 1000 ms
        mTimerId = startTimer(1000);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewFileTransfersWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

//    tAction = tMenu.addAction("Save statistic");
//    QIcon tIcon1;
//    tIcon1.addPixmap(QPixmap(":/images/22_22/Save.png"), QIcon::Normal, QIcon::Off);
//    tAction->setIcon(tIcon1);
//
//    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
//    if (tPopupRes != NULL)
//    {
//        if (tPopupRes->text().compare("Save statistic") == 0)
//        {
//            SaveStatistic();
//            return;
//        }
//    }
}

void OverviewFileTransfersWidget::FillCellText(QTableWidget *pTable, int pRow, int pCol, QString pText)
{
    if (pTable->item(pRow, pCol) != NULL)
        pTable->item(pRow, pCol)->setText(pText);
    else
    {
        QTableWidgetItem *tItem =  new QTableWidgetItem(pText);
        tItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        pTable->setItem(pRow, pCol, tItem);
    }
}

void OverviewFileTransfersWidget::FillRow(QTableWidget *pTable, int pRow, const FileTransferEntry &pEntry)
{
	if (pRow > pTable->rowCount() - 1)
        pTable->insertRow(pTable->rowCount());

    if (pTable->item(pRow, 0) != NULL)
        if (pEntry.Outgoing)
            pTable->item(pRow, 0)->setText(Homer::Gui::OverviewFileTransfersWidget::tr("outgoing"));
        else
            pTable->item(pRow, 0)->setText(Homer::Gui::OverviewFileTransfersWidget::tr("incoming"));
    else if (pEntry.Outgoing)
        pTable->setItem(pRow, 0, new QTableWidgetItem(Homer::Gui::OverviewFileTransfersWidget::tr("outgoing")));
    else
        pTable->setItem(pRow, 0, new QTableWidgetItem(Homer::Gui::OverviewFileTransfersWidget::tr("incoming")));
    pTable->item(pRow, 0)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QIcon tIcon11;
    if (pEntry.Outgoing)
        tIcon11.addPixmap(QPixmap(":/images/32_32/ArrowRight.png"), QIcon::Normal, QIcon::Off);
    else
        tIcon11.addPixmap(QPixmap(":/images/32_32/ArrowLeft.png"), QIcon::Normal, QIcon::Off);
    pTable->item(pRow, 0)->setIcon(tIcon11);

    FillCellText(pTable, pRow, 1, pEntry.FileName);
    double tProgress = 100 * pEntry.FileTransferredSize / pEntry.FileSize;
    //LOG(LOG_VERBOSE, "Progress is: %d, %"PRIu64" / %"PRIu64"", (int)tProgress,  pEntry.FileTransferredSize, pEntry.FileSize);
    FillCellText(pTable, pRow, 2, QString("%1 %").arg(tProgress));
    FillCellText(pTable, pRow, 3, QString("%1").arg(pEntry.FileSize));
    FillCellText(pTable, pRow, 4, QString("%1 bytes").arg(pEntry.GuiId));
}

void OverviewFileTransfersWidget::UpdateView()
{
	PacketStatistics::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1;

    //LOG(LOG_VERBOSE, "Updating view");

    if (mTwFiles->selectionModel()->currentIndex().isValid())
        tSelectedRow = mTwFiles->selectionModel()->currentIndex().row();

    // reset all
    mFileTransfersMutex.lock();

    if (mFileTransfers.size() > 0)
    {
        FileTransferEntry tEntry;
        foreach(tEntry, mFileTransfers)
        {
            FillRow(mTwFiles, tRow++, tEntry);
        }
    }
    mTwFiles->setRowCount(tRow);

    mFileTransfersMutex.unlock();

    if (tSelectedRow != -1)
        mTwFiles->selectRow(tSelectedRow);
}

void OverviewFileTransfersWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

void OverviewFileTransfersWidget::DelEntryDialog()
{
    QList<QTableWidgetItem*> tItems = mTwFiles->selectedItems();

    if (tItems.isEmpty())
        return;

    QTableWidgetItem* tItem;
    foreach(tItem, tItems)
    {
//        mTwFiles->removeItemWidget(tItem);
//        delete tItem;
    }
}

void OverviewFileTransfersWidget::AddEntryDialog()
{
    QStringList tFileNames;

    tFileNames = QFileDialog::getOpenFileNames(this, Homer::Gui::OverviewFileTransfersWidget::tr("Select files for transfer"),
													   CONF.GetDataDirectory(),
													   Homer::Gui::OverviewFileTransfersWidget::tr("All files") + " (*)",
													   NULL,
													   CONF_NATIVE_DIALOGS);

    if (tFileNames.isEmpty())
        return;

    QString tFirstFileName = *tFileNames.constBegin();
    CONF.SetDataDirectory(tFirstFileName.left(tFirstFileName.lastIndexOf('/')));

    AddNetworkSinkDialog tANSDialog(this, Homer::Gui::OverviewFileTransfersWidget::tr("Target for transferring") + " " + QString("%1").arg(tFileNames.size()) + " " + Homer::Gui::OverviewFileTransfersWidget::tr("file(s)"), DATA_TYPE_FILE, NULL);
    if (tANSDialog.exec() != QDialog::Accepted)
        return;

    Requirements *tRequs = tANSDialog.GetRequirements();
    QString tTarget = tANSDialog.GetTarget();
    QString tNAPIImpl = tANSDialog.GetNAPIImplementation();

    // get old NAPI implementation
    string tOldNAPIImpl = NAPI.getCurrentImplName();

    // set selected NAPI implementation
    NAPI.selectImpl(tNAPIImpl.toStdString());

    QString tFile;
    foreach (tFile, tFileNames)
    {
        FTMAN.SendFile(tTarget.toStdString(), tRequs, tFile.toStdString());
    }

    // set old NAPI implementation again
    NAPI.selectImpl(tOldNAPIImpl);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
