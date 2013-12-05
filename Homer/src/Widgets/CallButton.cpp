/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a modified QPushButton for call button
 * Since:   2008-12-15
 */

#include <Widgets/CallButton.h>
#include <Meeting.h>

#include <QPushButton>
#include <QString>

namespace Homer { namespace Gui {

using namespace Homer::Conference;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

CallButton::CallButton(QWidget* pParent) :
    QPushButton(pParent)
{
    connect(this, SIGNAL(clicked()), this, SLOT(HandleClick()));
}

CallButton::~CallButton()
{
}

///////////////////////////////////////////////////////////////////////////////

void CallButton::HandleClick()
{
    switch (MEETING.GetCallState(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport))
    {
        case CALLSTATE_STANDBY:
                MEETING.SendCall(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport);
                // fake call state "ringing", otherwise the user is maybe confused when the "ringing" state takes too long and the button still displays "Call"
                setText("Cancel call");
                break;

        case CALLSTATE_RINGING:
                MEETING.SendCallCancel(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport);
                break;

        case CALLSTATE_RUNNING:
                MEETING.SendHangUp(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport);
                break;
    }
    ShowNewState();
}

void CallButton::ShowNewState()
{
    switch(MEETING.GetCallState(QString(mParticipant.toLocal8Bit()).toStdString(), mParticipantTransport))
    {
        case CALLSTATE_STANDBY:
                setText(Homer::Gui::CallButton::tr("Call"));
                break;
        case CALLSTATE_RINGING:
                setText(Homer::Gui::CallButton::tr("Hangup"));
                break;
        case CALLSTATE_RUNNING:
                setText(Homer::Gui::CallButton::tr("Hangup"));
                break;
    }
}

void CallButton::SetPartner(QString pParticipant, enum TransportType pTransport)
{
    mParticipant = pParticipant;
    mParticipantTransport = pTransport;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
