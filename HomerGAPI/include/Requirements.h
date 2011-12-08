/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Purpose: Requirements
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#ifndef _GAPI_IREQUIREMENTS_
#define _GAPI_IREQUIREMENTS_

#include <IRequirement.h>
#include <HBMutex.h>

#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

typedef std::list<IRequirement*> RequirementSet;

///////////////////////////////////////////////////////////////////////////////

class Requirements
{
public:
    Requirements();
    virtual ~Requirements();

    virtual std::string toString();

    void operator+=(IRequirement *pAddRequ);
    void add(IRequirement *pRequ);
    bool contains(IRequirement *pRequ);

private:
    RequirementSet      mRequirementSet;
    Mutex               mRequirementSetMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
