/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of wrapper for os independent thread handling
 * Author:  Thomas Volkert
 * Since:   2010-09-28
 */

#include <HBThread.h>
#include <HBSystem.h>
#include <Logger.h>

#include <Header_Windows.h>

#include <string.h>
#include <stdlib.h>
#include <cstdio>

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#endif

#ifdef APPLE
// to get current time stamp
#include <mach/clock.h>
#include <mach/mach.h>
#endif

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Thread::Thread()
{
    Init();
}

Thread::~Thread()
{
}

void Thread::Init()
{
    LOG(LOG_VERBOSE, "Initialize thread object");
    mThreadHandle = 0;
}

///////////////////////////////////////////////////////////////////////////////

void Thread::Suspend(unsigned int pUSecs)
{
	if (pUSecs < 1)
		return;
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		if (usleep(pUSecs) != 0)
			LOGEX(Thread, LOG_ERROR, "Error from usleep: \"%s\"", strerror(errno));
	#endif
	#ifdef WIN32
		Sleep(pUSecs / 1000);
	#endif
}

int Thread::GetTId()
{
    #if defined(LINUX)
		// some magic from the depth of linux sources
		return syscall(__NR_gettid);
	#endif
    #if defined(APPLE)
		thread_info_data_t tThreadInfoData;
		thread_identifier_info_t tThreadIdentInfo;
		mach_msg_type_number_t tThreadInfoSize = THREAD_INFO_MAX;
		tThreadInfoSize = THREAD_INFO_MAX;
		if (thread_info(mach_thread_self(), THREAD_IDENTIFIER_INFO, (thread_info_t)tThreadInfoData, &tThreadInfoSize) != KERN_SUCCESS)
		{
			LOGEX(Thread, LOG_ERROR, "Failed to call thread_info()");
		}
		tThreadIdentInfo = (thread_identifier_info_t)tThreadInfoData;
		return (int)tThreadIdentInfo->thread_id;
    #endif
    #if defined(BSD)
		return (int)pthread_self();
    #endif
	#ifdef WIN32
		return (int)GetCurrentThreadId();
	#endif
}

int Thread::GetPId()
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		return getpid();
	#endif
	#ifdef WIN32
		return GetCurrentProcessId();
	#endif
}

int Thread::GetPPId()
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		return getppid();
	#endif
	#ifdef WIN32
		HANDLE tSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS /* include all system processes */, 0 /* current process only */);
		if (tSnapshot)
		{
			int tCurrentPid = GetPId();
			PROCESSENTRY32 tProcessEntry;
			// init size of process description structure
			tProcessEntry.dwSize = sizeof(tProcessEntry);

			// get first process entry
			if (Process32First(tSnapshot, &tProcessEntry))
			{
				do
				{
					if ((int)tProcessEntry.th32ProcessID == tCurrentPid)
					{
						CloseHandle(tSnapshot);
						return tProcessEntry.th32ParentProcessID;
					}
				} while (Process32Next(tSnapshot, &tProcessEntry));
			}
			CloseHandle(tSnapshot);
		}
	#endif
	return -1;
}

vector<int> Thread::GetTIds()
{
	vector<int> tResult;

    #if defined(LINUX)
		DIR *tDir;
		if ((tDir  = opendir("/proc/self/task/")) == NULL)
		{
			LOGEX(Thread, LOG_ERROR, "Files in PROC file system could'nt be listed to get verbose thread information");
			return tResult;
		}

		struct dirent *tDirEntry;
		while ((tDirEntry = readdir(tDir)) != NULL)
		{
			// filter "." and ".."
			if ((string(tDirEntry->d_name) != ".") && (string(tDirEntry->d_name) != ".."))
			{
				int tThreadId = atoi(tDirEntry->d_name);
				tResult.push_back(tThreadId);
			}
		}
		closedir(tDir);
	#endif
	#if defined(BSD)
		//TODO
	#endif
    #if defined(APPLE)
		thread_array_t tThreadList;
		mach_msg_type_number_t tThreadCount;

		if (task_threads(mach_task_self(), &tThreadList, &tThreadCount) == KERN_SUCCESS)
		{
			//LOGEX(Thread, LOG_VERBOSE, "Found %u threads", tThreadCount);
			for (unsigned int u = 0; u < tThreadCount; u++)
			{
				thread_info_data_t tThreadInfoData;
				thread_identifier_info_t tThreadIdentInfo;

				mach_msg_type_number_t tThreadInfoSize = THREAD_INFO_MAX;
				if (thread_info(tThreadList[u], THREAD_IDENTIFIER_INFO, (thread_info_t)tThreadInfoData, &tThreadInfoSize) != KERN_SUCCESS)
				{
					LOGEX(Thread, LOG_ERROR, "Failed to call thread_info()");
				}
				tThreadIdentInfo = (thread_identifier_info_t)tThreadInfoData;

				tResult.push_back((int)tThreadIdentInfo->thread_id);

				//LOGEX(Thread, LOG_VERBOSE, "..thread %u with TID %u", u, (unsigned int)tThreadIdentInfo->thread_id);
			}
			if (vm_deallocate(mach_task_self(), (vm_address_t)tThreadList, sizeof(tThreadList) * tThreadCount) != KERN_SUCCESS)
			{
				LOGEX(Thread, LOG_ERROR, "Failed to deallocate memory of thread list");
			}
		}else{
			LOGEX(Thread, LOG_ERROR, "Failed to get the list of OSX threads");
		}
    #endif
	#ifdef WIN32
		HANDLE tSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD /* include all system thread */, 0 /* current process only */);
		if (tSnapshot)
		{
			int tCurrentPid = GetPId();
			THREADENTRY32 tThreadEntry;
			// init size of thread description structure
			tThreadEntry.dwSize = sizeof(tThreadEntry);

			// get first thread entry
			if (Thread32First(tSnapshot, &tThreadEntry))
			{
				do
				{
					if ((int)tThreadEntry.th32OwnerProcessID == tCurrentPid)
					{
						// open a handle to this specific thread
						HANDLE tThreadHandle = OpenThread(THREAD_ALL_ACCESS, false, tThreadEntry.th32ThreadID);
						if (tThreadHandle)
						{
							// Check whether the thread is still running
							DWORD tThreadExitCode;
							GetExitCodeThread(tThreadHandle, &tThreadExitCode);

							// if still running we add this thread to the result
							if (tThreadExitCode == STILL_ACTIVE)
								tResult.push_back(tThreadEntry.th32ThreadID);

							CloseHandle(tThreadHandle);
						}else
							LOGEX(Thread, LOG_ERROR, "Could not create thread handle");
					}
				} while (Thread32Next(tSnapshot, &tThreadEntry));
			}else
				LOGEX(Thread, LOG_ERROR, "No threads found");
			CloseHandle(tSnapshot);
		}else
			LOGEX(Thread, LOG_ERROR, "Couldn't take system snapshot");
	#endif

	return tResult;
}

bool Thread::GetThreadStatistic(int pTid, unsigned long &pMemVirtual, unsigned long &pMemPhysical, int &pPid, int &pPPid, float &pLoadUser, float &pLoadSystem, float &pLoadTotal, int &pPriority, int &pBasePriority, int &pThreadCount, unsigned long long &pLastUserTicsThread, unsigned long long &pLastKernelTicsThread, unsigned long long &pLastSystemTime)
{
	bool tResult = false;

	pThreadCount = 0;
    pMemPhysical = 0;
    pMemVirtual = 0;
    pLoadUser = 0;
    pLoadSystem = 0;
    pLoadTotal = 0;
    pPriority = 0;
    pBasePriority = 0;

    #if defined(LINUX)
	    /*
	     * HINT: Linux reports relative cpu usage statistic which depends on the amount of available cpu cores.
	     *       Hence, one task can have a maximum of 25 % cpu usage for a 4 core cpu. This behavior is the same like in Windows.
	     */

		FILE *tFile;
		char tFileName[256];

		/* first entry in /proc/stat
		   ./fs/proc/from stat.c of kernel 2.6.39
                %llu    "cpu"         id string
                %llu    user          normal processes executing in user mode
                %llu    nice          niced processes executing in user mode
                %llu    system        processes executing in kernel mode
                %llu    idle          twiddling thumbs
                %llu    iowait        waiting for I/O to complete
                %llu    irq           servicing interrupts
                %llu    softirq       servicing softirqs
                %llu    steal
                %llu    guest
                %llu    guest_nice
		 */
        sprintf(tFileName, "/proc/stat");

        unsigned long long tSystemJiffiesUser = 0, tSystemJiffiesNice = 0;
        unsigned long long tSystemJiffiesKernel = 0, tSystemJiffiesIdle = 0, tSystemJiffiesIoWait = 0, tSystemJiffiesIrq = 0, tSystemJiffiesSoftIrq = 0;
        unsigned long long tSystemJiffies = 0;
        if ((tFile = fopen(tFileName, "r")) != NULL)
        {
            if (EOF == fscanf(tFile, "%*s %llu %llu %llu %llu %llu %llu %llu", &tSystemJiffiesUser, &tSystemJiffiesNice, &tSystemJiffiesKernel, &tSystemJiffiesIdle, &tSystemJiffiesIoWait, &tSystemJiffiesIrq, &tSystemJiffiesSoftIrq))
            	LOGEX(Thread, LOG_ERROR, "Failed to parse file content because of input failure");
            fclose(tFile);
        }
        tSystemJiffiesUser += tSystemJiffiesNice;
        tSystemJiffiesKernel += (tSystemJiffiesIdle + tSystemJiffiesIoWait + tSystemJiffiesIrq + tSystemJiffiesSoftIrq);
        tSystemJiffies = tSystemJiffiesUser + tSystemJiffiesKernel;

        /* entries in /proc/self/task/#/stat:
           from ./fs/proc/array.c of kernel 2.6.39 :  "%d (%s) %c %d %d  %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %d 0 %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld"
                %d      pid           process id
                %s      tcomm         filename of the executable
                %c      state         state (R is running, S is sleeping, D is sleeping in an none interruptible wait, Z is zombie, T is traced or stopped)
                %d      ppid          process id of the parent process
                %d      pgid          group id of the process
                %d      session       session id
                %d      tty_nr        tty the process uses
                %d      tty_pgrp      pgrp of the tty
                %u      flags         task flags
                %lu     minflt        number of minor faults, which haven't triggered a swap-in of memory pages from disc
                %lu     cminflt       number of minor faults with child's
                %lu     majflt        number of major faults, which triggered a swap-in of memory pages from disc
                %lu     cmajflt       number of major faults with child's
                %lu     utime         user mode jiffies
                %lu     stime         kernel mode jiffies
                %ld     cutime        user mode jiffies the task waited for children
                %ld     cstime        kernel mode jiffies the task waited for children
                %ld     priority      priority level: default nice value + 15
                %ld     nice          the nice value ranges from 19 (nicest) to -19 (not nice to others)
                %d      num_threads	  number of threads
                %ld     itrealvalue   the time in jiffies before the next SIGALRM is sent to the process due to an interval timer
                %llu    starttime     time the process started after system boot
                %lu     vsize         virtual memory size
                %ld     rss           resident set memory size
                %lu     rlim          current limit in bytes on the rss
                %lu     start_code    address above which program text can run
                %lu     end_code      address below which program text can run
                %lu     startstack    address of the start of the stack
                %lu     kstkesp       current value of ESP
                %lu     kstkeip       current value of EIP
                %lu     signal        bitmap of pending signals (obsolete)
                %lu     blocked       bitmap of blocked signals (obsolete)
                %lu     sigignore     bitmap of ignored signals (obsolete)
                %lu     sigcatch      bitmap of catched signals (obsolete)
                %lu     wchan         This is the "channel" in which the process is waiting. It is the address of a system call,
                                      and can be looked up in a namelist if textual name is needed.
                %llu    nswap(=0)     number of pages swapped (not maintained)
                %lu     cnswap(=0)    cumulative nswap for child processes (not maintained)
                %d      exit_signal   signal to send to parent thread on exit
                %d      processor     which CPU the task is scheduled on
                %lu     rt_priority   realtime priority (man sched-setscheduler)
                %lu     policy        scheduling policy (man sched_setscheduler)
                %ld     blkio_ticks   time spent waiting for block IO
                %ld     gtime
                %ld     cgtime

		*/
		sprintf(tFileName, "/proc/self/task/%d/stat", pTid);

        unsigned long tJiffiesUserMode = 0, tJiffiesKernelMode = 0;
        long int tJiffiesUserModeChildWait = 0, tJiffiesKernelModeChildWait = 0;
		if ((tFile = fopen(tFileName, "r")) != NULL)
		{
		    long int tPriority, tBasePriority;
		    if (EOF == fscanf(tFile, "%d %*s %*c %d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %ld %ld %ld %ld %d 0 %*llu %lu %ld", &pPid, &pPPid, &tJiffiesUserMode, &tJiffiesKernelMode, &tJiffiesUserModeChildWait, &tJiffiesKernelModeChildWait, &tPriority, &tBasePriority, &pThreadCount, &pMemVirtual, (long int*)&pMemPhysical))
		        LOGEX(Thread, LOG_ERROR, "Failed to parse file content because of input failure");
		    pPriority = tPriority;
		    pBasePriority = tBasePriority;
		    tJiffiesUserMode += (unsigned long)tJiffiesUserModeChildWait;
		    tJiffiesKernelMode += (unsigned long)tJiffiesKernelModeChildWait;
            fclose(tFile);
			tResult = true;
		}
		pMemPhysical *= 4096; // this value is given in 4k pages
		pLoadUser = 100 * ((float)(tJiffiesUserMode - pLastUserTicsThread)) / (float)(tSystemJiffies - pLastSystemTime);
		pLoadSystem = 100 * ((float)(tJiffiesKernelMode - pLastKernelTicsThread)) / (float)(tSystemJiffies - pLastSystemTime);
		pLastUserTicsThread = tJiffiesUserMode;
		pLastKernelTicsThread = tJiffiesKernelMode;
        pLastSystemTime = tSystemJiffies;
	#endif
	#if defined(APPLE)
	    /*
	     * HINT: OSX reports absolute cpu usage statistic.
	     *       Hence, one task can have a maximum of 100 % cpu usage. In this case the task consumes the calculation power of exactly one cpu core.
	     *       We have to align these values to the ones which are reported by Windows/Linux. This is done by dividing them by the amount of physically available cpu cores.
	     */
		pPid = GetPId();
		pPPid = GetPPId();

        // apple specific implementation for clock_gettime()
        unsigned long long tSystemTime = 0;
        clock_serv_t tCalenderClock;
        mach_timespec_t tMachTimeSpec;
        host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &tCalenderClock);
        clock_get_time(tCalenderClock, &tMachTimeSpec);
        mach_port_deallocate(mach_task_self(), tCalenderClock);
        tSystemTime += tMachTimeSpec.tv_sec * 1000 * 1000;
        tSystemTime += tMachTimeSpec.tv_nsec / 1000;

		thread_array_t tThreadList;
		mach_msg_type_number_t tThreadCount;
		mach_msg_type_number_t tThreadInfoSize = THREAD_INFO_MAX;

		if (task_threads(mach_task_self(), &tThreadList, &tThreadCount) == KERN_SUCCESS)
		{
			pThreadCount = tThreadCount;

			//LOGEX(Thread, LOG_VERBOSE, "Found %u threads", tThreadCount);

			// get statistic info per thread
			for (unsigned int u = 0; u < tThreadCount; u++)
			{
				thread_info_data_t tThreadInfoData;

				tThreadInfoSize = THREAD_INFO_MAX;

				// get THREAD_IDENTIFIER
				if (thread_info(tThreadList[u], THREAD_IDENTIFIER_INFO, (thread_info_t)tThreadInfoData, &tThreadInfoSize) == KERN_SUCCESS)
				{
					thread_basic_info_t tThreadBasicInfo;
					thread_identifier_info_t tThreadIdentInfo = (thread_identifier_info_t)tThreadInfoData;

					if ((int)tThreadIdentInfo->thread_id == pTid)
					{
						tThreadInfoSize = THREAD_INFO_MAX;

						// get THREAD_BASIC
						if (thread_info(tThreadList[u], THREAD_BASIC_INFO, (thread_info_t)tThreadInfoData, &tThreadInfoSize) == KERN_SUCCESS)
						{
					        unsigned long tTimeUserMode = 0, tTimeKernelMode = 0;

							tThreadBasicInfo = (thread_basic_info_t)tThreadInfoData;
							pPriority = tThreadBasicInfo->policy;

							// we need to know how many cores we have to scale the cpu usage
							int tCores = System::GetMachineCores();

							tTimeUserMode = tThreadBasicInfo->user_time.seconds * 1000 * 1000 + tThreadBasicInfo->user_time.microseconds;
							tTimeKernelMode = tThreadBasicInfo->system_time.seconds * 1000 * 1000 + tThreadBasicInfo->system_time.microseconds;
							pLoadUser = 100 * ((float)(tTimeUserMode - pLastUserTicsThread)) / (float)(tSystemTime - pLastSystemTime) / tCores;
							pLoadSystem = 100 * ((float)(tTimeKernelMode - pLastKernelTicsThread)) / (float)(tSystemTime - pLastSystemTime) / tCores;
							pLastUserTicsThread = tTimeUserMode;
							pLastKernelTicsThread = tTimeKernelMode;
					        pLastSystemTime = tSystemTime;
						}else
						{
							LOGEX(Thread, LOG_ERROR, "Failed to call thread_info()");
						}
						break;
					}
				}else
				{
					LOGEX(Thread, LOG_ERROR, "Failed to call thread_info()");
				}

				//LOGEX(Thread, LOG_VERBOSE, "..thread %u with TID %u", u, (unsigned int)tThreadIdentInfo->thread_id);
			}

			// get statistic info for our task
			task_basic_info_t tTaskBasicInfo;
			task_info_data_t tTaskInfoData;
			mach_msg_type_number_t tTaskInfoSize = TASK_INFO_MAX;
			if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tTaskInfoData, &tTaskInfoSize) != KERN_SUCCESS)
			{
				LOGEX(Thread, LOG_ERROR, "Failed to call task_info()");
			}
			tTaskBasicInfo = (task_basic_info_t)tTaskInfoData;
			pMemPhysical = tTaskBasicInfo->resident_size;
			pMemVirtual = tTaskBasicInfo->virtual_size;
			pBasePriority = tTaskBasicInfo->policy;
			
			if (vm_deallocate(mach_task_self(), (vm_address_t)tThreadList, sizeof(tThreadList) * tThreadCount) != KERN_SUCCESS)
			{
				LOGEX(Thread, LOG_ERROR, "Failed to deallocate memory of thread list");
			}
		}else{
			LOGEX(Thread, LOG_ERROR, "Failed to get the list of OSX threads");
		}
    #endif
    #if defined(BSD)
		pPid = GetPId();
		pPPid = GetPPId();
		//TODO:
		pThreadCount = 0;
        pMemPhysical = 0;
        pMemVirtual = 0;
        pLoadUser = 0;
        pLoadSystem = 0;
        pLoadTotal = 0;
        pLastUserTicsThread = 0;
        pLastKernelTicsThread = 0;
        pLastSystemTime = 0;
        pPriority = 0;
        pBasePriority = 0;
    #endif
	#ifdef WIN32
		/* Windows thread priorities: (based on http://msdn.microsoft.com/en-us/library/ms683235%28VS.85%29.aspx)
			 priority			| explanation
			--------------------+-----------
			  base priority 	|  defines the priority class
								| 	range from 0 to 31, where 0 is the lowest priority and 31 is the highest
			  current priority  |  defines the offset related to the base priority, often used values are:
								|  -15 (THREAD_PRIORITY_IDLE)			Base priority of 1 for IDLE_PRIORITY_CLASS,
								|										BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS,
								|										ABOVE_NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS
								|										processes, and a base priority of 16 for
								|										REALTIME_PRIORITY_CLASS processes
								|   -2 (THREAD_PRIORITY_LOWEST)			Priority 2 points below the priority class.
								|   -1 (THREAD_PRIORITY_BELOW_NORMAL)   Priority 1 point below the priority class.
								|    0 (THREAD_PRIORITY_NORMAL) 		Normal priority for the priority class.
								|    1 (THREAD_PRIORITY_ABOVE_NORMAL)	Priority 1 point above the priority class
								|    2 (THREAD_PRIORITY_HIGHEST) 		Priority 2 points above the priority class.
								|   15 (THREAD_PRIORITY_TIME_CRITICAL)	Base-priority level of 15 for IDLE_PRIORITY_CLASS,
								|										BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS,
								|										ABOVE_NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS
								|										processes, and a base-priority level of 31 for
								|										REALTIME_PRIORITY_CLASS processes.
		 */

		int tCurrentPid = GetPId();

		// open a handle to this specific thread
		HANDLE tProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, false, tCurrentPid);
		if (tProcessHandle)
		{
			PROCESS_MEMORY_COUNTERS tProcessMemInfo;
			// get the memory usage of the current process
			GetProcessMemoryInfo(tProcessHandle, &tProcessMemInfo, sizeof(tProcessMemInfo));

			pMemPhysical = (unsigned long)tProcessMemInfo.WorkingSetSize;
	        pMemVirtual = (unsigned long)tProcessMemInfo.PagefileUsage;

	        CloseHandle(tProcessHandle);
		}else
		{
			LOGEX(Thread, LOG_ERROR, "Could not create process handle");
			pMemVirtual = 0;
	        pMemPhysical = 0;
		}

		//###############################################################
		//### Pid, PPid, ThreadCount, BasePriority
		//###############################################################
        // get all threads of the current process and the descriptor for the current process itself
		HANDLE tSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS + TH32CS_SNAPTHREAD /* include all system processes + threads */, 0);
		if (tSnapshot)
		{
			//### Pid, PPid, ThreadCount
			PROCESSENTRY32 tProcessEntry;
			// init size of process description structure
			tProcessEntry.dwSize = sizeof(tProcessEntry);

			// get first process entry
			if (Process32First(tSnapshot, &tProcessEntry))
			{
				do
				{
					if ((int)tProcessEntry.th32ProcessID == tCurrentPid)
					{
						pPid = tCurrentPid;
						pPPid = (int)tProcessEntry.th32ParentProcessID;
						pThreadCount = (int)tProcessEntry.cntThreads;
						tResult = true;
						// end do-while-loop
						break;
					}
				} while (Process32Next(tSnapshot, &tProcessEntry));
			}else
				LOGEX(Thread, LOG_ERROR, "No process found");

			//### BasePriority
			// default value
			pBasePriority = 0;
			THREADENTRY32 tThreadEntry;
			// init size of thread description structure
			tThreadEntry.dwSize = sizeof(tThreadEntry);

			// get first thread entry
			if (Thread32First(tSnapshot, &tThreadEntry))
			{
				do
				{
					if (((int)tThreadEntry.th32OwnerProcessID == tCurrentPid) && ((int)tThreadEntry.th32ThreadID == pTid))
					{
						pBasePriority = tThreadEntry.tpBasePri;
						break;
					}
				} while (Thread32Next(tSnapshot, &tThreadEntry));
			}else
				LOGEX(Thread, LOG_ERROR, "No threads found");

			CloseHandle(tSnapshot);
		}else
			LOGEX(Thread, LOG_ERROR, "Couldn't take system snapshot");

		//###############################################################
		//### LoadUser, LoadSystem, Priority
		//###############################################################
		// get global system times
		FILETIME tSystemUserTime, tSystemKernelTime, tSystemIdleTime;
		if (!GetSystemTimes(&tSystemIdleTime, &tSystemKernelTime, &tSystemUserTime))
			LOGEX(Thread, LOG_ERROR, "Failed to get system times");
		// open a handle to this specific thread
		HANDLE tThreadHandle = OpenThread(THREAD_ALL_ACCESS, false, pTid);
		if (tThreadHandle)
		{
			// get thread times
			FILETIME tThreadCreationTime, tThreadExitTime, tThreadKernelTime, tThreadUserTime;
			if (GetThreadTimes(tThreadHandle, &tThreadCreationTime, &tThreadExitTime, &tThreadKernelTime, &tThreadUserTime))
			{
				// follow the nebulous instructions of MSDN and convert the FILETIME structures to _ULARG_INTEGER structures at first
				_ULARGE_INTEGER tTimeUserMode, tTimeKernelMode, tSystemTime, tSysTimeKernelMode, tSysTimeUserMode;
				tTimeUserMode.LowPart = tThreadUserTime.dwLowDateTime;
				tTimeUserMode.HighPart = tThreadUserTime.dwHighDateTime;
				tTimeKernelMode.LowPart = tThreadKernelTime.dwLowDateTime;
				tTimeKernelMode.HighPart = tThreadKernelTime.dwHighDateTime;
				tSysTimeKernelMode.LowPart = tSystemKernelTime.dwLowDateTime;
				tSysTimeKernelMode.HighPart = tSystemKernelTime.dwHighDateTime;
				tSysTimeUserMode.LowPart = tSystemUserTime.dwLowDateTime;
				tSysTimeUserMode.HighPart = tSystemUserTime.dwHighDateTime;
				tSystemTime.QuadPart = tSysTimeUserMode.QuadPart + tSysTimeKernelMode.QuadPart;
				// calculate the values for load related to user and kernel mode
				pLoadUser = 100 * ((float)(tTimeUserMode.QuadPart - pLastUserTicsThread)) / (float)(tSystemTime.QuadPart - pLastSystemTime);
				pLoadSystem = 100 * ((float)(tTimeKernelMode.QuadPart - pLastKernelTicsThread)) / (float)(tSystemTime.QuadPart - pLastSystemTime);
				pLastUserTicsThread = tTimeUserMode.QuadPart;
				pLastKernelTicsThread = tTimeKernelMode.QuadPart;
		        pLastSystemTime = tSysTimeUserMode.QuadPart + tSysTimeKernelMode.QuadPart;
			}else
			{
				LOGEX(Thread, LOG_ERROR, "Failed to get thread times");
				pLoadUser = 0;
		        pLoadSystem = 0;
				pLastUserTicsThread = 0;
				pLastKernelTicsThread = 0;
		        pLastSystemTime = 0;
			}
			// get thread priority
			pPriority = GetThreadPriority(tThreadHandle);
			if (pPriority == THREAD_PRIORITY_ERROR_RETURN)
				pPriority = 0;

			CloseHandle(tThreadHandle);
		}else
			LOGEX(Thread, LOG_VERBOSE, "Could not create thread handle, maybe thread was destroyed meanwhile");
	#endif

	pLoadTotal = pLoadUser + pLoadSystem;

    return tResult;
}

void* Thread::StartThreadStaticWrapperUniversal(void* pThread)
{
	LOGEX(Thread, LOG_VERBOSE, "Going to start thread main");

    Thread *tThreadObject = (Thread*)pThread;
    tThreadObject->mRunning = true;
    void* tResult = tThreadObject->mThreadMain(tThreadObject->mThreadArguments);
    tThreadObject->mRunning = false;
    LOGEX(Thread, LOG_VERBOSE, "Thread finished");
    tThreadObject->Init();
    return tResult;
}

void* Thread::StartThreadStaticWrapperRun(void* pThread)
{
	LOGEX(Thread, LOG_VERBOSE, "Going to start thread main (Run method)");

    Thread *tThreadObject = (Thread*)pThread;
    tThreadObject->mRunning = true;
    void* tResult = tThreadObject->Run(tThreadObject->mThreadArguments);
    tThreadObject->mRunning = false;
    LOGEX(Thread, LOG_VERBOSE, "Thread finished (Run method)");
    tThreadObject->Init();
    return tResult;
}

bool Thread::StartThread(void* pArgs)
{
    bool tResult = false;

    if (mThreadHandle != 0)
        return false;

    mThreadMain = 0;
    mThreadArguments = pArgs;

    #if defined(LINUX) || defined(APPLE) || defined(BSD)
        size_t tThreadStackSize;
        pthread_attr_t tThreadAttributes;

        // Initialize and set thread detached attribute
        pthread_attr_init(&tThreadAttributes);
        pthread_attr_getstacksize (&tThreadAttributes, &tThreadStackSize);
        pthread_attr_setdetachstate(&tThreadAttributes, PTHREAD_CREATE_JOINABLE);
        if (int tRes = pthread_create(&mThreadHandle, &tThreadAttributes, StartThreadStaticWrapperRun, (void*)this))
            LOG(LOG_ERROR, "Creation of thread failed because of \"%s\"", strerror(tRes));
        else
        {
            LOG(LOG_VERBOSE, "Thread started with stack size of %ld bytes", tThreadStackSize);
            tResult = true;
        }
        pthread_attr_destroy(&tThreadAttributes);
    #endif
    #ifdef WIN32
        mThreadHandle = CreateThread(
             NULL,                   // default security attributes
             THREAD_DEFAULT_STACK_SIZE,        // use Linux pthread default stack size, Windows would use 1 MB
             (LPTHREAD_START_ROUTINE)StartThreadStaticWrapperRun, // thread function name
             (LPVOID)this,           // argument to thread function
             0,                      // use default creation flags
             0);   					 // discard the thread identifier
        if (mThreadHandle == 0)
        	LOG(LOG_ERROR, "Creation of thread failed because of code \"%d\"", GetLastError());
        else
        {
            LOG(LOG_VERBOSE, "Thread started with stack size of %d bytes", THREAD_DEFAULT_STACK_SIZE);
        	tResult = true;
        }
    #endif

    return tResult;
}

bool Thread::StartThread(THREAD_MAIN pMain, void* pArgs)
{
	bool tResult = false;

    if (mThreadHandle != 0)
        return false;

    if (pMain == NULL)
        return false;

	mThreadMain = pMain;
	mThreadArguments = pArgs;

    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		size_t tThreadStackSize;
	    pthread_attr_t tThreadAttributes;

		// Initialize and set thread detached attribute
		pthread_attr_init(&tThreadAttributes);
		pthread_attr_getstacksize (&tThreadAttributes, &tThreadStackSize);
		pthread_attr_setdetachstate(&tThreadAttributes, PTHREAD_CREATE_JOINABLE);
		if (int tRes = pthread_create(&mThreadHandle, &tThreadAttributes, StartThreadStaticWrapperUniversal, (void*)this))
			LOG(LOG_ERROR, "Creation of thread failed because of \"%s\"", strerror(tRes));
		else
		{
			LOG(LOG_VERBOSE, "Thread started with stack size of %ld bytes", tThreadStackSize);
			tResult = true;
		}
		pthread_attr_destroy(&tThreadAttributes);
	#endif
    #ifdef WIN32
        mThreadHandle = CreateThread(
             NULL,                   // default security attributes
             0,                      // use default stack size
             (LPTHREAD_START_ROUTINE)StartThreadStaticWrapperUniversal, // thread function name
             (LPVOID)this,           // argument to thread function
             0,                      // use default creation flags
             0);    			 	 // discard the thread identifier
        if (mThreadHandle == 0)
        	LOG(LOG_ERROR, "Creation of thread failed because of code \"%d\"", GetLastError());
        else
        	tResult = true;
	#endif

	return tResult;
}

bool Thread::StopThread(int pTimeoutInMSecs, void** pResults)
{
	bool tResult = false;
	void* tThreadResult = NULL;

    if (mThreadHandle == 0)
    {
        LOG(LOG_VERBOSE, "Thread handle is NULL, assume thread was already stopped");
    	if (pResults != NULL)
    		*pResults = NULL;
        return true;
    }

    if (!IsRunning())
    {
        LOG(LOG_VERBOSE, "Thread isn't running at the moment, skipped StopThread()");
        if (pResults != NULL)
            *pResults = NULL;
        return true;
    }

    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		struct timespec tTimeout;

        #if defined(LINUX) || defined(BSD)
            if (clock_gettime(CLOCK_REALTIME, &tTimeout) == -1)
                LOG(LOG_ERROR, "Failed to get time from clock");
        #endif

		tTimeout.tv_sec += pTimeoutInMSecs / 1000;
		tTimeout.tv_nsec += (pTimeoutInMSecs % 1000) * 1000000;

        #if defined(LINUX)
		    if(pTimeoutInMSecs > 0)
		    {
		        if (int tRes = pthread_timedjoin_np(mThreadHandle, &tThreadResult, &tTimeout))
                    LOG(LOG_INFO, "Waiting (time limited to %d ms) for end of thread failed because \"%s\"(%d)", pTimeoutInMSecs, strerror(tRes), tRes);
                else
                {
                    LOG(LOG_VERBOSE, "Got end signal and thread results at %p", tThreadResult);
                    tResult = true;
                }
		    }else
		    {
                if (int tRes = pthread_join(mThreadHandle, &tThreadResult))
                    LOG(LOG_INFO, "Waiting for end of thread failed because \"%s\"", strerror(tRes));
                else
                {
                    LOG(LOG_VERBOSE, "Got end signal and thread results at %p", tThreadResult);
                    tResult = true;
                }
		    }
        #endif

        #if defined(APPLE) || defined(BSD)
            // OSX doesn't support pthread_timedjoin_np(), fall back to simple pthread_join()
            if (int tRes = pthread_join(mThreadHandle, &tThreadResult))
                LOG(LOG_INFO, "Waiting for end of thread failed because \"%s\"", strerror(tRes));
            else
            {
                LOG(LOG_VERBOSE, "Got end signal and thread results at %p", tThreadResult);
                tResult = true;
            }
        #endif
	#endif
	#ifdef WIN32
        if (pTimeoutInMSecs == 0)
        {
            pTimeoutInMSecs = INFINITE;
        }

        switch(WaitForSingleObject(mThreadHandle, (DWORD)pTimeoutInMSecs))
		{
			case WAIT_ABANDONED:
			case WAIT_TIMEOUT:
			case WAIT_FAILED:
				break;
			case WAIT_OBJECT_0:
				tResult = true;
				break;
		}
	#endif
	if (pResults != NULL)
		*pResults = tThreadResult;

	return tResult;
}

bool Thread::IsRunning()
{
    return mRunning;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
