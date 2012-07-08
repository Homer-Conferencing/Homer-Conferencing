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
 * Purpose: Implementation of simulated scenarios
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#include <Core/DomainNameService.h>
#include <Core/Scenario.h>
#include <Core/Cep.h>

#include <Logger.h>

#include <string>
#include <vector>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

#define SIMULATION_SOURCE_NODE              "Source"

#define NODE_SHAPE_WIDTH                    40
#define NODE_SHAPE_HEIGHT                   40

///////////////////////////////////////////////////////////////////////////////

Scenario::Scenario()
{
    mSourceNode = NULL;
    mDestinationNode = NULL;
}


Scenario::~Scenario()
{

}

///////////////////////////////////////////////////////////////////////////////

Scenario* Scenario::CreateScenario(int pIndex)
{
    Scenario *tScenario = NULL;

    // some tests
//    Node::GetDomain("1.2.3.4.5", 0);
//    Node::GetDomain("1.2.3.4.5", 1);
//    Node::GetDomain("1.2.3.4.5", 2);
//    Node::GetDomain("1.2.3.4.5", 3);
//    Node::GetDomain("1.2.3.4.5", 4);
//
//    Node::IsDomain("1.2.3.0");
//    Node::IsDomain("1.2.3.1");
//
//    Node::IsAddressOfDomain("12.3.4", "12.3.4");
//    Node::IsAddressOfDomain("12.3.4.8", "12.3.4");
//    Node::IsAddressOfDomain("12.3.4", "12.3.4.0.0.0");

    tScenario = new Scenario();

    // ### domains ###
    Coordinator *tCoord0_1_1_1 = tScenario->AddDomain3("1.1.1", (-3) * NODE_SHAPE_WIDTH + (-8) * NODE_SHAPE_WIDTH, (-3) * NODE_SHAPE_HEIGHT);
    Coordinator *tCoord0_1_1_2 = tScenario->AddDomain3("1.1.2", (+3) * NODE_SHAPE_WIDTH + (-8) * NODE_SHAPE_WIDTH, (-3) * NODE_SHAPE_HEIGHT);
    Coordinator *tCoord0_1_1_3 = tScenario->AddDomain3("1.1.3", (0) * NODE_SHAPE_WIDTH + (-8) * NODE_SHAPE_WIDTH, (+3) * NODE_SHAPE_HEIGHT);

    Coordinator *tCoord1_1_1 = tScenario->AddCoordinator("1.1.1.1", 1);
    tCoord1_1_1->AddChildCoordinator(tCoord0_1_1_1);
    tCoord1_1_1->AddChildCoordinator(tCoord0_1_1_2);
    tCoord1_1_1->AddChildCoordinator(tCoord0_1_1_3);


    Coordinator *tCoord0_1_2_1 = tScenario->AddDomain3("1.2.1", (-3) * NODE_SHAPE_WIDTH + (8) * NODE_SHAPE_WIDTH, (-3) * NODE_SHAPE_HEIGHT);
    Coordinator *tCoord0_1_2_2 = tScenario->AddDomain3("1.2.2", (+3) * NODE_SHAPE_WIDTH + (8) * NODE_SHAPE_WIDTH, (-3) * NODE_SHAPE_HEIGHT);
    Coordinator *tCoord0_1_2_3 = tScenario->AddDomain3("1.2.3", (0) * NODE_SHAPE_WIDTH + (8) * NODE_SHAPE_WIDTH, (+3) * NODE_SHAPE_HEIGHT);

    Coordinator *tCoord1_1_2 = tScenario->AddCoordinator("1.2.1.1", 1);
    tCoord1_1_2->AddChildCoordinator(tCoord0_1_2_1);
    tCoord1_1_2->AddChildCoordinator(tCoord0_1_2_2);
    tCoord1_1_2->AddChildCoordinator(tCoord0_1_2_3);

    Coordinator *tCoord2_1 = tScenario->AddCoordinator("1.2.1.1", 2);
    tCoord2_1->AddChildCoordinator(tCoord1_1_1);
    tCoord2_1->AddChildCoordinator(tCoord1_1_2);

    // ### inter-domain links ###
    tScenario->AddLink("1.1.1.2", "1.1.2.1");
    tScenario->AddLink("1.1.2.3", "1.1.3.2");
    tScenario->AddLink("1.1.1.3", "1.1.3.1");

    tScenario->AddLink("1.2.1.2", "1.2.2.1");
    tScenario->AddLink("1.2.2.3", "1.2.3.2");
    tScenario->AddLink("1.2.1.3", "1.2.3.1");

    tScenario->AddLink("1.1.2.2", "1.2.1.1");
    tScenario->AddLink("1.1.3.3", "1.2.3.3");

    // ### DNS entries ###
    tScenario->registerName("Source", "1.1.1.1");
    tScenario->registerName("Destination", "1.2.1.2");
    tScenario->registerName("Destination1", "1.2.2.2");
    tScenario->registerName("Destination2", "1.2.3.2");

    tScenario->SetRootCoordinator(tCoord2_1);

    // ### routing update ###
    tScenario->UpdateRouting();
//    tScenario->UpdateRouting();
//    tScenario->UpdateRouting();

    return tScenario;
}

void Scenario::UpdateRouting()
{
    Cep::LockGlobalForwarding();
    if (mRootCoordinator != NULL)
        mRootCoordinator->UpdateRouting();
    Cep::UnlockGlobalForwarding();
}

Cep* Scenario::AddServerCep(enum TransportType pTransportType, Name pNodeName, unsigned pPort)
{
    // get reference to the right node
    Node *tNode;

    if (mDestinationNode != NULL)
        tNode = mDestinationNode;
    else
        tNode = FindNode(pNodeName.toString());

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

Cep* Scenario::AddClientCep(enum TransportType pTransportType, Name pTargetNodeIdentifier, unsigned pTargetPort)
{
    // get reference to the right node
    Node *tNode = mSourceNode;
    if (tNode == NULL)
    {
        LOG(LOG_ERROR, "Source node name \"%s\" is unknown to the simulation", pTargetNodeIdentifier.toString().c_str());
        return NULL;
    }

    if (mDestinationNode != NULL)
        pTargetNodeIdentifier = mDestinationNode->GetAddress();

    // create CEP
    Cep* tCep = tNode->AddClient(pTransportType, pTargetNodeIdentifier.toString(), pTargetPort);

    mClientCepsMutex.lock();
    mClientCeps.push_back(tCep);
    mClientCepsMutex.unlock();

    return tCep;
}

bool Scenario::DeleteClientCep(Name pNodeName, unsigned int pPort)
{
    bool tResult = false;

    mClientCepsMutex.lock();
    if (mClientCeps.size() > 0)
    {
        Ceps::iterator tIt;
        for (tIt = mClientCeps.begin(); tIt != mClientCeps.end(); tIt++)
        {
            //LOG(LOG_VERBOSE, "Comparing CEP with local port %u and desired port %u", (*tIt)->GetLocalPort(), pPort);
            if (((*tIt)->GetLocalPort() == pPort) && ((*tIt)->GetLocalNode() == pNodeName.toString()))
            {
                LOG(LOG_VERBOSE, "Deleting client at %s:%u", pNodeName.toString().c_str(), pPort);
                mClientCeps.erase(tIt);
                LOG(LOG_VERBOSE, "New stream count: %d", mClientCeps.size());
                tResult = true;
                break;
            }
        }
    }
    mClientCepsMutex.unlock();

    return tResult;
}

Streams Scenario::GetStreams()
{
    Streams tResult;
    tResult.clear();

    mClientCepsMutex.lock();
    Ceps::iterator tIt;
    StreamDescriptor tEntry;
    for(tIt = mClientCeps.begin(); tIt != mClientCeps.end(); tIt++)
    {
        tEntry.PacketCount = (*tIt)->GetPacketCount();
        tEntry.QoSRequs = (*tIt)->GetQoS();
        tEntry.QoSRes = (*tIt)->GetQoSResults();
        tEntry.LocalNode = (*tIt)->GetLocalNode();
        tEntry.LocalPort = (*tIt)->GetLocalPort();
        tEntry.PeerNode = (*tIt)->GetPeerNode();
        tEntry.PeerPort = (*tIt)->GetPeerPort();
        tEntry.Id = (*tIt)->GetStreamId();
        tResult.push_back(tEntry);
    }
    mClientCepsMutex.unlock();

    return tResult;
}

void Scenario::SetSourceNode(std::string pAddress)
{
    mClientCepsMutex.lock();

    LOG(LOG_VERBOSE, "Setting source node to %s", pAddress.c_str());
    Node *tSource = FindNode(pAddress);
    mSourceNode = tSource;

    mClientCepsMutex.unlock();
}

void Scenario::SetDestinationNode(std::string pAddress)
{
    mClientCepsMutex.lock();

    LOG(LOG_VERBOSE, "Setting destination node to %s", pAddress.c_str());
    Node *tSource = FindNode(pAddress);
    mDestinationNode = tSource;

    mClientCepsMutex.unlock();
}

Domains Scenario::GetDomains()
{
    Domains tResult;

    mDomainsMutex.lock();
    tResult = mDomains;
    mDomainsMutex.unlock();

    return tResult;
}

Nodes Scenario::GetNodes()
{
    Nodes tResult;

    mNodesMutex.lock();
    tResult = mNodes;
    mNodesMutex.unlock();

    return tResult;
}

Links Scenario::GetLinks()
{
    Links tResult;

    mLinksMutex.lock();
    tResult = mLinks;
    mLinksMutex.unlock();

    return tResult;
}

Coordinator* Scenario::GetRootCoordinator()
{
    return mRootCoordinator;
}

Coordinator* Scenario::AddDomain(std::string pDomainPrefix, int pNodeCount, int pPosXHint, int pPosYHint)
{
    Nodes tNodes;
    Node *tNode;

    // create domain
    Domain *tDomain = new Domain(pDomainPrefix);

    // create all nodes
    for(int i = 1; i < pNodeCount + 1; i++)
    {
        string tCurAddr = pDomainPrefix + '.' + toString(i);
        tNode = AddNode("", tCurAddr);
        if (tNode == NULL)
        {
            LOG(LOG_ERROR, "Failed to create node");
            return NULL;
        }
        tNodes.push_back(tNode);

        // add node to domain
        tDomain->AddNode(tNode);
    }

    // create a full meshed network within domain: we do not
    for (int i = 1; i < pNodeCount; i++)
    {
        for (int j = i + 1; j < pNodeCount + 1; j++)
        {
            if (!AddLink(pDomainPrefix + '.' + toString(i), pDomainPrefix + '.' + toString(j)))
            {
                LOG(LOG_ERROR, "Failed to create link");
                return NULL;
            }
        }
    }

    // store domain
    mDomainsMutex.lock();
    mDomains.push_back(tDomain);
    mDomainsMutex.unlock();

    // create coordinator for this domain
    Coordinator *tCoordinator = AddCoordinator(pDomainPrefix + ".1", 0);
    if (tCoordinator == NULL)
    {
        LOG(LOG_ERROR, "Failed to create coordinator");
        return NULL;
    }

    // inform coordinator about nodes
    Nodes::iterator tIt;
    for(tIt = tNodes.begin(); tIt != tNodes.end(); tIt++)
    {
        tCoordinator->AddClusterMember(*tIt);
    }

    return tCoordinator;
}

Coordinator* Scenario::AddDomain3(std::string pDomainPrefix, int pPosXHint, int pPosYHint)
{
    Nodes tNodes;
    Node *tNode;

    // create domain
    Domain *tDomain = new Domain(pDomainPrefix);

    // create all nodes
    for(int i = 1; i < 3 + 1; i++)
    {
        string tCurAddr = pDomainPrefix + '.' + toString(i);
        switch(i)
        {
            case 1:
                tNode = AddNode("", tCurAddr, pPosXHint + (NODE_SHAPE_WIDTH * (-1)), pPosYHint + (NODE_SHAPE_HEIGHT * (-1)));
                break;
            case 2:
                tNode = AddNode("", tCurAddr, pPosXHint + (NODE_SHAPE_WIDTH * (+1)), pPosYHint + (NODE_SHAPE_HEIGHT * (-1)));
                break;
            case 3:
                tNode = AddNode("", tCurAddr, pPosXHint + (NODE_SHAPE_WIDTH * (0)), pPosYHint + (NODE_SHAPE_HEIGHT * (+1)));
                break;
            default:
                tNode = AddNode("", tCurAddr);
                break;
        }
        if (tNode == NULL)
        {
            LOG(LOG_ERROR, "Failed to create node");
            return NULL;
        }
        tNodes.push_back(tNode);

        // add node to domain
        tDomain->AddNode(tNode);
    }

    // create a full meshed network within domain: we do not
    for (int i = 1; i < 3; i++)
    {
        for (int j = i + 1; j < 3 + 1; j++)
        {
            if (!AddLink(pDomainPrefix + '.' + toString(i), pDomainPrefix + '.' + toString(j)))
            {
                LOG(LOG_ERROR, "Failed to create link");
                return NULL;
            }
        }
    }

    // store domain
    mDomainsMutex.lock();
    mDomains.push_back(tDomain);
    mDomainsMutex.unlock();

    // create coordinator for this domain
    Coordinator *tCoordinator = AddCoordinator(pDomainPrefix + ".1", 0);
    if (tCoordinator == NULL)
    {
        LOG(LOG_ERROR, "Failed to create coordinator");
        return NULL;
    }

    // inform coordinator about nodes
    Nodes::iterator tIt;
    for(tIt = tNodes.begin(); tIt != tNodes.end(); tIt++)
    {
        tCoordinator->AddClusterMember(*tIt);
    }

    return tCoordinator;
}

Node* Scenario::AddNode(string pName, std::string pAddressHint, int pPosXHint, int pPosYHint)
{
    // do we already know this address?
    if (FindNode(pAddressHint) != NULL)
        return NULL;

    mNodesMutex.lock();
    Node *tNode = new Node(pName, pAddressHint, pPosXHint, pPosYHint);
    mNodes.push_back(tNode);
    mNodesMutex.unlock();

    // set first created node as source node
    if (mSourceNode == NULL)
        mSourceNode = tNode;

    return tNode;
}

Link* Scenario::AddLink(std::string pFromAddress, std::string pToAddress)
{
    Node *tFrom = FindNode(pFromAddress);
    Node *tTo = FindNode(pToAddress);

    if (tFrom == NULL)
    {
        LOG(LOG_ERROR, "Couldn't create link from %s to %s, source address does not name a node", pFromAddress.c_str(), pToAddress.c_str());
        return NULL;
    }
    if (tTo == NULL)
    {
        LOG(LOG_ERROR, "Couldn't create link from %s to %s, destination address does not name a node", pFromAddress.c_str(), pToAddress.c_str());
        return NULL;
    }

    Link *tLink = new Link(tFrom, tTo);

    mLinksMutex.lock();
    mLinks.push_back(tLink);
    mLinksMutex.unlock();

    return tLink;
}

Coordinator* Scenario::AddCoordinator(string pNodeAddress, int pHierarchyLevel)
{
    Coordinator *tResult = NULL;
    Node *tNode = FindNode(pNodeAddress);
    if (tNode != NULL)
    {
        tResult = tNode->SetAsCoordinator(pHierarchyLevel);
    }else
        LOG(LOG_ERROR, "Cannot create a coordinator instance on node %s", pNodeAddress.c_str());

    return tResult;
}

Node* Scenario::FindNode(std::string pNodeIdentifier)
{
    Node* tResult = NULL;
    Nodes::iterator tIt;

    //TODO: support names and do not map 1:1

    pNodeIdentifier = DNS.query(pNodeIdentifier);

    mNodesMutex.lock();

    if (mNodes.size() > 0)
    {
        for (tIt = mNodes.begin(); tIt != mNodes.end(); tIt++)
        {
            //LOG(LOG_VERBOSE, "Comparing node ID %s with node %s", pNodeIdentifier.c_str(), (*tIt)->GetAddress().c_str());
            if ((*tIt)->GetAddress() == pNodeIdentifier)
            {
                tResult = *tIt;
                break;
            }
        }
    }

    mNodesMutex.unlock();

    return tResult;
}

bool Scenario::registerName(string pName, string pAddress)
{
    bool tResult = DNS.registerName(pName, pAddress);
    Node *tNode = FindNode(pAddress);
    if (tNode != NULL)
        tNode->SetName(pName);

    return tResult;
}

void Scenario::SetRootCoordinator(Coordinator *pCoordinator)
{
    LOG(LOG_VERBOSE, "Setting coordinator %s as root", pCoordinator->GetClusterAddress().c_str());
    mRootCoordinator = pCoordinator;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
