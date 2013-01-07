/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of OverviewThreadsWidget.h
 * Author:  Thomas Volkert
 * Since:   2009-05-17
 */

#include <Widgets/OverviewThreadsWidget.h>
#include <ProcessStatisticService.h>
#include <Configuration.h>
#include <Logger.h>
#include <HBSystem.h>
#include <Snippets.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QSizePolicy>
#include <QScrollBar>
#include <QMenu>
#include <QContextMenuEvent>

namespace Homer { namespace Gui {

using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

OverviewThreadsWidget::OverviewThreadsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mTimerId = -1;
    mScaleToOneCpuCore = true;
    mCpuCores = System::GetMachineCores();

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::TopDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(false);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    if (CONF.DebuggingEnabled())
    {
        SetVisible(CONF.GetVisibilityThreadsWidget());
        mAssignedAction->setChecked(CONF.GetVisibilityThreadsWidget());
    }else
    {
        SetVisible(false);
        mAssignedAction->setChecked(false);
        mAssignedAction->setVisible(false);
        toggleViewAction()->setVisible(false);
    }
}

OverviewThreadsWidget::~OverviewThreadsWidget()
{
	CONF.SetVisibilityThreadsWidget(isVisible());
}

///////////////////////////////////////////////////////////////////////////////

void OverviewThreadsWidget::initializeGUI()
{
    setupUi(this);

    // hide id column
    mTwThreads->setColumnHidden(12, true);
    mTwThreads->sortItems(12);
    mTwThreads->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    mTwThreads->horizontalHeader()->resizeSection(0, mTwThreads->horizontalHeader()->sectionSize(0) * 3);
    mTwThreads->horizontalHeader()->resizeSection(7, mTwThreads->horizontalHeader()->sectionSize(7) * 2);
    mTwThreads->horizontalHeader()->resizeSection(8, mTwThreads->horizontalHeader()->sectionSize(8) * 2);

    UpdateView();
}

void OverviewThreadsWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewThreadsWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction(Homer::Gui::OverviewThreadsWidget::tr("Scaled to one cpu core"));
    tAction->setCheckable(true);
    tAction->setChecked(mScaleToOneCpuCore);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare(Homer::Gui::OverviewThreadsWidget::tr("Scaled to one cpu core")) == 0)
        {
            mScaleToOneCpuCore = !mScaleToOneCpuCore;
            LOG(LOG_VERBOSE, "Scale to one cpu core: %d", mScaleToOneCpuCore);
            UpdateView();
            return;
        }
    }
}

void OverviewThreadsWidget::SetVisible(bool pVisible)
{
	CONF.SetVisibilityThreadsWidget(pVisible);
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

void OverviewThreadsWidget::FillCellText(int pRow, int pCol, QString pText)
{
    if (mTwThreads->item(pRow, pCol) != NULL)
        mTwThreads->item(pRow, pCol)->setText(pText);
    else
    {
        QTableWidgetItem *tItem =  new QTableWidgetItem(pText);
        tItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        mTwThreads->setItem(pRow, pCol, tItem);
    }
}

void OverviewThreadsWidget::FillRow(int pRow, ProcessStatistic *pStats)
{
	ThreadStatisticDescriptor tStatValues = pStats->GetThreadStatistic();

	int tScaleFactor = 1;
	if (mScaleToOneCpuCore)
	    tScaleFactor = mCpuCores;

	if (pRow > mTwThreads->rowCount() - 1)
        mTwThreads->insertRow(mTwThreads->rowCount());

    if (mTwThreads->item(pRow, 0) != NULL)
        mTwThreads->item(pRow, 0)->setText(QString(pStats->GetThreadName().c_str()));
    else
        mTwThreads->setItem(pRow, 0, new QTableWidgetItem(QString(pStats->GetThreadName().c_str())));
    mTwThreads->item(pRow, 0)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    mTwThreads->item(pRow, 0)->setBackground(QBrush(QColor(Qt::lightGray)));

    FillCellText(pRow, 1, QString("%1").arg(tStatValues.Tid));
    FillCellText(pRow, 2, QString("%1").arg(tStatValues.Pid));
    FillCellText(pRow, 3, QString("%1").arg(tStatValues.PPid));
    FillCellText(pRow, 4, QString("%1").arg(tStatValues.ThreadCount));
    FillCellText(pRow, 5, QString("%1").arg(tStatValues.PriorityBase));
    FillCellText(pRow, 6, QString("%1").arg(tStatValues.Priority));
    FillCellText(pRow, 7, Int2ByteExpression(tStatValues.MemVirtual) + " bytes");
    FillCellText(pRow, 8, Int2ByteExpression(tStatValues.MemPhysical) + " bytes");
    FillCellText(pRow, 9, QString("%1 %").arg(tStatValues.LoadUser * tScaleFactor, 0, 'f', 2));
    FillCellText(pRow, 10, QString("%1 %").arg(tStatValues.LoadSystem * tScaleFactor, 0, 'f', 2));
    FillCellText(pRow, 11, QString("%1 %").arg(tStatValues.LoadTotal * tScaleFactor, 0, 'f', 2));
    FillCellText(pRow, 12, QString("%1").arg(pRow));
}

void OverviewThreadsWidget::UpdateView()
{
    ProcessStatistics::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1;

    // save old widget state
    int tOldVertSliderPosition = mTwThreads->verticalScrollBar()->sliderPosition();
    int tOldHorizSliderPosition = mTwThreads->horizontalScrollBar()->sliderPosition();
    bool tWasVertMaximized = (tOldVertSliderPosition == mTwThreads->verticalScrollBar()->maximum());
    if (mTwThreads->selectionModel()->currentIndex().isValid())
        tSelectedRow = mTwThreads->selectionModel()->currentIndex().row();

    // update widget content
    ProcessStatistics tStatList = SVC_PROCESS_STATISTIC.GetProcessStatistics();
    for (tIt = tStatList.begin(); tIt != tStatList.end(); tIt++)
        FillRow(tRow++, *tIt);

    for (int i = mTwThreads->rowCount(); i > tRow; i--)
        mTwThreads->removeRow(i);

    // restore old widget state
    mTwThreads->setRowCount(tRow);
    if (tSelectedRow != -1)
        mTwThreads->selectRow(tSelectedRow);
    if (tWasVertMaximized)
        mTwThreads->verticalScrollBar()->setSliderPosition(mTwThreads->verticalScrollBar()->maximum());
    else
        mTwThreads->verticalScrollBar()->setSliderPosition(tOldVertSliderPosition);
    mTwThreads->horizontalScrollBar()->setSliderPosition(tOldHorizSliderPosition);
}

void OverviewThreadsWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
