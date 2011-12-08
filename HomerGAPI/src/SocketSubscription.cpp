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
 * Purpose: SocketSubscription
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <SocketSubscription.h>
#include <HBsocket.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

SocketSubscription::SocketSubscription(IRequirements *pRequirements)
{
//	mSocket = new Socket(SOCKET_IPv6)
}

SocketSubscription::~SocketSubscription()
{

}

///////////////////////////////////////////////////////////////////////////////

bool SocketSubscription::isClosed()
{
	//TODO
	return true;
}

void SocketSubscription::read(char* pBuffer, int &pBufferize)
{
	//TODO
}

void SocketSubscription::write(char* pBuffer, int pBufferSize)
{
	//TODO
}

void SocketSubscription::cancel()
{
	//TODO
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
