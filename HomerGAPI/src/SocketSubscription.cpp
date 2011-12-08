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
 * Purpose: SocketSubscription
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <SocketSubscription.h>
#include <RequirementLosslessTransmission.h>
#include <RequirementDatagramTransmission.h>
#include <HBSocket.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

SocketSubscription::SocketSubscription(std::string pTargetHost, unsigned int pTargetPort, Requirements *pRequirements)
{
    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;

    bool tTcp = ((pRequirements->contains(RequirementLosslessTransmission::type())) &&
                (!pRequirements->contains(RequirementDatagramTransmission::type())));

    if (tTcp)
        mSocket = new Socket(SOCKET_IPv6, SOCKET_TCP);
    else
        mSocket = new Socket(SOCKET_IPv6, SOCKET_UDP);

    mIsClosed = false;
}

SocketSubscription::~SocketSubscription()
{
    delete mSocket;
}

///////////////////////////////////////////////////////////////////////////////

bool SocketSubscription::isClosed()
{
	return mIsClosed;
}

void SocketSubscription::read(char* pBuffer, int &pBufferSize)
{
    string tSourceHost;
    unsigned int tSourcePort;
    ssize_t tBufferSize = pBufferSize;
    mIsClosed = !mSocket->Receive(tSourceHost, tSourcePort, (void*)pBuffer, tBufferSize);
    pBufferSize = (int)tBufferSize;
    //TODO: extended error signaling
}

void SocketSubscription::write(char* pBuffer, int pBufferSize)
{
    mIsClosed = !mSocket->Send(mTargetHost, mTargetPort, (void*)pBuffer, (ssize_t) pBufferSize);
    //TODO: extended error signaling
}

void SocketSubscription::cancel()
{
	//TODO
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
