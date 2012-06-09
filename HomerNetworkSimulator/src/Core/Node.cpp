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

Node::Node(string pName, string pAddressHint, int pPosXHint, int pPosYHint)
{
    for (int i = 0; i < MAX_HIERARCHY_DEPTH; i++)
        mCoordinators[i] = NULL;
    mName = pName;
    mAddress = pAddressHint; //TODO: SO address management
    mDomain = GetDomain(pAddressHint, HIERARCHY_HEIGHT - 1);
    mPosXHint = pPosXHint;
    mPosYHint = pPosYHint;
    mIsGateway = false;

    if ((pName != "") && (pAddressHint != ""))
        DNS.registerName(pName, pAddressHint);

    LOG(LOG_INFO, "Created node %s(address %s), domain %s", mName.c_str(), mAddress.c_str(), mDomain.c_str());
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

FibTable Node::GetFib()
{
    FibTable tResult;

    mFibTableMutex.lock();
    tResult = mFibTable;
    mFibTableMutex.unlock();

    return tResult;
}

bool Node::AddRibEntry(string pNodeAddress, RibTable *pTable, std::string pDestination, std::string pNextNode, int pHopCount, QoSSettings *pQoSSettings)
{
    bool tRouteAlreadyKnown = false;

    if (pNextNode == pNodeAddress)
    {
        LOGEX(Node, LOG_ERROR, "Routing loop detected, will drop this RIB entry");
        return false;
    }

    //TODO: search duplicates and remove more fine granular entries

    // create RIB entry
    RibEntry *tRibEntry = new RibEntry();
    tRibEntry->Destination = pDestination;
    tRibEntry->NextNode = pNextNode;
    tRibEntry->HopCount = pHopCount;
    if (pQoSSettings != NULL)
        tRibEntry->QoSCapabilities = *pQoSSettings;
    else{
        tRibEntry->QoSCapabilities.DataRate = 0;
        tRibEntry->QoSCapabilities.Delay = 0;
        tRibEntry->QoSCapabilities.Features = 0;
    }

    // if next node is a domain then we have to check if we already know the first part of the route towards this destination domain
    if (IsDomain(pNextNode))
    {
        #ifdef DEBUG_ROUTING
            //LOGEX(Node, LOG_WARN, "Next node is domain %s", pNextNode.c_str());
        #endif
        QoSSettings tQoSRequs;
        tQoSRequs.DataRate = 0;
        tQoSRequs.Delay = 0;
        tQoSRequs.Features = 0;
        string tNextNode = GetNextHop(pTable, pNextNode, tQoSRequs);
        if (tNextNode != "")
        {
            //LOGEX(Node, LOG_ERROR, "Set next hop to %s", tNextNode.c_str());
            tRibEntry->NextNode = tNextNode;
        }
    }

    RibTable::iterator tIt;
    if (pTable->size() > 0)
    {
        int tRibPosition = 0;
        for (tIt = pTable->begin(); tIt != pTable->end(); tIt++)
        {
            if (((*tIt)->Destination == tRibEntry->Destination) && ((*tIt)->NextNode == tRibEntry->NextNode))
            {
                #ifdef DEBUG_ROUTING
                    LOGEX(Node, LOG_WARN, "Already stored in RIB of node %s the entry: %s via %s at position %d", pNodeAddress.c_str(), tRibEntry->Destination.c_str(), tRibEntry->NextNode.c_str(), tRibPosition);
                #endif
                tRouteAlreadyKnown = true;
                break;
            }
            //LOG(LOG_ERROR, "Comparing %s and %s", GetForeignDomain((*tIt)->Destination).c_str(), GetForeignDomain(tRibEntry->Destination).c_str());
            if ((GetForeignDomain(pNodeAddress, (*tIt)->Destination) == GetForeignDomain(pNodeAddress, tRibEntry->Destination)) && ((*tIt)->NextNode == tRibEntry->NextNode))
            {
                #ifdef DEBUG_ROUTING
                    LOGEX(Node, LOG_WARN, "Replacing in RIB of node %s the entry: %s via %s by the aggregated entry %s via %s", pNodeAddress.c_str(), (*tIt)->Destination.c_str(), (*tIt)->NextNode.c_str(), tRibEntry->Destination.c_str(), tRibEntry->NextNode.c_str());
                #endif
            }
            tRibPosition++;
        }
    }
    // add entry to RIB
    if (!tRouteAlreadyKnown)
    {// new RIB entry
        #ifdef DEBUG_ROUTING
            LOGEX(Node, LOG_WARN, "Adding to RIB of node %s the entry: %s via %s", pNodeAddress.c_str(), tRibEntry->Destination.c_str(), tRibEntry->NextNode.c_str());
        #endif
            pTable->push_back(tRibEntry);
    }else
    {// already known RIB entry -> drop it
        #ifdef DEBUG_ROUTING
            //LOGEX(Node, LOG_WARN, "Already stored in RIB of node %s the entry: %s via %s", pNodeAddress.c_str(), tRibEntry->Destination.c_str(), tRibEntry->NextNode.c_str());
        #endif
        delete tRibEntry;
    }

    return !tRouteAlreadyKnown;
}

bool Node::AddRibEntry(string pDestination, string pNextNode, int pHopCount, QoSSettings *pQoSSettings)
{
    bool tResult = false;
    mRibTableMutex.lock();

    tResult = AddRibEntry(mAddress, &mRibTable, pDestination, pNextNode, pHopCount, pQoSSettings);

    mRibTableMutex.unlock();

    return tResult;
}

RibTable Node::GetRib()
{
    RibTable tResult;

    mRibTableMutex.lock();
    tResult = mRibTable;
    mRibTableMutex.unlock();

    return tResult;
}

void Node::LogRib()
{
    RibTable::iterator tIt;

    mRibTableMutex.lock();
    int tCount = 0;
    for (tIt = mRibTable.begin(); tIt != mRibTable.end(); tIt++)
    {
        LOG(LOG_VERBOSE, "Entry %d: %s via %s", tCount, (*tIt)->Destination.c_str(), (*tIt)->NextNode.c_str());
        tCount++;
    }
    mRibTableMutex.unlock();
}

bool Node::AddLink(Link *pLink)
{
    Node* tPeerNode = pLink->GetPeerNode(this);

    AddFibEntry(pLink, tPeerNode);

    string tPeerNodeAddr = tPeerNode->GetAddress();

    if (tPeerNodeAddr == mAddress)
    {
        LOG(LOG_ERROR, "Routing loop detected, will drop this RIB entry");
        return false;
    }
    // does the next node belongs to our cluster domain?
    if (GetDomain(tPeerNodeAddr, HIERARCHY_HEIGHT - 1)  !=  mDomain)
    {
        if (!mIsGateway)
        {
            #ifdef DEBUG_NEIOGHBOR_DISCOVERY
                LOG(LOG_VERBOSE, "Mark node %s as gateway to domain %s", mAddress.c_str(), GetDomain(pNextNode, HIERARCHY_HEIGHT - 1).c_str());
            #endif
            mIsGateway = true;
        }
    }

    RibEntry *tRibEntry = new RibEntry();
    tRibEntry->Destination = tPeerNodeAddr;
    tRibEntry->NextNode = tPeerNodeAddr;
    tRibEntry->HopCount = 1;
    tRibEntry->QoSCapabilities = pLink->GetQoSCapabilities();

    #ifdef DEBUG_ROUTING
        LOG(LOG_WARN, "Adding to (phys.) RIB of node %s the entry: %s via %s", mAddress.c_str(), tPeerNodeAddr.c_str(), tPeerNodeAddr.c_str());
    #endif

    mTopologyTableMutex.lock();
    mTopologyTable.push_back(tRibEntry);
    mTopologyTableMutex.unlock();

    AddRibEntry(tPeerNodeAddr, tPeerNodeAddr, 1, &tRibEntry->QoSCapabilities);

    return true;
}

bool Node::IsNeighbor(std::string pAddress)
{
    bool tResult = false;

    mTopologyTableMutex.lock();
    if (mTopologyTable.size() > 0)
    {
        RibTable::iterator tIt;
        for (tIt = mTopologyTable.begin(); tIt != mTopologyTable.end(); tIt++)
        {
            if (pAddress == (*tIt)->NextNode)
            {
                tResult = true;
                break;
            }
        }
    }
    mTopologyTableMutex.unlock();

    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
        LOG(LOG_VERBOSE, "Determined neighbor state at %s with %s as %d", mAddress.c_str(), pAddress.c_str(), tResult);
    #endif

    return tResult;
}

NodeList Node::GetSiblings()
{
    NodeList tResult;

    if (mCoordinator != NULL)
        tResult = mCoordinator->GetClusterMembers();

    // remove self pointer
    if (tResult.size() > 0)
    {
        NodeList::iterator tIt;
        for (tIt = tResult.begin(); tIt != tResult.end(); tIt++)
        {
            if ((*tIt)->GetAddress() == GetAddress())
            {
                tResult.erase(tIt);
                break;
            }
        }
    }

    return tResult;
}

void Node::UpdateRouting()
{
    #ifdef DEBUG_ROUTING
        LOG(LOG_WARN, "################## Updating routing of node %s ##################", mAddress.c_str());
        LOG(LOG_WARN, "Resetting RIB of node %s", mAddress.c_str());
    #endif

    // delete old RIB
    mRibTableMutex.lock();
    RibTable::iterator tIt;
    for (tIt = mRibTable.begin(); tIt != mRibTable.end(); tIt++)
        delete *tIt;
    mRibTable.clear();
    mRibTableMutex.unlock();

    // move entries from physical RIB to main RIB
    mTopologyTableMutex.lock();
    for (tIt = mTopologyTable.begin(); tIt != mTopologyTable.end(); tIt++)
    {
        QoSSettings tQoSSet;
        Link *tLink = GetNextLink((*tIt)->NextNode);
        if (tLink != NULL)
            tQoSSet = tLink->GetQoSCapabilities();
        AddRibEntry(GetForeignDomain(mAddress, (*tIt)->Destination), (*tIt)->NextNode, 1, &tQoSSet);
    }
    mTopologyTableMutex.unlock();
}

bool Node::IsGateway()
{
    return mIsGateway;
}

Coordinator* Node::SetAsCoordinator(int pHierarchyLevel)
{
    Coordinator *tResult = NULL;

    mCoordinatorsMutex.lock();

    // delete old instance
    if (mCoordinators[pHierarchyLevel] != NULL)
    {
        delete mCoordinators[pHierarchyLevel];
        mCoordinators[pHierarchyLevel] = NULL;
    }

    // determine cluster address
    string tClusterAddress = GetDomain(mAddress, HIERARCHY_HEIGHT - pHierarchyLevel - 1);

    // create new
    tResult = new Coordinator(this, tClusterAddress, pHierarchyLevel);
    mCoordinators[pHierarchyLevel] = tResult;

    mCoordinatorsMutex.unlock();

    return tResult;
}

void Node::SetCoordinator(Coordinator *pCoordinator)
{
    mCoordinator = pCoordinator;
}

string Node::GetDomain()
{
    return mDomain;
}

bool Node::IsCoordinator()
{
    return (mCoordinators[0] != NULL);
}

bool Node::IsAddressOfDomain(const string pAddress, const string pDomain)
{
    size_t tPos = 0;
    string tDomain = pDomain;

    // trim domain
    do{
        tPos = tDomain.rfind(".");
        if (tPos != string::npos)
        {
            //LOGEX(Node, LOG_WARN, "Dom: %s, pos: %d", tDomain.c_str(), (int)tPos);
            if (tDomain.substr(tPos, tDomain.length() - tPos) == ".0")
            {
                tDomain = tDomain.substr(0, tPos);
            }else
                break;
        }
    }while (tPos != string::npos);

    //LOGEX(Node, LOG_ERROR, "Address %s, domain %s", pAddress.c_str(), tDomain.c_str());

    // search domain in address
    if (pAddress.substr(0, tDomain.length()) == tDomain)
        return true;
    else
        return false;
}

string Node::GetDomain(const string pAddress, int pHierarchyDepth)
{
    int tOrgDepth = pHierarchyDepth;
    string tResult = "";

    size_t tPos = 0;
    do{
        tPos = pAddress.find(".", tPos + 1);
        pHierarchyDepth--;
        //LOGEX(Node, LOG_WARN, "   Dom: %s, pos: %d", pAddress.substr(0, tPos).c_str(), (int)tPos);
    }while(pHierarchyDepth >= 0);

    tResult = pAddress.substr(0, tPos);

    for (int i = 0; i < HIERARCHY_HEIGHT - tOrgDepth; i++)
        tResult += ".0";

    //LOGEX(Node, LOG_VERBOSE, "Address %s belongs to domain %s at hierarchy depth %d", pAddress.c_str(), tResult.c_str(), tOrgDepth);

    return tResult;
}

string Node::GetForeignDomain(const string pOwnAddress, const string pAddress)
{
    int tHierarchyLevel = 1;
    string tResult = "";

    size_t tPos = pAddress.length();
    size_t tLastPos = tPos;
    do{
        tPos = pAddress.rfind(".", tPos - 1);
        if (pOwnAddress.substr(0, tPos) == pAddress.substr(0, tPos))
            break;
        tHierarchyLevel++;
        tLastPos = tPos;
        //LOGEX(Node, LOG_WARN, "   Foreign dom: %s, pos: %d", pAddress.substr(0, tPos).c_str(), (int)tPos);
    }while(tHierarchyLevel < HIERARCHY_HEIGHT);

    tResult = pAddress.substr(0, tLastPos);

    for (int i = 0; i < tHierarchyLevel - 1; i++)
        tResult += ".0";

    //LOGEX(Node, LOG_VERBOSE, "Foreign domain of %s is %s", pAddress.c_str(), tResult.c_str());

    return tResult;
}

bool Node::IsDomain(const std::string pAddress)
{
    size_t tPos = pAddress.rfind(".");
    if (tPos == string::npos)
        return false;

    //LOGEX(Node, LOG_WARN, "Adr part: %s", pAddress.substr(tPos, pAddress.length() - tPos).c_str());

    if (pAddress.substr(tPos, pAddress.length() - tPos) == ".0")
        return true;
    else
        return false;
}

Cep* Node::AddServer(enum TransportType pTransportType, unsigned int pPort)
{
    LOG(LOG_VERBOSE, "Adding server for port %u", pPort);

    Cep *tCep = new Cep(this, pTransportType, pPort);

    mServerCepsMutex.lock();
    mServerCeps.push_back(tCep);
    mServerCepsMutex.unlock();

    return tCep;
}

bool Node::DeleteServer(unsigned int pPort)
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Deleting server at %s:%u", GetAddress().c_str(), pPort);

    mServerCepsMutex.lock();
    if (mServerCeps.size() > 0)
    {
        CepList::iterator tIt;
        for (tIt = mServerCeps.begin(); tIt != mServerCeps.end(); tIt)
        {
            //LOG(LOG_VERBOSE, "Comparing CEP with local port %u and desired port %u", (*tIt)->GetLocalPort(), pPort);
            if ((*tIt)->GetLocalPort() == pPort)
            {
                mServerCeps.erase(tIt);
                tResult = true;
                break;
            }
        }
    }
    mServerCepsMutex.unlock();

    return tResult;
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

void Node::SetName(string pName)
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
        for (tIt = mServerCeps.begin(); tIt != mServerCeps.end(); tIt++)
        {
            //LOG(LOG_VERBOSE, "Comparing CEP with local port %u and desired port %u", (*tIt)->GetLocalPort(), pPort);
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

void Node::LogServerCeps()
{
    CepList::iterator tIt;

    mServerCepsMutex.lock();
    int tCount = 0;
    for (tIt = mServerCeps.begin(); tIt != mServerCeps.end(); tIt++)
    {
        LOG(LOG_VERBOSE, "Entry %d: %s:%u", tCount, (*tIt)->GetLocalNode().c_str(), (*tIt)->GetLocalPort());
        tCount++;
    }
    mServerCepsMutex.unlock();
}

bool Node::HandlePacket(Packet *pPacket)
{
    #ifdef DEBUG_FORWARDING
        LOG(LOG_VERBOSE, "Handling packet from %s at node %s(%s) with destination %s", pPacket->Source.c_str(), mName.c_str(), mAddress.c_str(), pPacket->Destination.c_str());
    #endif

    pPacket->RecordedRoute.push_back("node " + mAddress);

    // are we the packet's destination?
    if (pPacket->Destination == mAddress)
    {
        #ifdef DEBUG_FORWARDING
            LOG(LOG_VERBOSE, "Destination %s reached at node %s(%s)", pPacket->Destination.c_str(), mName.c_str(), mAddress.c_str());
        #endif
        Cep *tServerCep = FindServerCep(pPacket->DestinationPort);

        // have we found a fitting server CEP?
        if (tServerCep == NULL)
        {
            LOG(LOG_ERROR, "No server CEP matched on node %s, registered CEPs are..", mAddress.c_str());
            LogServerCeps();
            return false;
        }

        // deliver packet to local server CEP
        return tServerCep->HandlePacket(pPacket);
    }

    pPacket->TTL--;
    if (pPacket->TTL <= 0)
    {
        LOG(LOG_ERROR, "TTL exceeded for packet from %s at node %s, packet was transfered..", pPacket->Source.c_str(), mAddress.c_str());
        list<string>::iterator tIt;
        for (tIt = pPacket->RecordedRoute.begin(); tIt != pPacket->RecordedRoute.end(); tIt++)
        {
            LOG(LOG_ERROR, "..via %s", (*tIt).c_str());
        }
        return false;
    }

    // we are not the destination, we lookup the next node via our RIB
    string tNextHop = GetNextHop(pPacket->Destination, pPacket->QoSRequirements);
    if (tNextHop == "")
    {
        LOG(LOG_ERROR, "Routing failure on node %s: cannot determine next hop for %s, routing table is..", mAddress.c_str(), pPacket->Destination.c_str());
        LogRib();
        return false;
    }

    // we have found the next node and now we do a lookup in our FIB
    Link *tNextLink = GetNextLink(tNextHop);
    if (tNextLink  == NULL)
    {
        LOG(LOG_ERROR, "Forwarding failure on node %s: cannot determine next link for %s", mAddress.c_str(), tNextHop.c_str());
        return false;
    }

    // forward packet to next link which directs the packet to the next hop
    return tNextLink->HandlePacket(pPacket, this);
}

string Node::GetNextHop(RibTable *pTable, string pDestination, const QoSSettings pQoSRequirements)
{
    string tResult = "";
    int tBestHopCosts = INT_MAX;

    if (pTable->size() > 0)
    {
        RibTable::iterator tIt;
        for (tIt = pTable->begin(); tIt != pTable->end(); tIt++)
        {
            if (tBestHopCosts > (*tIt)->HopCount)
            {
                //TODO: QoS
                if (!IsDomain((*tIt)->NextNode))
                {
                    //LOGEX(Node, LOG_ERROR, "Comparing %s and %s", pDestination.c_str(), (*tIt)->Destination.c_str());
                    if ((*tIt)->Destination == pDestination)
                    {// MATCH: entry is an explicit destination

                        tResult = (*tIt)->NextNode;
                        tBestHopCosts = (*tIt)->HopCount;
                        break;
                    }
                    //LOGEX(Node, LOG_ERROR, "Comparing %s and (%s via %s)", pDestination.c_str(), (*tIt)->Destination.c_str(), (*tIt)->NextNode.c_str());
                    if ((IsDomain((*tIt)->Destination)) && ((IsAddressOfDomain((*tIt)->Destination, pDestination)) || (IsAddressOfDomain(pDestination, (*tIt)->Destination))))
                    {// MATCH: entry is a cluster domain
                        tResult = (*tIt)->NextNode;
                        tBestHopCosts = (*tIt)->HopCount;
                        break;
                    }
                }
            }
        }
    }

    return tResult;
}

string Node::GetNextHop(string pDestination, const QoSSettings pQoSRequirements)
{
    string tResult = "";

    mRibTableMutex.lock();

    tResult = GetNextHop(&mRibTable, pDestination, pQoSRequirements);

    mRibTableMutex.unlock();

    return tResult;
}

Link* Node::GetNextLink(string pNextNodeAdr)
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

int Node::GetPosXHint()
{
    return mPosXHint;
}

int Node::GetPosYHint()
{
    return mPosYHint;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
