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
 * Purpose: NAPI
 * Since:   2011-12-08
 */

#ifndef _NAPI_
#define _NAPI_

#include <HBMutex.h>

#include <Name.h>
#include <ISetup.h>
#include <IConnection.h>
#include <IBinding.h>

#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

#define NAPI NAPIService::getInstance()

struct SetupInterfaceDescription{
	ISetup			*Interface;
	std::string 	Name;
};

typedef std::list<SetupInterfaceDescription> SetupInterfacesPool;
typedef std::list<std::string> SetupInterfacesNames;

// the following de/activates debugging of received packets
//#define NAPI_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class NAPIService:
	public ISetup
{
public:
	NAPIService();
    virtual ~NAPIService( );

    static NAPIService& getInstance();

    /* plugin system for different ISetup implementations */
    bool registerImpl(ISetup* pSetupInterface, std::string pName);
    bool selectImpl(std::string pName);
    std::string getCurrentImplName();
    SetupInterfacesNames getAllImplNames();

    /* ISetup */
    virtual IConnection* connect(Name *pName, Requirements *pRequirements = 0);
    virtual IBinding* bind(Name *pName, Requirements *pRequirements = 0);
    virtual Requirements getCapabilities(Name *pName, Requirements *pImportantRequirements = 0);

private:
    ISetup						*mSetupInterface;
    std::string                 mSetupInterfaceName;
    SetupInterfacesPool			mSetupInterfacesPool;
    Mutex						mSetupInterfacesPoolMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
