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
 * Purpose: Implementation of AvailabilityWidget.h
 * Author:  Thomas Volkert
 * Since:   2009-05-09
 */

#include <Widgets/AvailabilityWidget.h>
#include <Configuration.h>
#include <Meeting.h>

#include <QWidget>
#include <QMenu>
#include <QMessageBox>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

AvailabilityWidget::AvailabilityWidget(QWidget* pParent):
    QWidget(pParent)
{
    initializeGUI();
}

AvailabilityWidget::~AvailabilityWidget()
{
}

///////////////////////////////////////////////////////////////////////////////

void AvailabilityWidget::initializeGUI()
{
    setupUi(this);

    mMenu = new QMenu(this);
    InitializeMenuOnlineStatus(mMenu);
    mTbAvailability->setMenu(mMenu);
    mTbAvailability->setText(QString(MEETING.getAvailabilityStateStr().c_str()));
    if (MEETING.getAvailabilityStateStr() == "Online (auto)")
    {
        mTbAvailability->setIcon(QPixmap(":/images/UserAvailable.png"));
    }
    if (MEETING.getAvailabilityStateStr() == "Online")
    {
        mTbAvailability->setIcon(QPixmap(":/images/UserAvailable.png"));
    }
    if (MEETING.getAvailabilityStateStr() == "Offline")
    {
        mTbAvailability->setIcon(QPixmap(":/images/UserUnavailable.png"));
    }

    connect(mTbAvailability, SIGNAL(triggered(QAction *)), this, SLOT(Selected(QAction *)));
}

void AvailabilityWidget::InitializeMenuOnlineStatus(QMenu *pMenu)
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

    pMenu->addAction(QPixmap(":/images/UserAvailable.png"), "Online (auto)");
    pMenu->addAction(QPixmap(":/images/UserAvailable.png"),"Online");
    pMenu->addAction(QPixmap(":/images/UserUnavailable.png"),"Offline");
}

///////////////////////////////////////////////////////////////////////////////

void AvailabilityWidget::Selected(QAction *pAction)
{
    mTbAvailability->setText(pAction->text());
    mTbAvailability->setIcon(pAction->icon());
    QString tNewState = pAction->text();

    if ((MEETING.GetServerRegistrationState()) && (pAction->text() == "Offline"))
    {
        MEETING.UnregisterAtServer();
    }

    if ((!MEETING.GetServerRegistrationState()) && (pAction->text() != "Offline"))
    {
        MEETING.RegisterAtServer();
    }

    MEETING.setAvailabilityState(tNewState.toStdString());
}

}} //namespace
