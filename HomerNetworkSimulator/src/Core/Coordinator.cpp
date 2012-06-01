/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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

Coordinator::Coordinator(Node *pNode, string pClusterName, int pHierarchyLevel)
{
    mHierarchyLevel = pHierarchyLevel;
    mNode = pNode;
    mClusterName = pClusterName;

    LOG(LOG_INFO, "Setting node %s as coordinator for cluster %s and hierarchy level %d", mNode->GetAddress().c_str(), pClusterName.c_str(), pHierarchyLevel);
}

Coordinator::~Coordinator()
{
}

///////////////////////////////////////////////////////////////////////////////

void Coordinator::AddClusterMember(Node *pNode)
{
    LOG(LOG_INFO, "Adding node %s to cluster %s at hierarchy level %d", pNode->GetAddress().c_str(), mClusterName.c_str(), mHierarchyLevel);

    mClusterMembersMutex.lock();
    mClusterMembers.push_back(pNode);
    mClusterMembersMutex.unlock();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
