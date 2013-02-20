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
 * Author:  Thomas Volkert
 * Since:   2008-11-25
*/

#include <HBTime.h>
#include <HomerApplication.h>
#include <Logger.h>
#include <Configuration.h>

#include <string>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

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
            pSignalName = "SIGTSTP";
            pSignalDescription = "stop signal from tty";
            break;
        case 19:
            pSignalName = "SIGCONT";
            pSignalDescription = "continue signal from tty";
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

int64_t sStopTime = -1;
static void HandlerSignal(int pSignal, siginfo_t *pSignalInfo, void *pArg)
{
    string tSignalName;
    string tSignalDescription;
    GetSignalDescription(pSignal, tSignalName, tSignalDescription);
    LOGEX(MainWindow, LOG_WARN, "Signal \"%s\"(%s) detected.", tSignalName.c_str(), tSignalDescription.c_str());
    if (pSignalInfo != NULL)
    {
        switch(pSignal)
        {
            case SIGSEGV:
                {
                    LOGEX(MainWindow, LOG_ERROR, "The segmentation fault was caused at memory location: %p", pSignalInfo->si_addr);
                    void *tBtArray[64];
                    int tBtSize;
                    char **tBtStrings;

                    tBtSize = backtrace(tBtArray, 64);
                    tBtStrings = backtrace_symbols(tBtArray, tBtSize);

                    LOGEX(MainWindow, LOG_ERROR, "Resolved a backtrace with %d entries:", (int)tBtSize);

                    for (int i = 0; i < tBtSize; i++)
                        LOGEX(MainWindow, LOG_ERROR, "#%2d %s", i, tBtStrings[i]);

                    free(tBtStrings);
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
            		kill(getpid(), SIGSTOP);
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
            LOGEX(MainWindow, LOG_ERROR, "This signal occurred because \"%s\"(%d)", strerror(pSignalInfo->si_errno), pSignalInfo->si_errno);
        if (pSignalInfo->si_code != 0)
            LOGEX(MainWindow, LOG_ERROR, "Signal code is %d", pSignalInfo->si_code);
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

static void sQtDebugMessageOutput(QtMsgType pType, const char *pMsg)
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

///////////////////////////////////////////////////////////////////////////////
#if defined(LINUX) || defined(APPLE) || defined(BSD)
int main(int pArgc, char* pArgv[])
{
#endif
#if defined(WINDOWS)
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
#endif

	string tFirstArg = (pArgc > 1) ? pArgv[1] : "";

	if ((tFirstArg == "-help") || (tFirstArg == "-?") || (tFirstArg == "-h") || (tFirstArg == "--help"))
	{
	    printf("Homer Conferencing, version "RELEASE_VERSION_STRING"\n");
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
        printf("   -Enable=NetSim                      enables network simulator\n");
        printf("   -ListVideoCodecs                    list all supported video codecs of the used libavcodec\n");
        printf("   -ListAudioCodecs                    list all supported audio codecs of the used libavcodec\n");
        printf("   -ListInputFormats                   list all supported input formats of the used libavformat\n");
        printf("   -ListOutputFormats                  list all supported output formats of the used libavformat\n");
        printf("\n");
	    exit(0);
	}

	if ((tFirstArg == "-version") || (tFirstArg == "--version"))
	{
        printf("Homer Conferencing, version "RELEASE_VERSION_STRING"\n");
        exit(0);
	}

    #ifdef RELEASE_VERSION
	    printf("Homer Conferencing, version "RELEASE_VERSION_STRING"\n");
	    printf("Copyright (C) 2008-2013 Thomas Volkert <thomas@homer-conferencing.com>\n");
        printf("For updates visit http://www.homer-conferencing.com\n");
    #endif

	SetHandlers();

	HomerApplication *tApp = new HomerApplication(pArgc, pArgv);

	LOGEX(HomerApplication, LOG_VERBOSE, "Setting Qt message handler");
	qInstallMsgHandler(sQtDebugMessageOutput);
	showMood();

    if (tApp != NULL)
    	tApp->showGUI();
    else
    	LOGEX(HomerApplication, LOG_ERROR, "Invalid HomerApplication object");

    LOGEX(HomerApplication, LOG_VERBOSE, "Executing Qt main window");
    tApp->exec();

    return 0;
}
