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
 * Name:    LogSinkNet.cpp
 * Purpose: Implementation of log sink for file output
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#include <LogSinkNet.h>
#include <Logger.h>

#include <string>
#include <stdio.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

LogSinkNet::LogSinkNet(string pTargetHost, unsigned short pTargetPort)
{
    mBrokenPipe = false;
    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
    mMediaId = "NET: " + mTargetHost + "<" + toString(mTargetPort) + ">";
    if ((mTargetHost != "") && (mTargetPort != 0))
        mDataSocket = new Socket(IS_IPV6_ADDRESS(mTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, SOCKET_UDP);
    printf("Logging debug output via IPv%d to net %s:%u\n", IS_IPV6_ADDRESS(mTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, mTargetHost.c_str(), mTargetPort);
}

LogSinkNet::~LogSinkNet()
{
}

///////////////////////////////////////////////////////////////////////////////

void LogSinkNet::ProcessMessage(int pLevel, string pTime, string pSource, int pLine, string pMessage)
{
    if (mDataSocket == NULL)
        return;

    if ((mTargetHost == "") || (mTargetPort == 0))
    {
        LOG(LOG_ERROR, "Remote network address invalid");
        return;
    }
    if (mBrokenPipe)
    {
        LOG(LOG_VERBOSE, "Skipped fragment transmission because of broken pipe");
        return;
    }

    string tData;
    tData += "LEVEL=" + toString(pLevel) + "\n";
    tData += "TIME=" + pTime + "\n";
    tData += "SOURCE=" + pSource + "\n";
    tData += "LINE=" + toString(pLine) + "\n";
    tData += "MESSAGE=" + pMessage + "\n";

    if (!mDataSocket->Send(mTargetHost, mTargetPort, (void*)tData.c_str(), (ssize_t)tData.size()))
    {
        LOG(LOG_ERROR, "Error when sending data through UDP socket to %s:%u, will skip further transmissions", mTargetHost.c_str(), mTargetPort);
        mBrokenPipe = true;
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
