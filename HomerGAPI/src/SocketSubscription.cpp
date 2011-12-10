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
#include <SocketName.h>
#include <SocketSubscription.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitWaterfall.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>
#include <HBSocket.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

SocketSubscription::SocketSubscription(std::string pTargetHost, unsigned int pTargetPort, Requirements *pRequirements)
{
    bool tFoundTransport = false;

    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
    mIsClosed = true;


    /* transport requirements */
    if ((pRequirements->contains(RequirementChunksTransmission::type())) && (pRequirements->contains(RequirementWaterfallTransmission::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Waterfall\"");
    }

    if ((pRequirements->contains(RequirementLosslessTransmission::type())) && (!pRequirements->contains(RequirementWaterfallTransmission::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Lossless\" and \"Req:!Waterfall\"");
    }

    bool tTcp = ((pRequirements->contains(RequirementLosslessTransmission::type())) ||
                ((!pRequirements->contains(RequirementChunksTransmission::type())) &&
                (pRequirements->contains(RequirementWaterfallTransmission::type()))));

    bool tUdp = ((!pRequirements->contains(RequirementLosslessTransmission::type())) &&
                (pRequirements->contains(RequirementChunksTransmission::type())) &&
                (!pRequirements->contains(RequirementWaterfallTransmission::type())));

    if (tTcp)
    {
        mSocket = new Socket(SOCKET_IPv6, SOCKET_TCP);
        tFoundTransport = true;
        mIsClosed = false;
    }


    if (tUdp)
    {
        if(!tFoundTransport)
        {
            mSocket = new Socket(SOCKET_IPv6, SOCKET_UDP);
            tFoundTransport = true;
            mIsClosed = false;
        }else
        {
            LOG(LOG_ERROR, "Ambiguous requirements");
        }
    }

    if(!tFoundTransport)
    {
        LOG(LOG_ERROR, "Haven't found correct mapping from application requirements to transport protocol");
    }

    /* QoS requirements */
    int tMaxDelay = 0;
    int tMinDatRate = 0;
    int tMaxDatRate = 0;
    bool tLossless = pRequirements->contains(RequirementLosslessTransmission::type());
    if(pRequirements->contains(RequirementLimitDelay::type()))
    {
        RequirementLimitDelay* tReqDelay = pRequirements->get(RequirementLimitDelay::type());
    }
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
    if(mSocket != NULL)
    {
        string tSourceHost;
        unsigned int tSourcePort;
        ssize_t tBufferSize = pBufferSize;
        mIsClosed = !mSocket->Receive(tSourceHost, tSourcePort, (void*)pBuffer, tBufferSize);
        pBufferSize = (int)tBufferSize;
        //TODO: extended error signaling
    }
}

void SocketSubscription::write(char* pBuffer, int pBufferSize)
{
    if(mSocket != NULL)
    {
        mIsClosed = !mSocket->Send(mTargetHost, mTargetPort, (void*)pBuffer, (ssize_t) pBufferSize);
        //TODO: extended error signaling
    }
}

void SocketSubscription::cancel()
{
    if(mSocket != NULL)
    {
        delete mSocket;
        mSocket = NULL;
    }
}

IName* SocketSubscription::name()
{
    if(mSocket != NULL)
    {
        return new SocketName(mSocket->GetLocalHost(), mSocket->GetLocalPort());
    }else
    {
        return NULL;
    }
}

IName* SocketSubscription::peer()
{
    if(mSocket != NULL)
    {
        return new SocketName(mSocket->GetPeerHost(), mSocket->GetPeerPort());
    }else
    {
        return NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
