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
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

SessionInfoWidget::SessionInfoWidget(QWidget* pParent):
    QWidget(pParent)
{
    parentWidget()->hide();
    hide();
}

void SessionInfoWidget::Init(QString pParticipant, enum TransportType pParticipantTransport, bool pVisible)
{
    mParticipant = pParticipant;
    mParticipantTransport = pParticipantTransport;

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

void SessionInfoWidget::InitializeMenuSessionInfoSettings(QMenu *pMenu)
{
    QAction *tAction;

    pMenu->clear();

    if (isVisible())
        tAction = pMenu->addAction(QPixmap(":/images/22_22/Close.png"), Homer::Gui::SessionInfoWidget::tr("Close"));
    else
        tAction = pMenu->addAction(QPixmap(":/images/22_22/Messages.png"), Homer::Gui::SessionInfoWidget::tr("Show"));
}

void SessionInfoWidget::SelectedMenuSessionInfoSettings(QAction *pAction)
{
    if (pAction != NULL)
    {
        if (pAction->text().compare(Homer::Gui::SessionInfoWidget::tr("Show")) == 0)
        {
            ToggleVisibility();
            return;
        }
        if (pAction->text().compare(Homer::Gui::SessionInfoWidget::tr("Close")) == 0)
        {
            ToggleVisibility();
            return;
        }
    }
}

void SessionInfoWidget::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QMenu tMenu(this);

    InitializeMenuSessionInfoSettings(&tMenu);

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
        SelectedMenuSessionInfoSettings(tPopupRes);
}

void SessionInfoWidget::UpdateView()
{
    struct SessionInfo tInfo;
    if (MEETING.GetSessionInfo(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport, &tInfo))
    {
        if (!mLeParticipant->hasSelectedText())
            mLeParticipant->setText(QString(MEETING.SipCreateId(tInfo.User, tInfo.Host, tInfo.Port).c_str()) + "[" + QString(tInfo.Transport.c_str()) + "]");
        if (!mLeSipInterface->hasSelectedText())
        	mLeSipInterface->setText(mSipInterface);
        if (!mLeCallState->hasSelectedText())
            mLeCallState->setText(QString((tInfo.CallState).c_str()));
        if (!mLeNat->hasSelectedText())
            mLeNat->setText(QString((tInfo.OwnIp + "<" + tInfo.OwnPort + ">").c_str()));
        if (!mLeVideo->hasSelectedText())
            mLeVideo->setText(QString((tInfo.RemoteVideoCodec + "@" + tInfo.RemoteVideoHost + "<" + tInfo.RemoteVideoPort + ">[PT: " + toString(tInfo.RTPPayloadIDVideo) + "]").c_str()));
        if (!mLeAudio->hasSelectedText())
            mLeAudio->setText(QString((tInfo.RemoteAudioCodec + "@" + tInfo.RemoteAudioHost + "<" + tInfo.RemoteAudioPort + ">[PT: " + toString(tInfo.RTPPayloadIDAudio) + "]").c_str()));
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
}


///////////////////////////////////////////////////////////////////////////////

}} //namespace
