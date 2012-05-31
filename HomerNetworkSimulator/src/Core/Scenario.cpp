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
 * Purpose: Implementation of simulated scenarios
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/Scenario.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////
Scenario::Scenario()
{
}


Scenario::~Scenario()
{

}

///////////////////////////////////////////////////////////////////////////////

Scenario* Scenario::CreateScenario(int pIndex)
{
    Scenario *tScenario = NULL;

    switch(pIndex)
    {
        case 0:
            tScenario = new Scenario();

            // ### nodes ###
            tScenario->AddNode("", "1.1.1.1");
            tScenario->AddNode("", "1.1.1.2");
            tScenario->AddNode("Source", "1.1.1.3");

            tScenario->AddNode("", "1.1.2.1");
            tScenario->AddNode("", "1.1.2.2");
            tScenario->AddNode("", "1.1.2.3");

            tScenario->AddNode("", "1.1.3.1");
            tScenario->AddNode("", "1.1.3.2");
            tScenario->AddNode("", "1.1.3.3");


            tScenario->AddNode("", "1.2.1.1");
            tScenario->AddNode("", "1.2.1.2");
            tScenario->AddNode("", "1.2.1.3");

            tScenario->AddNode("", "1.2.2.1");
            tScenario->AddNode("Destination", "1.2.2.2");
            tScenario->AddNode("", "1.2.2.3");

            tScenario->AddNode("", "1.2.3.1");
            tScenario->AddNode("", "1.2.3.2");
            tScenario->AddNode("", "1.2.3.3");

            // ### links ###
            tScenario->AddLink("1.1.1.1", "1.1.1.2");
            tScenario->AddLink("1.1.1.2", "1.1.1.3");
            tScenario->AddLink("1.1.1.3", "1.1.1.1");

            tScenario->AddLink("1.1.1.2", "1.1.2.1");

            tScenario->AddLink("1.1.2.1", "1.1.2.2");
            tScenario->AddLink("1.1.2.2", "1.1.2.3");
            tScenario->AddLink("1.1.2.3", "1.1.2.1");

            tScenario->AddLink("1.1.2.3", "1.1.3.2");

            tScenario->AddLink("1.1.3.1", "1.1.3.2");
            tScenario->AddLink("1.1.3.2", "1.1.3.3");
            tScenario->AddLink("1.1.3.3", "1.1.3.1");

            tScenario->AddLink("1.1.1.3", "1.1.3.1");



            tScenario->AddLink("1.2.1.1", "1.2.1.2");
            tScenario->AddLink("1.2.1.2", "1.2.1.3");
            tScenario->AddLink("1.2.1.3", "1.2.1.1");

            tScenario->AddLink("1.2.1.2", "1.2.2.1");

            tScenario->AddLink("1.2.2.1", "1.2.2.2");
            tScenario->AddLink("1.2.2.2", "1.2.2.3");
            tScenario->AddLink("1.2.2.3", "1.2.2.1");

            tScenario->AddLink("1.2.2.3", "1.2.3.2");

            tScenario->AddLink("1.2.3.1", "1.2.3.2");
            tScenario->AddLink("1.2.3.2", "1.2.3.3");
            tScenario->AddLink("1.2.3.3", "1.2.3.1");

            tScenario->AddLink("1.2.1.3", "1.2.3.1");


            tScenario->AddLink("1.1.2.2", "1.2.1.1");
            tScenario->AddLink("1.1.3.3", "1.2.3.3");
            break;
        default:
            LOGEX(Scenario, LOG_ERROR, "Unsupported scenario index %d", pIndex);
            break;
    }
    return tScenario;
}

Cep* Scenario::AddServerCep(enum TransportType pTransportType, Name pNodeName, unsigned pPort)
{
    // get reference to the right node
    Node *tNode = FindNode(pNodeName.toString());
    if (tNode == NULL)
    {
        LOG(LOG_ERROR, "Node name \"%s\" is unknown to the simulation", pNodeName.toString().c_str());
        return NULL;
    }

    // create CEP
    return tNode->AddServer(pTransportType, pPort);
}

bool Scenario::DeleteServerCep(Name pNodeName, unsigned int pPort)
{
    // get reference to the right node
    Node *tNode = FindNode(pNodeName.toString());
    if (tNode == NULL)
    {
        LOG(LOG_ERROR, "Node name \"%s\" is unknown to the simulation", pNodeName.toString().c_str());
        return NULL;
    }

    // create CEP
    return tNode->DeleteServer(pPort);
}

Cep* Scenario::AddClientCep(enum TransportType pTransportType, Name pTargetNodeName, unsigned pTargetPort)
{
    // get reference to the right node
    Node *tNode = FindNode(pTargetNodeName.toString());
    if (tNode == NULL)
    {
        LOG(LOG_ERROR, "Target node name \"%s\" is unknown to the simulation", pTargetNodeName.toString().c_str());
        return NULL;
    }

    // create CEP
    return tNode->AddClient(pTransportType, pTargetNodeName.toString(), pTargetPort);
}

bool Scenario::AddNode(string pName, std::string pAddressHint)
{
    // do we already know this address?
    if (FindNode(pAddressHint) != NULL)
        return false;

    mNodesMutex.lock();
    Node *tNode = new Node(pName, pAddressHint);
    mNodes.push_back(tNode);
    mNodesMutex.unlock();

    return true;
}

bool Scenario::AddLink(std::string pFromAddress, std::string pToAddress)
{
    Node *tFrom = FindNode(pFromAddress);
    Node *tTo = FindNode(pToAddress);

    if (tFrom == NULL)
    {
        LOG(LOG_ERROR, "Couldn't create link from %s to %s, source address does not name a node", pFromAddress.c_str(), pToAddress.c_str());
        return false;
    }
    if (tTo == NULL)
    {
        LOG(LOG_ERROR, "Couldn't create link from %s to %s, destination address does not name a node", pFromAddress.c_str(), pToAddress.c_str());
        return false;
    }

    Link *tLink = new Link(tFrom, tTo);

    mLinksMutex.lock();
    mLinks.push_back(tLink);
    mLinksMutex.unlock();

    return true;
}

Node* Scenario::FindNode(std::string pNodeIdentifier)
{
    Node* tResult = NULL;
    NodeList::iterator tIt;

    //TODO: support names and do not map 1:1

    mNodesMutex.lock();

    if (mNodes.size() > 0)
    {
        for (tIt = mNodes.begin(); tIt != mNodes.end(); tIt++)
        {
            if (((*tIt)->GetName() == pNodeIdentifier) || ((*tIt)->GetAddress() == pNodeIdentifier))
            {
                tResult = *tIt;
                break;
            }
        }
    }

    mNodesMutex.unlock();

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
