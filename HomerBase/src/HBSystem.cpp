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
 * Name:    System.cpp
 * Purpose: Implementation of several basic system functions
 * Author:  Thomas Volkert
 * Since:   2011-02-25
 * Version: $Id$
 */

#include <Logger.h>
#include <HBSystem.h>

#include <string.h>
#include <stdlib.h>

#ifdef LINUX
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

System::System()
{
}

System::~System()
{
}

///////////////////////////////////////////////////////////////////////////////

bool System::GetWindowsKernelVersion(int &pMajor, int &pMinor)
{
    pMajor = 0;
    pMinor = 0;

    #ifdef WIN32
        OSVERSIONINFOEX tVersionInfo;
        ZeroMemory(&tVersionInfo, sizeof(OSVERSIONINFOEX));
        tVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if (GetVersionEx((LPOSVERSIONINFO)&tVersionInfo) == 0)
        {
            LOGEX(System, LOG_ERROR, "Failed when calling \"GetVersionEx\"");
            return false;
        }
        pMajor = tVersionInfo.dwMajorVersion;
        pMinor = tVersionInfo.dwMinorVersion;
    #endif

    return true;
}

string System::GetLinuxKernelVersion()
{
    string tResult = "";

    #ifdef LINUX
        struct utsname tInfo;
        uname(&tInfo);

        if (tInfo.release != NULL)
            tResult = string(tInfo.release);
    #endif

    LOGEX(System, LOG_VERBOSE, "Found linux kernel \"%s\"", tResult.c_str());

    return tResult;
}

int System::GetMachineCores()
{
    int tResult = 1;
    #ifdef LINUX
        tResult = sysconf(_SC_NPROCESSORS_ONLN);

    #endif
    #ifdef WIN32
        SYSTEM_INFO tSysInfo;

        GetSystemInfo(&tSysInfo);

        tResult = tSysInfo.dwNumberOfProcessors;
    #endif

    LOGEX(System, LOG_VERBOSE, "Found machine cores: %d", tResult);

    return tResult;
}

/*
 * possible machine types:
 *          x86      => Intel x86 (32 bit)
 *          amd64    => AMD 64 - compatible to Intel x86 (64 bit)
 *          ia-64    => Itanium - not compatible to Intel x86 (64 bit)
 *          unknown  => misc. cases
 *
 */
string System::GetMachineType()
{
    string tResult = "unknown";

    #ifdef LINUX
        struct utsname tInfo;
        uname(&tInfo);

        if (tInfo.machine != NULL)
        {
            string tType = string(tInfo.machine);
            if(tType == "i686")
                tResult = "x86";
            if(tType == "x86_64")
                tResult = "amd64";
        }
    #endif
    #ifdef WIN32
        SYSTEM_INFO tSysInfo;

        GetSystemInfo(&tSysInfo);

        switch(tSysInfo.wProcessorArchitecture)
        {
            case PROCESSOR_ARCHITECTURE_AMD64:
                tResult = "amd64";
                break;
            case PROCESSOR_ARCHITECTURE_IA64:
                tResult = "ia-64";
                break;
            case PROCESSOR_ARCHITECTURE_INTEL:
                tResult = "x86";
                break;
            case PROCESSOR_ARCHITECTURE_UNKNOWN:
                LOG(LOG_VERBOSE, "Windows reported unknown machine architecture");
                tResult = "unknown";
        }
    #endif

    LOGEX(System, LOG_VERBOSE, "Found machine type \"%s\"", tResult.c_str());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
