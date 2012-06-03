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
 * Purpose: Implementation of Coordinator
 * Author:  Thomas Volkert
 * Since:   2012-06-01
 */

#include <Core/Coordinator.h>
#include <Core/Node.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Coordinator::Coordinator(Node *pNode, string pClusterAddress, int pHierarchyLevel)
{
    mHierarchyLevel = pHierarchyLevel;
    mNode = pNode;
    mClusterAddress = pClusterAddress;
    mSuperior = NULL;

    LOG(LOG_INFO, "Setting node %s as coordinator for cluster %s and hierarchy level %d", mNode->GetAddress().c_str(), mClusterAddress.c_str(), pHierarchyLevel);
}

Coordinator::~Coordinator()
{
}

///////////////////////////////////////////////////////////////////////////////

string Coordinator::GetClusterAddress()
{
    return mClusterAddress;
}

void Coordinator::SetSuperior(Coordinator *pSuperior)
{
    LOG(LOG_INFO, "Setting coordinator %s as superior of cluster %s at hierarchy level %d", pSuperior->GetClusterAddress().c_str(), mClusterAddress.c_str(), mHierarchyLevel);
    mSuperior = pSuperior;
}

NodeList Coordinator::GetClusterMembers()
{
    NodeList tResult;

    mClusterMembersMutex.lock();
    tResult = mClusterMembers;
    mClusterMembersMutex.unlock();

    return tResult;
}

CoordinatorList Coordinator::GetChildCoordinators()
{
    CoordinatorList tResult;

    mChildCoordinatorsMutex.lock();
    tResult = mChildCoordinators;
    mChildCoordinatorsMutex.unlock();

    return tResult;
}

void Coordinator::AddClusterMember(Node *pNode)
{
    LOG(LOG_INFO, "Adding node %s to cluster %s at hierarchy level %d", pNode->GetAddress().c_str(), mClusterAddress.c_str(), mHierarchyLevel);
    pNode->SetCoordinator(this);

    mClusterMembersMutex.lock();
    mClusterMembers.push_back(pNode);
    mClusterMembersMutex.unlock();
}

void Coordinator::AddChildCoordinator(Coordinator *pChild)
{
    LOG(LOG_INFO, "Adding child coordinator %s to cluster %s at hierarchy level %d", pChild->GetClusterAddress().c_str(), mClusterAddress.c_str(), mHierarchyLevel);
    pChild->SetSuperior(this);

    mChildCoordinatorsMutex.lock();
    mChildCoordinators.push_back(pChild);
    mChildCoordinatorsMutex.unlock();
}

bool Coordinator::IsForeignAddress(std::string pAddress)
{
    return !Node::IsAddressOfDomain(pAddress, mClusterAddress);
}

bool Coordinator::DistributeRibEntry(string pDestination, string pNextCluster, int pHopCount, QoSSettings *pQoSSettings)
{
    string tIntermediateRouteDestination = "";
    bool tEntryIsUsable = false;

    if (!IsForeignAddress(pNextCluster))
    {
        LOG(LOG_ERROR, "Routing loop detected, will drop this higher coordinator RIB entry");
        return false;
    }

    //TODO: search duplicates and remove more fine granular entries

    #ifdef DEBUG_ROUTING
        LOG(LOG_VERBOSE, "Adding to RIB's of children of coordinator %s the entry: %s via %s", mClusterAddress.c_str(), pDestination.c_str(), pNextCluster.c_str());
    #endif

    RibTable tRib = GetNeighbors();
    RibTable::iterator tRibIt;

    // ITERATE over all RIB entries
    for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
    {
        // link combination possible?
        if ((*tRibIt)->Destination == pNextCluster)
        {
            #ifdef DEBUG_ROUTING
                LOG(LOG_WARN, "..use route to %s via %s for a route towards %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), pDestination.c_str());
            #endif
            tIntermediateRouteDestination = (*tRibIt)->NextNode;
            tEntryIsUsable = true;
            break;
        }else
        {
            #ifdef DEBUG_ROUTING
                //LOG(LOG_VERBOSE, "..DON'T USE route to %s via %s for a route towards %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), pDestination.c_str());
            #endif
        }
    }

    if (tEntryIsUsable)
    {
        if (mHierarchyLevel == 0)
        {// level 0 coordinator
            NodeList::iterator tIt2;
            // AGGREGATE ROUTE and distribute topology knowledge among cluster nodes
            for (tIt2 = mClusterMembers.begin(); tIt2 != mClusterMembers.end(); tIt2++)
            {
                if (!(*tIt2)->IsNeighbor(tIntermediateRouteDestination))
                {// next cluster is only reachable via intermediate node
                    (*tIt2)->AddRibEntry(pDestination, pNextCluster, pHopCount + 1 /* one for traversing this cluster */, pQoSSettings);
                }else
                {// next cluster is a direct neighbor
                    (*tIt2)->AddRibEntry(pDestination, pNextCluster, pHopCount, pQoSSettings);
                }
            }
        }else
        {// higher coordinator
            LOG(LOG_ERROR, "Implement DistributeRibEntry() here");
        }
        //    mRibTableMutex.lock();
        //    mRibTable.push_back(tRibEntry);
        //    mRibTableMutex.unlock();
        //
    }else
    {
        #ifdef DEBUG_ROUTING
            LOG(LOG_VERBOSE, "..ignoring this RIB entry");
        #endif
    }

    return tEntryIsUsable;
}

void Coordinator::UpdateRouting()
{
    #ifdef DEBUG_ROUTING
        LOG(LOG_WARN, "################## Updating routing of coordinator %s ##################", mClusterAddress.c_str());
    #endif

    if (mHierarchyLevel == 0)
    {// layer 0 coordinator
        mClusterMembersMutex.lock();
        NodeList::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes and update their routing
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            // update the routing of the node
            (*tIt)->UpdateRouting();
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Topology distribution of coordinator %s ##################", mClusterAddress.c_str());
        #endif

        // ITERATE over all known cluster nodes
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            string tCurNodeAddr = (*tIt)->GetAddress();

            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
            if ((*tIt)->IsGateway())
            {
                RibTable tRib = (*tIt)->GetNeighbors();
                RibTable::iterator tRibIt;

                // ITERATE over all RIB entries
                for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
                {
                    // direct link? / no aggregated one
                    if ((*tRibIt)->Destination == (*tRibIt)->NextNode)
                    {
                        // does link belong to this domain?
                        if (IsForeignAddress((*tRibIt)->Destination))
                        {
                            string tDomain = Node::GetDomain((*tRibIt)->Destination, HIEARACHY_DEPTH - 2 - mHierarchyLevel);
                            LOG(LOG_VERBOSE, "Found RIB entry (destination %s via %s) at %s which routes to foreign domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurNodeAddr.c_str(), tDomain.c_str());

                            // AGGREGATE ROUTE and distribute topology knowledge among cluster nodes
                            for (tIt2 = mClusterMembers.begin(); tIt2 != mClusterMembers.end(); tIt2++)
                            {
                                // tell other nodes that the current node can reach a foreign cluster
                                if (tCurNodeAddr != (*tIt2)->GetAddress())
                                    (*tIt2)->AddRibEntry(tDomain, tCurNodeAddr, 2, &(*tRibIt)->QoSCapabilities);
                                else
                                {// tell the current node that this route reaches all nodes of the foreign cluster
                                    (*tIt2)->AddRibEntry(tDomain, (*tRibIt)->NextNode, 1, &(*tRibIt)->QoSCapabilities);
                                }
                            }
                        }
                    }
                }
            }
        }
        mClusterMembersMutex.unlock();
    }else
    {// higher coordinator
        mChildCoordinatorsMutex.lock();
        CoordinatorList::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes and update their routing
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            // first update the routing of child cluster
            (*tIt)->UpdateRouting();
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Topology distribution of coordinator %s ##################", mClusterAddress.c_str());
        #endif

        // ITERATE over all known cluster nodes
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            string tCurClusterAddr = (*tIt)->GetClusterAddress();

            RibTable tRib = (*tIt)->GetNeighbors();
            RibTable::iterator tRibIt;

            // ITERATE over all RIB entries
            for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
            {
                // ignore explicit node-to-node routes, only use aggregated links
                if ((*tRibIt)->Destination != (*tRibIt)->NextNode)
                {
                    // does link belong to this domain?
                    if (!IsForeignAddress((*tRibIt)->Destination))
                    {// cluster internal link
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIEARACHY_DEPTH - 2 - mHierarchyLevel + 1);
                        #ifdef DEBUG_ROUTING
                            LOG(LOG_VERBOSE, "Found higher coordinator RIB entry (destination %s via %s) at %s which routes to its sibling domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurClusterAddr.c_str(), tDomain.c_str());
                        #endif

                        // AGGREGATE ROUTE and distribute topology knowledge among cluster nodes
                        for (tIt2 = mChildCoordinators.begin(); tIt2 != mChildCoordinators.end(); tIt2++)
                        {
                            // tell other child clusters that the current cluster can reach a foreign cluster
                            if ((*tIt)->GetClusterAddress() != (*tIt2)->GetClusterAddress())
                            {
                                // don't tell the next cluster about a route to itself
                                if ((*tIt2)->GetClusterAddress() != tDomain)
                                {
                                    (*tIt2)->DistributeRibEntry(tDomain, tCurClusterAddr, (*tRibIt)->HopCount + 2 /* one hop for traversing this higher cluster and one for traversing this child cluster */, &(*tRibIt)->QoSCapabilities);
                                }
                            }else
                            {// don't have to tell the current cluster that this route reaches all sub-clusters of the foreign cluster, he already has this knowledge
                                //(*tIt2)->AddRibEntry(tDomain, (*tRibIt)->NextNode, (*tRibIt)->HopCount + 2, &(*tRibIt)->QoSCapabilities);
                            }
                        }
                    }else
                    {// link to foreign cluster
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIEARACHY_DEPTH - 2 - mHierarchyLevel);
                        #ifdef DEBUG_ROUTING
                            LOG(LOG_VERBOSE, "Found higher coordinator RIB entry (destination %s via %s) at %s which routes to foreign domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurClusterAddr.c_str(), tDomain.c_str());
                        #endif

                        // AGGREGATE ROUTE and distribute topology knowledge among cluster nodes
                        for (tIt2 = mChildCoordinators.begin(); tIt2 != mChildCoordinators.end(); tIt2++)
                        {
                            // tell other child clusters that the current cluster can reach a foreign cluster
                            if ((*tIt)->GetClusterAddress() != (*tIt2)->GetClusterAddress())
                            {
                                // don't tell the next cluster about a route to itself
                                if ((*tIt2)->GetClusterAddress() != tDomain)
                                {
                                    (*tIt2)->DistributeRibEntry(tDomain, tCurClusterAddr, (*tRibIt)->HopCount + 2 /* one hop for traversing this higher cluster and one for traversing this child cluster */, &(*tRibIt)->QoSCapabilities);
                                }
                            }else
                            {// don't have to tell the current cluster that this route reaches all sub-clusters of the foreign cluster, he already has this knowledge
                                //(*tIt2)->AddRibEntry(tDomain, (*tRibIt)->NextNode, (*tRibIt)->HopCount + 2, &(*tRibIt)->QoSCapabilities);
                            }
                        }
                    }
                }
            }
        }
        mChildCoordinatorsMutex.unlock();
    }
}

RibTable Coordinator::GetNeighbors()
{
    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
        LOG(LOG_VERBOSE, "Determining neighbors of coordinator %s, at hierarchy level %d", mClusterAddress.c_str(), mHierarchyLevel);
    #endif

    //HINT: function assumes that the routing in the child nodes/clusters is already updated
    RibTable tResult;

    if (mHierarchyLevel == 0)
    {// layer 0 coordinator
        mClusterMembersMutex.lock();
        NodeList::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            string tCurNodeAddr = (*tIt)->GetAddress();

            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
            if ((*tIt)->IsGateway())
            {
                RibTable tRib = (*tIt)->GetRib();//Neighbors();
                RibTable::iterator tRibIt;

                // ITERATE over all RIB entries
                for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
                {
                    //LOG(LOG_ERROR, "RIB entry: %s via %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str());

                    // does destination belong to foreign domain and this link is a direct one?
                    if ((IsForeignAddress((*tRibIt)->Destination)))// && ((*tRibIt)->HopCount == 1))
                    {
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIEARACHY_DEPTH - 2 - mHierarchyLevel);
                        #ifdef DEBUG_NEIOGHBOR_DISCOVERY
                            LOG(LOG_VERBOSE, "Found cluster neighbor RIB entry (destination %s via %s) at %s which routes to foreign domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurNodeAddr.c_str(), tDomain.c_str());
                        #endif

                        // AGGREGATE neighbor addresses and create new entry
                        RibEntry *tRibEntry = new RibEntry();
                        tRibEntry->Destination = tDomain; // aggregate here
                        tRibEntry->NextNode = (*tRibIt)->NextNode;
                        tRibEntry->HopCount = (*tRibIt)->HopCount;
                        tRibEntry->QoSCapabilities = (*tRibIt)->QoSCapabilities;

                        tResult.push_back(tRibEntry);
                    }
                }
            }
        }
        mClusterMembersMutex.unlock();
    }else
    {// higher coordinator
        LOG(LOG_ERROR, "Implement GetNeighbors() here");
    }

    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
        LOG(LOG_VERBOSE, "Found %d neighbors", tResult.size());
    #endif
    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
