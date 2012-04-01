/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a widget for displaying extended session information
 * Author:  Thomas Volkert
 * Since:   2010-09-18
 */

#include <Widgets/SessionInfoWidget.h>
#include <Configuration.h>
#include <Meeting.h>

#include <QWidget>
#include <QTime>
#include <QMenu>
#include <QContextMenuEvent>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

SessionInfoWidget::SessionInfoWidget(QWidget* pParent):
    QWidget(pParent)
{
    parentWidget()->hide();
    hide();
}

void SessionInfoWidget::Init(QString pParticipant, bool pVisible)
{
    mParticipant = pParticipant;
    mTimerId = -1;

    initializeGUI();

    //####################################################################
    //### update GUI
    //####################################################################

    setWindowTitle("Session information: " + mParticipant);
    SetVisible(pVisible);
}

SessionInfoWidget::~SessionInfoWidget()
{
}

///////////////////////////////////////////////////////////////////////////////

void SessionInfoWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Close session info");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/Close.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Close session info") == 0)
        {
            ToggleVisibility();
            return;
        }
    }
}

void SessionInfoWidget::UpdateView()
{
    struct SessionInfo tInfo;
    if (MEETING.GetSessionInfo(QString(mParticipant.toLocal8Bit()).toStdString(), &tInfo))
    {
        if (!mLeParticipant->hasSelectedText())
            mLeParticipant->setText(QString(MEETING.SipCreateId(tInfo.User, tInfo.Host, tInfo.Port).c_str()));
        if (!mLeSipInterface->hasSelectedText())
        	mLeSipInterface->setText(mSipInterface);
        if (!mLeCallState->hasSelectedText())
            mLeCallState->setText(QString((tInfo.CallState).c_str()));
        if (!mLeNat->hasSelectedText())
            mLeNat->setText(QString((tInfo.OwnIp + "<" + tInfo.OwnPort + ">").c_str()));
        if (!mLeVideo->hasSelectedText())
            mLeVideo->setText(QString((tInfo.RemoteVideoCodec + "@" + tInfo.RemoteVideoHost + "<" + tInfo.RemoteVideoPort + ">").c_str()));
        if (!mLeAudio->hasSelectedText())
            mLeAudio->setText(QString((tInfo.RemoteAudioCodec + "@" + tInfo.RemoteAudioHost + "<" + tInfo.RemoteAudioPort + ">").c_str()));
        if (!mLeLocalAudio->hasSelectedText())
            mLeLocalAudio->setText(QString((tInfo.RemoteAudioCodec + "@" + MEETING.GetHostAdr() + "<" + tInfo.LocalAudioPort + ">").c_str()));
        if (!mLeLocalVideo->hasSelectedText())
            mLeLocalVideo->setText(QString((tInfo.RemoteVideoCodec + "@" + MEETING.GetHostAdr() + "<" + tInfo.LocalVideoPort + ">").c_str()));
    }
}

void SessionInfoWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    if (pEvent->timerId() == mTimerId)
        UpdateView();
}

void SessionInfoWidget::closeEvent(QCloseEvent *pEvent)
{
    ToggleVisibility();
}

void SessionInfoWidget::ToggleVisibility()
{
    if (isVisible())
        SetVisible(false);
    else
        SetVisible(true);
}

void SessionInfoWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        move(mWinPos);
        parentWidget()->show();
        show();

        // update GUI elements every 250 ms
        mTimerId = startTimer(250);
    }else
    {
        if (mTimerId != -1)
            killTimer(mTimerId);
        mWinPos = pos();
        parentWidget()->hide();
        hide();
    }
}

void SessionInfoWidget::SetSipInterface(QString pSipInterface)
{
	mSipInterface = pSipInterface;
}

void SessionInfoWidget::initializeGUI()
{
    setupUi(this);

    QPalette palette;
    QBrush brush(QColor(250, 250, 255, 255));
    QBrush brush1(QColor(145, 191, 155, 255));

    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
            brush.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Active, QPalette::Base, brush);
            brush1.setStyle(Qt::SolidPattern);
            palette.setBrush(QPalette::Active, QPalette::Window, brush1);
            palette.setBrush(QPalette::Inactive, QPalette::Base, brush);
            palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
            palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
            palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
            setPalette(palette);
            break;
        default:
            break;
    }
}


///////////////////////////////////////////////////////////////////////////////

}} //namespace
