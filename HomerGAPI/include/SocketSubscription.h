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
 * Purpose: ISubscription
 * Author:  Thomas Volkert
 * Since:   2011-12-08
 */

#ifndef _GAPI_SOCKET_SUBSCRIPTION_
#define _GAPI_SOCKET_SUBSCRIPTION_

#include <HBSocket.h>

#include <Requirements.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class SocketSubscription:
	public ISubscription
{
public:
	SocketSubscription(std::string pTargetHost, unsigned int pTargetPort, Requirements *pRequirements);
    virtual ~SocketSubscription( );

    virtual bool isClosed();
    virtual void read(char* pBuffer, int &pBufferSize);
    virtual void write(char* pBuffer, int pBufferSize);
    virtual void cancel();
    virtual IName* name();
    virtual IName* peer();
    virtual bool update(Requirements *pRequirements);

private:
    int 		mSocketHandle;
    Socket		*mSocket;
    bool        mIsClosed;
    std::string mTargetHost;
    unsigned int mTargetPort;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
