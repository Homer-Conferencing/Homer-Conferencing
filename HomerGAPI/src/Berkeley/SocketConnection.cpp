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
 * Purpose: SocketConnection
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <Berkeley/SocketName.h>
#include <Berkeley/SocketConnection.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>

#include <HBSocket.h>
#include <HBSocketQoSSettings.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////
//HINT: lossless transmission is not implemented by using TCP but by rely on a reaction by the network
SocketConnection::SocketConnection(std::string pTarget, Requirements *pRequirements)
{
    bool tFoundTransport = false;

    mBlockingMode = true;
    mTargetHost = pTarget;
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        mTargetPort = tRequPort->getPort();

    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        mTargetPort = 0;
    }
    mIsClosed = true;


    /* network requirements */
    bool tIPv6 = IS_IPV6_ADDRESS(pTarget);

    /* transport requirements */
    if ((pRequirements->contains(RequirementTransmitChunks::type())) && (pRequirements->contains(RequirementTransmitStream::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Stream\"");
    }

    bool tTcp = (((!pRequirements->contains(RequirementTransmitChunks::type())) &&
                (pRequirements->contains(RequirementTransmitStream::type()))));

    bool tUdp = ((pRequirements->contains(RequirementTransmitChunks::type())) &&
                (!pRequirements->contains(RequirementTransmitStream::type())));

    bool tUdpLite = ((pRequirements->contains(RequirementTransmitChunks::type())) &&
                    (!pRequirements->contains(RequirementTransmitStream::type())) &&
                    (pRequirements->contains(RequirementTransmitBitErrors::type())));

    if (tTcp)
    {
        mSocket = Socket::CreateClientSocket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_TCP);
        tFoundTransport = true;
        mIsClosed = false;
    }


    if (tUdp)
    {
        if(!tFoundTransport)
        {
            mSocket = Socket::CreateClientSocket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, tUdpLite ? SOCKET_UDP_LITE : SOCKET_UDP);
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

    /* QoS requirements and additional transport requirements */
    changeRequirements(pRequirements);
}

SocketConnection::~SocketConnection()
{
    delete mSocket;
}

///////////////////////////////////////////////////////////////////////////////

bool SocketConnection::isClosed()
{
	return mIsClosed;
}

int SocketConnection::availableBytes()
{
	return 0; //TODO
}

void SocketConnection::read(char* pBuffer, int &pBufferSize)
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

void SocketConnection::write(char* pBuffer, int pBufferSize)
{
    if(mSocket != NULL)
    {
        mIsClosed = !mSocket->Send(mTargetHost, mTargetPort, (void*)pBuffer, (ssize_t) pBufferSize);
        //TODO: extended error signaling
    }
}

bool SocketConnection::getBlocking()
{
	return mBlockingMode;
}

void SocketConnection::setBlocking(bool pState)
{
	mBlockingMode = pState;
}

void SocketConnection::cancel()
{
    if(mSocket != NULL)
    {
        LOG(LOG_VERBOSE, "Connection will be canceled now");
        delete mSocket;
        mSocket = NULL;
    }
}

Name* SocketConnection::getName()
{
    if(mSocket != NULL)
    {
        return new SocketName(mSocket->GetLocalHost(), mSocket->GetLocalPort());
    }else
    {
        return NULL;
    }
}

Name* SocketConnection::getRemoteName()
{
    if(mSocket != NULL)
    {
        return new SocketName(mSocket->GetPeerHost(), mSocket->GetPeerPort());
    }else
    {
        return NULL;
    }
}

bool SocketConnection::changeRequirements(Requirements *pRequirements)
{
    bool tResult = false;

    /* additional transport requirements */
    if (pRequirements->contains(RequirementTransmitBitErrors::type()))
    {
        RequirementTransmitBitErrors* tReqBitErr = (RequirementTransmitBitErrors*)pRequirements->get(RequirementTransmitBitErrors::type());
        int tSecuredFrontDataSize = tReqBitErr->getSecuredFrontDataSize();
        mSocket->UDPLiteSetCheckLength(tSecuredFrontDataSize);
    }

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

    mRequirements = *pRequirements; //TODO: maybe some requirements were dropped?

    return tResult;
}

Requirements SocketConnection::getRequirements()
{
	return mRequirements;
}

Events SocketConnection::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
