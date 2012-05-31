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

Cep* Node::AddServer(enum TransportType pTransportType, unsigned int pPort)
{
    Cep *tCep = new Cep(this, pTransportType, pPort);

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
///////////////////////////////////////////////////////////////////////////////

}} //namespace
