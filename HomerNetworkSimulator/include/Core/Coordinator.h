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
 * Purpose: Coordinator
 * Author:  Thomas Volkert
 * Since:   2012-06-01
 */

#ifndef _GAPI_SIMULATION_COORDINATOR_
#define _GAPI_SIMULATION_COORDINATOR_

#include <HBMutex.h>

#include <Core/Node.h>

#include <list>
#include <string>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

#define HIERARCHY_HEIGHT                     3

///////////////////////////////////////////////////////////////////////////////

class Coordinator;
typedef std::list<Coordinator*> CoordinatorList;

class Node;

class Coordinator
{
public:
    Coordinator(Node *pNode, std::string pClusterAddress, int pHierarchyLevel);
    virtual ~Coordinator();

    void AddClusterMember(Node *pNode);
    void AddChildCoordinator(Coordinator *pChild);

    NodeList GetClusterMembers(); // for GUI
    CoordinatorList GetChildCoordinators(); // for GUI
    CoordinatorList GetSiblings(); // for GUI
    int GetHierarchyLevel(); // for GUI

    std::string GetClusterAddress();
    Node* GetNode();

    /* RIB management */
    bool DistributeAggregatedRibEntry(std::string pDestination, std::string pNextCluster, int pHopCosts = 1, QoSSettings *pQoSSettings = NULL);
    void UpdateRouting();
    RibTable GetRib();

private:
    Coordinator* GetSibling(std::string pClusterAddress);
//    void MergeIntraClusterCosts(std::string pFromAddress, std::string pToAddress, QoSSettings *pQoSSet, int *pHopCosts);
    void AddClusterTraversalCosts(std::string pFromAddress, std::string pToAddress, QoSSettings *pQoSSet, int *pHopCosts);
    bool IsForeignAddress(std::string pAddress);

    void            SetSuperior(Coordinator *pSuperior);
    Coordinator     *mSuperior;

    int             mHierarchyLevel;
    std::string     mClusterAddress;
    Node            *mNode;

    std::list<RibTable*> mRibUpdateTables;

    NodeList        mClusterMembers;
    Mutex           mClusterMembersMutex;

    CoordinatorList mChildCoordinators;
    Mutex           mChildCoordinatorsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
