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
 * Purpose: ChannelConnection
 * Author:  Thomas Volkert
 * Since:   2012-04-14
 */

#include <GAPI.h>
#include <Core/ChannelConnection.h>
#include <Core/ChannelName.h>
#include <Core/Cep.h>
#include <Core/Scenario.h>
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
ChannelConnection::ChannelConnection(Scenario *pScenario, std::string pTarget, Requirements *pRequirements)
{
    mScenario = pScenario;
    if (pScenario == NULL)
        LOG(LOG_ERROR, "Scenario is invalid");

    bool tFoundTransport = false;
    mCep = NULL;

    mBlockingMode = true;
    mPeerNode = pTarget;
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        mPeerPort = tRequPort->getPort();

    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        mPeerPort = 0;
    }
    mIsClosed = true;

    /* transport requirements */
    if ((pRequirements->contains(RequirementTransmitChunks::type())) && (pRequirements->contains(RequirementTransmitStream::type())))
    {
        LOG(LOG_ERROR, "Detected requirement conflict between \"Req:Chunks\" and \"Req:Waterfall\"");
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
        mCep = pScenario->AddClientCep(SOCKET_TCP, Name(mPeerNode), mPeerPort);
        tFoundTransport = true;
    }

    if (tUdp)
    {
        if(!tFoundTransport)
        {
            mCep = pScenario->AddClientCep(tUdpLite ? SOCKET_UDP_LITE : SOCKET_UDP, Name(mPeerNode), mPeerPort);
            tFoundTransport = true;
        }else
            LOG(LOG_ERROR, "Ambiguous requirements");
    }

    if (mCep != NULL)
    {
        mIsClosed = false;
        mPeerNode = mCep->GetPeerNode(); // maybe the scenario has changed the destination to a pre-defined one? -> this happens if the network simulator view is used to create a stream
    }else
        LOG(LOG_ERROR, "CEP object invalid");

    if(!tFoundTransport)
    {
        LOG(LOG_ERROR, "Haven't found correct mapping from application requirements to transport protocol");
    }

    /* QoS requirements and additional transport requirements */
    changeRequirements(pRequirements);

    if (mCep != NULL)
        LOG(LOG_VERBOSE, "New simulation association with target %s and requirements %s created", getRemoteName()->toString().c_str(), mRequirements.getDescription().c_str());
}

ChannelConnection::ChannelConnection(Cep *pCep)
{
    mScenario = NULL;
    mIsClosed = false;
    mCep = pCep;
    mBlockingMode = true;
    mPeerNode = "";
    mPeerPort = 0;
    LOG(LOG_VERBOSE, "New simulation association with local name %s created", getName()->toString().c_str(), mRequirements.getDescription().c_str());
}

ChannelConnection::~ChannelConnection()
{
    cancel();

    delete mCep;
    mCep = NULL;
}

///////////////////////////////////////////////////////////////////////////////

bool ChannelConnection::isClosed()
{
	return mIsClosed;
}

int ChannelConnection::availableBytes()
{
	return 0; //TODO
}

void ChannelConnection::read(char* pBuffer, int &pBufferSize)
{
    if(mCep != NULL)
    {
        string tSourceNode;
        unsigned int tSourcePort;
        ssize_t tBufferSize = pBufferSize;
        mIsClosed = !mCep->Receive(tSourceNode, tSourcePort, (void*)pBuffer, tBufferSize);
        mPeerNode = tSourceNode;
        mPeerPort = tSourcePort;
        pBufferSize = (int)tBufferSize;
        //TODO: extended error signaling
    }else
        LOG(LOG_ERROR, "Invalid CEP");
}

void ChannelConnection::write(char* pBuffer, int pBufferSize)
{
    if(mCep != NULL)
    {
        if ((mPeerNode != "") && (mPeerPort != 0))
        mIsClosed = !mCep->Send(mPeerNode, mPeerPort, (void*)pBuffer, (ssize_t) pBufferSize);
        //TODO: extended error signaling
    }else
        LOG(LOG_ERROR, "Invalid socket");
}

bool ChannelConnection::getBlocking()
{
	return mBlockingMode;
}

void ChannelConnection::setBlocking(bool pState)
{
	mBlockingMode = pState;
}

void ChannelConnection::cancel()
{
    if(mCep != NULL)
    {
        LOG(LOG_VERBOSE, "Channel to %s will be canceled now", getName()->toString().c_str());
        if (mBlockingMode)
        {
            if (mScenario != NULL)
                mScenario->DeleteClientCep(mCep->GetLocalNode(), mCep->GetLocalPort());
            mCep->Close();
        }
    }
}

Name* ChannelConnection::getName()
{
    if(mCep != NULL)
    {
        return new ChannelName(mCep->GetLocalNode(), mCep->GetLocalPort());
    }else
    {
        return NULL;
    }
}

Name* ChannelConnection::getRemoteName()
{
    if(mCep != NULL)
    {
        return new ChannelName(mPeerNode, mPeerPort);
    }else
    {
        return NULL;
    }
}

bool ChannelConnection::changeRequirements(Requirements *pRequirements)
{
    bool tResult = false;

    if (mCep == NULL)
        return false;

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
        tResult = mCep->SetQoS(tQoSSettings);
    }

    mRequirements = *pRequirements; //TODO: maybe some requirements were dropped?

    return tResult;
}

Requirements ChannelConnection::getRequirements()
{
	return mRequirements;
}

Events ChannelConnection::getEvents()
{
	Events tResult;

	//TODO:

	return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
