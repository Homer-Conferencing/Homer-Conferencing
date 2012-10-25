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
 * Purpose: Implementation of a dialog for identity edit
 * Author:  Thomas Volkert
 * Since:   2008-12-02
 */

#include <Dialogs/IdentityDialog.h>
#include <Configuration.h>
#include <Meeting.h>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

IdentityDialog::IdentityDialog(QWidget* pParent) :
    QDialog(pParent)
{
    initializeGUI();
    LoadConfiguration();
}

IdentityDialog::~IdentityDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

int IdentityDialog::exec()
{
    int tResult = QDialog::exec();

    if (tResult == QDialog::Accepted)
        SaveConfiguration();

    return tResult;
}

void IdentityDialog::initializeGUI()
{
    setupUi(this);
}

void IdentityDialog::LoadConfiguration()
{
	if(CONF.GetSipInfrastructureMode() == 1)
		mLbServerAddress->setText(QString(MEETING.GetServerConferenceId().c_str()));
	else
		mLbServerAddress->setText("Server based contacting is disabled. Check configuration!");
	QString tLocalContactAddr = QString(MEETING.GetLocalConferenceId().c_str());
	switch(MEETING.GetHostPortTransport())
    {
        default:
        case SOCKET_TRANSPORT_AUTO:
        case SOCKET_UDP:
            break;
        case SOCKET_TCP:
            tLocalContactAddr += "[TCP]";
            break;
    }

    mLbDirectAddress->setText(tLocalContactAddr);

	if(MEETING.GetServerRegistrationState())
	{
		mServerStatus->setPixmap(QPixmap(":/images/32_32/UserAvailable.png"));
	}else
	{
		mServerStatus->setPixmap(QPixmap(":/images/32_32/UserUnavailable.png"));
	}

    if (MEETING.GetStunNatIp() != "")
        mLbNatAdr->setText(QString(MEETING.GetStunNatIp().c_str()));
    else
        mLbNatAdr->setText("Not yet detected. Check configuration!");
    if (MEETING.GetStunNatType() != "")
        mLbNatType->setText(QString(MEETING.GetStunNatType().c_str()));
    else
        mLbNatType->setText("Not yet detected. Check configuration!");
    mLeName->setText(CONF.GetUserName());
    mLeMail->setText(CONF.GetUserMail());
}

void IdentityDialog::SaveConfiguration()
{
    CONF.SetUserName(mLeName->text());
    CONF.SetUserMail(mLeMail->text());
    MEETING.SetLocalUserName(QString(CONF.GetUserName().toLocal8Bit()).toStdString());
    MEETING.SetLocalUserMailAdr(QString(CONF.GetUserMail().toLocal8Bit()).toStdString());
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
