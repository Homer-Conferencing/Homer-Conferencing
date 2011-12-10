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
#include <RequirementUseIPv6.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitWaterfall.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>

#include <HBSocket.h>
#include <HBSocketQoSSettings.h>

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


    /* network requirements */
    bool tIPv6 = pRequirements->contains(RequirementUseIPv6::type());

    /* transport requirements */
    if ((pRequirements->contains(RequirementTransmitChunks::type())) && (pRequirements->contains(RequirementWaterfallTransmission::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Waterfall\"");
    }

    if ((pRequirements->contains(RequirementTransmitLossless::type())) && (!pRequirements->contains(RequirementWaterfallTransmission::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Lossless\" and \"Req:!Waterfall\"");
    }

    bool tTcp = ((pRequirements->contains(RequirementTransmitLossless::type())) ||
                ((!pRequirements->contains(RequirementTransmitChunks::type())) &&
                (pRequirements->contains(RequirementWaterfallTransmission::type()))));

    bool tUdp = ((!pRequirements->contains(RequirementTransmitLossless::type())) &&
                (pRequirements->contains(RequirementTransmitChunks::type())) &&
                (!pRequirements->contains(RequirementWaterfallTransmission::type())));

    bool tUdpLite = ((!pRequirements->contains(RequirementTransmitLossless::type())) &&
                    (pRequirements->contains(RequirementTransmitChunks::type())) &&
                    (!pRequirements->contains(RequirementWaterfallTransmission::type())) &&
                    (pRequirements->contains(RequirementTransmitBitErrors::type())));

    if (tTcp)
    {
        mSocket = new Socket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_TCP);
        tFoundTransport = true;
        mIsClosed = false;
    }


    if (tUdp)
    {
        if(!tFoundTransport)
        {
            if(!tUdpLite)
            {
                mSocket = new Socket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_UDP);
            }else
            {
                mSocket = new Socket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_UDP_LITE);
            }
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
    update(pRequirements);
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
        LOG(LOG_VERBOSE, "Subscription will be canceled now");
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

bool SocketSubscription::update(Requirements *pRequirements)
{
    bool tResult = false;

    /* QoS requirements */
    int tMaxDelay = 0;
    int tMinDataRate = 0;
    int tMaxDataRate = 0;
    // get lossless transmission activation
    bool tLossless = pRequirements->contains(RequirementTransmitLossless::type());
    // get delay values
    if(pRequirements->contains(RequirementLimitDelay::type()))
    {
        RequirementLimitDelay* tReqDelay = (RequirementLimitDelay*)pRequirements->get(RequirementLimitDelay::type());
        tMaxDelay = tReqDelay->getMaxDelay();
    }
    // get data rate values
    if(pRequirements->contains(RequirementLimitDataRate::type()))
    {
        RequirementLimitDataRate* tReqDataRate = (RequirementLimitDataRate*)pRequirements->get(RequirementLimitDataRate::type());
        tMinDataRate = tReqDataRate->getMinDataRate();
        tMaxDataRate = tReqDataRate->getMaxDataRate();
    }

    if((tLossless) || (tMaxDelay) || (tMinDataRate))
    {
        QoSSettings tQoSSettings;
        tQoSSettings.DataRate = tMinDataRate;
        tQoSSettings.Delay = tMaxDelay;
        tQoSSettings.Features = (tLossless ? QOS_FEATURE_LOSSLESS : 0);
        tResult = mSocket->SetQoS(tQoSSettings);
    }

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
