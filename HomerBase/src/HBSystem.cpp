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
 * Purpose: Implementation of several basic system functions
 * Author:  Thomas Volkert
 * Since:   2011-02-25
 */

//HINT: http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

#include <Logger.h>
#include <HBSystem.h>

#include <string.h>
#include <stdlib.h>

#if defined(LINUX)
#include <sys/sysinfo.h>
#endif

#if defined(APPLE) || defined(BSD)
#include <sys/sysctl.h>
#include <CoreServices/CoreServices.h>
#endif

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include <Header_Windows.h>

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

	#if defined(WINDOWS)
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

string System::GetKernelVersion()
{
    static string tResult = "";

    if (tResult == "")
    {
		#if defined(LINUX) || defined(APPLE) || defined(BSD)
			struct utsname tInfo;
			uname(&tInfo);

			if (tInfo.release != NULL)
				tResult = string(tInfo.release);
		#endif
		#if defined(WINDOWS)
			OSVERSIONINFOEX tVersionInfo;
			ZeroMemory(&tVersionInfo, sizeof(OSVERSIONINFOEX));
			tVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
			if (GetVersionEx((LPOSVERSIONINFO)&tVersionInfo) == 0)
			{
				LOGEX(System, LOG_ERROR, "Failed when calling \"GetVersionEx\"");
				return "";
			}
			int tMajor = tVersionInfo.dwMajorVersion;
			int tMinor = tVersionInfo.dwMinorVersion;
			char tVersionStr[32];
			sprintf(tVersionStr, "%d.%d", tMajor, tMinor);

			tResult = string(tVersionStr);
		#endif

		//LOGEX(System, LOG_VERBOSE, "Found kernel \"%s\"", tResult.c_str());
    }

    return tResult;
}

int System::GetMachineCores()
{
    static int tResult = -1;

    if (tResult == -1)
    {
		#if defined(LINUX) || defined(APPLE) || defined(BSD)
			tResult = sysconf(_SC_NPROCESSORS_ONLN);
		#endif
		#if defined(WINDOWS)
			SYSTEM_INFO tSysInfo;

			GetSystemInfo(&tSysInfo);

			tResult = tSysInfo.dwNumberOfProcessors;
		#endif

		//LOGEX(System, LOG_VERBOSE, "Found machine cores: %d", tResult);
    }

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
    string tResult = "";

    if (tResult == "")
    {
		#if defined(LINUX) || defined(APPLE) || defined(BSD)
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
		#if defined(WINDOWS)
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
					LOGEX(System, LOG_VERBOSE, "Windows reported unknown machine architecture");
					tResult = "unknown";
					break;
				default:
					LOGEX(System, LOG_ERROR, "Unsupported result code");
					break;
			}
		#endif

		//LOGEX(System, LOG_VERBOSE, "Found machine type \"%s\"", tResult.c_str());
    }

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
string System::GetTargetMachineType()
{
    string tResult = "unknown";

	#if ARCH_BITS == 32
    	tResult = "x86";
	#endif
	#if ARCH_BITS == 64
    	if (GetMachineType() == "amd64")
    		tResult = "amd64";
    	else
			tResult = "ia-64";
	#endif

	//LOGEX(System, LOG_VERBOSE, "Target machine type \"%s\"", tResult.c_str());

	return tResult;
}

int64_t System::GetMachineMemoryPhysical()
{
    int64_t tResult = 0;

	#if defined(APPLE) || defined(BSD)
		int tMib[2];
		uint64_t tPhysRamSize;
		size_t tSize;
		tMib[0] = CTL_HW;
		tMib[1] = HW_MEMSIZE; /* uint64_t: physical ram size */
		tSize = sizeof(tPhysRamSize);
		if (sysctl(tMib, 2, &tPhysRamSize, &tSize, NULL, 0) != 0)
			LOGEX(System, LOG_ERROR, "Failed to determine the amount of physical memory");
		else
			tResult = (int64_t)tPhysRamSize;
	#endif
    #if defined(LINUX)
        long tPages = sysconf(_SC_PHYS_PAGES);
        long tPageSize = sysconf(_SC_PAGE_SIZE);
        tResult = (int64_t)tPages * tPageSize;
    #endif
	#if defined(WINDOWS)
        MEMORYSTATUSEX tMemStatus;
        tMemStatus.dwLength = sizeof(tMemStatus);
        GlobalMemoryStatusEx(&tMemStatus);
        tResult = (int64_t)tMemStatus.ullTotalPhys;
    #endif

    //LOGEX(System, LOG_VERBOSE, "Found machine memory (phys.): %"PRId64" MB", tResult / 1024 / 1024);
    return tResult;
}

int64_t System::GetMachineMemorySwap()
{
    int64_t tResult = 0;

    #if defined(LINUX)
        struct sysinfo tSysInfo;
        if (sysinfo(&tSysInfo) < 0)
        {
            LOGEX(System, LOG_ERROR, "Error in sysinfo()");
            return 0;
        }
        tResult = tSysInfo.totalswap;
    #endif
    #if defined(APPLE) || defined(BSD)
        xsw_usage tVmUsage;
        size_t tSize = sizeof(tVmUsage);
        if (sysctlbyname("vm.swapusage", &tVmUsage, &tSize, NULL, 0) != 0)
        	LOGEX(System, LOG_ERROR, "Failed to determine the amount of swap space" );
        else
        	tResult = (int64_t)tVmUsage.xsu_total;
    #endif
	#if defined(WINDOWS)
        MEMORYSTATUSEX tMemStatus;
        tMemStatus.dwLength = sizeof(tMemStatus);
        GlobalMemoryStatusEx(&tMemStatus);
        tResult = (int64_t)tMemStatus.ullTotalPageFile /* overall virt. memory */ - (int64_t)tMemStatus.ullTotalPhys /* phys. installed memory */;
    #endif

    //LOGEX(System, LOG_VERBOSE, "Found machine memory (swap.): %"PRId64" MB", tResult / 1024 / 1024);
    return tResult;
}

void System::SendActivityToSystem()
{
	#ifdef APPLE
		UpdateSystemActivity(UsrActivity);
	#endif
	#ifdef WINDOWS
	    if (SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) == 0)
	    {
	    	LOGEX(System, LOG_ERROR, "Unable to signal activity to the system");
		}
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
