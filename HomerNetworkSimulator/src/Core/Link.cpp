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
 * Purpose: Implementation of simulated links
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Link.h>
#include <Core/Node.h>
#include <Core/Cep.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Link::Link(Node *pNodeOne, Node *pNodeTwo)
{
    LOG(LOG_INFO, "Created link from %s to %s", pNodeOne->GetAddress().c_str(), pNodeTwo->GetAddress().c_str());

    mPacketCount = 0;
    mPacketLossCount = 0;
    mNodes[0] = pNodeOne;
    mNodes[1] = pNodeTwo;
    mQoSCapabilities.DataRate = 40;
    mQoSCapabilities.Delay = 5;
    mQoSCapabilities.Features = 0;

    // simulate hello protocol
    mNodes[0]->AddLink(this);
    mNodes[1]->AddLink(this);
}

Link::~Link()
{

}

///////////////////////////////////////////////////////////////////////////////

Node* Link::GetPeerNode(Node *pNode)
{
    if (pNode->GetAddress() == mNodes[0]->GetAddress())
        return mNodes[1];
    else
        return mNodes[0];
}

bool Link::HandlePacket(Packet *pPacket, Node* pLastNode)
{
    #ifdef DEBUG_FORWARDING
        LOG(LOG_VERBOSE, "Handling packet from %s at link between %s and %s", pPacket->Source.c_str(), mNodes[0]->GetAddress().c_str(), mNodes[1]->GetAddress().c_str());
    #endif

    // store stream id in internal list
    std::list<int>::iterator tIt;
    bool tFound = false;

    // store as seen stream
    mSeenStreamsMutex.lock();
    for (tIt = mSeenStreams.begin(); tIt != mSeenStreams.end(); tIt++)
    {
        if (*tIt == pPacket->TrackingStreamId)
            tFound = true;
    }
    if (!tFound)
        mSeenStreams.push_back(pPacket->TrackingStreamId);
    mSeenStreamsMutex.unlock();

    // check if available data rate on this link matches the desired data rate of the packet:
    //TODO later: derive the available data rate from the formerly seen packets in a defined time slot
    if (pPacket->mStreamDataRate > (int)mQoSCapabilities.DataRate)
    {
        // simulate packet loss
        float tLossProb =  1.0 - ((float)mQoSCapabilities.DataRate / pPacket->mStreamDataRate);
        int tLossRandRange = tLossProb * RAND_MAX;
        if (rand() < tLossRandRange)
        {
            mPacketLossCount++;
            #ifdef DEBUG_PACKET_LOSS
                LOG(LOG_WARN, "Packet is lost within simulation at link %s/%s, loss probability: %f", mNodes[0]->GetAddress().c_str(), mNodes[1]->GetAddress().c_str(), tLossProb);
            #endif
            return true;
        }
    }

    // increase the E2E delay of this packet
    pPacket->QoSResults.Delay += mQoSCapabilities.Delay;

    // store the minimum data rate along the entire route within the packet
    if (pPacket->QoSResults.DataRate > mQoSCapabilities.DataRate)
        pPacket->QoSResults.DataRate = mQoSCapabilities.DataRate;

    // packet statistic
    mPacketCount++;

    // check from which interface the packet was received and forward it to the other one
    if (pLastNode->GetAddress() == mNodes[0]->GetAddress())
    {
        pPacket->RecordedRoute.push_back("link " + mNodes[0]->GetAddress() + "/" + mNodes[1]->GetAddress());
        return mNodes[1]->HandlePacket(pPacket);
    }else if (pLastNode->GetAddress() == mNodes[1]->GetAddress())
    {
        pPacket->RecordedRoute.push_back("link " + mNodes[1]->GetAddress() + "/" + mNodes[0]->GetAddress());
        return mNodes[0]->HandlePacket(pPacket);
    }else{ // last node was none of the linked nodes! -> invalid behavior
        LOG(LOG_ERROR, "Forwarding failure on link between %s and %s, this should never happen but it did!", mNodes[0]->GetName().c_str(), mNodes[1]->GetName().c_str());
        return false;
    }
}

void Link::SetQoSCapabilities(QoSSettings pCaps)
{
    mQoSCapabilities = pCaps;
}

QoSSettings Link::GetQoSCapabilities()
{
    return mQoSCapabilities;
}

Node* Link::GetNode0()
{
    return mNodes[0];
}

Node* Link::GetNode1()
{
    return mNodes[1];
}

int Link::GetPacketCount()
{
    return mPacketCount;
}

int Link::GetLostPacketCount()
{
    return mPacketLossCount;
}

list<int> Link::GetSeenStreams()
{
    list<int> tResult;

    mSeenStreamsMutex.lock();
    tResult = mSeenStreams;
    mSeenStreams.clear();
    mSeenStreamsMutex.unlock();

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
