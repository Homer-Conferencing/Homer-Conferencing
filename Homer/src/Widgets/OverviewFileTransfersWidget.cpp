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
 * Purpose: Implementation of OverviewFileTransfersWidget.h
 * Author:  Thomas Volkert
 * Since:   2011-01-30
 */

#include <Widgets/OverviewFileTransfersWidget.h>
#include <PacketStatisticService.h>
#include <Configuration.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QFileDialog>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>

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
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    SetVisible(CONF.GetVisibilityFileTransfersWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityFileTransfersWidget());
}

OverviewFileTransfersWidget::~OverviewFileTransfersWidget()
{
    CONF.SetVisibilityFileTransfersWidget(isVisible());
}

///////////////////////////////////////////////////////////////////////////////

void OverviewFileTransfersWidget::initializeGUI()
{
    setupUi(this);

    // hide id column
    mTwFiles->setColumnHidden(5, true);
    mTwFiles->sortItems(5);
    mTwFiles->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    for (int i = 0; i < 2; i++)
        mTwFiles->horizontalHeader()->resizeSection(i, mTwFiles->horizontalHeader()->sectionSize(i) * 2);

    QPalette palette;
    QBrush brush1(QColor(0, 128, 128, 255));
    QBrush brush2(QColor(155, 220, 198, 255));
    QBrush brush3(QColor(98, 99, 98, 255));
    QBrush brush4(QColor(100, 102, 100, 255));
    QBrush brush(QColor(0, 255, 255, 255));
    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
            brush.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
            brush1.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Active, QPalette::Button, brush1);
            brush2.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Active, QPalette::ButtonText, brush2);

            palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
            palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
            palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush2);

            brush3.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
            palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
            brush4.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
            setPalette(palette);

            setStyleSheet(QString::fromUtf8(" QDockWidget::close-button, QDockWidget::float-button {\n"
                                            "     border: 1px solid;\n"
                                            "     background: #9BDCC6;\n"
                                            " }\n"
                                            " QDockWidget::title {\n"
                                            "     padding-left: 20px;\n"
                                            "     text-align: left;\n"
                                            "     background: #008080;\n"
                                            " }"));
            break;
        default:
            break;
    }

    UpdateView();
}

void OverviewFileTransfersWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewFileTransfersWidget::SetVisible(bool pVisible)
{
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
//    QAction *tAction;
//
//    QMenu tMenu(this);
//
//    tAction = tMenu.addAction("Save statistic");
//    QIcon tIcon1;
//    tIcon1.addPixmap(QPixmap(":/images/DriveSave.png"), QIcon::Normal, QIcon::Off);
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

//void OverviewFileTransfersWidget::FillRow(QTableWidget *pTable, int pRow, PacketStatistic *pStats)
//{
//	if (pStats == NULL)
//		return;
//
//	PacketStatisticDescriptor tStatValues = pStats->GetPacketStatistic();
//
//	if (pRow > pTable->rowCount() - 1)
//        pTable->insertRow(pTable->rowCount());
//
//    if (pTable->item(pRow, 0) != NULL)
//        pTable->item(pRow, 0)->setText(QString(pStats->GetStreamName().c_str()));
//    else
//        pTable->setItem(pRow, 0, new QTableWidgetItem(QString(pStats->GetStreamName().c_str())));
//    pTable->item(pRow, 0)->setBackground(QBrush(QColor(Qt::lightGray)));
//
//	if (pTable->item(pRow, 1) != NULL)
//		pTable->item(pRow, 1)->setText(Int2String(tStatValues.MinPacketSize) + " bytes");
//	else
//		pTable->setItem(pRow, 1, new QTableWidgetItem(Int2String(tStatValues.MinPacketSize) + " bytes"));
//	pTable->item(pRow, 1)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//	if (pTable->item(pRow, 2) != NULL)
//		pTable->item(pRow, 2)->setText(Int2String(tStatValues.MaxPacketSize) + " bytes");
//	else
//		pTable->setItem(pRow, 2, new QTableWidgetItem(Int2String(tStatValues.MaxPacketSize) + " bytes"));
//	pTable->item(pRow, 2)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//	if (pTable->item(pRow, 3) != NULL)
//		pTable->item(pRow, 3)->setText(Int2String(tStatValues.AvgPacketSize) + " bytes");
//	else
//		pTable->setItem(pRow, 3, new QTableWidgetItem(Int2String(tStatValues.AvgPacketSize) + " bytes"));
//	pTable->item(pRow, 3)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//	if (pTable->item(pRow, 4) != NULL)
//		pTable->item(pRow, 4)->setText(QString("%1").arg(tStatValues.PacketCount));
//	else
//		pTable->setItem(pRow, 4, new QTableWidgetItem(QString("%1").arg(tStatValues.PacketCount)));
//	pTable->item(pRow, 4)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//	if (pTable->item(pRow, 5) != NULL)
//		pTable->item(pRow, 5)->setText(QString("%1").arg(tStatValues.LostPacketCount));
//	else
//		pTable->setItem(pRow, 5, new QTableWidgetItem(QString("%1").arg(tStatValues.LostPacketCount)));
//	pTable->item(pRow, 5)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//	if (pTable->item(pRow, 6) != NULL)
//        if (tStatValues.Outgoing)
//            pTable->item(pRow, 6)->setText(QString("outgoing"));
//        else
//            pTable->item(pRow, 6)->setText(QString("incoming"));
//    else if (tStatValues.Outgoing)
//        pTable->setItem(pRow, 6, new QTableWidgetItem(QString("outgoing")));
//    else
//        pTable->setItem(pRow, 6, new QTableWidgetItem(QString("incoming")));
//    pTable->item(pRow, 6)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
//    QIcon tIcon11;
//    if (tStatValues.Outgoing)
//        tIcon11.addPixmap(QPixmap(":/images/ArrowRightGreen.png"), QIcon::Normal, QIcon::Off);
//    else
//        tIcon11.addPixmap(QPixmap(":/images/ArrowLeftYellow.png"), QIcon::Normal, QIcon::Off);
//    pTable->item(pRow, 6)->setIcon(tIcon11);
//
//    QString tPacketTypeStr = QString(pStats->GetPacketTypeStr().c_str());
//    if (pTable->item(pRow, 7) != NULL)
//        pTable->item(pRow, 7)->setText(tPacketTypeStr);
//    else
//        pTable->setItem(pRow, 7, new QTableWidgetItem(tPacketTypeStr));
//    pTable->item(pRow, 7)->setTextAlignment(Qt::AlignCenter|Qt::AlignVCenter);
//
//    if (pTable->item(pRow, 8) != NULL)
//        pTable->item(pRow, 8)->setText(Int2String(tStatValues.AvgBandwidth) + " bytes/s");
//    else
//        pTable->setItem(pRow, 8, new QTableWidgetItem(Int2String(tStatValues.AvgBandwidth) + " bytes/s"));
//    pTable->item(pRow, 8)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//
//    if (pTable->item(pRow, 9) != NULL)
//        pTable->item(pRow, 9)->setText(QString("%1").arg(pRow));
//    else
//        pTable->setItem(pRow, 9, new QTableWidgetItem(QString("%1").arg(pRow)));
//    pTable->item(pRow, 9)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
//}

void OverviewFileTransfersWidget::UpdateView()
{
	PacketStatisticsList::iterator tIt;
    int tRowAudio = 0, tRowVideo = 0;
    int tSelectedRow = -1;

    //PacketStatisticsList tStatList = SVC_PACKET_STATISTIC.GetPacketStatistics();

    if (mTwFiles->selectionModel()->currentIndex().isValid())
        tSelectedRow = mTwFiles->selectionModel()->currentIndex().row();

    // reset all
    mTwFiles->clearContents();

//    if (tStatList.size() > 0)
//    {
//        tIt = tStatList.begin();
//
//        while (tIt != tStatList.end())
//        {
//        	switch((*tIt)->GetDataType())
//        	{
//				case DATA_TYPE_VIDEO:
//                    FillRow(mTwVideo, tRowVideo++, *tIt);
//					break;
//				case DATA_TYPE_AUDIO:
//                    FillRow(mTwAudio, tRowAudio++, *tIt);
//					break;
//        	}
//            tIt++;
//        }
//    }
    //mTwFiles->setRowCount(tRowAudio);

    if (tSelectedRow != -1)
        mTwFiles->selectRow(tSelectedRow);
}

void OverviewFileTransfersWidget::timerEvent(QTimerEvent *pEvent)
{
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
