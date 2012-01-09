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
 * Purpose: Implementation of OverviewNetworkStreamsWidget.h
 * Author:  Thomas Volkert
 * Since:   2011-11-16
 */

#include <Widgets/OverviewNetworkStreamsWidget.h>
#include <HBSocket.h>
#include <HBSocketControlService.h>
#include <Configuration.h>
#include <Snippets.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QFileDialog>
#include <QSizePolicy>
#include <QScrollBar>
#include <QMenu>
#include <QContextMenuEvent>
#include <QItemDelegate>

#include <string>

namespace Homer { namespace Gui {

using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

TableEntryLossless::TableEntryLossless(QWidget *pParent):
    QWidget(pParent)
{
    mParent = pParent;
    mSocket = NULL;

    QHBoxLayout *tLayout = new QHBoxLayout(this);
    setLayout(tLayout);
    mCheckBox = new QCheckBox(mParent);
    tLayout->addWidget(mCheckBox);
    tLayout->setAlignment(mCheckBox, Qt::AlignHCenter | Qt::AlignVCenter);
    tLayout->setContentsMargins(0, 0, 0, 0);
    tLayout->setSpacing(0);
    mCheckBox->setCheckable(true);
}

void TableEntryLossless::AssignSocket(Socket *pSocket)
{
    if(pSocket != mSocket)
    {
        mSocket = pSocket;
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);

        mCheckBox->setCheckState((tQoSSettings.Features & QOS_FEATURE_LOSSLESS) ? Qt::Checked : Qt::Unchecked);

        connect(mCheckBox, SIGNAL(clicked(bool)), this, SLOT(BoxToggled(bool)));
    }
}

void TableEntryLossless::BoxToggled(bool pChecked)
{
    bool tApplied = false;

    if(mSocket == NULL)
        return;

    SocketsList tSocketList = SVC_SOCKET_CONTROL.GetClientSocketsControl();

    if(SVC_SOCKET_CONTROL.IsClientSocketAvailable(mSocket))
    {
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);
        if (pChecked)
            tQoSSettings.Features |= QOS_FEATURE_LOSSLESS;
        else
            tQoSSettings.Features ^= QOS_FEATURE_LOSSLESS;
        mSocket->SetQoS(tQoSSettings);
        tApplied = true;
    }

    SVC_SOCKET_CONTROL.ReleaseClientSocketsControl();

    if (tApplied)
        ShowInfo("New QoS settings applied", "The required feature \"lossless\" was " + (pChecked ? QString("activated") : QString("deactivated")) + " in the socket settings");
}

///////////////////////////////////////////////////////////////////////////////

TableEntryDelay::TableEntryDelay(QWidget *pParent):
    QWidget(pParent)
{
    mParent = pParent;
    mSocket = NULL;

    QHBoxLayout *tLayout = new QHBoxLayout(this);
    setLayout(tLayout);
    mLineEdit = new QLineEdit("0");
    mLineEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mLineEdit->setFrame(false);
    tLayout->addStretch(1);
    tLayout->addWidget(mLineEdit);
    tLayout->addWidget(new QLabel("ms"));
    tLayout->setAlignment(mLineEdit, Qt::AlignHCenter | Qt::AlignVCenter);
    tLayout->setContentsMargins(0, 0, 0, 0);
    tLayout->setSpacing(0);
}

void TableEntryDelay::AssignSocket(Socket *pSocket)
{
    if(pSocket != mSocket)
    {
        mSocket = pSocket;
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);

        mLineEdit->setText(QString("%1").arg(tQoSSettings.Delay));

        connect(mLineEdit, SIGNAL(returnPressed()), this, SLOT(EditFinished()));
    }
}

void TableEntryDelay::EditFinished()
{
    bool tApplied = false;
    unsigned int tNewDelay = 0, tOldDelay = 0;

    if(mSocket == NULL)
        return;

    SocketsList tSocketList = SVC_SOCKET_CONTROL.GetClientSocketsControl();

    if(SVC_SOCKET_CONTROL.IsClientSocketAvailable(mSocket))
    {
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);
        tOldDelay = tQoSSettings.Delay;
        bool tOk = false;
        tNewDelay = mLineEdit->text().toInt(&tOk, 10);
        if (tOk)
        {
            if(tNewDelay != tOldDelay)
            {
                tQoSSettings.Delay = (unsigned int)tNewDelay;
                mSocket->SetQoS(tQoSSettings);
                tApplied = true;
            }
        }else
        {
            LOG(LOG_WARN, "Wrong user input for new delay value, will use old value");
            mLineEdit->setText(QString("%1").arg(tQoSSettings.Delay));
        }
    }

    SVC_SOCKET_CONTROL.ReleaseClientSocketsControl();

    if (tApplied)
        ShowInfo("New QoS settings applied", "The required delay was set from " + QString("%1").arg(tOldDelay) + " to " + QString("%1").arg(tNewDelay) + " ms in the socket settings");
}

///////////////////////////////////////////////////////////////////////////////

TableEntryDatarate::TableEntryDatarate(QWidget *pParent):
    QWidget(pParent)
{
    mParent = pParent;
    mSocket = NULL;

    QHBoxLayout *tLayout = new QHBoxLayout(this);
    setLayout(tLayout);
    mLineEdit = new QLineEdit("0");
    mLineEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mLineEdit->setFrame(false);
    tLayout->addStretch(1);
    tLayout->addWidget(mLineEdit);
    tLayout->addWidget(new QLabel("Kb/s"));
    tLayout->setAlignment(mLineEdit, Qt::AlignHCenter | Qt::AlignVCenter);
    tLayout->setContentsMargins(0, 0, 0, 0);
    tLayout->setSpacing(0);
}

void TableEntryDatarate::AssignSocket(Socket *pSocket)
{
    if(pSocket != mSocket)
    {
        mSocket = pSocket;
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);

        mLineEdit->setText(QString("%1").arg(tQoSSettings.DataRate));

        connect(mLineEdit, SIGNAL(returnPressed()), this, SLOT(EditFinished()));
    }
}

void TableEntryDatarate::EditFinished()
{
    bool tApplied = false;
    unsigned int tNewDatarate = 0, tOldDatarate = 0;

    if(mSocket == NULL)
        return;

    SocketsList tSocketList = SVC_SOCKET_CONTROL.GetClientSocketsControl();

    if(SVC_SOCKET_CONTROL.IsClientSocketAvailable(mSocket))
    {
        QoSSettings tQoSSettings;
        mSocket->GetQoS(tQoSSettings);
        tOldDatarate = tQoSSettings.DataRate;
        bool tOk = false;
        tNewDatarate = mLineEdit->text().toInt(&tOk, 10);
        if (tOk)
        {
            if(tNewDatarate != tOldDatarate)
            {
                tQoSSettings.DataRate = (unsigned int)tNewDatarate;
                mSocket->SetQoS(tQoSSettings);
                tApplied = true;
            }
        }else
        {
            LOG(LOG_WARN, "Wrong user input for new data rate value, will use old value");
            mLineEdit->setText(QString("%1").arg(tQoSSettings.DataRate));
        }
    }

    SVC_SOCKET_CONTROL.ReleaseClientSocketsControl();

    if (tApplied)
        ShowInfo("New QoS settings applied", "The required data rate was set from " + QString("%1").arg(tOldDatarate) + " to " + QString("%1").arg(tNewDatarate) + " Kb/s in the socket settings");
}

///////////////////////////////////////////////////////////////////////////////

OverviewNetworkStreamsWidget::OverviewNetworkStreamsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mTimerId = -1;

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::TopDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(true);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTwOutgoingStreams, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(TwOutgoingStreamsCustomContextMenuEvent(const QPoint&)));
    if (CONF.DebuggingEnabled())
    {
        SetVisible(CONF.GetVisibilityNetworkStreamsWidget());
        mAssignedAction->setChecked(CONF.GetVisibilityNetworkStreamsWidget());
    }else
    {
        SetVisible(false);
        mAssignedAction->setChecked(false);
        mAssignedAction->setVisible(false);
        toggleViewAction()->setVisible(false);
    }
}

OverviewNetworkStreamsWidget::~OverviewNetworkStreamsWidget()
{
	CONF.SetVisibilityNetworkStreamsWidget(isVisible());
}

///////////////////////////////////////////////////////////////////////////////

void OverviewNetworkStreamsWidget::initializeGUI()
{
    setupUi(this);

    // hide id column
    mTwOutgoingStreams->setColumnHidden(7, true);
    mTwOutgoingStreams->sortItems(7);
    mTwOutgoingStreams->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    mTwOutgoingStreams->horizontalHeader()->resizeSection(0, mTwOutgoingStreams->horizontalHeader()->sectionSize(0) * 3);
    mTwOutgoingStreams->horizontalHeader()->resizeSection(1, mTwOutgoingStreams->horizontalHeader()->sectionSize(1) * 3);
    mTwOutgoingStreams->horizontalHeader()->resizeSection(4, (int)(mTwOutgoingStreams->horizontalHeader()->sectionSize(4) * 1.5));
    mTwOutgoingStreams->horizontalHeader()->resizeSection(5, mTwOutgoingStreams->horizontalHeader()->sectionSize(5) * 2);

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

void OverviewNetworkStreamsWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewNetworkStreamsWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
        mTimerId = startTimer(VIEW_NETWORK_STREAMS_UPDATE_PERIOD);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewNetworkStreamsWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Save values");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/DriveSave.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Save values") == 0)
        {
            SaveValues();
            return;
        }
    }
}

void OverviewNetworkStreamsWidget::TwOutgoingStreamsCustomContextMenuEvent(const QPoint &pPos)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Save values");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/DriveSave.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(QCursor::pos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Save values") == 0)
        {
            SaveValues();
            return;
        }
    }
}

void OverviewNetworkStreamsWidget::SaveValues()
{
    QString tFileName = QFileDialog::getSaveFileName(this,
                                                     "Save network streams parameters",
                                                     QDir::homePath() + "/NetworkStreamsParameters.csv",
                                                     "Comma-Separated Values File (*.csv)",
                                                     NULL,
                                                     QFileDialog::DontUseNativeDialog);

    if (tFileName.isEmpty())
        return;

    // write history to file
    QFile tFile(tFileName);
    if (!tFile.open(QIODevice::ReadWrite))
        return;

    //#####################################################
    //### write header to csv
    //#####################################################
    QString tHeader = "LocalAddr,PeerAddr,Network,Transport,RequLossless,RequDelay,RequDatarate\n";
    if (!tFile.write(tHeader.toStdString().c_str(), tHeader.size()))
        return;

    //#####################################################
    //### write one entry per line
    //#####################################################
    QString tLine;
    SocketsList::iterator tIt;
    SocketsList tSockList = SVC_SOCKET_CONTROL.GetClientSocketsControl();
    if (tSockList.size() > 0)
    {
        tIt = tSockList.begin();

        while (tIt != tSockList.end())
        {
            //#######################
            //### calculate line
            //#######################
            string tHost;
            unsigned int tPort;

            tPort = (*tIt)->GetLocalPort();
            tHost = (*tIt)->GetLocalHost();
            tLine = QString(tHost.c_str()) + "<" + QString("%1").arg(tPort) + ">,";
            tHost = (*tIt)->GetPeerHost();
            tPort = (*tIt)->GetPeerPort();
            tLine += QString(tHost.c_str()) + "<" + QString("%1").arg(tPort) + ">,";
            tLine += QString(Socket::NetworkType2String((*tIt)->GetNetworkType()).c_str()) + ",";
            tLine += QString(Socket::TransportType2String((*tIt)->GetTransportType()).c_str()) + ",";
            QoSSettings tQoSSettings;
            (*tIt)->GetQoS(tQoSSettings);
            tLine += (tQoSSettings.Features & QOS_FEATURE_LOSSLESS) ? "active," : "inactive,";
            tLine += QString("%1").arg(tQoSSettings.Delay) + ",";
            tLine += QString("%1").arg(tQoSSettings.DataRate);
            tLine += "\n";

            //#######################
            //### write to file
            //#######################
            tFile.write(tLine.toStdString().c_str(), tLine.size());

            //#######################
            //### next entry
            //#######################
            tIt++;
        }
    }
    SVC_SOCKET_CONTROL.ReleaseClientSocketsControl();

    tFile.close();
}

void OverviewNetworkStreamsWidget::FillCellText(int pRow, int pCol, QString pText)
{
    if (mTwOutgoingStreams->item(pRow, pCol) != NULL)
        mTwOutgoingStreams->item(pRow, pCol)->setText(pText);
    else
    {
        QTableWidgetItem *tItem =  new QTableWidgetItem(pText);
        tItem->setTextAlignment(Qt::AlignCenter|Qt::AlignVCenter);
        mTwOutgoingStreams->setItem(pRow, pCol, tItem);
    }
}

void OverviewNetworkStreamsWidget::FillRow(int pRow, Socket *pSocket)
{
    QString tText;

    if (pSocket == NULL)
		return;

	if (pRow > mTwOutgoingStreams->rowCount() - 1)
	    mTwOutgoingStreams->insertRow(mTwOutgoingStreams->rowCount());

	string tHost;
	unsigned int tPort;

    tPort = pSocket->GetLocalPort();
    tHost = pSocket->GetLocalHost();
    tText = QString(tHost.c_str()) + "<" + QString("%1").arg(tPort) + ">";
    if (mTwOutgoingStreams->item(pRow, 0) != NULL)
        mTwOutgoingStreams->item(pRow, 0)->setText(tText);
    else
    {
        mTwOutgoingStreams->setItem(pRow, 0, new QTableWidgetItem(tText));
        mTwOutgoingStreams->item(pRow, 0)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        mTwOutgoingStreams->item(pRow, 0)->setBackground(QBrush(QColor(Qt::lightGray)));
    }

	tHost = pSocket->GetPeerHost();
    tPort = pSocket->GetPeerPort();
	tText = QString(tHost.c_str()) + "<" + QString("%1").arg(tPort) + ">";
    if (mTwOutgoingStreams->item(pRow, 1) != NULL)
        mTwOutgoingStreams->item(pRow, 1)->setText(tText);
    else
    {
        mTwOutgoingStreams->setItem(pRow, 1, new QTableWidgetItem(tText));
        mTwOutgoingStreams->item(pRow, 1)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        mTwOutgoingStreams->item(pRow, 1)->setBackground(QBrush(QColor(Qt::lightGray)));
    }

    FillCellText(pRow, 2, QString(Socket::NetworkType2String(pSocket->GetNetworkType()).c_str()));
    FillCellText(pRow, 3, QString(Socket::TransportType2String(pSocket->GetTransportType()).c_str()));

	TableEntryLossless *tEntryLossless;
    if (mTwOutgoingStreams->cellWidget(pRow, 4) != NULL)
    {
        tEntryLossless = (TableEntryLossless*)mTwOutgoingStreams->cellWidget(pRow, 4);
        tEntryLossless->AssignSocket(pSocket);
    }else
    {
        tEntryLossless = new TableEntryLossless(mTwOutgoingStreams);
        tEntryLossless->AssignSocket(pSocket);
        mTwOutgoingStreams->setCellWidget(pRow, 4, tEntryLossless);
    }

    TableEntryDelay *tEntryDelay;
    if (mTwOutgoingStreams->cellWidget(pRow, 5) != NULL)
    {
        tEntryDelay = (TableEntryDelay*)mTwOutgoingStreams->cellWidget(pRow, 5);
        tEntryDelay->AssignSocket(pSocket);
    }else
    {
        tEntryDelay = new TableEntryDelay(mTwOutgoingStreams);
        tEntryDelay->AssignSocket(pSocket);
        mTwOutgoingStreams->setCellWidget(pRow, 5, tEntryDelay);
    }

    TableEntryDatarate *tTableEntryDatarate;
    if (mTwOutgoingStreams->cellWidget(pRow, 6) != NULL)
    {
        tTableEntryDatarate = (TableEntryDatarate*)mTwOutgoingStreams->cellWidget(pRow, 6);
        tTableEntryDatarate->AssignSocket(pSocket);
    }else
    {
        tTableEntryDatarate = new TableEntryDatarate(mTwOutgoingStreams);
        tTableEntryDatarate->AssignSocket(pSocket);
        mTwOutgoingStreams->setCellWidget(pRow, 6, tTableEntryDatarate);
    }
}

void OverviewNetworkStreamsWidget::UpdateView()
{
    SocketsList::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1, tVSelectedRow = -1;

    // save old widget state
    int tOldVertSliderPosition = mTwOutgoingStreams->verticalScrollBar()->sliderPosition();
    int tOldHorizSliderPosition = mTwOutgoingStreams->horizontalScrollBar()->sliderPosition();
    bool tWasVertMaximized = (tOldVertSliderPosition == mTwOutgoingStreams->verticalScrollBar()->maximum());
    if (mTwOutgoingStreams->selectionModel()->currentIndex().isValid())
        tSelectedRow = mTwOutgoingStreams->selectionModel()->currentIndex().row();

    // update widget content
    SocketsList tSockList = SVC_SOCKET_CONTROL.GetClientSocketsControl();
    if (tSockList.size() > 0)
    {
        tIt = tSockList.begin();

        while (tIt != tSockList.end())
        {
            FillRow(tRow++, *tIt);
            tIt++;
        }
    }
    SVC_SOCKET_CONTROL.ReleaseClientSocketsControl();

    for (int i = mTwOutgoingStreams->rowCount(); i > tRow; i--)
        mTwOutgoingStreams->removeRow(i);

    // restore old widget state
    mTwOutgoingStreams->setRowCount(tRow);
    if (tSelectedRow != -1)
        mTwOutgoingStreams->selectRow(tSelectedRow);
    if (tWasVertMaximized)
        mTwOutgoingStreams->verticalScrollBar()->setSliderPosition(mTwOutgoingStreams->verticalScrollBar()->maximum());
    else
        mTwOutgoingStreams->verticalScrollBar()->setSliderPosition(tOldVertSliderPosition);
    mTwOutgoingStreams->horizontalScrollBar()->setSliderPosition(tOldHorizSliderPosition);
}

void OverviewNetworkStreamsWidget::timerEvent(QTimerEvent *pEvent)
{
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
