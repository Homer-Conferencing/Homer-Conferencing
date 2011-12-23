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
 * Purpose: Implementation of a modified QPushButton for call button
 * Author:  Thomas Volkert
 * Since:   2008-12-15
 */

#include <Widgets/CallButton.h>
#include <Meeting.h>

#include <QPushButton>
#include <QString>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

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
    switch (MEETING.GetCallState(QString(mPartner.toLocal8Bit()).toStdString()))
    {
        case CALLSTATE_STANDBY:
                MEETING.SendCall(QString(mPartner.toLocal8Bit()).toStdString());
                // fake call state "ringing", otherwise the user is maybe confused when the "ringing" state takes too long and the button still displays "Call"
                setText("Cancel call");
                break;

        case CALLSTATE_RINGING:
                MEETING.SendCallCancel(QString(mPartner.toLocal8Bit()).toStdString());
                break;

        case CALLSTATE_RUNNING:
                MEETING.SendHangUp(QString(mPartner.toLocal8Bit()).toStdString());
                break;
    }
}

void CallButton::ShowNewState()
{
    switch(MEETING.GetCallState(QString(mPartner.toLocal8Bit()).toStdString()))
    {
        case CALLSTATE_STANDBY:
                setText("Conference");
                break;
        case CALLSTATE_RINGING:
                setText("Cancel call");
                break;
        case CALLSTATE_RUNNING:
                setText("Stop session");
                break;
    }
}

void CallButton::SetPartner(QString pPartner)
{
    mPartner = pPartner;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
