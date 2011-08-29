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
 * Name:    System.h
 * Purpose: header for system specific and unspecific basic functions
 * Author:  Thomas Volkert
 * Since:   2011-02-25
 * Version: $Id$
 */

#ifndef _BASE_SYSTEM_
#define _BASE_SYSTEM_

#include <string.h>

#ifdef LINUX
#endif

#ifdef WIN32
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class System
{
public:
    System( );

    virtual ~System( );

    static bool GetWindowsKernelVersion(int &pMajor, int &pMinor);
    static std::string GetLinuxKernelVersion();
private:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
