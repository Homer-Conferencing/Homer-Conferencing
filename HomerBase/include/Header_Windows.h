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
 * Purpose: Windows includes, compatibility definitions for VC
 * Author:  Thomas Volkert
 * Since:   2011-12-20
 */

// HINT: this abstractino of header includes is needed to have a defined order how we include the headers of Windows (esp. for WinSock2.h and Windows.h)
#ifndef _HEADER_WINDOWS_
#define _HEADER_WINDOWS_

///////////////////////////////////////////////////////////////////////////////

// use secured functions instead of old function for string handling
#ifdef _MSC_VER

// make sure the 64 bit version is marked as active
#ifndef WIN64
#define WIN64
#endif

#endif

// include all needed headers for 32 bit and 64 bit Windows
#if defined(WIN32) || defined(WIN64)

// activate some additional definitions within Windows includes
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h> // has to be included before Windows.h
#include <windows.h>
#include <ws2tcpip.h>
#include <io.h>
#include <BaseTsd.h>
#include <sys/timeb.h>
#include <stdio.h>
#include <Psapi.h>
#include <Tlhelp32.h>
#include <stdlib.h>

// additional definitions for compatibility with gcc
#ifndef ssize_t
#define ssize_t SSIZE_T
#endif

#ifndef __const
#define __const
#endif

#endif

///////////////////////////////////////////////////////////////////////////////

#endif
