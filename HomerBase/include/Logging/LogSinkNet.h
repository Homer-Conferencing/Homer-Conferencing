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
 * Purpose: header file for log sink for net outputs
 * Author:  Thomas Volkert
 * Since:   2011-07-29
 */

#ifndef _LOGGER_LOG_SINK_NET_
#define _LOGGER_LOG_SINK_NET_

#include <string>
#include <LogSink.h>
#include <HBSocket.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSinkNet:
    public LogSink
{
public:

    LogSinkNet(std::string pTargetHost, unsigned short pTargetPort);

    /// The destructor
    virtual ~LogSinkNet();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

private:
    std::string         mTargetHost;
    unsigned short      mTargetPort;
    Socket 				*mDataSocket;
    bool                mBrokenPipe;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
