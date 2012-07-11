/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: ChannelBinding
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <GAPI.h>
#include <Core/Cep.h>
#include <Core/ChannelName.h>
#include <Core/ChannelConnection.h>
#include <Core/ChannelBinding.h>
#include <RequirementTransmitLossless.h>
#include <RequirementTransmitChunks.h>
#include <RequirementTransmitStream.h>
#include <RequirementTransmitBitErrors.h>
#include <RequirementTargetPort.h>
#include <RequirementLimitDelay.h>
#include <RequirementLimitDataRate.h>

#include <HBSocket.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////
//HINT: lossless transmission is not implemented by using TCP but by rely on a reaction by the network
ChannelBinding::ChannelBinding(Scenario *pScenario, std::string pLocalName, Requirements *pRequirements)
{
    mScenario = pScenario;
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
        mCep = mScenario->AddServerCep(SOCKET_TCP, Name(pLocalName), tLocalPort);
        tFoundTransport = true;
        mIsClosed = false;
    }


    if (tUdp)
    {
        if(!tFoundTransport)
        {
            mCep = mScenario->AddServerCep(tUdpLite ? SOCKET_UDP_LITE : SOCKET_UDP, Name(pLocalName), tLocalPort);
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

ChannelBinding::~ChannelBinding()
{
    cancel();
}

///////////////////////////////////////////////////////////////////////////////

bool ChannelBinding::isClosed()
{
	return mIsClosed;
}

IConnection* ChannelBinding::readConnection()
{
    if (mConnection == NULL)
    {
        LOG(LOG_VERBOSE, "Creating new association");
        switch(mCep->GetTransportType())
        {
            case SOCKET_UDP:
            case SOCKET_UDP_LITE:
            case SOCKET_TCP:
                mConnection = new ChannelConnection(mCep);
                break;
            default:
                LOG(LOG_ERROR, "Unsupported transport type");
                break;
        }
    }

    return mConnection;
}

Name* ChannelBinding::getName()
{
    if(mCep != NULL)
    {
        return new ChannelName(mCep->GetLocalNode(), mCep->GetLocalPort());
    }else
    {
        return NULL;
    }
}

void ChannelBinding::cancel()
{
    if(mCep != NULL)
    {
        LOG(LOG_VERBOSE, "All connections will be canceled now");

        if (mConnection != NULL)
        {
            mScenario->DeleteServerCep(mCep->GetLocalNode(), mCep->GetLocalPort());
            delete mConnection;
            mConnection = NULL;
        }else
            delete mCep;
        mCep = NULL;
    }
}

bool ChannelBinding::changeRequirements(Requirements *pRequirements)
{
    bool tResult = true;

    if (mConnection != NULL)
        tResult = mConnection->changeRequirements(pRequirements);

    if (tResult)
        mRequirements = pRequirements;

    return tResult;
}

Requirements* ChannelBinding::getRequirements()
{
	return mRequirements;
}

Events ChannelBinding::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
