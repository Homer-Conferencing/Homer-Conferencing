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
 * Purpose: Socket control service
 * Author:  Thomas Volkert
 * Since:   2011-11-16
 */

#ifndef _BASE_SOCKET_CONTROL_SERVICE_
#define _BASE_SOCKET_CONTROL_SERVICE_

#include <HBMutex.h>
#include <HBSocket.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

#define SVC_SOCKET_CONTROL SocketControlService::GetInstance()

typedef std::list<Socket*>  SocketsList;

///////////////////////////////////////////////////////////////////////////////

class SocketControlService
{
public:
    /// The default constructor
	SocketControlService();

    /// The destructor.
    virtual ~SocketControlService();

    static SocketControlService& GetInstance();

    /* get client socket control */
    SocketsList GetClientSocketsControl();
    void ReleaseClientSocketsControl();

    /* registration interface */
    Socket* RegisterClientSocket(Socket *pSocket);
    bool UnregisterClientSocket(Socket *pSocket);

private:
    SocketsList         mClientSockets;
    Mutex 		        mClientSocketsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
