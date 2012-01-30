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
 * Purpose: G-Lab API: proof-of-concept of the definition from the current official paper
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#ifndef _GAPI_
#define _GAPI_

#include <HBMutex.h>

#include <ISetup.h>

#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

#define BERKEYLEY_SOCKETS           "Berkeley Sockets"

///////////////////////////////////////////////////////////////////////////////

#define GAPI GapiService::GetInstance()

struct SetupInterfaceDescription{
	ISetup			*Interface;
	std::string 	Name;
};

typedef std::list<SetupInterfaceDescription*> SetupInterfacesPool;
typedef std::list<std::string> SetupInterfacesNames;

// the following de/activates debugging of received packets
//#define GAPI_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class GapiService:
	public ISetup
{
public:
	GapiService();
    virtual ~GapiService( );

    static GapiService& GetInstance();

    /* plugin system for different ISetup implementations */
    bool registerSetupInterface(ISetup* pSetupInterface, std::string pName);
    bool selectSetupInterface(std::string pName);
    std::string currentSetupInterface();
    SetupInterfacesNames enumSetupInterfaces();

    /* ISetup */
    virtual ISubscription* subscribe(IName *pName, Requirements *pRequirements = 0);
    virtual IRegistration* publish(IName *pName, Requirements *pRequirements = 0);

private:
    ISetup						*mSetupInterface;
    std::string                 mSetupInterfaceName;
    SetupInterfacesPool			mSetupInterfacesPool;
    Mutex						mSetupInterfacesPoolMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
