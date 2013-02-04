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
 * Purpose: Extension for sys/socket.h
 * Author:  Thomas Volkert
 * Since:   2011-10-20
 */

#ifndef _BASE_SYS_SOCKET_EXT_
#define _BASE_SYS_SOCKET_EXT_

#if defined(LINUX) || defined(APPLE)
#include <sys/socket.h>
#endif

#include <Header_Windows.h>

#include <string.h>
#include <Logger.h>
#include <HBSocketQoSSettings.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

// overview of IP options: http://www.iana.org/assignments/ip-parameters
// structure of IP options: http://www.tcpipguide.com/free/t_IPDatagramOptionsandOptionFormat.htm
struct QoSIpOption{
    unsigned char Number:5;         // ID of this option indicating its purpose
    unsigned char Class:2;          // "0" for control option, "2" for debugging/measurement
    unsigned char Copied:1;         // "1" to copy this option to all fragments of a packet
    unsigned char Length;           // length of the entire IP option

    unsigned char Pointer:8;          // time stamp specific
    unsigned char Flags:8;            // time stamp specific
    unsigned int  InternetAddress;  // time stamp specific

    struct QoSSettings Settings;
};

// define setqos if is not defined by linked libs
#ifndef setqos
inline int setqos(int pFd, unsigned int pDataRate, unsigned short int pDelay, unsigned short int pFeatures)
{
    int tResult = -1;

    #ifdef QOS_SETTINGS
        struct QoSIpOption tQoSIpOption;
        memset(&tQoSIpOption, 0, sizeof(tQoSIpOption));
        tQoSIpOption.Copied = false; // don't copy this option to every fragment
        tQoSIpOption.Class = 2; // measurement
        tQoSIpOption.Number = 4; // time stamps
        tQoSIpOption.Length = sizeof(tQoSIpOption);

        tQoSIpOption.Pointer = 5; // smallest legal value
        tQoSIpOption.Flags = 0; // time stamps only, stored in consecutive 32-bit words
        tQoSIpOption.InternetAddress = 0;

        tQoSIpOption.Settings.DataRate = pDataRate;
        tQoSIpOption.Settings.Delay = pDelay;
        tQoSIpOption.Settings.Features = pFeatures;

        LOGEX(QoSIpOption, LOG_VERBOSE, "Setting IP options of %d bytes", tQoSIpOption.Length); // max. options of 40 bytes length possible

        if ((tResult = setsockopt(pFd, IPPROTO_IP, IP_OPTIONS, (char*)&tQoSIpOption, tQoSIpOption.Length)) < 0)
            LOGEX(QoSIpOption, LOG_ERROR, "Failed to set IP options for transmitting QoS settings for socket %d because \"%s\"(%d)", pFd, strerror(errno), tResult);
    #endif

    return tResult;
}
#endif

///////////////////////////////////////////////////////////////////////////////

}} //namespace

#endif
