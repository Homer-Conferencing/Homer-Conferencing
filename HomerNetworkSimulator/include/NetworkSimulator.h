/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Network simulation
 * Author:  Thomas Volkert
 * Since:   2012-05-31
 */

#ifndef _NETWORK_SIMULATION_
#define _NETWORK_SIMULATION_

#include <Core/Scenario.h>
#include <Core/ChannelSetup.h>
#include <Widgets/OverviewNetworkSimulationWidget.h>

#include <QMenu>
#include <QMainWindow>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class NetworkSimulator
{
public:
    NetworkSimulator();
    virtual ~NetworkSimulator();

    bool Init(QMenu *pAssignedMenu, QMainWindow *pMainWindow);

    Scenario *GetScenario();

private:
    ChannelSetup    *mGAPISetup;
    Scenario        *mScenario;
    Homer::Gui::OverviewNetworkSimulationWidget *mOverviewNetworkSimulationWidget;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
