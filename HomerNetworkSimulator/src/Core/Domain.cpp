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
 * Purpose: Implementation of simulated broadcast domains
 * Author:  Thomas Volkert
 * Since:   2012-06-10
 */

#include <Core/Domain.h>
#include <Core/Node.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Domain::Domain(string pDomainAddress)
{
    mDomainAddress = pDomainAddress;
    LOG(LOG_INFO, "Created domain %s", mDomainAddress.c_str());
}

Domain::~Domain()
{

}

///////////////////////////////////////////////////////////////////////////////

string Domain::GetDomainAddress()
{
    return mDomainAddress;
}

void Domain::AddNode(Node *pNode)
{
    mNodesMutex.lock();
    mNodes.push_back(pNode);
    mNodesMutex.unlock();
}

NodeList Domain::GetNodes()
{
    NodeList tResult;

    mNodesMutex.lock();
    tResult = mNodes;
    mNodesMutex.unlock();

    return tResult;
}

Node* Domain::GetNode(int pIndex)
{
    Node *tResult = NULL;
    NodeList::iterator tIt;

    int tCount = 0;
    mNodesMutex.lock();
    for (tIt = mNodes.begin(); tIt != mNodes.end(); tIt++)
    {
        if (tCount == pIndex)
        {
            tResult = *tIt;
            break;
        }
        tCount++;
    }
    mNodesMutex.unlock();

    return tResult;
}

int Domain::GetNodeCount()
{
    int tResult = -1;

    mNodesMutex.lock();
    tResult = mNodes.size();
    mNodesMutex.unlock();

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
