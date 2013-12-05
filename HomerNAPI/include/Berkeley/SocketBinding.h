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
 * Purpose: SocketBinding
 * Since:   2012-05-19
 */

#ifndef _NAPI_SOCKET_BINDING_
#define _NAPI_SOCKET_BINDING_

#include <HBSocket.h>
#include <HBMutex.h>

#include <Requirements.h>

#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class SocketBinding:
	public IBinding
{
public:
	SocketBinding(std::string pLocalName, Requirements *pRequirements);
    virtual ~SocketBinding( );

    virtual bool isClosed();
    virtual IConnection* readConnection();
    virtual Name* getName();
    virtual void cancel();
    virtual bool changeRequirements(Requirements *pRequirements);
    virtual Requirements* getRequirements();
    virtual Events getEvents();

private:
    IConnection*    mConnection; // we support only one association
    Requirements	*mRequirements;
    Socket		    *mSocket;
    bool            mIsClosed;
    std::string     mLocalHost;
    unsigned int    mLocalPort;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
