/*
 * Name:    Mutex.C
 * Purpose: Implementation of wrapper for os independent random number generator
 * Author:  Thomas Volkert
 * Since:   2010-09-28
 * Version: $Id: HBRandom.cpp 6 2011-08-22 13:06:22Z silvo $
 */

#include <HBRandom.h>
#include <HBTime.h>

#include <stdlib.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Random::Random()
{
}

Random::~Random()
{
}

///////////////////////////////////////////////////////////////////////////////

unsigned long Random::GenerateNumber()
{
	static bool sFirstStart = true;

	// if first start then init random number generator
	if (sFirstStart)
	{
		sFirstStart = false;
		#ifdef LINUX
				srandom((unsigned int)Time::GetTimeStamp());
		#endif
		#ifdef WIN32
				srand((unsigned)Time::GetTimeStamp());
		#endif
	}

	// generate random number
	#ifdef LINUX
		return random();
	#endif
	#ifdef WIN32
		return rand();
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
