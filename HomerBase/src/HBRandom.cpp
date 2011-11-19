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
 * Purpose: Implementation of wrapper for os independent random number generator
 * Author:  Thomas Volkert
 * Since:   2010-09-28
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
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		return random();
	#endif
	#ifdef WIN32
		return rand();
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
