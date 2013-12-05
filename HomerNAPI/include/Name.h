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
 * Purpose: Name
 * Since:   2011-12-08
 */

#ifndef _NAPI_NAME_
#define _NAPI_NAME_

#include <string>

namespace Homer { namespace Base {

//////////////////////////////////////////////////////////////////////////////

class Name
{
public:
	Name(std::string pName = ""):mName(pName){ }
    virtual ~Name(){ }

    virtual std::string toString(){ return mName; }

private:
    std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
