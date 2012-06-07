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
 * Purpose: Scenario
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#ifndef _GAPI_SIMULATION_SCENARIO_
#define _GAPI_SIMULATION_SCENARIO_

#include <HBMutex.h>
#include <Name.h>
#include <Core/Coordinator.h>
#include <Core/Link.h>
#include <Core/Node.h>

#include <list>

namespace Homer { namespace Base {

class Link;
typedef std::list<Link*> LinkList;

struct StreamDescriptor{
    int         PacketCount;
    QoSSettings QoSRequs;
    QoSSettings QoSRes;
    std::string LocalNode, PeerNode;
    int         LocalPort, PeerPort;
    int         Id;
};
typedef std::list<struct StreamDescriptor> StreamList;

///////////////////////////////////////////////////////////////////////////////

class Scenario
{
public:
    static Scenario* CreateScenario(int pIndex = 0);
    virtual ~Scenario();

    /* server CEP management */
    Cep* AddServerCep(enum TransportType pTransportType, Name pNodeName, unsigned pPort);
    bool DeleteServerCep(Name pNodeName, unsigned int pPort);

    /* client CEP management */
    Cep* AddClientCep(enum TransportType pTransportType, Name pTargetNodeIdentifier, unsigned pTargetPort);
    bool DeleteClientCep(Name pNodeName, unsigned int pPort);
    StreamList GetStreams(); // for GUI
    void SetSourceNode(std::string pAddress);
    void SetDestinationNode(std::string pAddress);

    /* list topology */
    NodeList GetNodes(); // for GUI
    LinkList GetLinks(); // for GUI
    Coordinator* GetRootCoordinator();

    /* routing */
    void UpdateRouting();

    /* name management */
    //TODO

private:
    Scenario();

    /* topology management */
    // complex topology structures
    Coordinator* AddDomain(std::string pDomainPrefix, int pNodeCount, int pPosXHint = 0, int pPosYHint = 0);
    Coordinator* AddDomain3(std::string pDomainPrefix, int pPosXHint = 0, int pPosYHint = 0);
    // basic topology elements
    Node* AddNode(std::string pName, std::string pAddressHint = "", int pPosXHint = 0, int pPosYHint = 0);
    Link* AddLink(std::string pFromAddress, std::string pToAddress);
    Coordinator* AddCoordinator(std::string pNodeAddress, int pHierarchyLevel);

    /* database management */
    Node* FindNode(std::string pNodeIdentifier);

    /* DNS */
    bool registerName(string pName, string pAddress);

    Name        mName;

    void        SetRootCoordinator(Coordinator *pCoordinator);
    Coordinator *mRootCoordinator;

    Node        *mSourceNode, *mDestinationNode;
    NodeList    mNodes;
    Mutex       mNodesMutex;

    LinkList    mLinks;
    Mutex       mLinksMutex;

    /* client CEPs */
    CepList     mClientCeps;
    Mutex       mClientCepsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
