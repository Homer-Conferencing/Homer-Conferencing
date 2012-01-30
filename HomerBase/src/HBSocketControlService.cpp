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
 * Purpose: Implemenation of socket control service as singleton
 * Author:  Thomas Volkert
 * Since:   2011-11-16
*/
#include <Header_Windows.h>
#include <HBSocketControlService.h>

#include <Logger.h>

namespace Homer { namespace Base {

SocketControlService sSocketControlService;

///////////////////////////////////////////////////////////////////////////////

SocketControlService::SocketControlService()
{

}

SocketControlService::~SocketControlService()
{

}

SocketControlService& SocketControlService::GetInstance()
{
    return sSocketControlService;
}

///////////////////////////////////////////////////////////////////////////////

SocketsList SocketControlService::GetClientSocketsControl()
{
    SocketsList tResult;

	// lock
    mClientSocketsMutex.lock();

    tResult = mClientSockets;

    return tResult;
}

void SocketControlService::ReleaseClientSocketsControl()
{

    // unlock
    mClientSocketsMutex.unlock();
}

bool SocketControlService::IsClientSocketAvailable(Socket *pSocket)
{
    SocketsList::iterator tIt;
    bool tFound = false;

    if (pSocket == NULL)
        return false;

    LOG(LOG_VERBOSE, "Searching socket");

    for (tIt = mClientSockets.begin(); tIt != mClientSockets.end(); tIt++)
    {
        if (*tIt == pSocket)
        {
            LOG(LOG_VERBOSE, "..found");
            tFound = true;
            break;
        }
    }

    if(!tFound)
        LOG(LOG_VERBOSE, "..not found");

    return tFound;
}

Socket* SocketControlService::RegisterClientSocket(Socket *pSocket)
{
    SocketsList::iterator tIt;
    bool tFound = false;

    if (pSocket == NULL)
        return NULL;

    LOG(LOG_VERBOSE, "Registering socket control for client socket to peer: %s", pSocket->GetPeerName().c_str());

    // lock
    mClientSocketsMutex.lock();

    for (tIt = mClientSockets.begin(); tIt != mClientSockets.end(); tIt++)
    {
        if (*tIt == pSocket)
        {
            LOG(LOG_VERBOSE, "Statistic already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    	mClientSockets.push_back(pSocket);

    // unlock
    mClientSocketsMutex.unlock();

    return pSocket;
}

bool SocketControlService::UnregisterClientSocket(Socket *pSocket)
{
    SocketsList::iterator tIt;
    bool tFound = false;

    if (pSocket == NULL)
        return false;

    LOG(LOG_VERBOSE, "Unregistering socket control for client socket to peer: %s", pSocket->GetPeerName().c_str());

    // lock
    mClientSocketsMutex.lock();

    for (tIt = mClientSockets.begin(); tIt != mClientSockets.end(); tIt++)
    {
        if (*tIt == pSocket)
        {
            tFound = true;
            mClientSockets.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mClientSocketsMutex.unlock();

    return tFound;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace
