/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: creating application context
 * Since:   2008-11-25
*/

#include <HBTime.h>
#include <HBThread.h>
#include <HomerApplication.h>
#include <Logger.h>
#include <Configuration.h>

#include <string>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#if HOMER_QT5
    #warning "QT5 support enabled"
	#include <QMessageLogContext>
#endif

#include <Header_Windows.h>

using namespace Homer::Gui;
using namespace std;

#if defined(LINUX) || defined(APPLE)
#include <execinfo.h>
void GetSignalDescription(int pSignal, string &pSignalName, string &pSignalDescription)
{
    switch(pSignal)
    {
        case 1:
            pSignalName = "SIGHUP";
            pSignalDescription = "hangup detected on controlling terminal or death of controlling process";
            break;
        case 2:
            pSignalName = "SIGINT";
            pSignalDescription = "interrupt from keyboard";
            break;
        case 3:
            pSignalName = "SIGQUIT";
            pSignalDescription = "quit from keyboard";
            break;
        case 4:
            pSignalName = "SIGILL";
            pSignalDescription = "illegal Instruction";
            break;
        case 6:
            pSignalName = "SIGABRT";
            pSignalDescription = "abort signal from abort()";
            break;
        case 8:
            pSignalName = "SIGFPE";
            pSignalDescription = "floating point exception";
            break;
        case 9:
            pSignalName = "SIGKILL";
            pSignalDescription = "kill signal";
            break;
        case 10:
            pSignalName = "SIGBUS";
            pSignalDescription = "bus error";
            break;
        case 11:
            pSignalName = "SIGSEGV";
            pSignalDescription = "invalid memory reference";
            break;
        case 12:
            pSignalName = "SIGSYS";
            pSignalDescription = "bad argument to system call";
            break;
        case 13:
            pSignalName = "SIGPIPE";
            pSignalDescription = "broken pipe: write to pipe with no readers";
            break;
        case 14:
            pSignalName = "SIGALRM";
            pSignalDescription = "timer signal from alarm()";
            break;
        case 15:
            pSignalName = "SIGTERM";
            pSignalDescription = "termination signal";
            break;
        case 18:
            pSignalName = "SIGCONT";
            pSignalDescription = "continue signal from tty";
            break;
        case 19:
            pSignalName = "SIGSTOP";
            pSignalDescription = "stop signal from tty";
            break;
        case 20:
            pSignalName = "SIGTSTP";
            pSignalDescription = "stop signal from user (keyboard)";
            break;
        case 16:
        case 30:
            pSignalName = "SIGUSR1";
            pSignalDescription = "user-defined signal 1";
            break;
        case 17:
        case 31:
            pSignalName = "SIGUSR2";
            pSignalDescription = "user-defined signal 2";
            break;
        default:
            pSignalName = "unsupported signal";
            pSignalDescription = "unsupported signal occurred";
            break;
    }
}

#include <cxxabi.h>

int64_t sStopTime = -1;
static void HandlerSignal(int pSignal, siginfo_t *pSignalInfo, void *pArg)
{
    string tSignalName;
    string tSignalDescription;
    GetSignalDescription(pSignal, tSignalName, tSignalDescription);
    LOGEX(MainWindow, LOG_WARN, "Signal \"%s\"(%d: %s) detected.", tSignalName.c_str(), pSignal, tSignalDescription.c_str());
    if (pSignalInfo != NULL)
    {
        switch(pSignal)
        {
            case SIGSEGV:
                {
                    LOGEX(MainWindow, LOG_ERROR, "The segmentation fault was caused at memory location: %p", pSignalInfo->si_addr);
                    void *tStackTrace[128];
                    int tStackTraceSize;
                    char **tStackTraceList;
                    int tStackTraceStep = 0;

                    tStackTraceSize = backtrace(tStackTrace, 128);
                    tStackTraceList = backtrace_symbols(tStackTrace, tStackTraceSize);

                    LOGEX(MainWindow, LOG_ERROR, "Resolved a backtrace with %d entries:", (int)tStackTraceSize);

                    size_t tFuncNameSize = 256;
                    char* tFuncnName = (char*)malloc(tFuncNameSize);

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
                            char* tFuncnName = abi::__cxa_demangle(tBeginFuncName, tFuncnName, &tFuncNameSize, &tDemanglingRes);
                            long tBinaryOffset = strtoul(tBeginBinaryOffset, NULL, 16);
                            if (tDemanglingRes == 0)
                            {
                                LOGEX(MainWindow, LOG_ERROR, "#%02d 0x%016x in %s:[%s] from %s", tStackTraceStep, tBinaryOffset, tFuncnName, tBeginFuncOffset, tBeginBinaryName);
                                tStackTraceStep++;
                            }else{
                                if(strlen(tBeginFuncName))
                                {
                                    LOGEX(MainWindow, LOG_ERROR, "#%02d 0x%016x in %s:[%s] from %s:[%s]", tStackTraceStep, tBinaryOffset, tBeginFuncName, tBeginFuncOffset, tBeginBinaryName);
                                    tStackTraceStep++;
                                }
                            }
                        }else{
                            //LOGEX(MainWindow, LOG_ERROR, "?? #%2d %s", i, tStackTraceList[i]);
                        }
                    }

                    free(tStackTraceList);
                    free(tFuncnName);

                    LOGEX(MainWindow, LOG_ERROR, "");
                    LOGEX(MainWindow, LOG_ERROR, "Homer Conferencing will exit now. Please, report this to the Homer development team.", tSignalName.c_str(), tSignalDescription.c_str());
                    LOGEX(MainWindow, LOG_ERROR, "-");
                    LOGEX(MainWindow, LOG_ERROR, "Restart Homer Conferencing via \"Homer -DebugOutputFile=debug.log\" to generate verbose debug data.");
                    LOGEX(MainWindow, LOG_ERROR, "Afterwards attach the file debug.log to your bug report and send both by mail to homer@homer-conferencing.com.");
                    LOGEX(MainWindow, LOG_ERROR, " ");
                    exit(0);
                }
                break;
            case SIGINT:
				{
					LOGEX(MainWindow, LOG_WARN, "Homer Conferencing will exit now...");
					exit(0);
				}
				break;
            case SIGTERM:
				{
					LOGEX(MainWindow, LOG_WARN, "Homer Conferencing will exit now...");
					exit(0);
				}
				break;
            case SIGTSTP:
            	{
            		LOGEX(MainWindow, LOG_WARN, "Suspending Homer Conferencing now...");
            		sStopTime = Time::GetTimeStamp();
            		kill(Thread::GetPId(), SIGSTOP);
            	}
				break;
            case SIGCONT:
            	{
            		//TODO: re-sync. RT - grabbing and all time measurements
            		LOGEX(MainWindow, LOG_WARN, "Continuing Homer Conferencing now...");
            		if (sStopTime != -1)
            		{
            			float tSuspendTime = ((float)(Time::GetTimeStamp() - sStopTime)) / 1000 / 1000;
						LOGEX(MainWindow, LOG_WARN, "Homer Conferencing was suspended for %.2f seconds.", tSuspendTime);
            		}else
                		LOGEX(MainWindow, LOG_ERROR, "Invalid timestamp found as start time of suspend mode");
            	}
				break;
            default:
                break;
        }
        if (pSignalInfo->si_errno != 0)
            LOGEX(MainWindow, LOG_VERBOSE, "This signal occurred because \"%s\"(%d)", strerror(pSignalInfo->si_errno), pSignalInfo->si_errno);
        if (pSignalInfo->si_code != 0)
            LOGEX(MainWindow, LOG_VERBOSE, "Signal code is %d", pSignalInfo->si_code);
    }
}

static void SetHandlers()
{
    // set handler
    struct sigaction tSigAction;
    memset(&tSigAction, 0, sizeof(tSigAction));
    sigemptyset(&tSigAction.sa_mask);
    tSigAction.sa_sigaction = HandlerSignal;
    tSigAction.sa_flags   = SA_SIGINFO; // Invoke signal-catching function with three arguments instead of one
    sigaction(SIGINT, &tSigAction, NULL);
    sigaction(SIGTERM, &tSigAction, NULL);
    sigaction(SIGTSTP, &tSigAction, NULL);
    sigaction(SIGCONT, &tSigAction, NULL);
    sigaction(SIGSEGV, &tSigAction, NULL);

    // set handler stack
    stack_t tStack;
    tStack.ss_sp = malloc(SIGSTKSZ);
    if (tStack.ss_sp == NULL)
    {
        LOGEX(MainWindow, LOG_ERROR, "Couldn't allocate signal handler stack");
        exit(1);
    }
    tStack.ss_size = SIGSTKSZ;
    tStack.ss_flags = 0;
    if (sigaltstack(&tStack, NULL) == -1)
    {
        LOGEX(MainWindow, LOG_ERROR, "Could not set signal handler stack");
        exit(1);
    }
}
#else
static void SetHandlers(){ }
#endif

const char* sCandle = ""
"░░░░░░░░░░░░░░█░░░░░░░░░░░░░░\n"
"░░░░░░░░░░░░░███░░░░░░░░░░░░░\n"
"░░░░░░░░░░░░██░██░░░░░░░░░░░░\n"
"░░░░░░░░░░░░██░██░░░░░░░░░░░░\n"
"░░░░░░░░░░░██░░░██░░░░░░░░░░░\n"
"░░░░░░░░░░██░░░░░██░░░░░░░░░░\n"
"░░░░░░░░░██░░░░░░░██░░░░░░░░░\n"
"░░░░░░░░██░░░░░░░░░██░░░░░░░░\n"
"░░░░░░░░██░░░░░░░░░██░░░░░░░░\n"
"░░░░░░░░░██░░░█░░░██░░░░░░░░░\n"
"░░░░░░░░░░░██░█░██░░░░░░░░░░░\n"
"░░░░░░░░░░░░░███░░░░░░░░░░░░░\n"
"░░░░░░░░░░█████████░░░░░░░░░░\n"
"░░░░░███████████████████░░░░░\n"
"░░░░█████████████████████░░░░\n"
"░░░███████████████████████░░░\n"
"░░░░█████████████████████░░░░\n"
"░░░░░███████████████████░░░░░\n"
"░░░░░░█████████████████░░░░░░\n"
"░░░░░░░░█████████████░░░░░░░░\n"
"░░░░░░░░░███████████░░░░░░░░░\n"
"░░░░░░░░░░█████████░░░░░░░░░░\n"
"░░░░░░█████████████████░░░░░░\n"
"░░░░░███████████████████░░░░░\n";

const char* sMerryXmas = ""
"░░░░░░░░░░░░░░░*░░░░░░░░░░░░░░░\n"
"░░░░░░░░░░░░░░*o*░░░░░░░░░░░░░░\n"
"░░░░░░░░░░░░░*o*o*░░░░░░░░░░░░░\n"
"░░░░░░░░░░░░*o*o*o*░░░░░░░░░░░░\n"
"░░░░░░░░░░░*o*o*o*o*░░░░░░░░░░░\n"
"░░░░░░░░░░*o*o*o*o*o*░░░░░░░░░░\n"
"░░░░░░░░░*o* HO-HO *o*░░░░░░░░░\n"
"░░░░░░░░*o*o*o*o*o*o*o*░░░░░░░░\n"
"░░░░░░░░░░*o*o*o*o*o*░░░░░░░░░░\n"
"░░░░░░░░░*o*o*o*o*o*o*░░░░░░░░░\n"
"░░░░░░░░*o*o*o*o*o*o*o*░░░░░░░░\n"
"░░░░░░░*o HO-HOO-HOOO o*░░░░░░░\n"
"░░░░░░*o*o*o*o*o*o*o*o*o*░░░░░░\n"
"░░░░░░░░░░*o*o*o*o*o*░░░░░░░░░░\n"
"░░░░░░░░░*o*o*o*o*o*o*░░░░░░░░░\n"
"░░░░░░░░*o*  MERRY  *o*░░░░░░░░\n"
"░░░░░░░*o* CHRISTMAS *o*░░░░░░░\n"
"░░░░░░*o*o*o*o + o*o*o*o*░░░░░░\n"
"░░░░░*o*o*o* HAPPY *o*o*o*░░░░░\n"
"░░░░*o*o*o*o* NEW *o*o*o*o*░░░░\n"
"░░░*o*o*o* YEAR %d *o*o*o*░░░\n";

static void showMood()
{
    int tDay, tMonth, tYear;
    Time::GetNow(&tDay, &tMonth, &tYear);

    if ((tDay < 24) && (tMonth == 12))
    {
        printf("\nLooking forward to Christmas? Then it's time for a candle\n");
        printf("%s", sCandle);
    }

    if ((tDay > 23) && (tMonth == 12))
    {
        printf(sMerryXmas, tYear + 1);
    }
}

static void sQt4DebugMessageOutput(QtMsgType pType, const char *pMsg)
{
    // ignore buggy Qt warnings about mysterious Qt timers
    string tCurMsg = string(pMsg);
    if (tCurMsg.find("Fix application.") != string::npos)
        return;

    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		switch (pType)
		{
			case QtDebugMsg:
				LOGEX(MainWindow, LOG_INFO, "\033[01;33m QtDebug: \"%s\"", pMsg);
				break;
			case QtWarningMsg:
				LOGEX(MainWindow, LOG_INFO, "\033[01;33m QtWarning: \"%s\"", pMsg);
				break;
			case QtCriticalMsg:
				LOGEX(MainWindow, LOG_ERROR, "\033[01;33m QtCritical: \"%s\"", pMsg);
				break;
			case QtFatalMsg:
				LOGEX(MainWindow, LOG_ERROR, "\033[01;33m QtFatal: \"%s\"", pMsg);
				abort();
				break;
		}
	#endif
	#ifdef WINDOWS
		switch (pType)
		{
			case QtDebugMsg:
				LOGEX(MainWindow, LOG_INFO, " QtDebug: \"%s\"", pMsg);
				break;
			case QtWarningMsg:
				LOGEX(MainWindow, LOG_INFO, " QtWarning: \"%s\"", pMsg);
				break;
			case QtCriticalMsg:
				LOGEX(MainWindow, LOG_ERROR, " QtCritical: \"%s\"", pMsg);
				break;
			case QtFatalMsg:
				LOGEX(MainWindow, LOG_ERROR, " QtFatal: \"%s\"", pMsg);
				abort();
				break;
		}
	#endif
}

#if HOMER_QT5
static void sQt5DebugMessageOutput(QtMsgType pType, const QMessageLogContext &pContext, const QString &pMsg)
{
	QByteArray tLocalMsg = pMsg.toLocal8Bit();
	string tDebugMessage = pContext.function;// + "(" + toString(pContext.line) + "): " + string(tLocalMsg.constData());
	sQt4DebugMessageOutput(pType, tDebugMessage.c_str());
}
#endif

///////////////////////////////////////////////////////////////////////////////
#if defined(LINUX) || defined(APPLE) || defined(BSD)

int main(int pArgc, char* pArgv[])
{

#endif


///////////////////////////////////////////////////////////////////////////////
#if defined(WINDOWS)

static const WORD CONSOLE_HISTORY = 1000; // lines
static bool sIOIsRedirected = false; // make sure we redirect the I/O only once

void RedirectIOToConsole()
{
	if (sIOIsRedirected)
		return;

	int tConHandle;
	HANDLE tStdOutHandle;
	CONSOLE_SCREEN_BUFFER_INFO tConScreenBufferInfo;
	FILE *fp;

	AllocConsole();

	// get the console settings
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &tConScreenBufferInfo);

	// adapt the console history
	tConScreenBufferInfo.dwSize.Y = CONSOLE_HISTORY;

	// set adapted console settings
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), tConScreenBufferInfo.dwSize);


	//#####################
	// redirect STDOUT to the console
	//#####################
	tStdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	tConHandle = _open_osfhandle((long)tStdOutHandle, _O_TEXT);
	fp = _fdopen(tConHandle, "w");

	// overwrite STD-OUT
	*stdout = *fp;

	setvbuf(stdout, NULL, _IONBF, 0);


	//#####################
	// redirect STDIN to the console
	//#####################
	tStdOutHandle = GetStdHandle(STD_INPUT_HANDLE);
	tConHandle = _open_osfhandle((long)tStdOutHandle, _O_TEXT);
	fp = _fdopen(tConHandle, "r");

	// overwrite STD-IN
	*stdin = *fp;

	setvbuf(stdin, NULL, _IONBF, 0);

	//#####################
	// redirect STDERR to the console
	//#####################
	tStdOutHandle = GetStdHandle(STD_ERROR_HANDLE);
	tConHandle = _open_osfhandle((long)tStdOutHandle, _O_TEXT);
	fp = _fdopen(tConHandle, "w");

	// overwrite STD-ERR
	*stderr = *fp;

	setvbuf(stderr, NULL, _IONBF, 0);

	// let "cout, wcout, cin, wcin, wcerr, cerr, wclog, clog" point to the console
	ios::sync_with_stdio();

	sIOIsRedirected = true;
}

bool StartedFromWindowsConsole()
{
    HANDLE tParentProcHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Thread::GetPPId());
    if (tParentProcHandle)
    {
        TCHAR tPath[MAX_PATH];
        if (GetModuleFileNameExA(tParentProcHandle, 0, tPath, MAX_PATH))
        {// got full path to the executable
        	size_t tPathLen = strlen(tPath);
        	const char *tExecutableName = strrchr(tPath, '\\');
        	tExecutableName++;
        	if (strcmp(tExecutableName, "cmd.exe") == 0)
        		return true;
        }else
        {// error
        	// this can be caused when we are 32 bit and the cmd.exe is a 64 bit executable (which normally is)
        	LOGEX(HomerApplication, LOG_ERROR, "Unable to get module file name because %s(%d, %u)", strerror(errno), errno, (unsigned int)GetLastError());
        }
        CloseHandle(tParentProcHandle);
    }else
    	LOGEX(HomerApplication, LOG_ERROR, "Unable to open parent process %d", Thread::GetPPId());

	char * tTerm = getenv("TERM");
	if (tTerm != NULL)
	{
		if (strcmp(tTerm, "") != 0)
		{// Cygwin/MinGW command line
			printf("Cygwin/MinGW console detected\n");
			return false;
		}
	}

	// GUI
	return false;
}

int WINAPI WinMain(HINSTANCE pInstance,	HINSTANCE pPrevInstance, LPSTR pCmdLine, int pShowCmd)
{
	int pArgc = 0;
	char *tArgv[16];
	char **pArgv = &tArgv[0];
	LPWSTR *tArgvWin = CommandLineToArgvW(GetCommandLineW(), &pArgc);

	// convert wide char based strings to ANSI based
	if ((tArgvWin != NULL) && (pArgc > 0))
	{
		for(int j = 0; j < pArgc; j++)
		{
			if (j >= 16)
				break;
			wstring tWideArg = tArgvWin[j];
			string tAnsiArg;
			pArgv[j] = (char*)malloc(tWideArg.length() + 1);
			size_t i = 0;
			for (i = 0; i < tWideArg.length();i++)
				pArgv[j][i] = tWideArg[i];
			pArgv[j][i] = 0;
		}
	}

	if (StartedFromWindowsConsole())
		RedirectIOToConsole();

#endif

///////////////////////////////////////////////////////////////////////////////
	// active memory debugger as early as possible
	Thread::ActiveMemoryDebugger();

	string tFirstArg = (pArgc > 1) ? pArgv[1] : "";

	if ((tFirstArg == "-version") || (tFirstArg == "--version"))
	{
        printf("Homer Conferencing, version "RELEASE_VERSION_STRING"\n");
        exit(0);
	}

    #ifdef RELEASE_VERSION
	    printf("Homer Conferencing, version "RELEASE_VERSION_STRING"\n");
        printf("For updates visit http://www.homer-conferencing.com\n");
    #endif

	if ((tFirstArg == "-help") || (tFirstArg == "-?") || (tFirstArg == "-h") || (tFirstArg == "--help"))
	{

		#ifdef WINDOWS
			RedirectIOToConsole();
		#endif

		printf("\n");
		printf("Usage:\n");
		printf("   Homer [Options]\n");
		printf("\n");
		printf("Options:\n");
		printf("   -help                               show this help text and exit\n");
		printf("   -version                            show version information and exit\n");
		printf("\n");
		printf("Options for failure recovery:\n");
		printf("   -SetDefaults                        start the program with default settings\n");
		printf("   -DebugLevel=<level>                 defines the level of debug outputs, possible values are: \"Error, Info, Verbose, World\"\n");
		printf("   -DebugOutputFile=<file>             write verbose debug data to the given file\n");
		printf("   -DebugOutputNetwork=<host>:<port>   send verbose debug data to the given target host and port, UDP is used for message transport\n");
		printf("\n");
		printf("Options for feature selection:\n");
		printf("   -Disable=AudioCapture               disable audio capture from devices\n");
		printf("   -Disable=AudioOutput                disable audio playback support\n");
		printf("   -Disable=Conferencing               disable conference functions (disables ports for SIP/STUN management and file transfers)\n");
		printf("   -Disable=IPv6                       disable IPv6 support\n");
		printf("   -Disable=QoS                        disable QoS support\n");
		printf("   -Enable=NetSim                      enable network simulator\n");
		printf("   -ListVideoCodecs                    list all supported video codecs of the used libavcodec\n");
		printf("   -ListAudioCodecs                    list all supported audio codecs of the used libavcodec\n");
		printf("   -ListInputFormats                   list all supported input formats of the used libavformat\n");
		printf("   -ListOutputFormats                  list all supported output formats of the used libavformat\n");
		printf("   -ShowBroadcastInFullScreen          show the broadcast view in fullscreen mode\n");
		printf("   -ShowPreviewInFullScreen            show the preview view in fullscreen mode\n");
		printf("   -ShowPreviewNetworkStreams          show a preview of network streams\n");
		printf("\n");
		#ifdef RELEASE_VERSION
			#ifdef WINDOWS
				while(true)
				{

				}
			#endif
		#endif
		exit(0);
	}

	SetHandlers();

	HomerApplication *tApp = new HomerApplication(pArgc, pArgv);

	LOGEX(HomerApplication, LOG_VERBOSE, "Setting Qt message handler");
	#if HOMER_QT5
		qInstallMessageHandler(sQt5DebugMessageOutput);
	#else
		qInstallMsgHandler(sQt4DebugMessageOutput);
	#endif
	showMood();

    if (tApp != NULL)
    	tApp->showGUI();
    else
    	LOGEX(HomerApplication, LOG_ERROR, "Invalid HomerApplication object");

    LOGEX(HomerApplication, LOG_VERBOSE, "Executing Qt main window");
    tApp->exec();

    return 0;
}
