/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This Peer is published in the hope that it will be useful, but
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
 * Purpose: Implementation of network simulator
 * Author:  Thomas Volkert
 * Since:   2012-05-31
 */

#include <NetworkSimulator.h>
#include <Core/Scenario.h>
#include <Core/ChannelSetup.h>

#include <GAPI.h>

#include <Logger.h>

#include <QAction>
#include <QMenu>

namespace Homer { namespace Base {

using namespace std;
using namespace Homer::Gui;

///////////////////////////////////////////////////////////////////////////////

NetworkSimulator::NetworkSimulator()
{
    mOverviewNetworkSimulationWidget = NULL;
}

NetworkSimulator::~NetworkSimulator()
{
    if (mOverviewNetworkSimulationWidget != NULL)
        delete mOverviewNetworkSimulationWidget;
}

///////////////////////////////////////////////////////////////////////////////

bool NetworkSimulator::Init(QMenu *pAssignedMenu, QMainWindow *pMainWindow)
{
    LOG(LOG_VERBOSE, "Initialization of network simulator..");

    LOG(LOG_VERBOSE, "..creating network simulator scenario..");
    mScenario = Scenario::CreateScenario();

    LOG(LOG_VERBOSE, "..creating GAPI setup..");
    mGAPISetup = new ChannelSetup();

    LOG(LOG_VERBOSE, "..registering network simulator scenario at GAPI setup..");
    mGAPISetup->registerScenario(mScenario);

    LOG(LOG_VERBOSE, "..registering GAPI setup at GAPI service..");
    GAPI.registerImpl(mGAPISetup, NETWORK_SIMULATION);

    QAction *tAction = new QAction("Network simulator", pMainWindow);
    tAction->setCheckable(true);
    tAction->setChecked(true);//CONF.GetVisibilityNetworkSimulationWidget());
    QIcon icon29;
    icon29.addFile(QString::fromUtf8(":/images/46_46/Network.png"), QSize(), QIcon::Normal, QIcon::Off);
    icon29.addFile(QString::fromUtf8(":/images/22_22/Checked.png"), QSize(), QIcon::Normal, QIcon::On);
    tAction->setIcon(icon29);
    pAssignedMenu->addAction(tAction);

    mOverviewNetworkSimulationWidget = new OverviewNetworkSimulationWidget(tAction, pMainWindow, mScenario);

    return true;
}

Scenario* NetworkSimulator::GetScenario()
{
    return mScenario;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
