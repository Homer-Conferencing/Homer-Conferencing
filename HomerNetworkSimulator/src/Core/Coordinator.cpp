/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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

Node* Coordinator::GetNode()
{
    return mNode;
}

void Coordinator::SetSuperior(Coordinator *pSuperior)
{
    LOG(LOG_INFO, "Setting coordinator %s as superior of cluster %s at hierarchy level %d", pSuperior->GetClusterAddress().c_str(), mClusterAddress.c_str(), mHierarchyLevel);
    mSuperior = pSuperior;
}

Nodes Coordinator::GetClusterMembers()
{
    Nodes tResult;

    mClusterMembersMutex.lock();
    tResult = mClusterMembers;
    mClusterMembersMutex.unlock();

    return tResult;
}

Coordinators Coordinator::GetChildCoordinators()
{
    Coordinators tResult;

    mChildCoordinatorsMutex.lock();
    tResult = mChildCoordinators;
    mChildCoordinatorsMutex.unlock();

    return tResult;
}

Coordinators Coordinator::GetSiblings()
{
    Coordinators tResult;

    if (mSuperior != NULL)
        tResult = mSuperior->GetChildCoordinators();

    // remove self pointer
    if (tResult.size() > 0)
    {
        Coordinators::iterator tIt;
        for (tIt = tResult.begin(); tIt != tResult.end(); tIt++)
        {
            if ((*tIt)->GetClusterAddress() == GetClusterAddress())
            {
                tResult.erase(tIt);
                break;
            }
        }
    }

    return tResult;
}

int Coordinator::GetHierarchyLevel()
{
    return mHierarchyLevel;
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
    if (pChild == NULL)
    {
        LOG(LOG_ERROR, "Cannot add invalid coordinator as child coordinator to %s", GetClusterAddress().c_str());
        return;
    }
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

Coordinator* Coordinator::GetSibling(std::string pClusterAddress)
{
    Coordinator* tResult = NULL;

    if (mSuperior == NULL)
        return NULL;

    Coordinators tSiblings = mSuperior->mChildCoordinators;;
    Coordinators::iterator tIt;
    for (tIt = tSiblings.begin(); tIt != tSiblings.end(); tIt++)
    {
        if ((*tIt)->GetClusterAddress() == pClusterAddress)
        {
            tResult = *tIt;
            break;
        }
    }

    return tResult;
}

//void Coordinator::MergeIntraClusterCosts(std::string pFromAddress, std::string pToAddress, QoSSettings *pQoSSet, int *pHopCosts)
//{
//    QoSSettings tFoundQoSSet;
//    tFoundQoSSet.DataRate = 0;
//    tFoundQoSSet.Delay = SHRT_MAX;
//    tFoundQoSSet.Features = 0;
//    Node* tSourceNode = NULL;
//    string tDestinationNodeAddr = "";
//    int tHopCosts = 0;
//
//    if (mHierarchyLevel == 0)
//    {// level 0 coordinator
//        #ifdef DEBUG_ROUTING
//            LOG(LOG_VERBOSE, "Determining intra-domain costs for a route from %s to %s through %s", pFromAddress.c_str(), pToAddress.c_str(), mClusterAddress.c_str());
//        #endif
//
//        //*pHopCosts += 1; /* one hop for traversing this cluster */
//
//        //HINT: no locking of mClusterMembersMutex because this function is called from UpdateRouting which controls the mutex
//        NodeList::iterator tIt, tIt2;
//        long long tBestCostForRouteTowardsSource = INT_MAX;
//
//        // ITERATE over all known cluster nodes
//        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
//        {
//            string tCurNodeAddr = (*tIt)->GetAddress();
//
//            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
//            if ((*tIt)->IsGateway())
//            {
//                long long tCosts = (*tIt)->GetRouteCosts(pFromAddress);
//                if (tBestCostForRouteTowardsSource > tCosts)
//                {
//                    tSourceNode = *tIt;
//                }
//            }
//        }
//
//        #ifdef DEBUG_ROUTING
//            if (tSourceNode != NULL)
//                LOG(LOG_VERBOSE, "..ingres node: %s", tSourceNode->GetAddress().c_str());
//        #endif
//    }else
//    {// higher coordinator
//        #ifdef DEBUG_ROUTING
//            LOG(LOG_ERROR, "Determining intra-cluster costs for a route from %s to %s through %s", pFromAddress.c_str(), pToAddress.c_str(), mClusterAddress.c_str());
//        #endif
//
//        tHopCosts = 1; /* one hop for traversing this cluster */
//        //TODO: QoS value adaption, not needed in demo
//        //LOG(LOG_ERROR, "Implement MergeIntraClusterRouteQoS() here");
//    }
//}

void Coordinator::AddClusterTraversalCosts(string pFromAddress, string pToAddress, QoSSettings *pQoSSet, int *pHopCosts)
{
    if ((pFromAddress == mClusterAddress) || (pToAddress == mClusterAddress))
        return;

    if (mHierarchyLevel == 0)
    {// level 0 coordinator
        #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
            LOG(LOG_VERBOSE, "Determining domain traversal costs for a route from %s to %s through %s", pFromAddress.c_str(), pToAddress.c_str(), mClusterAddress.c_str());
        #endif

        //HINT: no locking of mClusterMembersMutex because this function is called from UpdateRouting which controls the mutex
        Nodes::iterator tIt, tIt2;

        //########### calculate best ingres node
        Node* tIngresNode = NULL;
        long long tBestCostsForRouteTowardsSource = LONG_LONG_MAX;
        // ITERATE over all known cluster nodes
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
            long long tRouteCosts = (*tIt)->GetRouteCosts(pFromAddress);
            #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
                LOG(LOG_VERBOSE, "Checking for best costs for from address at node %s brings costs %lld", (*tIt)->GetAddress().c_str(), tRouteCosts);
            #endif
            if (tBestCostsForRouteTowardsSource > tRouteCosts)
            {
                tIngresNode = *tIt;
                tBestCostsForRouteTowardsSource = tRouteCosts;
            }
        }

        //########### calculate best outgres node
        Node* tOutgresNode = NULL;
        long long tBestCostsForRouteTowardsDestination = LONG_LONG_MAX;
        // ITERATE over all known cluster nodes
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
            long long tRouteCosts = (*tIt)->GetRouteCosts(pToAddress);
            #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
                LOG(LOG_VERBOSE, "Checking for best costs for to address at node %s brings costs %lld", (*tIt)->GetAddress().c_str(), tRouteCosts);
            #endif
            if (tBestCostsForRouteTowardsDestination > tRouteCosts)
            {
                tOutgresNode = *tIt;
                tBestCostsForRouteTowardsDestination = tRouteCosts;
            }
        }

        #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
            if (tIngresNode != NULL)
                LOG(LOG_VERBOSE, "..ingres node: %s", tIngresNode->GetAddress().c_str());
            if (tOutgresNode != NULL)
                LOG(LOG_VERBOSE, "..outgres node: %s", tOutgresNode->GetAddress().c_str());
            LOG(LOG_VERBOSE, "..route cost to \"from address\": %lld", tBestCostsForRouteTowardsSource);
            LOG(LOG_VERBOSE, "..route cost to \"to address\": %lld", tBestCostsForRouteTowardsDestination);
        #endif
        if ((tIngresNode != NULL) && (tOutgresNode != NULL))
        {
            tIngresNode->AddRouteCosts(tOutgresNode->GetAddress(), pQoSSet, pHopCosts);
        }else
        {
            #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
                LOG(LOG_VERBOSE, "Haven't found a traversing route from %s to %s through %s", pFromAddress.c_str(), pToAddress.c_str(), mClusterAddress.c_str());
            #endif
            *pHopCosts += 1; /* one hop for traversing this cluster */
        }
    }else
    {// higher coordinator
        #ifdef DEBUG_ROUTING_TRAVERSAL_COSTS
            LOG(LOG_VERBOSE, "Determining cluster traversal costs for a route from %s to %s through %s", pFromAddress.c_str(), pToAddress.c_str(), mClusterAddress.c_str());
        #endif

        *pHopCosts += 1; /* one hop for traversing this cluster */
        //TODO: QoS value adaption, not needed in demo
        //LOG(LOG_VERBOSE, "Implement AddClusterTraversalCosts() here");
    }
}

bool Coordinator::DistributeAggregatedRibEntry(string pDestination, string pNextCluster, int pHopCosts, QoSSettings *pQoSSettings)
{
    string tIntermediateRouteDestination = "";
    bool tResult = false;

    if (pQoSSettings == NULL)
    {
        LOG(LOG_ERROR, "Invalid QoS settings");
        return false;
    }

    if (!IsForeignAddress(pNextCluster))
    {
        LOG(LOG_ERROR, "Routing loop detected, will drop this higher coordinator RIB entry");
        return false;
    }

    // prepare QoS settings
    QoSSettings *tQoSSet = new QoSSettings();
    *tQoSSet = *pQoSSettings;

    #ifdef DEBUG_ROUTING
        LOG(LOG_VERBOSE, "Adding to RIB's of children of coordinator %s the entry: %s via %s (hc: %d, dr: %d, delay: %d)", mClusterAddress.c_str(), pDestination.c_str(), pNextCluster.c_str(), pHopCosts, pQoSSettings->DataRate, pQoSSettings->Delay);
    #endif

    int tHopCosts = pHopCosts;
    if (mHierarchyLevel == 0)
    {// level 0 coordinator
        Nodes::iterator tIt2;
        // AGGREGATE ROUTE and distribute topology knowledge among cluster nodes
        for (tIt2 = mClusterMembers.begin(); tIt2 != mClusterMembers.end(); tIt2++)
        {
            (*tIt2)->DistributeAggregatedRibEntry(pDestination, pNextCluster, tHopCosts, tQoSSet);
        }
    }else
    {// higher coordinator
        //TODO, not needed in demo
        //LOG(LOG_ERROR, "Implement DistributeRibEntry() here"); //TODO: not needed in demo, will be needed if there is a "topology line" of 3 higher coordinators
    }

    return tResult;
}

void Coordinator::UpdateRouting()
{
    #ifdef DEBUG_ROUTING
        LOG(LOG_WARN, "############################### Updating routing of coordinator %s #################################", mClusterAddress.c_str());
    #endif

    if (mHierarchyLevel == 0)
    {// layer 0 coordinator
        mClusterMembersMutex.lock();
        Nodes::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes and update their routing
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            // update the routing of the node
            (*tIt)->UpdateRouting();
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Collect topology and prepare RIB updates for nodes in %s ##################", mClusterAddress.c_str());
        #endif

        // create one RIB update table per cluster member (a node)
        mRibUpdateTables.clear();
        for (int i = 0; i < (int)mClusterMembers.size(); i++)
        {
            RibTable *tRibTable = new RibTable();
            if (tRibTable != NULL)
            {
                tRibTable->clear();
                mRibUpdateTables.push_back(tRibTable);
            }else
                LOG(LOG_ERROR, "Update RIB table is invalid");
        }

        // ITERATE over all known cluster nodes
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            string tCurNodeAddr = (*tIt)->GetAddress();

            // has node interesting routing data? (use this for implementation speed-up, otherwise we would have to search the entire RIB database for interesting entries)
            if ((*tIt)->IsGateway())
            {
                #ifdef DEBUG_NEIOGHBOR_DISCOVERY
                    LOG(LOG_VERBOSE, "Node %s is interesting gateway", (*tIt)->GetAddress().c_str());
                #endif
                RibTable tRib = (*tIt)->GetRib();
                RibTable::iterator tRibIt;

                // ITERATE over all RIB entries
                for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
                {
                    // does link belong to this domain?
                    if (IsForeignAddress((*tRibIt)->Destination))
                    {
                        LOG(LOG_VERBOSE, "Found RIB entry (destination %s via %s, hc: %d, dr: %d, delay: %d) at %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), (*tRibIt)->HopCount, (*tRibIt)->QoSCapabilities.DataRate, (*tRibIt)->QoSCapabilities.Delay, tCurNodeAddr.c_str());

                        // ITERATE over all known cluster nodes and prepare an update RIB for all of them
                        RibTables::iterator tUpdateRibIt = mRibUpdateTables.begin();
                        for (tIt2 = mClusterMembers.begin(); tIt2 != mClusterMembers.end(); tIt2++)
                        {
                            // calculate inner cluster QoS and "add" them
                            QoSSettings tQoSSet = (*tRibIt)->QoSCapabilities;
                            int tHopCosts = (*tRibIt)->HopCount;
//                            MergeIntraClusterCosts((*tRibIt)->Destination, tCurNodeAddr, &tQoSSet, &tHopCosts);

                            //LOG(LOG_ERROR, "%s and %s", tCurNodeAddr.c_str(), (*tIt2)->GetAddress().c_str());
                            if (tCurNodeAddr != (*tIt2)->GetAddress())
                            {// tell other nodes that the current node can reach a foreign cluster
//                                QoSSettings *tQoSSet = new QoSSettings();
//                                tQoSSet->DataRate = MIN(*tRibIt)->QoSCapabilities
                                Node::AddRibEntry((*tIt2)->GetAddress(), (*tUpdateRibIt), (*tRibIt)->Destination, tCurNodeAddr, tHopCosts, &tQoSSet);
                            }
//                            // tell other nodes that the current node can reach a foreign cluster
//                            if (tCurNodeAddr != (*tIt2)->GetAddress())
//                                (*tIt2)->AddRibEntry((*tRibIt)->Destination, tCurNodeAddr, 2, &(*tRibIt)->QoSCapabilities);
//                            else
//                            {// tell the current node that this route reaches all nodes of the foreign cluster
//                                (*tIt2)->AddRibEntry((*tRibIt)->Destination, (*tRibIt)->NextNode, 1, &(*tRibIt)->QoSCapabilities);
//                            }
                            tUpdateRibIt++;
                        }
                    }
                }
            }
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Topology distribution of cluster %s ##################", mClusterAddress.c_str());
        #endif

        // now send the updates to all nodes
        RibTable::iterator tRibIt;
        RibTables::iterator tUpdateRibIt = mRibUpdateTables.begin();
        for (tIt = mClusterMembers.begin(); tIt != mClusterMembers.end(); tIt++)
        {
            for (tRibIt = (*tUpdateRibIt)->begin(); tRibIt != (*tUpdateRibIt)->end(); tRibIt++)
            {
                (*tIt)->DistributeAggregatedRibEntry((*tRibIt)->Destination, (*tRibIt)->NextNode, (*tRibIt)->HopCount, &(*tRibIt)->QoSCapabilities);
            }
            tUpdateRibIt++;
        }

        mClusterMembersMutex.unlock();
    }else
    {// higher coordinator
        mChildCoordinatorsMutex.lock();
        Coordinators::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes and update their routing
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            // first update the routing of child cluster
            (*tIt)->UpdateRouting();
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Collect topology and prepare RIB updates for all children of coordinator %s ##################", mClusterAddress.c_str());
        #endif

        // create one RIB update table per cluster member (a node)
        mRibUpdateTables.clear();
        for (int i = 0; i < (int)mChildCoordinators.size(); i++)
        {
            RibTable *tRibTable = new RibTable();
            if (tRibTable != NULL)
            {
                tRibTable->clear();
                mRibUpdateTables.push_back(tRibTable);
            }else
                LOG(LOG_ERROR, "Update RIB table is invalid");
        }

        // ITERATE over all child coordinators
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            string tCurClusterAddr = (*tIt)->GetClusterAddress();

            RibTable tRib = (*tIt)->GetRib();
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
                        // AGGREGATE TOPOLOGY
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIERARCHY_HEIGHT - 1 - mHierarchyLevel + 1);
                        #ifdef DEBUG_ROUTING
                            LOG(LOG_VERBOSE, "Found higher coordinator RIB entry (destination %s via %s) at %s which routes to its sibling domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurClusterAddr.c_str(), tDomain.c_str());
                        #endif

                        // ITERATE over all child coordinators and prepare an update RIB for all of them
                        RibTables::iterator tUpdateRibIt = mRibUpdateTables.begin();
                        for (tIt2 = mChildCoordinators.begin(); tIt2 != mChildCoordinators.end(); tIt2++)
                        {
                            // calculate inner cluster QoS and "add" them
                            QoSSettings tQoSSet = (*tRibIt)->QoSCapabilities;
                            int tHopCosts = (*tRibIt)->HopCount;
                            (*tIt)->AddClusterTraversalCosts((*tIt2)->GetClusterAddress(), tDomain, &tQoSSet, &tHopCosts);
                            //MergeIntraClusterCosts((*tIt2)->GetClusterAddress(), tDomain, &tQoSSet, &tHopCosts);

                            // tell other child clusters that the current cluster can reach a foreign cluster
                            if ((*tIt)->GetClusterAddress() != (*tIt2)->GetClusterAddress())
                            {
                                // don't tell the next cluster about a route to itself
                                if ((*tIt2)->GetClusterAddress() != tDomain)
                                {
                                    //LOG(LOG_ERROR, "Cluster %s via %s", tDomain.c_str(), tCurClusterAddr.c_str());
                                    Node::AddRibEntry((*tIt2)->GetClusterAddress(), (*tUpdateRibIt), tDomain, tCurClusterAddr, tHopCosts, &tQoSSet);
//                                    (*tIt2)->DistributeRibEntry(tDomain, tCurClusterAddr, (*tRibIt)->HopCount + 2 /* one hop for traversing this higher cluster and one for traversing this child cluster */, &(*tRibIt)->QoSCapabilities);
                                }
                            }else
                            {// don't have to tell the current cluster that this route reaches all sub-clusters of the foreign cluster, he already has this knowledge
                                //(*tIt2)->AddRibEntry(tDomain, (*tRibIt)->NextNode, (*tRibIt)->HopCount + 2, &(*tRibIt)->QoSCapabilities);
                            }
                            tUpdateRibIt++;
                        }
                    }else
                    {// link to foreign cluster
                        // AGGREGATE TOPOLOGY
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIERARCHY_HEIGHT - 1 - mHierarchyLevel);
                        #ifdef DEBUG_ROUTING
                            LOG(LOG_VERBOSE, "Found higher coordinator RIB entry (destination %s via %s) at %s which routes to foreign domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurClusterAddr.c_str(), tDomain.c_str());
                        #endif

                        // ITERATE over all child coordinators and prepare an update RIB for all of them
                        RibTables::iterator tUpdateRibIt = mRibUpdateTables.begin();
                        for (tIt2 = mChildCoordinators.begin(); tIt2 != mChildCoordinators.end(); tIt2++)
                        {
                            // calculate inner cluster QoS and "add" them
                            QoSSettings tQoSSet = (*tRibIt)->QoSCapabilities;
                            int tHopCosts = (*tRibIt)->HopCount;
                            (*tIt)->AddClusterTraversalCosts((*tIt2)->GetClusterAddress(), tDomain, &tQoSSet, &tHopCosts);

                            // tell other child clusters that the current cluster can reach a foreign cluster
                            if ((*tIt)->GetClusterAddress() != (*tIt2)->GetClusterAddress())
                            {
                                // don't tell the next cluster about a route to itself
                                if ((*tIt2)->GetClusterAddress() != tDomain)
                                {
                                    Node::AddRibEntry((*tIt2)->GetClusterAddress(), (*tUpdateRibIt), tDomain, tCurClusterAddr, tHopCosts, &tQoSSet);
//                                    (*tIt2)->DistributeRibEntry(tDomain, tCurClusterAddr, (*tRibIt)->HopCount + 2 /* one hop for traversing this higher cluster and one for traversing this child cluster */, &(*tRibIt)->QoSCapabilities);
                                }
                            }else
                            {// don't have to tell the current cluster that this route reaches all sub-clusters of the foreign cluster, he already has this knowledge
                                //(*tIt2)->AddRibEntry(tDomain, (*tRibIt)->NextNode, (*tRibIt)->HopCount + 2, &(*tRibIt)->QoSCapabilities);
                            }
                            tUpdateRibIt++;
                        }
                    }
                }
            }
        }

        #ifdef DEBUG_ROUTING
            LOG(LOG_WARN, "################## Topology distribution of coordinator %s ##################", mClusterAddress.c_str());
        #endif

        // now send the updates to all nodes
        RibTable::iterator tRibIt;
        RibTables::iterator tUpdateRibIt = mRibUpdateTables.begin();
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            for (tRibIt = (*tUpdateRibIt)->begin(); tRibIt != (*tUpdateRibIt)->end(); tRibIt++)
            {
                (*tIt)->DistributeAggregatedRibEntry((*tRibIt)->Destination, (*tRibIt)->NextNode, (*tRibIt)->HopCount, &(*tRibIt)->QoSCapabilities);
            }
            tUpdateRibIt++;
        }

        mChildCoordinatorsMutex.unlock();
    }
}

RibTable Coordinator::GetRib()
{
    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
        LOG(LOG_VERBOSE, "Determining neighbors of coordinator %s, at hierarchy level %d", mClusterAddress.c_str(), mHierarchyLevel);
    #endif

    //HINT: function assumes that the routing in the child nodes/clusters is already updated
    RibTable tResult;

    if (mHierarchyLevel == 0)
    {// layer 0 coordinator
        mClusterMembersMutex.lock();
        Nodes::iterator tIt, tIt2;
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

                    // does destination belong to foreign domain?
                    if (IsForeignAddress((*tRibIt)->Destination))
                    {
                        string tDomain = Node::GetDomain((*tRibIt)->Destination, HIERARCHY_HEIGHT - 1 - mHierarchyLevel);
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
        mChildCoordinatorsMutex.lock();
        Coordinators::iterator tIt, tIt2;
        // ITERATE over all known cluster nodes
        for (tIt = mChildCoordinators.begin(); tIt != mChildCoordinators.end(); tIt++)
        {
            string tCurClusterAddr = (*tIt)->GetClusterAddress();

            RibTable tRib = (*tIt)->GetRib();//Neighbors();
            RibTable::iterator tRibIt;

            // ITERATE over all RIB entries
            for (tRibIt = tRib.begin(); tRibIt != tRib.end(); tRibIt++)
            {
                //LOG(LOG_ERROR, "Child coordinator RIB entry: %s via %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str());

                // does destination belong to foreign domain
                if ((IsForeignAddress((*tRibIt)->Destination)))
                {
                    string tDomain = Node::GetDomain((*tRibIt)->Destination, HIERARCHY_HEIGHT - 1 - mHierarchyLevel);
                    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
                        LOG(LOG_VERBOSE, "Found cluster neighbor RIB entry (destination %s via %s) at %s which routes to foreign domain %s", (*tRibIt)->Destination.c_str(), (*tRibIt)->NextNode.c_str(), tCurClusterAddr.c_str(), tDomain.c_str());
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
        mChildCoordinatorsMutex.unlock();
    }

    #ifdef DEBUG_NEIOGHBOR_DISCOVERY
        LOG(LOG_VERBOSE, "Found %d neighbors", tResult.size());
    #endif
    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
