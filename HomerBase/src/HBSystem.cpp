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
 * Purpose: Implementation of system information functions
 * Since:   2011-02-25
 */

//HINT: http://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process

#include <Logger.h>
#include <HBSystem.h>
#include <HBThread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cxxabi.h>

#if defined(LINUX)
#include <sys/sysinfo.h>

//TODO: available in OSX?
#include <execinfo.h>
#endif

#if defined(APPLE) || defined(BSD)
#include <sys/sysctl.h>
#endif

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include <Header_Windows.h>

namespace Homer { namespace Base {

using namespace std;

#define MAX_STACK_TRACE_DEPTH             200
#define MAX_STACK_TRACE_STEP_LENGTH       512

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
		#if (_MSC_VER < 1800)
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
		#else
			//TODO: implementation for VS2013 and above
		#endif
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
			#if (_MSC_VER < 1800)
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
			#else
				//TODO: implementation for VS2013 and above
			#endif
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

	#if defined(APPLE)
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
	//TODO: FreeBSD?
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
	//TODO: FreeBSD?
    #if defined(APPLE)
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

list<string> System::GetStackTrace()
{
    list<string> tResult;
	int tStackTraceStep = 0;
    char tStringBuf[MAX_STACK_TRACE_STEP_LENGTH];

	#ifdef LINUX
	    void *tStackTrace[MAX_STACK_TRACE_DEPTH];
	    int tStackTraceSize;
	    char **tStackTraceList;

	    tStackTraceSize = backtrace(tStackTrace, MAX_STACK_TRACE_DEPTH);
	    tStackTraceList = backtrace_symbols(tStackTrace, tStackTraceSize);

	    size_t tFuncNameSize = MAX_STACK_TRACE_STEP_LENGTH / 2;
	    char* tFuncName = (char*)malloc(tFuncNameSize);

	    for (int i = 0; i < tStackTraceSize; i++)
	    {
	        // get the pointers to the name, offset and end of offset
	        char *tBeginFuncName = 0;
	        char *tBeginFuncOffset = 0;
	        char *tEndFuncOffset = 0;
	        char *tBeginBinaryName = tStackTraceList[i];
	        char *tBeginBinaryOffset = 0;
	        char *tEndBinaryOffset = 0;
	        for (char *tEntryPointer = tStackTraceList[i]; *tEntryPointer; ++tEntryPointer)
	        {
	            if (*tEntryPointer == '(')
	            {
	                tBeginFuncName = tEntryPointer;
	            }else if (*tEntryPointer == '+')
	            {
	                tBeginFuncOffset = tEntryPointer;
	            }else if (*tEntryPointer == ')' && tBeginFuncOffset)
	            {
	                tEndFuncOffset = tEntryPointer;
	            }else if (*tEntryPointer == '[')
	            {
	                tBeginBinaryOffset = tEntryPointer;
	            }else if (*tEntryPointer == ']' && tBeginBinaryOffset)
	            {
	                tEndBinaryOffset = tEntryPointer;
	                break;
	            }
	        }

	        if (tBeginFuncName && tBeginFuncOffset && tEndFuncOffset && tBeginFuncName < tBeginFuncOffset)
	        {
	            // terminate the C strings
	            *tBeginFuncName++ = '\0';
	            *tBeginFuncOffset++ = '\0';
	            *tEndFuncOffset = '\0';
	            *tBeginBinaryOffset++ = '\0';
	            *tEndBinaryOffset = '\0';

	            int tDemanglingRes;
	            char* tFuncName = abi::__cxa_demangle(tBeginFuncName, tFuncName, &tFuncNameSize, &tDemanglingRes);
	            unsigned int tBinaryOffset = strtoul(tBeginBinaryOffset, NULL, 16);
	            if (tDemanglingRes == 0)
	            {
	                string tFuncNameStr = (tFuncName ? tFuncName : "");
	                if(strncmp(tFuncName, "Homer::", 7) == 0)
	                    tFuncNameStr = ">> " + tFuncNameStr;

	                if(tBeginBinaryName && strlen(tBeginBinaryName))
	                    sprintf(tStringBuf, "#%02d 0x%016x in %s:[%s] from %s", tStackTraceStep, tBinaryOffset, tFuncNameStr.c_str(), tBeginFuncOffset, tBeginBinaryName);
	                else
	                    sprintf(tStringBuf, "#%02d 0x%016x in %s from %s", tStackTraceStep, tBinaryOffset, tFuncNameStr.c_str(), tBeginFuncOffset);
	                tStackTraceStep++;
	            }else{
	                if(tBeginBinaryName && strlen(tBeginBinaryName))
	                    sprintf(tStringBuf, "#%02d 0x%016x in %s:[%s] from %s", tStackTraceStep, tBinaryOffset, tBeginFuncName, tBeginFuncOffset, tBeginBinaryName);
	                else
	                    sprintf(tStringBuf, "#%02d 0x%016x in %s:[%s]", tStackTraceStep, tBinaryOffset, tBeginFuncName, tBeginFuncOffset);
	                tStackTraceStep++;
	            }
	        }else{
	            sprintf(tStringBuf, "#%02d %s\n", tStackTraceStep, tStackTraceList[i]);
	            tStackTraceStep++;
	        }

	        // append the line to the result
	        tResult.push_back(string(tStringBuf));
	    }

	    // memory cleanup
	    free(tStackTraceList);
	    free(tFuncName);
	#else
    	// inspired from: http://www.codeproject.com/Articles/11132/Walking-the-callstack

		HANDLE          tProcess = GetCurrentProcess();
		HANDLE          tThread = GetCurrentThread();
		CONTEXT         tProcessContext;
		STACKFRAME64    tStackFrame;
		SYMBOL_INFO 	*tSymDesc = NULL;
		char 			tNameBuffer[MAX_STACK_TRACE_STEP_LENGTH];
		unsigned int 	tNameBufferSize = MAX_STACK_TRACE_STEP_LENGTH;

	    // set symbol style
		SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_INCLUDE_32BIT_MODULES);

		// init process symbols
	    if(!SymInitialize(tProcess, NULL, TRUE))
	    {
	    	LOGEX(System, LOG_ERROR, "Failed to initialize the process symbols");
	        return tResult;
	    }

		// prepare the buffer for the symbol descriptor
		size_t tSymDescSize = sizeof(SYMBOL_INFO) + MAX_STACK_TRACE_STEP_LENGTH * sizeof(TCHAR);
		tSymDesc = (SYMBOL_INFO*)malloc(tSymDescSize);
		memset(tSymDesc, 0, tSymDescSize);

		// prepare the buffer for the module info
		IMAGEHLP_MODULE64 tModuleInfo;
		memset(&tModuleInfo, 0, sizeof(tModuleInfo));
		tModuleInfo.SizeOfStruct = sizeof(tModuleInfo);

		// prepare the buffer for the module line number
		IMAGEHLP_LINE64 tLineInfo;
		memset(&tLineInfo, 0, sizeof(tLineInfo));
		tLineInfo.SizeOfStruct = sizeof(tLineInfo);

		// iterate over all thread IDs
		vector<int> tThreadIds = Thread::GetTIds();
		vector<int>::iterator tIt;
		for(tIt = tThreadIds.begin(); tIt != tThreadIds.end(); tIt++)
		{
			tThread = OpenThread(THREAD_ALL_ACCESS, false, *tIt);
			if(tThread != NULL)
			{
				tStackTraceStep = 0;

		    	memset(&tProcessContext, 0, sizeof(CONTEXT));
		    	tProcessContext.ContextFlags = CONTEXT_FULL;
			    if (tThread == GetCurrentThread())
			    {
			    	sprintf(tStringBuf, "Call stack for current thread %d:", *tIt);
			        // append the line to the result
			        tResult.push_back(string(tStringBuf));

				    // get process context of the current thread
					RtlCaptureContext(&tProcessContext);
			    }
			    else
			    {
			    	sprintf(tStringBuf, "Call stack for thread %d:", *tIt);
			        // append the line to the result
			        tResult.push_back(string(tStringBuf));

				    // get process context of the foreign thread
			    	//SuspendThread(tThread);
			    	if (GetThreadContext(tThread, &tProcessContext) == FALSE)
			    	{
			    		ResumeThread(tThread);
			    		continue;
			    	}
			    }

				// prepare the buffer for the stack trace
				memset(&tStackFrame, 0, sizeof(STACKFRAME64));
				tStackFrame.AddrPC.Offset    = tProcessContext.Eip;
				tStackFrame.AddrPC.Mode      = AddrModeFlat;
				tStackFrame.AddrStack.Offset = tProcessContext.Esp;
				tStackFrame.AddrStack.Mode   = AddrModeFlat;
				tStackFrame.AddrFrame.Offset = tProcessContext.Ebp;
				tStackFrame.AddrFrame.Mode   = AddrModeFlat;

				// walk through the stack trace of the currently selected thread
				while(StackWalk64(IMAGE_FILE_MACHINE_I386, tProcess, tThread, &tStackFrame, &tProcessContext, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				{
					string tFileName = "";
					string tModuleName = "";
					int tFileLine = 0;
					string tFuncName = "??";

					// get the function name
					tSymDesc->SizeOfStruct = sizeof(SYMBOL_INFO);
					tSymDesc->MaxNameLen = MAX_STACK_TRACE_STEP_LENGTH;
					if(SymFromAddr(tProcess, (ULONG64)tStackFrame.AddrPC.Offset, 0, tSymDesc))
					{
						if(tSymDesc->Name != NULL)
							tFuncName = string(tSymDesc->Name);
					}else
					{
						//LOGEX(System, LOG_WARN, "Can not determine the symbol at address %p", tStackFrame.AddrPC.Offset);
					}

					// get the file name
					if(SymGetModuleInfo64(tProcess, tStackFrame.AddrPC.Offset, &tModuleInfo))
					{
						if(tModuleInfo.ModuleName != NULL)
							tModuleName = tModuleInfo.ModuleName;
					}else
					{
						//LOGEX(System, LOG_WARN, "Can not determine the module info for address %p", tStackFrame.AddrPC.Offset);
					}

					// get the file line
					if(SymGetLineFromAddr64(tProcess, tStackFrame.AddrPC.Offset, 0, &tLineInfo))
					{
						if(tLineInfo.FileName != NULL)
							tFileName = tLineInfo.FileName;
						tFileLine = tLineInfo.LineNumber;
					}else
					{
						//LOGEX(System, LOG_WARN, "Can not determine the module line number for address %p because of code \"%d\"", tStackFrame.AddrFrame.Offset, GetLastError());
					}

//					printf(
//						"Frame %d:\n"
//						"    Symbol:    	 %s, %s, %s, %d\n"
//						"    PC address:     0x%08LX\n"
//						"    Stack address:  0x%08LX\n"
//						"    Frame address:  0x%08LX\n"
//						"\n",
//						tStackTraceStep,
//						tModuleName.c_str(), tFileName.c_str(), tFuncName.c_str(), tFileLine,
//						( ULONG64 )tStackFrame.AddrPC.Offset,
//						( ULONG64 )tStackFrame.AddrStack.Offset,
//						( ULONG64 )tStackFrame.AddrFrame.Offset
//					);

					int tDemanglingRes;
					char *tDemangledFuncName = abi::__cxa_demangle(("_" /* fix the universe */ + tFuncName).c_str(), tNameBuffer, &tNameBufferSize, &tDemanglingRes);
					if(tFileName != "")
					{
						if (tDemanglingRes == 0)
						{
							sprintf(tStringBuf, "  #%02d 0x%08x in %s at %s:%d from %s", tStackTraceStep, (unsigned long)tStackFrame.AddrPC.Offset, tDemangledFuncName, tFileName.c_str(), tFileLine, tModuleName.c_str());
						}else{
							sprintf(tStringBuf, "  #%02d 0x%08x in %s() at %s:%d from %s", tStackTraceStep, (unsigned long)tStackFrame.AddrPC.Offset, tFuncName.c_str(), tFileName.c_str(), tFileLine, tModuleName.c_str());
						}
					}else{
						if (tDemanglingRes == 0)
						{
							sprintf(tStringBuf, "  #%02d 0x%08x in %s from %s", tStackTraceStep, (unsigned long)tStackFrame.AddrPC.Offset, tDemangledFuncName, tModuleName.c_str());
						}else{
							sprintf(tStringBuf, "  #%02d 0x%08x in %s() from %s", tStackTraceStep, (unsigned long)tStackFrame.AddrPC.Offset, tFuncName.c_str(), tModuleName.c_str());
						}
					}
			        // append the line to the result
			        if(strlen(tStringBuf) > 0)
			        {
				        tResult.push_back(string(tStringBuf));
			        }

					tStackTraceStep++;
				}
			}
			CloseHandle(tThread);
		}

		free(tSymDesc);
	#endif

    return tResult;

}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
