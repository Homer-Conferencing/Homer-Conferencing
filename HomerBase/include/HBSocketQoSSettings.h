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
 * Purpose: QoS settings support
 * Author:  Thomas Volkert
 * Since:   2011-10-20
 */

#ifndef _BASE_SOCKET_QOS_SETTINGS_
#define _BASE_SOCKET_QOS_SETTINGS_

#include <string>
#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

// QoS settings support
#define QOS_SETTINGS

#ifndef QOS_FEATURE_NONE
#define QOS_FEATURE_NONE									0x0000
#define QOS_FEATURE_LOSSLESS								0x0001
#endif

struct QoSSettings
{
    unsigned int DataRate; /* in KB/s */
    unsigned short int Delay:16; /* in ms */
    unsigned short int Features:16;
};

struct QoSProfileDescriptor
{
    std::string Name;
    QoSSettings Settings;
};

typedef std::list<QoSProfileDescriptor*> QoSProfileList;

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
