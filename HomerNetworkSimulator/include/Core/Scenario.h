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
    Cep* AddClientCep(enum TransportType pTransportType, Name pTargetNodeName, unsigned pTargetPort);

    /* name management */
    //TODO

private:
    Scenario();

    /* topology management */
    // complex topology structures
    Coordinator* AddDomain(std::string pDomainPrefix, int pNodeCount);
    // basic topology elements
    Node* AddNode(std::string pName, std::string pAddressHint = "");
    Link* AddLink(std::string pFromAddress, std::string pToAddress);
    Coordinator* AddCoordinator(std::string pNodeAddress, std::string pClusterName, int pHierarchyLevel);

    /* database management */
    Node* FindNode(std::string pNodeIdentifier);

    /* DNS */
    bool registerName(string pName, string pAddress);

    Name        mName;
    NodeList    mNodes;
    Mutex       mNodesMutex;
    LinkList    mLinks;
    Mutex       mLinksMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
