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
 * Since:   2009-05-09
 */

//HINT: we add two space after each entry in the combo box in order to move the QMenu arrow more to the right

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
    mTbAvailability->setText(QString(MEETING.GetAvailabilityStateStr().c_str()) + "  ");
    if (MEETING.GetAvailabilityStateStr() == "Online (auto)")
    {
        mTbAvailability->setIcon(QPixmap(":/images/32_32/UserAvailable.png"));
    }
    if (MEETING.GetAvailabilityStateStr() == "Online")
    {
        mTbAvailability->setIcon(QPixmap(":/images/32_32/UserAvailable.png"));
    }
    if (MEETING.GetAvailabilityStateStr() == "Offline")
    {
        mTbAvailability->setIcon(QPixmap(":/images/32_32/UserUnavailable.png"));
    }

    connect(mTbAvailability, SIGNAL(triggered(QAction *)), this, SLOT(Selected(QAction *)));
}

void AvailabilityWidget::InitializeMenuOnlineStatus(QMenu *pMenu)
{
    pMenu->addAction(QPixmap(":/images/32_32/UserAvailable.png"), "Online (auto)  ");
    pMenu->addAction(QPixmap(":/images/32_32/UserAvailable.png"),"Online  ");
    pMenu->addAction(QPixmap(":/images/32_32/UserUnavailable.png"),"Offline  ");
}

///////////////////////////////////////////////////////////////////////////////

void AvailabilityWidget::Selected(QAction *pAction)
{
    mTbAvailability->setText(pAction->text());
    mTbAvailability->setIcon(pAction->icon());
    QString tNewState = pAction->text().left(pAction->text().length() - 2);

    if ((MEETING.GetServerRegistrationState()) && (pAction->text() == "Offline"))
    {
        MEETING.UnregisterAtServer();
    }

    if ((!MEETING.GetServerRegistrationState()) && (pAction->text() != "Offline"))
    {
        MEETING.RegisterAtServer();
    }

    MEETING.SetAvailabilityState(tNewState.toStdString());
}

}} //namespace
