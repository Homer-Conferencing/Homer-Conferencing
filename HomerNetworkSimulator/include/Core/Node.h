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
 * Purpose: Node
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#ifndef _GAPI_SIMULATION_NODE_
#define _GAPI_SIMULATION_NODE_

#include <HBMutex.h>

#include <list>
#include <string>

#include <Core/Cep.h>

#include <Core/Link.h>

namespace Homer { namespace Base {

class Node;
typedef std::list<Node*> NodeList;


class Coordinator;

///////////////////////////////////////////////////////////////////////////////

#define MAX_HIERARCHY_DEPTH             10

///////////////////////////////////////////////////////////////////////////////

struct FibEntry{
    Node *NextNode;
    Link *ViaLink;
};

struct RibEntry{
    string Destination;
    string NextNode;
};

typedef std::list<FibEntry*> FibTable;
typedef std::list<RibEntry*> RibTable;

class Node
{
public:
    Node(std::string pName, std::string pAddressHint = "");
    virtual ~Node();

    /* FIB management */
    bool AddFibEntry(Link *pLink, Node *pNextNode);

    /* RIB management */
    bool AddRibEntry(std::string pDestination, std::string pNextNode);

    /* coordinator handling */
    Coordinator* SetAsCoordinator(std::string pClusterName, int pHierarchyLevel);

    /* CEP management */
    Cep* AddServer(enum TransportType pTransportType, unsigned int pPort);
    bool DeleteServer(unsigned int pPort);
    Cep* AddClient(enum TransportType pTransportType, std::string pTarget, unsigned pTargetPort);

    /* address management */
    std::string GetAddress();

    /* name management */
    std::string GetName();
    void SetName(std::string pName);

    bool HandlePacket(Packet *pPacket);

private:
    /* RIB lookup */
    std::string GetNextHop(std::string pDestination, const QoSSettings pQoSRequirements);
    RibTable    mRibTable;
    Mutex       mRibTableMutex;

    /* FIB lookup */
    Link*       GetNextLink(std::string pNextNodeAdr);
    FibTable    mFibTable;
    Mutex       mFibTableMutex;

    /* coordinators */
    Coordinator *mCoordinators[MAX_HIERARCHY_DEPTH];
    Mutex       mCoordinatorsMutex;

    /* server CEPs */
    Cep*        FindServerCep(unsigned int pPort);
    CepList     mServerCeps;
    Mutex       mServerCepsMutex;

    /* addressing, naming */
    std::string mAddress;
    std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
