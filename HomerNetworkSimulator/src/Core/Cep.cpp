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
 * Purpose: Implementation of CEP
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Cep.h>
#include <Core/DomainNameService.h>
#include <Core/Node.h>

#include <GAPI.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;
using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

Cep::Cep(Node *pNode, enum TransportType pTransportType, unsigned int pLocalPort)
{
    mClosed = false;
    mLocalPort = pLocalPort;
    mPeerPort = 0;
    mPeerNode = "";
    mNode = pNode;
    mTransportType = pTransportType;
    mPacketQueue = new MediaFifo(CEP_QUEUE_SIZE, CEP_QUEUE_ENTRY_SIZE, "PacketQueue@" + pNode->GetAddress());

    LOG(LOG_VERBOSE, "Created new server CEP on node %s for local port %u", pNode->GetAddress().c_str(), pLocalPort);
}

Cep::Cep(Node *pNode, enum TransportType pTransportType, string pTarget, unsigned int pTargetPort)
{
    static int sLastClientPort = 1023;

    mClosed = false;
    mNode = pNode;
    mLocalPort = ++sLastClientPort;

    mPeerNode = pTarget;
    mPeerPort = pTargetPort;
    mTransportType = pTransportType;
    mPacketQueue = new MediaFifo(CEP_QUEUE_SIZE, CEP_QUEUE_ENTRY_SIZE, "PacketQueue@" + pNode->GetAddress());

    LOG(LOG_VERBOSE, "Created new client CEP on node %s for target %s:%u", pNode->GetAddress().c_str(), pTarget.c_str(), pTargetPort);
}

Cep::~Cep()
{
    LOG(LOG_VERBOSE, "Deleting CEP on node %s and peer %s:%u", mNode->GetAddress().c_str(), mPeerNode.c_str(), mPeerPort);
    delete mPacketQueue;
}

///////////////////////////////////////////////////////////////////////////////

bool Cep::SetQoS(const QoSSettings &pQoSSettings)
{
    LOG(LOG_VERBOSE, "Desired QoS: %u KB/s min. data rate, %u ms max. delay, features: 0x%hX", pQoSSettings.DataRate, pQoSSettings.Delay, pQoSSettings.Features);

    mQoSSettings = pQoSSettings;

    return true;
}

QoSSettings Cep::GetQoS()
{
    return mQoSSettings;
}

bool Cep::Close()
{
    if (!mClosed)
    {
        mClosed = true;
        char tData[4];
        mPacketQueue->WriteFifo(tData, 0);
        mPacketQueue->WriteFifo(tData, 0);
        return true;
    }
        return false;
}

bool Cep::Send(std::string pTargetNode, unsigned int pTargetPort, void *pBuffer, ssize_t pBufferSize)
{
    LOG(LOG_VERBOSE, "Sending %d bytes to %s:%u", (int)pBufferSize, mPeerNode.c_str(), mPeerPort);

    if (mClosed)
    {
        LOG(LOG_WARN, "CEP is already closed, ignoring send request");
        return false;
    }

    // create packet descriptor
    Packet *tPacket = new Packet();
    tPacket->Source = mNode->GetAddress();
    tPacket->Destination = DNS.query(pTargetNode);
    tPacket->SourcePort = mLocalPort;
    tPacket->DestinationPort = pTargetPort;
    tPacket->QoSRequirements = mQoSSettings;
    tPacket->Data = pBuffer;
    tPacket->DataSize = pBufferSize;
    tPacket->TTL = TTL_INIT;

    // send packet towards destination
    return mNode->HandlePacket(tPacket);
}

bool Cep::Receive(std::string &pPeerNode, unsigned int &pPeerPort, void *pBuffer, ssize_t &pBufferSize)
{
    if (mClosed)
    {
        LOG(LOG_WARN, "CEP is already closed, ignoring receive request");
        return false;
    }

    int tBufferSize = pBufferSize;
    mPacketQueue->ReadFifo((char*)pBuffer, tBufferSize);
    pBufferSize = tBufferSize;
    pPeerNode = mPeerNode;
    pPeerPort = mPeerPort;

    LOG(LOG_VERBOSE, "Received %d bytes from %s:%u", (int)pBufferSize, mPeerNode.c_str(), mPeerPort);

    return true;
}

enum TransportType Cep::GetTransportType()
{
    return mTransportType;
}

unsigned int Cep::GetLocalPort()
{
    return mLocalPort;
}

string Cep::GetLocalNode()
{
    return mNode->GetAddress();
}

unsigned int Cep::GetPeerPort()
{
    return mPeerPort;
}

string Cep::GetPeerNode()
{
    return mPeerNode;
}

bool Cep::HandlePacket(Packet *pPacket)
{
    #ifdef DEBUG_ROUTING
        LOG(LOG_VERBOSE, "Handling packet from %s at CEP@node %s(%s)", pPacket->Source.c_str(), mNode->GetName().c_str(), mNode->GetAddress().c_str());
    #endif

    #ifdef DEBUG_ROUTING_RECORDS
        LOG(LOG_INFO, "Received packet from %s", pPacket->Source.c_str());
        list<string>::iterator tIt;
        for (tIt = pPacket->RecordedRoute.begin(); tIt != pPacket->RecordedRoute.end(); tIt++)
        {
            LOG(LOG_INFO, "  via %s", (*tIt).c_str());
        }
    #endif

    mPacketQueue->WriteFifo((char*)pPacket->Data, pPacket->DataSize);
    mPeerNode = pPacket->Source;
    mPeerPort = pPacket->SourcePort;

    // EOL for packet descriptor
    delete pPacket;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
