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
 * Name:    socket_ext.h
 * Purpose: Extension for sys/socket.h
 * Author:  Thomas Volkert
 * Since:   2011-10-20
 * Version: $Id: HBSocket.cpp 80 2011-10-19 12:13:20Z silvo $
 */

#include <sys/socket.h>

#include <Logger.h>
#include <HBSocketQoSSettings.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

// define setqos if is not defined by linked libs
#ifndef setqos
int setqos(int pFd, unsigned int pDataRate, unsigned int pDelay, unsigned int pFeatures)
{
    int tResult = -1;

    #ifdef QOS_SETTINGS
        struct QoSSettings tQoSSettings;
        tQoSSettings.MinDataRate = pDataRate;
        tQoSSettings.MaxDelay = pDelay;
        tQoSSettings.Features = pFeatures;

        if ((tResult = setsockopt(pFd, IPPROTO_IP, IP_OPTIONS, (char*)&tQoSSettings, sizeof(QoSSettings))) < 0)
            LOGEX(Socket, LOG_ERROR, "Failed to set IP options for transmitting QoS settings for socket %d because \"%s\"", pFd, strerror(errno));
    #endif

    return tResult;
}
#endif

///////////////////////////////////////////////////////////////////////////////

}} //namespace
