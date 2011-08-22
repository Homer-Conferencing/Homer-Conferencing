/*
 * Name:    Reflection.h
 * Purpose: header for Java-Reflection for C++
 * Author:  Thomas Volkert
 * Since:   2011-03-10
 * Version: $Id$
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
