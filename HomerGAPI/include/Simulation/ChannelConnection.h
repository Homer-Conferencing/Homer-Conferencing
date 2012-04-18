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
 * Purpose: ChannelConnection
 * Author:  Thomas Volkert
 * Since:   2012-04-14
 */

#ifndef _GAPI_CHANNEL_CONNECTION_
#define _GAPI_CHANNEL_CONNECTION_

#include <HBSocket.h>

#include <Requirements.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class ChannelConnection:
	public IConnection
{
public:
    ChannelConnection(std::string pTarget, Requirements *pRequirements);
    virtual ~ChannelConnection( );

    virtual bool isClosed();
    virtual int availableBytes();
    virtual void read(char* pBuffer, int &pBufferSize); //TODO: support non blocking mode
    virtual void write(char* pBuffer, int pBufferSize);
    virtual bool getBlocking();
    virtual void setBlocking(bool pState);
    virtual void cancel();
    virtual Name* getName();
    virtual Name* getRemoteName();
    virtual bool changeRequirements(Requirements *pRequirements);
    virtual Requirements getRequirements();
    virtual Events getEvents();

private:
    bool		mBlockingMode;
    Requirements mRequirements;
//    int 		mSocketHandle;
//    Socket		*mSocket;
    bool        mIsClosed;
    std::string mTarget;
//    unsigned int mTargetPort;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
