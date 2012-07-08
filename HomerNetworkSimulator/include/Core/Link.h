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
 * Purpose: Link
 * Author:  Thomas Volkert
 * Since:   2012-05-30
 */

#ifndef _GAPI_SIMULATION_LINK_
#define _GAPI_SIMULATION_LINK_

#include <Core/Cep.h>

#include <HBMutex.h>
#include <Name.h>

#include <vector>

namespace Homer { namespace Base {

class Link;
typedef std::vector<Link*> Links;

class Node;

///////////////////////////////////////////////////////////////////////////////

class Link
{
public:
    Link(Node *pNodeOne, Node *pNodeTwo);
    virtual ~Link();

    bool HandlePacket(Packet *pPacket, Node* pLastNode);

    /* QoS settings */
    void SetQoSCapabilities(QoSSettings pCaps);
    QoSSettings GetQoSCapabilities();

    Node* GetNode0();
    Node* GetNode1();
    Node* GetPeerNode(Node *pNode);

    /* statistics */
    int GetPacketCount();
    int GetLostPacketCount();

    std::vector<int> GetSeenStreams();
private:
    Node            *mNodes[2];
    QoSSettings     mQoSCapabilities;
    std::vector<int> mSeenStreams;
    Mutex           mSeenStreamsMutex;
    int             mPacketCount;
    int             mPacketLossCount;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
