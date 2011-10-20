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
 * Name:    Socket.h
 * Purpose: QoS settings support
 * Author:  Thomas Volkert
 * Since:   2011-10-20
 * Version: $Id: HBSocket.h 81 2011-10-19 12:16:35Z silvo $
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
#define QOS_FEATURE_NONE									0x00000000
#define QOS_FEATURE_LOSLESS									0x00000001
#endif

struct QoSSettings
{
    unsigned int MinDataRate; /* in KB/s */
    unsigned int MaxDelay; /* in ms */
    union{
        unsigned int Features;
        struct{
            bool Lossless; /* dropping allowed? */
        }__attribute__((__packed__))Feature;
    };
}__attribute__((__packed__));

struct QoSProfileDescriptor
{
    std::string Name;
    QoSSettings Settings;
};

typedef std::list<QoSProfileDescriptor*> QoSProfileList;

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
