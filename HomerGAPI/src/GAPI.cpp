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
 * Purpose: Implementation of G-Lab API
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <SocketSetup.h>

#include <Logger.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

GapiService sGapiService;

///////////////////////////////////////////////////////////////////////////////

GapiService::GapiService()
{
	mSetupInterface = 0;
	mSetupInterfaceName = "";
	registerSetupInterface(new SocketSetup(), BERKEYLEY_SOCKETS);
}

GapiService::~GapiService()
{

}

GapiService& GapiService::GetInstance()
{
    return sGapiService;
}

///////////////////////////////////////////////////////////////////////////////

bool GapiService::registerSetupInterface(ISetup* pSetupInterface, std::string pName)
{
	bool tFound = false;
	SetupInterfacesPool::iterator tIt;


	mSetupInterfacesPoolMutex.lock();

	if(mSetupInterfacesPool.size() == 0)
	{
		mSetupInterface = pSetupInterface;
		mSetupInterfaceName = pName;
	}

	for(tIt = mSetupInterfacesPool.begin(); tIt != mSetupInterfacesPool.end(); tIt++)
	{
		if(pName == (*tIt)->Name)
		{
			tFound = true;
		}
	}

	if(!tFound)
	{
		SetupInterfaceDescription *tSetupInterfaceDescription = new SetupInterfaceDescription();
		tSetupInterfaceDescription->Interface = pSetupInterface;
		tSetupInterfaceDescription->Name = pName;
		mSetupInterfacesPool.push_back(tSetupInterfaceDescription);
	}else
	{
		LOG(LOG_WARN, "Setup interface already registered, ignoring registration request");
	}

	mSetupInterfacesPoolMutex.unlock();

	return !tFound;
}

bool GapiService::selectSetupInterface(std::string pName)
{
    bool tFound = false;
    SetupInterfacesPool::iterator tIt;


    mSetupInterfacesPoolMutex.lock();

    for(tIt = mSetupInterfacesPool.begin(); tIt != mSetupInterfacesPool.end(); tIt++)
    {
        if(pName == (*tIt)->Name)
        {
            LOG(LOG_VERBOSE, "Setup interface \"%s\" selected", pName.c_str());
            mSetupInterface = (*tIt)->Interface;
            mSetupInterfaceName = pName;
            tFound = true;
        }
    }

    mSetupInterfacesPoolMutex.unlock();

    return tFound;
}

string GapiService::currentSetupInterface()
{
    return mSetupInterfaceName;
}

SetupInterfacesNames GapiService::enumSetupInterfaces()
{
	SetupInterfacesNames tResult;
	SetupInterfacesPool::iterator tIt;

	mSetupInterfacesPoolMutex.lock();

	for(tIt = mSetupInterfacesPool.begin(); tIt != mSetupInterfacesPool.end(); tIt++)
	{
		tResult.push_back((*tIt)->Name);
	}

	mSetupInterfacesPoolMutex.unlock();

	return tResult;
}

ISubscription* GapiService::subscribe(IName *pName, Requirements *pRequirements)
{
	LOG(LOG_VERBOSE, "Got call to GAPI::subscribe() with name \"%s\" and requirements \"%s\"", pName->toString().c_str(), pRequirements->getDescription().c_str());

	if(mSetupInterface == 0)
	{
		LOG(LOG_ERROR, "No setup interface available");
		return 0;
	}

	return mSetupInterface->subscribe(pName, pRequirements);
}

IRegistration* GapiService::publish(IName *pName, Requirements *pRequirements)
{
	LOG(LOG_VERBOSE, "Got call to GAPI::publish() with name \"%s\" and requirements \"%s\"", pName->toString().c_str(), pRequirements->getDescription().c_str());

	if(mSetupInterface == 0)
	{
		LOG(LOG_ERROR, "No setup interface available");
		return 0;
	}

	return mSetupInterface->publish(pName, pRequirements);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
