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
 * Purpose: SocketBinding
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <Berkeley/SocketName.h>
#include <Berkeley/SocketConnection.h>
#include <Berkeley/SocketBinding.h>
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
SocketBinding::SocketBinding(std::string pLocalName, Requirements *pRequirements)
{
    bool tFoundTransport = false;
    unsigned int tLocalPort = 0;
    //TODO: parse pLocalName and bind to a defined local IP address
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        tLocalPort = tRequPort->getPort();

    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        tLocalPort = 0;
    }
    mIsClosed = true;
    mConnection = NULL;


    /* network requirements */
    bool tIPv6 = IS_IPV6_ADDRESS(pLocalName);

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
        mSocket = Socket::CreateServerSocket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_TCP, tLocalPort);
        tFoundTransport = true;
    }


    if (tUdp)
    {
        if(!tFoundTransport)
        {
            mSocket = Socket::CreateServerSocket(tIPv6 ? SOCKET_IPv6 : SOCKET_IPv4, tUdpLite ? SOCKET_UDP_LITE : SOCKET_UDP, tLocalPort);
            tFoundTransport = true;
        }else
        {
            LOG(LOG_ERROR, "Ambiguous requirements");
        }
    }

    if(tFoundTransport)
    {
        if (mSocket != NULL)
        {
            // per default we set receive buffer of 2 MB
            mSocket->SetReceiveBufferSize(2 * 1024 * 1024);

            mIsClosed = false;

            /* QoS requirements and additional transport requirements */
            changeRequirements(pRequirements);
        }else
            LOG(LOG_ERROR, "Returned Berkeley socket is invalid");
    }else
    {
        LOG(LOG_ERROR, "Haven't found correct mapping from application requirements to transport protocol");
    }
}

SocketBinding::~SocketBinding()
{
    cancel();
}

///////////////////////////////////////////////////////////////////////////////

IConnection* SocketBinding::readConnection()
{
    if (mIsClosed)
        return NULL;

    if (mConnection == NULL)
    {
        LOG(LOG_VERBOSE, "Creating new association");
        switch(mSocket->GetTransportType())
        {
            case SOCKET_UDP:
            case SOCKET_UDP_LITE:
            case SOCKET_TCP:
                mConnection = new SocketConnection(mSocket);
                break;
            default:
                LOG(LOG_ERROR, "Unsupported transport type");
                break;
        }
    }

    return mConnection;
}

Name* SocketBinding::getName()
{
    if(mSocket != NULL)
    {
        return new SocketName(mSocket->GetLocalHost(), mSocket->GetLocalPort());
    }else
    {
        return NULL;
    }
}

void SocketBinding::cancel()
{
    if(mSocket != NULL)
    {
        LOG(LOG_VERBOSE, "All connections will be canceled now");

        if (mConnection != NULL)
        {
            delete mConnection;
            mConnection = NULL;
        }else
            delete mSocket;
        mSocket = NULL;
    }
}

bool SocketBinding::changeRequirements(Requirements *pRequirements)
{
    bool tResult = true;

    if (mConnection != NULL)
        tResult = mConnection->changeRequirements(pRequirements);

    if (tResult)
        mRequirements = *pRequirements;

    return tResult;
}

Requirements SocketBinding::getRequirements()
{
	return mRequirements;
}

Events SocketBinding::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
