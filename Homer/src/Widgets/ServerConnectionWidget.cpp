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
 * Purpose: Implementation of ServerConnectionWidget.h
 * Author:  Thomas Volkert
 * Since:   2011-10-08
 */

#include <Widgets/ServerConnectionWidget.h>
#include <Configuration.h>
#include <Meeting.h>
#include <Logger.h>
#include <Snippets.h>

#include <QWidget>
#include <QMenu>
#include <QMessageBox>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

ServerConnectionWidget::ServerConnectionWidget(QWidget* pParent):
    QWidget(pParent)
{
    initializeGUI();
}

ServerConnectionWidget::~ServerConnectionWidget()
{
}

///////////////////////////////////////////////////////////////////////////////

void ServerConnectionWidget::initializeGUI()
{
    setupUi(this);

    mMenu = new QMenu(this);
    InitializeRegistrationMenu(mMenu);
    mTbRegistration->setMenu(mMenu);
    if (MEETING.GetServerRegistrationState())
    {
        mTbRegistration->setText("Registered");
        mTbRegistration->setIcon(QPixmap(":/images/UserAvailable.png"));
    }else
    {
        mTbRegistration->setText("Not registered");
        mTbRegistration->setIcon(QPixmap(":/images/UserUnavailable.png"));
    }

    connect(mTbRegistration, SIGNAL(triggered(QAction *)), this, SLOT(Selected(QAction *)));
}

void ServerConnectionWidget::InitializeRegistrationMenu(QMenu *pMenu)
{
    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
        	pMenu->setStyleSheet(" QMenu { background-color: #ABABAB; border: 1px solid black; } QMenu::item { background-color: transparent; } QMenu::item:selected { background-color: #654321; }");
            break;
        default:
            break;
    }

    pMenu->addAction(QPixmap(":/images/UserAvailable.png"), "Registered");
    pMenu->addAction(QPixmap(":/images/UserUnavailable.png"),"Not registered");
}

///////////////////////////////////////////////////////////////////////////////

void ServerConnectionWidget::UpdateState(bool pRegistered)
{
    LOG(LOG_VERBOSE, "Updating server connection state to: %d", pRegistered);
    if(pRegistered)
    {
        mTbRegistration->setText("Registered");
        mTbRegistration->setIcon(QPixmap(":/images/UserAvailable.png"));
    }else
    {
        mTbRegistration->setText("Not registered");
        mTbRegistration->setIcon(QPixmap(":/images/UserUnavailable.png"));
    }
}

void ServerConnectionWidget::Selected(QAction *pAction)
{
    LOG(LOG_VERBOSE, "New action selected");
    if (!CONF.GetSipInfrastructureMode())
    {
        ShowWarning("Check configuration", "Server support is deactivated in Homer's network configuration!");
        return;
    }

    if ((MEETING.GetServerRegistrationState()) && (pAction->text() == "Not registered"))
    {
        mTbRegistration->setText(pAction->text());
        mTbRegistration->setIcon(pAction->icon());
        MEETING.UnregisterAtServer();
        return;
    }

    if ((!MEETING.GetServerRegistrationState()) && (pAction->text() == "Registered"))
    {
        MEETING.RegisterAtServer();
        return;
    }
}

}} //namespace
