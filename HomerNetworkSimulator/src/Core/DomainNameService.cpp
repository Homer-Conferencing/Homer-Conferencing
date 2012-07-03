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
 * Purpose: Implementation of simulated links
 * Author:  Thomas Volkert
 * Since:   2012-05-31
 */

#include <Core/DomainNameService.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

DomainNameService sDomainNameService;

///////////////////////////////////////////////////////////////////////////////

DomainNameService::DomainNameService()
{

}

DomainNameService::~DomainNameService()
{

}

DomainNameService& DomainNameService::getInstance()
{
    return sDomainNameService;
}

///////////////////////////////////////////////////////////////////////////////

bool DomainNameService::registerName(string pName, string pAddress)
{
    DnsEntry tEntry;
    tEntry.Name = pName;
    tEntry.Address = pAddress;

    LOG(LOG_VERBOSE, "Registering DNS entry (\"%s\", %s)", pName.c_str(), pAddress.c_str());

    mDnsMappingMutex.lock();
    mDnsMapping.push_back(tEntry);
    mDnsMappingMutex.unlock();

    return true;
}

string DomainNameService::query(string pName)
{
    string tResult = "";
    bool tFound = false;

    if (pName == "")
        return tResult;

    mDnsMappingMutex.lock();
    if (mDnsMapping.size() > 0)
    {
        DnsMappingList::iterator tIt;
        for (tIt = mDnsMapping.begin(); tIt != mDnsMapping.end(); tIt ++)
        {
            if (tIt->Name == pName)
            {
                tResult = tIt->Address;
                tFound = true;
                break;
            }
        }
    }

    // if we haven't found an entry we fall back to original name (maybe it was the address?)
    if (!tFound)
    {
        tResult = pName;
    }

    mDnsMappingMutex.unlock();

    //LOG(LOG_VERBOSE, "DNS lookup for %s resulted in %s", pName.c_str(), tResult.c_str());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
