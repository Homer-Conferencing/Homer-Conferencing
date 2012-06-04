/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of NetworkSimulator
 * Author:  Thomas Volkert
 * Since:   2012-05-31
 */

#include <NetworkSimulator.h>
#include <Core/Scenario.h>
#include <Core/ChannelSetup.h>

#include <GAPI.h>

#include <Logger.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

NetworkSimulator::NetworkSimulator()
{

}

NetworkSimulator::~NetworkSimulator()
{

}

///////////////////////////////////////////////////////////////////////////////

bool NetworkSimulator::Init()
{
    LOG(LOG_VERBOSE, "Creating network simulator scenario..");
    mScenario = Scenario::CreateScenario();

    LOG(LOG_VERBOSE, "Creating GAPI setup..");
    mGAPISetup = new ChannelSetup();

    LOG(LOG_VERBOSE, "Registering network simulator scenario at GAPI setup..");
    mGAPISetup->registerScenario(mScenario);

    LOG(LOG_VERBOSE, "Registering GAPI setup at GAPI service..");
    GAPI.registerImpl(mGAPISetup, NETWORK_SIMULATION);

    return true;
}

Scenario* NetworkSimulator::GetScenario()
{
    return mScenario;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
