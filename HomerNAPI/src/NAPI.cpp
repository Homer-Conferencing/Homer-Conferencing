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
 * Since:   2011-12-08
 */

#include <NAPI.h>
#include <Berkeley/SocketSetup.h>

#include <Logger.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

NAPIService sNAPIService;

///////////////////////////////////////////////////////////////////////////////

NAPIService::NAPIService()
{
	mSetupInterface = NULL;
	mSetupInterfaceName = "";
	registerImpl(new SocketSetup(), BERKEYLEY_SOCKETS);
}

NAPIService::~NAPIService()
{

}

NAPIService& NAPIService::getInstance()
{
    return sNAPIService;
}

///////////////////////////////////////////////////////////////////////////////

bool NAPIService::registerImpl(ISetup* pSetupInterface, std::string pName)
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
		if(pName == tIt->Name)
		{
			tFound = true;
		}
	}

	if(!tFound)
	{
		SetupInterfaceDescription tSetupInterfaceDescription;
		tSetupInterfaceDescription.Interface = pSetupInterface;
		tSetupInterfaceDescription.Name = pName;
		mSetupInterfacesPool.push_back(tSetupInterfaceDescription);
	}else
	{
		LOG(LOG_WARN, "Setup interface already registered, ignoring registration request");
	}

	mSetupInterfacesPoolMutex.unlock();

	return !tFound;
}

bool NAPIService::selectImpl(std::string pName)
{
    bool tFound = false;
    SetupInterfacesPool::iterator tIt;


    mSetupInterfacesPoolMutex.lock();

    for(tIt = mSetupInterfacesPool.begin(); tIt != mSetupInterfacesPool.end(); tIt++)
    {
        if(pName == tIt->Name)
        {
            LOG(LOG_VERBOSE, "Setup interface \"%s\" selected", pName.c_str());
            mSetupInterface = tIt->Interface;
            mSetupInterfaceName = pName;
            tFound = true;
        }
    }

    mSetupInterfacesPoolMutex.unlock();

    return tFound;
}

string NAPIService::getCurrentImplName()
{
    return mSetupInterfaceName;
}

SetupInterfacesNames NAPIService::getAllImplNames()
{
	SetupInterfacesNames tResult;
	SetupInterfacesPool::iterator tIt;

	mSetupInterfacesPoolMutex.lock();

	for(tIt = mSetupInterfacesPool.begin(); tIt != mSetupInterfacesPool.end(); tIt++)
	{
		tResult.push_back(tIt->Name);
	}

	mSetupInterfacesPoolMutex.unlock();

	return tResult;
}

IConnection* NAPIService::connect(Name *pName, Requirements *pRequirements)
{
	IConnection *tResult = NULL;

	LOG(LOG_VERBOSE, "Got call to NAPI::connect() with name \"%s\" and requirements \"%s\"", pName->toString().c_str(), (pRequirements != NULL) ? pRequirements->getDescription().c_str() : "none");

	if(mSetupInterface == NULL)
		LOG(LOG_ERROR, "No setup interface available");
	else
		tResult = mSetupInterface->connect(pName, pRequirements);

	return tResult;
}

IBinding* NAPIService::bind(Name *pName, Requirements *pRequirements)
{
	IBinding *tResult = NULL;

	LOG(LOG_VERBOSE, "Got call to NAPI::bind() with name \"%s\" and requirements \"%s\"", pName->toString().c_str(), (pRequirements != NULL) ? pRequirements->getDescription().c_str() : "none");

	if(mSetupInterface == NULL)
		LOG(LOG_ERROR, "No setup interface available");
	else
		tResult = mSetupInterface->bind(pName, pRequirements);

	return tResult;
}

Requirements NAPIService::getCapabilities(Name *pName, Requirements *pImportantRequirements)
{
	Requirements tResult;

	LOG(LOG_VERBOSE, "Got call to NAPI::getCapabilities() with name \"%s\" and requirements \"%s\"", pName->toString().c_str(), (pImportantRequirements != NULL) ? pImportantRequirements->getDescription().c_str() : "none");

	if(mSetupInterface == NULL)
		LOG(LOG_ERROR, "No setup interface available");
	else
		tResult = mSetupInterface->getCapabilities(pName, pImportantRequirements);

	return tResult;
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
