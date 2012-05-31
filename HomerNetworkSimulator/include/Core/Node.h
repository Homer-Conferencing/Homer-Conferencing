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

#include <Core/Link.h>
#include <Core/Cep.h>

namespace Homer { namespace Base {

class Node;
typedef std::list<Node*> NodeList;

///////////////////////////////////////////////////////////////////////////////

struct FibEntry{
    Node *NextNode;
    Link *ViaLink;
};

typedef std::list<FibEntry*> FibTable;

class Node
{
public:
    Node(std::string pName, std::string pAddressHint = "");
    virtual ~Node();

    /* FIB management */
    bool AddFibEntry(Link *pLink, Node *pNextNode);

    /* CEP management */
    Cep* AddServer(enum TransportType pTransportType, unsigned int pPort);
    bool DeleteServer(unsigned int pPort);
    Cep* AddClient(enum TransportType pTransportType, std::string pTarget, unsigned pTargetPort);

    /* address management */
    std::string GetAddress();

    /* name management */
    std::string GetName();

private:
    FibTable    mFibTable;
    Mutex       mFibTableMutex;

    std::string mAddress;
    std::string mName;

    CepList     mServerCeps;
    Mutex       mServerCepsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
