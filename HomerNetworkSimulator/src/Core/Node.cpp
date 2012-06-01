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
 * Purpose: Implementation of simulated nodes
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Coordinator.h>
#include <Core/DomainNameService.h>
#include <Core/Node.h>
#include <Core/ChannelName.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Node::Node(string pName, string pAddressHint)
{
    LOG(LOG_INFO, "Created node %s(address %s)", pName.c_str(), pAddressHint.c_str());
    for (int i = 0; i < MAX_HIERARCHY_DEPTH; i++)
        mCoordinators[i] = NULL;
    mName = pName;
    mAddress = pAddressHint; //TODO: SO address management

    if ((pName != "") && (pAddressHint != ""))
        DNS.registerName(pName, pAddressHint);
}

Node::~Node()
{

}

///////////////////////////////////////////////////////////////////////////////

bool Node::AddFibEntry(Link *pLink, Node *pNextNode)
{
    FibEntry *tFibEntry = new FibEntry();
    tFibEntry->NextNode = pNextNode;
    tFibEntry->ViaLink = pLink;

    mFibTableMutex.lock();
    mFibTable.push_back(tFibEntry);
    mFibTableMutex.unlock();

    return true;
}

bool Node::AddRibEntry(std::string pDestination, std::string pNextNode)
{
    RibEntry *tRibEntry = new RibEntry();
    tRibEntry->Destination = pDestination;
    tRibEntry->NextNode = pNextNode;

    mRibTableMutex.lock();
    mRibTable.push_back(tRibEntry);
    mRibTableMutex.unlock();

//    if (mCoordinators[0] != NULL)
//        mCoordinators[0]->PublishTopology..

    return true;
}

Coordinator* Node::SetAsCoordinator(string pClusterName, int pHierarchyLevel)
{
    Coordinator *tResult = NULL;

    mCoordinatorsMutex.lock();

    // delete old instance
    if (mCoordinators[pHierarchyLevel] != NULL)
    {
        delete mCoordinators[pHierarchyLevel];
        mCoordinators[pHierarchyLevel] = NULL;
    }

    // create new
    tResult = new Coordinator(this, pClusterName, pHierarchyLevel);
    mCoordinators[pHierarchyLevel] = tResult;

    mCoordinatorsMutex.unlock();

    return tResult;
}

Cep* Node::AddServer(enum TransportType pTransportType, unsigned int pPort)
{
    Cep *tCep = new Cep(this, pTransportType, pPort);

    mServerCepsMutex.lock();
    mServerCeps.push_back(tCep);
    mServerCepsMutex.unlock();

    return tCep;
}

bool Node::DeleteServer(unsigned int pPort)
{
    //TODO

    return true;
}

Cep* Node::AddClient(enum TransportType pTransportType, string pTarget, unsigned pTargetPort)
{
    Cep *tCep = new Cep(this, pTransportType, DNS.query(pTarget), pTargetPort);

    return tCep;
}

string Node::GetAddress()
{
    return mAddress;
}

string Node::GetName()
{
    return mName;
}

void Node::SetName(std::string pName)
{
    mName = pName;
}

Cep* Node::FindServerCep(unsigned int pPort)
{
    Cep* tResult = NULL;

    mServerCepsMutex.lock();
    if (mServerCeps.size() > 0)
    {
        CepList::iterator tIt;
        for (tIt = mServerCeps.begin(); tIt != mServerCeps.end(); tIt)
        {
            LOG(LOG_VERBOSE, "Comparing CEP with local port %u and desired port %u", (*tIt)->GetLocalPort(), pPort);
            if ((*tIt)->GetLocalPort() == pPort)
            {
                tResult = *tIt;
                break;
            }
        }
    }
    mServerCepsMutex.unlock();

    return tResult;
}

bool Node::HandlePacket(Packet *pPacket)
{
    LOG(LOG_VERBOSE, "Handling packet from %s at node %s(%s)", pPacket->Source.c_str(), mName.c_str(), mAddress.c_str());

    // are we the packet's destination?
    if (pPacket->Destination == mAddress)
    {
        Cep *tServerCep = FindServerCep(pPacket->DestinationPort);

        // have we found a fitting server CEP?
        if (tServerCep == NULL)
        {
            LOG(LOG_ERROR, "No server CEP matched");
            return false;
        }

        // deliver packet to local server CEP
        return tServerCep->HandlePacket(pPacket);
    }

    // we are not the destination, we lookup the next node via our RIB
    string tNextHop = GetNextHop(pPacket->Destination, pPacket->QoSRequirements);
    if (tNextHop == "")
    {
        LOG(LOG_ERROR, "Routing failure on node %s: cannot determine next hop for %s", mName.c_str(), pPacket->Destination.c_str());
        return false;
    }

    // we have found the next node and now we do a lookup in our FIB
    Link *tNextLink = GetNextLink(tNextHop);
    if (tNextLink  == NULL)
    {
        LOG(LOG_ERROR, "Forwarding failure on node %s: cannot determine next link for %s", mName.c_str(), tNextHop.c_str());
        return false;
    }

    // forward packet to next link which directs the packet to the next hop
    return tNextLink->HandlePacket(pPacket, this);
}

string Node::GetNextHop(string pDestination, const QoSSettings pQoSRequirements)
{
    string tResult = "";

    mRibTableMutex.lock();
    if (mRibTable.size() > 0)
    {
        RibTable::iterator tIt;
        for (tIt = mRibTable.begin(); tIt != mRibTable.end(); tIt++)
        {
            if ((*tIt)->Destination == pDestination)
            {
                tResult = (*tIt)->NextNode;
                break;
            }
        }
    }
    mRibTableMutex.unlock();

    return tResult;
}

Link* Node::GetNextLink(std::string pNextNodeAdr)
{
    Link* tResult = NULL;

    mFibTableMutex.lock();
    if (mFibTable.size() > 0)
    {
        FibTable::iterator tIt;
        for (tIt = mFibTable.begin(); tIt != mFibTable.end(); tIt++)
        {
            if ((*tIt)->NextNode->GetAddress() == pNextNodeAdr)
            {
                tResult = (*tIt)->ViaLink;
                break;
            }
        }
    }
    mFibTableMutex.unlock();

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
