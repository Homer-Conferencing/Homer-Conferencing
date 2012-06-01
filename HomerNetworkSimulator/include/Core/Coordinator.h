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

class Node;

class Coordinator
{
public:
    Coordinator(Node *pNode, std::string pClusterName, int pHierarchyLevel);
    virtual ~Coordinator();

    void AddClusterMember(Node *pNode);

private:
    int             mHierarchyLevel;
    std::string     mClusterName;
    Node            *mNode;

    NodeList        mClusterMembers;
    Mutex           mClusterMembersMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
