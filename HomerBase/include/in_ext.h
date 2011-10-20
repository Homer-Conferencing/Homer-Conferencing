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
 * Name:    in_ext.h
 * Purpose: Extension for netinet/in.h for MinGw environments
 * Author:  Thomas Volkert
 * Since:   2011-10-20
 * Version: $Id: HBSocket.cpp 80 2011-10-19 12:13:20Z silvo $
 */

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

// definitions for UDP-Lite
// HINT: homepage for UDP-Lite is http://www.erg.abdn.ac.uk/users/gerrit/udp-lite/
#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE  						        136
#endif

#ifndef UDPLITE_SEND_CSCOV
#define UDPLITE_SEND_CSCOV     				             10
#endif

#ifndef UDPLITE_RECV_CSCOV
#define UDPLITE_RECV_CSCOV     		                     11
#endif

//define IPV6_V6ONLY for MinGW/MSYS
//HINT: strange API because Windows screams for RFCs 3493 and 3542 meanwhile Linux takes another way by defining IPV6_ONLY with value 26
#ifdef __MINGW32__
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY									     27
#endif
#endif

///////////////////////////////////////////////////////////////////////////////

}} //namespace
