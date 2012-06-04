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
 * Purpose: Implementation of OverviewNetworkSimulationWidget.h
 * Author:  Thomas Volkert
 * Since:   2012-06-03
 */

#include <Core/Coordinator.h>
#include <Core/Scenario.h>
#include <Dialogs/ContactEditDialog.h>
#include <Widgets/OverviewNetworkSimulationWidget.h>
#include <MainWindow.h>
#include <ContactsPool.h>
#include <Configuration.h>
#include <Logger.h>

#include <QStandardItemModel>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QDockWidget>
#include <QModelIndex>
#include <QHostInfo>
#include <QPoint>
#include <QFileDialog>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class HierarchyItem:
    public QStandardItem
{
public:
    HierarchyItem(QString pText, Coordinator *pCoordinator, Node* pNode):QStandardItem()
    {
        mCoordinator = pCoordinator;
        mNode = pNode;
        setText(pText);
        //LOG(LOG_VERBOSE, "Creating hierarchy item %s with %d children", pText.toStdString().c_str(), pChildCount);
    }
    ~HierarchyItem(){ }

    Coordinator* GetCoordinator(){ return mCoordinator; }
    Node* GetNode(){ return mNode; }
private:
    Coordinator *mCoordinator;
    Node* mNode;
};

static struct StreamDescriptor sEmptyStreamDesc = {0, {0, 0, 0}, "", "", 0, 0};

class StreamItem:
    public QStandardItem
{
public:
    StreamItem(QString pText, struct StreamDescriptor pStreamDesc):QStandardItem(pText), mText(pText)
    {
        mStreamDesc = pStreamDesc;
        //LOG(LOG_VERBOSE, "Creating stream item %s", pText.toStdString().c_str());
    }
    ~StreamItem(){ }

    struct StreamDescriptor GetDesc(){ return mStreamDesc; }
    QString GetText(){ return mText; }

private:
    struct StreamDescriptor mStreamDesc;
    QString                 mText;
};

///////////////////////////////////////////////////////////////////////////////

OverviewNetworkSimulationWidget::OverviewNetworkSimulationWidget(QAction *pAssignedAction, QMainWindow* pMainWindow, Scenario *pScenario):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mMainWindow = pMainWindow;
    mScenario = pScenario;
    mStreamRow = 0;
    mHierarchyRow = 0;
    mHierarchyCol = 0;
    mHierarchyEntry = NULL;

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::TopDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(true);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTvHierarchy, SIGNAL(clicked(QModelIndex)), this, SLOT(SelectedCoordinator(QModelIndex)));
    connect(mTvStreams, SIGNAL(clicked(QModelIndex)), this, SLOT(SelectedStream(QModelIndex)));

    SetVisible(CONF.GetVisibilityNetworkSimulationWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityNetworkSimulationWidget());

    mTvHierarchyModel = new QStandardItemModel(this);
    mTvHierarchy->setModel(mTvHierarchyModel);
    mTvStreamsModel = new QStandardItemModel(this);
    mTvStreams->setModel(mTvStreamsModel);

    UpdateHierarchyView();
    UpdateStreamsView();
}

OverviewNetworkSimulationWidget::~OverviewNetworkSimulationWidget()
{
    CONF.SetVisibilityNetworkSimulationWidget(isVisible());
    delete mTvHierarchyModel;
    delete mTvStreamsModel;
}

///////////////////////////////////////////////////////////////////////////////

void OverviewNetworkSimulationWidget::initializeGUI()
{
    setupUi(this);
}

void OverviewNetworkSimulationWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewNetworkSimulationWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
        // update GUI elements every x ms
        mTimerId = startTimer(NETWORK_SIMULATION_GUI_UPDATE_TIME);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewNetworkSimulationWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
//    QAction *tAction;
//
//    QMenu tMenu(this);
//
//    tAction = tMenu.addAction("Add contact");
//    QIcon tIcon1;
//    tIcon1.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
//    tAction->setIcon(tIcon1);
//
//    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
//    if (tPopupRes != NULL)
//    {
//        if (tPopupRes->text().compare("Add contact") == 0)
//        {
//            InsertNew();
//            return;
//        }
//    }
}

void OverviewNetworkSimulationWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
    {
        UpdateStreamsView();
        UpdateNetworkView();
    }
}

void OverviewNetworkSimulationWidget::SelectedCoordinator(QModelIndex pIndex)
{
    if(!pIndex.isValid())
        return;

    int tCurHierarchyRow = pIndex.row();
    int tCurHierarchyCol = pIndex.column();
    void *tCurHierarchyEntry = pIndex.internalPointer();
    if (tCurHierarchyEntry != NULL)
        tCurHierarchyEntry = ((QStandardItem*)tCurHierarchyEntry)->child(tCurHierarchyRow, tCurHierarchyCol);

    HierarchyItem *tItem = (HierarchyItem*)tCurHierarchyEntry;

    if (tItem != NULL)
    {
        if (tItem->GetCoordinator() != NULL)
        {
            LOG(LOG_VERBOSE, "User selected coordinator in row: %d and col.: %d, coordinator: %s", tCurHierarchyRow, tCurHierarchyCol, tItem->GetCoordinator()->GetClusterAddress().c_str());
        }else if (tItem->GetNode() != NULL)
        {
            LOG(LOG_VERBOSE, "User selected node in row: %d and col.: %d, node: %s", tCurHierarchyRow, tCurHierarchyCol, tItem->GetNode()->GetAddress().c_str());
        }else
        {
            LOG(LOG_VERBOSE, "User selected coordinator in row: %d and col.: %d, internal pointer: %p", tCurHierarchyRow, tCurHierarchyCol, pIndex.internalPointer());
        }
    }

    if ((tCurHierarchyRow != mHierarchyRow) || (tCurHierarchyCol != mHierarchyCol) || (tCurHierarchyEntry != mHierarchyEntry))
    {
        LOG(LOG_VERBOSE, "Update of hierarchy view needed");
        mHierarchyRow = tCurHierarchyRow;
        mHierarchyCol = tCurHierarchyCol;
        mHierarchyEntry = tCurHierarchyEntry;
        ShowHierarchyDetails((tItem->GetCoordinator() != NULL) ? tItem->GetCoordinator() : NULL, tItem->GetNode() ? tItem->GetNode() : NULL);
    }
}

void OverviewNetworkSimulationWidget::SelectedStream(QModelIndex pIndex)
{
    if(!pIndex.isValid())
        return;

    int tCurStreamRow = pIndex.row();
    LOG(LOG_VERBOSE, "User selected stream in row: %d", tCurStreamRow);

    if(tCurStreamRow != mStreamRow)
    {
        LOG(LOG_VERBOSE, "Update of streams view needed");
        mStreamRow = tCurStreamRow;
    }
}

void OverviewNetworkSimulationWidget::ShowHierarchyDetails(Coordinator *pCoordinator, Node* pNode)
{
    if (pCoordinator != NULL)
    {
        mLbHierarchyLevel->setText(QString("%1").arg(pCoordinator->GetHierarchyLevel()));
        mLbSiblings->setText(QString("%1").arg(pCoordinator->GetSiblings().size()));
        if (pCoordinator->GetHierarchyLevel() != 0)
            mLbChildren->setText(QString("%1").arg(pCoordinator->GetChildCoordinators().size()));
        else
            mLbChildren->setText(QString("%1").arg(pCoordinator->GetClusterMembers().size()));
    }else
    {
        if (pNode != NULL)
        {
            mLbHierarchyLevel->setText("node");
            mLbSiblings->setText(QString("%1").arg(pNode->GetSiblings().size()));
            mLbChildren->setText("0");
        }else
        {
            mLbHierarchyLevel->setText("-");
            mLbSiblings->setText("-");
            mLbChildren->setText("-");
        }
    }
}

void OverviewNetworkSimulationWidget::UpdateHierarchyView()
{
    Coordinator *tRootCoordinator = mScenario->GetRootCoordinator();
    mTvHierarchyModel->clear();
    QStandardItem *tRootItem = mTvHierarchyModel->invisibleRootItem();

    // create root entry of the tree
    HierarchyItem *tRootCoordinatorItem = new HierarchyItem("Root: " + QString(tRootCoordinator->GetClusterAddress().c_str()), tRootCoordinator, NULL);
    tRootItem->appendRow(tRootCoordinatorItem);

    // recursive creation of tree items
    AppendHierarchySubItems(tRootCoordinatorItem);

    // show all entries of the tree
    mTvHierarchy->expandAll();

    if (mHierarchyEntry == NULL)
        ShowHierarchyDetails(NULL, NULL);
}

void OverviewNetworkSimulationWidget::AppendHierarchySubItems(HierarchyItem *pParentItem)
{
    Coordinator *tParentCoordinator = pParentItem->GetCoordinator();

    if (tParentCoordinator->GetHierarchyLevel() == 0)
    {// end of tree reached
        NodeList tNodeList = tParentCoordinator->GetClusterMembers();
        NodeList::iterator tIt;
        int tCount = 0;
        for (tIt = tNodeList.begin(); tIt != tNodeList.end(); tIt++)
        {
            HierarchyItem* tItem = new HierarchyItem("Node  " + QString((*tIt)->GetAddress().c_str()), NULL, *tIt);
            pParentItem->setChild(tCount, tItem);
            tCount++;
        }
    }else
    {// further sub trees possible
        CoordinatorList tCoordinatorList = tParentCoordinator->GetChildCoordinators();
        CoordinatorList::iterator tIt;
        QList<QStandardItem*> tItems;
        int tCount = 0;
        for (tIt = tCoordinatorList.begin(); tIt != tCoordinatorList.end(); tIt++)
        {
            HierarchyItem *tItem = new HierarchyItem("Coord. " + QString((*tIt)->GetClusterAddress().c_str()), *tIt, NULL);
            pParentItem->setChild(tCount, tItem);
            //LOG(LOG_VERBOSE, "Appending coordinator %s", tItem->GetCoordinator()->GetClusterAddress().c_str());
            AppendHierarchySubItems(tItem);
            tCount++;
        }
    }
}

// #####################################################################
// ############ streams view
// #####################################################################
void OverviewNetworkSimulationWidget::ShowStreamDetails(const struct StreamDescriptor pDesc)
{
    mLbPackets->setText(QString("%1").arg(pDesc.PacketCount));
    mLbDataRate->setText(QString("%1").arg(pDesc.QoSRequs.DataRate));
    mLbDelay->setText(QString("%1").arg(pDesc.QoSRequs.Delay));
}

QString OverviewNetworkSimulationWidget::CreateStreamId(const struct StreamDescriptor pDesc)
{
    return QString(pDesc.LocalNode.c_str()) + ":" + QString("%1").arg(pDesc.LocalPort) + " <==> " +QString(pDesc.PeerNode.c_str()) + ":" + QString("%1").arg(pDesc.PeerPort);
}

void OverviewNetworkSimulationWidget::UpdateStreamsView()
{
    bool tResetNeeded = false;

    //LOG(LOG_VERBOSE, "Updating streams view");

    StreamList tStreams = mScenario->GetStreams();
    StreamList::iterator tIt;
    QStandardItem *tRootItem = mTvStreamsModel->invisibleRootItem();

    if (tStreams.size() == 0)
    {
        ShowStreamDetails(sEmptyStreamDesc);
        // if the QTreeView is already empty we can return immediately
        if (tRootItem->rowCount() == 0)
            return;
    }

    int tCount = 0;
    // check if update is need
    if (tRootItem->rowCount() != 0)
    {
        for(tIt = tStreams.begin(); tIt != tStreams.end(); tIt++)
        {
            QString tDesiredString  = CreateStreamId(*tIt);
            StreamItem* tCurItem = (StreamItem*) mTvStreamsModel->item(tCount, 0);
            if (tCurItem != NULL)
            {
                LOG(LOG_WARN, "Comparing %s and %s", tDesiredString.toStdString().c_str(), tCurItem->GetText().toStdString().c_str());
                if (tDesiredString != tCurItem->GetText())
                    tResetNeeded = true;
            }else
                tResetNeeded = true;
            if (tCount == mStreamRow)
                ShowStreamDetails(*tIt);
            tCount++;
        }
    }else
        tResetNeeded = true;

    // update the entire view
    if (tResetNeeded)
    {
        tCount = 0;
        mTvStreamsModel->clear();
        tRootItem = mTvStreamsModel->invisibleRootItem();
        if (tRootItem == NULL)
        {
            LOG(LOG_WARN, "Root item is invalid");
            tRootItem = new QStandardItem();
            mTvStreamsModel->appendRow(tRootItem);
        }
        for(tIt = tStreams.begin(); tIt != tStreams.end(); tIt++)
        {
            StreamItem* tItem = new StreamItem(CreateStreamId(*tIt), *tIt);
            tRootItem->appendRow(tItem);
            if (tCount == mStreamRow)
                ShowStreamDetails(*tIt);
            tCount++;
        }
    }
}

// #####################################################################
// ############ network view
// #####################################################################
void OverviewNetworkSimulationWidget::UpdateNetworkView()
{

}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
