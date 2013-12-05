/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Since:   2010-09-28
 */

#include <HBRandom.h>
#include <HBTime.h>

#include <stdlib.h>

namespace Homer { namespace Base {

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
		#if defined(LINUX) || defined(APPLE) || defined(BSD)
				srandom((unsigned int)Time::GetTimeStamp());
		#endif
		#if defined(WINDOWS)
				srand((unsigned)Time::GetTimeStamp());
		#endif
	}

	// generate random number
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		return random();
	#endif
	#if defined(WINDOWS)
		return rand();
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
