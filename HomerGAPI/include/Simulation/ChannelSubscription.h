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
 * Purpose: ChannelSubscription
 * Author:  Thomas Volkert
 * Since:   2012-04-14
 */

#ifndef _GAPI_CHANNEL_SUBSCRIPTION_
#define _GAPI_CHANNEL_SUBSCRIPTION_

#include <HBSocket.h>

#include <Requirements.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class ChannelSubscription:
	public ISubscription
{
public:
    ChannelSubscription(std::string pTarget, Requirements *pRequirements);
    virtual ~ChannelSubscription( );

    virtual bool isClosed();
    virtual void read(char* pBuffer, int &pBufferSize);
    virtual void write(char* pBuffer, int pBufferSize);
    virtual void cancel();
    virtual Name* name();
    virtual Name* peer();
    virtual bool update(Requirements *pRequirements);

private:
//    int 		mSocketHandle;
//    Socket		*mSocket;
    bool        mIsClosed;
    std::string mTarget;
//    unsigned int mTargetPort;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
