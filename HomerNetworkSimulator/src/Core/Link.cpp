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
 * Purpose: Implementation of simulated links
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Link.h>
#include <Core/Node.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Link::Link(Node *pNodeOne, Node *pNodeTwo)
{
    LOG(LOG_INFO, "Created link from %s to %s", pNodeOne->GetAddress().c_str(), pNodeTwo->GetAddress().c_str());

    mNodes[0] = pNodeOne;
    mNodes[1] = pNodeTwo;
    mQoSCapabilities.DataRate = 0;
    mQoSCapabilities.Delay = 0;
    mQoSCapabilities.Features = 0;

    // simulate hello protocol
    mNodes[0]->AddFibEntry(this, mNodes[1]);
    mNodes[0]->AddTopologyEntry(mNodes[1]->GetAddress(), mNodes[1]->GetAddress());
    mNodes[1]->AddFibEntry(this, mNodes[0]);
    mNodes[1]->AddTopologyEntry(mNodes[0]->GetAddress(), mNodes[0]->GetAddress());
}

Link::~Link()
{

}

///////////////////////////////////////////////////////////////////////////////

bool Link::HandlePacket(Packet *pPacket, Node* pLastNode)
{
    #ifdef DEBUG_FORWARDING
        LOG(LOG_VERBOSE, "Handling packet from %s at link between %s and %s", pPacket->Source.c_str(), mNodes[0]->GetAddress().c_str(), mNodes[1]->GetAddress().c_str());
    #endif

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

///////////////////////////////////////////////////////////////////////////////

}} //namespace
