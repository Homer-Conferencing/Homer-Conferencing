/*
 * Name:    Thread.cpp
 * Purpose: Implementation of wrapper for os independent thread handling
 * Author:  Thomas Volkert
 * Since:   2010-09-28
 * Version: $Id$
 */

#include <HBThread.h>
#include <Logger.h>

#include <string.h>
#include <stdlib.h>
#include <cstdio>

#ifdef LINUX
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#endif
#ifdef WIN32
#include <Windows.h>
#include <Psapi.h>
#include <Tlhelp32.h>
#endif

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

#define PAGE_SIZE 4096

///////////////////////////////////////////////////////////////////////////////

Thread::Thread()
{
    mThreadHandle = 0;
}

Thread::~Thread()
{
}

///////////////////////////////////////////////////////////////////////////////

void Thread::Suspend(unsigned int pUSecs)
{
	if (pUSecs < 1)
		return;
	#ifdef LINUX
		if (usleep(pUSecs) != 0)
			LOGEX(Thread, LOG_ERROR, "Error from usleep: \"%s\"", strerror(errno));
	#endif
	#ifdef WIN32
		Sleep(pUSecs / 1000);
	#endif
}

int Thread::GetTId()
{
	#ifdef LINUX
		// some magic from the depth of linux sources
		return syscall(__NR_gettid);
	#endif
	#ifdef WIN32
		return (int)GetCurrentThreadId();
	#endif
}

int Thread::GetPId()
{
	#ifdef LINUX
		return getpid();
	#endif
	#ifdef WIN32
		return GetCurrentProcessId();
	#endif
}

int Thread::GetPPId()
{
	#ifdef LINUX
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

	#if LINUX
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

bool Thread::GetThreadStatistic(int pTid, int &pMemVirtual, int &pMemPhysical, int &pPid, int &pPPid, float &pLoadUser, float &pLoadSystem, float &pLoadTotal, int &pPriority, int &pBasePriority, int &pThreadCount, unsigned long long &pLastUserTicsThread, unsigned long long &pLastKernelTicsThread, unsigned long long &pLastUserTicsSystem, unsigned long long &pLastKernelTicsSystem)
{
	bool tResult = false;

	#ifdef LINUX
		FILE *tFile;
		char tFileName[256];
		unsigned long tJiffiesUserMode = 0, tJiffiesKernelMode = 0;
		unsigned long tSystemJiffiesUserMode = 0, tSystemJiffiesKernelMode = 0;
		unsigned long tSystemJiffies = 0;
		/* first entry in /proc/stat
		        "cpu"         id string
                user          normal processes executing in user mode
                nice          niced processes executing in user mode
                system        processes executing in kernel mode
                idle          twiddling thumbs
                iowait        waiting for I/O to complete
                irq           servicing interrupts
                softirq       servicing softirqs
		 */
        sprintf(tFileName, "/proc/stat");
        if ((tFile = fopen(tFileName, "r")) != NULL)
        {
            if (EOF == fscanf(tFile, "%*s %d %*d %d", (int*)&tSystemJiffiesUserMode, (int*)&tSystemJiffiesKernelMode))
            	LOGEX(Thread, LOG_ERROR, "Failed to parse file content because of input failure");
            fclose(tFile);
        }
        tSystemJiffies = tSystemJiffiesUserMode + tSystemJiffiesKernelMode;

        /* entries in /proc/self/task/#/stat:
                pid           process id
                tcomm         filename of the executable
                state         state (R is running, S is sleeping, D is sleeping in an none interruptible wait, Z is zombie, T is traced or stopped)
                ppid          process id of the parent process
                pgrp          pgrp of the process
                sid           session id
                tty_nr        tty the process uses
                tty_pgrp      pgrp of the tty
                flags         task flags
                min_flt       number of minor faults
                cmin_flt      number of minor faults with child's
                maj_flt       number of major faults
                cmaj_flt      number of major faults with child's
                utime         user mode jiffies
                stime         kernel mode jiffies
                cutime        user mode jiffies with child's
                cstime        kernel mode jiffies with child's
                priority      priority level
                nice          nice level
                num_threads   number of threads
                it_real_value (obsolete, always 0)
                start_time    time the process started after system boot
                vsize         virtual memory size
                rss           resident set memory size
                rsslim        current limit in bytes on the rss
                start_code    address above which program text can run
                end_code      address below which program text can run
                start_stack   address of the start of the stack
                esp           current value of ESP
                eip           current value of EIP
                pending       bitmap of pending signals (obsolete)
                blocked       bitmap of blocked signals (obsolete)
                sigign        bitmap of ignored signals (obsolete)
                sigcatch      bitmap of catched signals (obsolete)
                wchan         address where process went to sleep
                0             (place holder)
                0             (place holder)
                exit_signal   signal to send to parent thread on exit
                task_cpu      which CPU the task is scheduled on
                rt_priority   realtime priority
                policy        scheduling policy (man sched_setscheduler)
                blkio_ticks   time spent waiting for block IO
		*/
		sprintf(tFileName, "/proc/self/task/%d/stat", pTid);

		if ((tFile = fopen(tFileName, "r")) != NULL)
		{
			if (EOF == fscanf(tFile, "%d %*s %*c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %d %d %*d %*d %d %d %d %*d %*d %d %d", &pPid, &pPPid, (int*)&tJiffiesUserMode, (int*)&tJiffiesKernelMode, &pPriority, &pBasePriority, &pThreadCount, &pMemVirtual, &pMemPhysical))
				LOGEX(Thread, LOG_ERROR, "Failed to parse file content because of input failure");
			fclose(tFile);
			tResult = true;
		}
		pMemPhysical *= PAGE_SIZE; // this value is given in 4k pages
		pLoadUser = 100 * ((float)(tJiffiesUserMode - pLastUserTicsThread)) / (float)(tSystemJiffies - pLastUserTicsSystem - pLastKernelTicsSystem);
		pLoadSystem = 100 * ((float)(tJiffiesKernelMode - pLastKernelTicsThread)) / (float)(tSystemJiffies - pLastUserTicsSystem - pLastKernelTicsSystem);
		pLastUserTicsThread = tJiffiesUserMode;
		pLastKernelTicsThread = tJiffiesKernelMode;
        pLastUserTicsSystem = tSystemJiffiesUserMode;
		pLastKernelTicsSystem = tSystemJiffiesKernelMode;
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

			pMemPhysical = tProcessMemInfo.WorkingSetSize;
	        pMemVirtual = tProcessMemInfo.PagefileUsage;
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
						pBasePriority = tThreadEntry.tpBasePri;
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
				_ULARGE_INTEGER tTimeUserMode, tTimeKernelMode, tSysTime, tSysTimeKernelMode, tSysTimeUserMode;
				tTimeUserMode.LowPart = tThreadUserTime.dwLowDateTime;
				tTimeUserMode.HighPart = tThreadUserTime.dwHighDateTime;
				tTimeKernelMode.LowPart = tThreadKernelTime.dwLowDateTime;
				tTimeKernelMode.HighPart = tThreadKernelTime.dwHighDateTime;
				tSysTimeKernelMode.LowPart = tSystemKernelTime.dwLowDateTime;
				tSysTimeKernelMode.HighPart = tSystemKernelTime.dwHighDateTime;
				tSysTimeUserMode.LowPart = tSystemUserTime.dwLowDateTime;
				tSysTimeUserMode.HighPart = tSystemUserTime.dwHighDateTime;
				tSysTime.QuadPart = tSysTimeUserMode.QuadPart + tSysTimeKernelMode.QuadPart;
				// calculate the values for load related to user and kernel mode
				pLoadUser = 100 * ((float)(tTimeUserMode.QuadPart - pLastUserTicsThread)) / (float)(tSysTime.QuadPart - pLastUserTicsSystem - pLastKernelTicsSystem);
				pLoadSystem = 100 * ((float)(tTimeKernelMode.QuadPart - pLastKernelTicsThread)) / (float)(tSysTime.QuadPart - pLastUserTicsSystem - pLastKernelTicsSystem);
				pLastUserTicsThread = tTimeUserMode.QuadPart;
				pLastKernelTicsThread = tTimeKernelMode.QuadPart;
		        pLastUserTicsSystem = tSysTimeUserMode.QuadPart;
				pLastKernelTicsSystem = tSysTimeKernelMode.QuadPart;
			}else
			{
				LOGEX(Thread, LOG_ERROR, "Failed to get thread times");
				pLoadUser = 0;
		        pLoadSystem = 0;
				pLastUserTicsThread = 0;
				pLastKernelTicsThread = 0;
		        pLastUserTicsSystem = 0;
				pLastKernelTicsSystem = 0;
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
    return tThreadObject->mThreadMain(tThreadObject->mThreadArguments);
}

void* Thread::StartThreadStaticWrapperRun(void* pThread)
{
	LOGEX(Thread, LOG_VERBOSE, "Going to start thread main (Run method)");

    Thread *tThreadObject = (Thread*)pThread;
    return tThreadObject->Run(tThreadObject->mThreadArguments);
}

bool Thread::StartThread(void* pArgs)
{
    bool tResult = false;

    if (mThreadHandle != 0)
        return false;

    mThreadMain = 0;
    mThreadArguments = pArgs;

    #ifdef LINUX
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
             0,                      // use default stack size
             (LPTHREAD_START_ROUTINE)StartThreadStaticWrapperRun, // thread function name
             (LPVOID)this,           // argument to thread function
             0,                      // use default creation flags
             0);   					 // discard the thread identifier
        if (mThreadHandle == 0)
        	LOG(LOG_ERROR, "Creation of thread failed because of code \"%d\"", GetLastError());
        else
        	tResult = true;
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

	#ifdef LINUX
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
        return false;

    #ifdef LINUX
		struct timespec tTimeout;

		if (clock_gettime(CLOCK_REALTIME, &tTimeout) == -1)
			LOG(LOG_ERROR, "Failed to get time from realtime clock");

		// add 5 secs to current time stamp
		tTimeout.tv_sec += pTimeoutInMSecs / 1000;
		tTimeout.tv_nsec += (pTimeoutInMSecs % 1000) * 1000000;

		if (int tRes = pthread_timedjoin_np(mThreadHandle, &tThreadResult, &tTimeout))
			LOG(LOG_INFO, "Waiting for end of thread failed because of \"%s\"", strerror(tRes));
		else
		{
			LOG(LOG_VERBOSE, "Got end signal and thread results at %p", tThreadResult);
			tResult = true;
		}
	#endif
	#ifdef WIN32
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

	mThreadHandle = 0;
	return tResult;
}

void* Thread::Run(void* pArgs)
{
    LOG(LOG_ERROR, "You should overload the RUN method with your own implementation");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
