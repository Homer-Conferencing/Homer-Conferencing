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

namespace Homer { namespace Gui {

using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

OverviewThreadsWidget::OverviewThreadsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow)
{
    mAssignedAction = pAssignedAction;
    mTimerId = -1;
    mSummarizeCpuCores = true;
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

    QPalette palette;
    QBrush brush(QColor(0, 255, 255, 255));
    QBrush brush1(QColor(0, 128, 128, 255));
    QBrush brush2(QColor(155, 220, 198, 255));
    QBrush brush3(QColor(98, 99, 98, 255));
    QBrush brush4(QColor(100, 102, 100, 255));
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

void OverviewThreadsWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewThreadsWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Summarize cpu cores");
    tAction->setCheckable(true);
    tAction->setChecked(mSummarizeCpuCores);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Summarize cpu cores") == 0)
        {
            mSummarizeCpuCores = !mSummarizeCpuCores;
            LOG(LOG_VERBOSE, "Cpu cores are summarized: %d", mSummarizeCpuCores);
            UpdateView();
            return;
        }
    }
}

void OverviewThreadsWidget::SetVisible(bool pVisible)
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
	if (pStats == NULL)
		return;

	ThreadStatisticDescriptor tStatValues = pStats->GetThreadStatistic();

	int tCpuCores = 1;
	if (mSummarizeCpuCores)
	    tCpuCores = mCpuCores;

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
    FillCellText(pRow, 9, QString("%1 %").arg(tStatValues.LoadUser * tCpuCores, 0, 'f', 2));
    FillCellText(pRow, 10, QString("%1 %").arg(tStatValues.LoadSystem * tCpuCores, 0, 'f', 2));
    FillCellText(pRow, 11, QString("%1 %").arg(tStatValues.LoadTotal * tCpuCores, 0, 'f', 2));
    FillCellText(pRow, 12, QString("%1").arg(pRow));
}

void OverviewThreadsWidget::UpdateView()
{
    ProcessStatisticsList::iterator tIt;
    int tRow = 0;
    int tSelectedRow = -1;

    // save old widget state
    int tOldVertSliderPosition = mTwThreads->verticalScrollBar()->sliderPosition();
    int tOldHorizSliderPosition = mTwThreads->horizontalScrollBar()->sliderPosition();
    bool tWasVertMaximized = (tOldVertSliderPosition == mTwThreads->verticalScrollBar()->maximum());
    if (mTwThreads->selectionModel()->currentIndex().isValid())
        tSelectedRow = mTwThreads->selectionModel()->currentIndex().row();

    // update widget content
    ProcessStatisticsList tStatList = SVC_PROCESS_STATISTIC.GetProcessStatistics();
    if (tStatList.size() > 0)
    {
        tIt = tStatList.begin();

        while (tIt != tStatList.end())
        {
            FillRow(tRow++, *tIt);
            tIt++;
        }
    }

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
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
