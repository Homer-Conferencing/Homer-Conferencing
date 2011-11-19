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
 * Purpose: header for Java-Reflection for C++
 * Author:  Thomas Volkert
 * Since:   2011-03-10
 */

#ifndef _BASE_REFLECTION_
#define _BASE_REFLECTION_

#include <typeinfo>
#include <stdio.h>
#include <stdlib.h>

inline std::string ParseRawObjectName(std::string pRawName)
{
	std::string tResult = "";
	int tPos = 2; //ignore prefix "PN"
	int tRawNameLength = pRawName.length();

	for(;;)
	{
		int tSize = 0;
	    std::string tSizeStr = "";
		while ((pRawName[tPos] >= (char)'0') && (pRawName[tPos] <= (char)'9') && (tPos < tRawNameLength))
		{
			tSizeStr += pRawName[tPos];
			//LOG(LOG_ERROR, "# %s", tSizeStr.c_str());
			tPos++;
		}
		tSize = atoi(tSizeStr.c_str());
		//LOG(LOG_ERROR, "Size %d", tSize);
		if (tSize == 0)
			return tResult;
		tResult += pRawName.substr(tPos, tSize);

		// go to next entry within pRawName
		tPos += tSize;
		//LOG(LOG_ERROR, "%d %d", tPos, tRawNameLength);

		// are we at the end? (ignore "E" at the end of such a typeid string)
		if (tPos >= tRawNameLength -1)
			return tResult;
		else
			tResult += "::";
	}
	return "";
}

#define GetObjectNameRawStr(x)  (toString(typeid(x).name()))
#define GetObjectNameStr(x) (ParseRawObjectName(GetObjectNameRawStr(x)))

#ifdef LINUX
#endif

#ifdef WIN32
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class Reflection
{
public:
    Reflection( );

    virtual ~Reflection( );

private:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
