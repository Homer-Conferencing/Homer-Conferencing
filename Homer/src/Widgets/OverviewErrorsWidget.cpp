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
 * Purpose: Implementation of OverviewErrorsWidget
 * Author:  Thomas Volkert
 * Since:   2011-08-14
 */

#include <Widgets/OverviewErrorsWidget.h>
#include <PacketStatisticService.h>
#include <Configuration.h>
#include <Logger.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimerEvent>
#include <QHeaderView>
#include <QFileDialog>
#include <QScrollBar>
#include <QSizePolicy>
#include <QMenu>
#include <QContextMenuEvent>

namespace Homer { namespace Gui {

using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

OverviewErrorsWidget::OverviewErrorsWidget(QAction *pAssignedAction, QMainWindow* pMainWindow):
    QDockWidget(pMainWindow),
    LogSink()
{
    mNewLogMessageReceived = true;
    mAssignedAction = pAssignedAction;
    mTimerId = -1;
    mLogSinkId = "Qt-based error view";

    initializeGUI();

    setAllowedAreas(Qt::AllDockWidgetAreas);
    pMainWindow->addDockWidget(Qt::BottomDockWidgetArea, this);

    if (mAssignedAction != NULL)
    {
        connect(mAssignedAction, SIGNAL(triggered(bool)), this, SLOT(SetVisible(bool)));
        mAssignedAction->setChecked(false);
    }
    connect(toggleViewAction(), SIGNAL(toggled(bool)), mAssignedAction, SLOT(setChecked(bool)));
    connect(mTeErrorLog, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(ErrorLogCustomContextMenuEvent(const QPoint&)));
    SetVisible(CONF.GetVisibilityErrorsWidget());
    mAssignedAction->setChecked(CONF.GetVisibilityErrorsWidget());

    LOGGER.RegisterLogSink(this);
}

OverviewErrorsWidget::~OverviewErrorsWidget()
{
    CONF.SetVisibilityErrorsWidget(isVisible());
    LOGGER.UnregisterLogSink(this);
}

///////////////////////////////////////////////////////////////////////////////

void OverviewErrorsWidget::initializeGUI()
{
    setupUi(this);

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

void OverviewErrorsWidget::closeEvent(QCloseEvent* pEvent)
{
    SetVisible(false);
}

void OverviewErrorsWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        show();
        mTimerId = startTimer(VIEW_ERROR_LOG_UPDATE_PERIOD);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        hide();
    }
}

void OverviewErrorsWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Save error log");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/DriveSave.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Save error log") == 0)
        {
            SaveLog();
            return;
        }
    }
}

void OverviewErrorsWidget::ErrorLogCustomContextMenuEvent(const QPoint &pPos)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Save error log");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/DriveSave.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(QCursor::pos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Save error log") == 0)
        {
            SaveLog();
            return;
        }
    }
}

void OverviewErrorsWidget::SaveLog()
{
    QString tFileName = QFileDialog::getSaveFileName(this,
                                                     "Save error log",
                                                     QDir::homePath() + "/HomerErrorLog.txt",
                                                     "Text Document File (*.txt)",
                                                     NULL,
                                                     QFileDialog::DontUseNativeDialog);

    if (tFileName.isEmpty())
        return;

    // write history to file
    QFile tFile(tFileName);
    if (!tFile.open(QIODevice::ReadWrite))
        return;

    tFile.write("======================================\n");
    tFile.write("=======> Homer "RELEASE_VERSION_STRING" <=======\n");
    tFile.write("======================================\n");
    mLogBufferMutex.lock();
    tFile.write(mTeErrorLog->toPlainText().toAscii());
    mLogBufferMutex.unlock();

    tFile.close();
}

void OverviewErrorsWidget::ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage)
{
    if (pLevel != LOG_ERROR)
        return;

    QString tMessage = QString(pMessage.c_str());
    QString tTime = QString(pTime.c_str());
    QString tSource = QString(pSource.c_str());

    // replace ENTER with corresponding html tag
    // hint: necessary because this QTextEdit is in html-mode and caused by this it ignores "\n"
    tMessage.replace(QString("\n"), QString("<br>"));

    QString tLogEntry = "<font color=blue><b>" + tTime + "</b></font> <font color=black>" + tSource + "(" + QString("%1").arg(pLine) + "):</font> <font color=red> " + tMessage + "</font><br>";

    if (mLogBufferMutex.tryLock(10 * 1000))
    {
        mNewLogMessageReceived = true;
        mLogBuffer += tLogEntry;
        mLogBufferMutex.unlock();
    }else
        printf("Got timeout for lock which is responsible for logging debug message to GUI based logger, dropping this message");
}



void OverviewErrorsWidget::UpdateView()
{
    // nothing to be done? -> return immediately, otherwise we would waste resources
    if(!mNewLogMessageReceived)
        return;

    //printf("Call to UpdateView\n");
    if (mLogBufferMutex.tryLock(10 * 1000))
    {
        QString tLogBuffer = mLogBuffer;
        mNewLogMessageReceived = false;
        mLogBufferMutex.unlock();

        int tOldVertSliderPosition = mTeErrorLog->verticalScrollBar()->sliderPosition();
        int tOldHorizSliderPosition = mTeErrorLog->horizontalScrollBar()->sliderPosition();
        bool tWasVertMaximized = (tOldVertSliderPosition == mTeErrorLog->verticalScrollBar()->maximum());
        mTeErrorLog->setText(tLogBuffer);
        if (tWasVertMaximized)
            mTeErrorLog->verticalScrollBar()->setSliderPosition(mTeErrorLog->verticalScrollBar()->maximum());
        else
            mTeErrorLog->verticalScrollBar()->setSliderPosition(tOldVertSliderPosition);
        mTeErrorLog->horizontalScrollBar()->setSliderPosition(tOldHorizSliderPosition);
    }else
        printf("Got timeout for lock which is responsible for log buffer, skipping update of error widget");
}

void OverviewErrorsWidget::timerEvent(QTimerEvent *pEvent)
{
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
