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
 * Purpose: creating application context
 * Author:  Thomas Volkert
 * Since:   2008-11-25
*/

#include <QApplication>
#include <QDate>
#include <QWidget>
#include <QPixmap>
#include <QThread>
#include <QSplashScreen>
#include <QResource>

#include <HBTime.h>
#include <MainWindow.h>
#include <Logger.h>
#include <Configuration.h>

#include <string>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

using namespace Homer::Gui;
using namespace std;

#if defined(LINUX)
static void HandlerSigSegfault(int pSignal, siginfo_t *pSignalInfo, void *pArg)
{
    LOGEX(MainWindow, LOG_ERROR, "Segmentation fault detected, Homer will exit now. Please, report this bug to the Homer team.");
    LOGEX(MainWindow, LOG_ERROR, "-");
    LOGEX(MainWindow, LOG_ERROR, "Restart Homer via \"Homer -DebugOutputFile=debug.log\" to generate verbose debug data.");
    LOGEX(MainWindow, LOG_ERROR, "Afterwards attach the file debug.log to your bug report and sent it by mail to homer@homer-conferencing.com.");
    exit(0);
}

static void SetHandlers()
{
    struct sigaction tSigAction;

    memset(&tSigAction, 0, sizeof(tSigAction));
    sigemptyset(&tSigAction.sa_mask);
    tSigAction.sa_sigaction = HandlerSigSegfault;
    tSigAction.sa_flags   = SA_SIGINFO; // Invoke signal-catching function with three arguments instead of one

    sigaction(SIGSEGV, &tSigAction, NULL);
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
        printf((const char*)sCandle);
    }

//    if ((tDay > 23) && (tMonth == 12))
//    {
        printf(sMerryXmas, tYear + 1);
//    }
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
		}
	#endif
	#ifdef WIN32
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
		}
	#endif
}

///////////////////////////////////////////////////////////////////////////////
#if defined(LINUX) || defined(APPLE) || defined(BSD)
int main(int pArgc, char* pArgv[])
{
#endif
#ifdef WIN32
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

	SetHandlers();

	string tFirstArg = (pArgc > 1) ? pArgv[1] : "";

	if ((tFirstArg == "-help") || (tFirstArg == "-?") || (tFirstArg == "-h") || (tFirstArg == "--help"))
	{
	    printf("Homer-conferencing, Version "RELEASE_VERSION_STRING"\n");
	    printf("\n");
        printf("Usage:\n");
        printf("   Homer [Options]\n");
        printf("\n");
        printf("Options for logging:\n");
        printf("   -help                               show this help text and exit\n");
        printf("   -version                            show version information and exit\n");
        printf("   -DebugOutputFile=<file>             write verbose debug data to the given file\n");
        printf("   -DebugOutputNetwork=<host>:<port>   send verbose debug data to the given target host and port, UDP is used for message transport\n");
        printf("\n");
        printf("Options for features:\n");
        printf("   -Disable=IPv6                       disable IPv6 support\n");
        printf("   -Disable=QoS                        disable QoS support\n");
        printf("\n");
	    exit(0);
	}

	if ((tFirstArg == "-version") || (tFirstArg == "--version"))
	{
        printf("Homer-conferencing, Version "RELEASE_VERSION_STRING"\n");
        exit(0);
	}

    #ifdef RELEASE_VERSION
	    printf("Homer-conferencing, Version "RELEASE_VERSION_STRING"\n");
        printf("For updates visit http://www.homer-conferencing.com\n");
    #endif

    qInstallMsgHandler(sQtDebugMessageOutput);

    string tAbsBinPath;
    QApplication *tApp = new QApplication(pArgc, pArgv);

    // make sure every icon is visible within menus: otherwise the Ubuntu-packages will have no icons visible
    tApp->setAttribute(Qt::AA_DontShowIconsInMenus, false);

    // get the absolute path to our binary
    if (pArgc > 0)
    {
    	string tArgv0 = "";
		tArgv0 = pArgv[0];

		size_t tSize;
		tSize = tArgv0.rfind('/');

		// Windows path?
		if (tSize == string::npos)
			tSize = tArgv0.rfind('\\');

		// nothing found?
		if (tSize != string::npos)
			tSize++;
		else
			tSize = 0;
		tAbsBinPath = tArgv0.substr(0, tSize);
    }

    // load the icon resources
    QResource::registerResource(QString((tAbsBinPath + "Icons.rcc").c_str()));

    #ifdef RELEASE_VERSION
        QPixmap tLogo(":/images/Splash.png");
        QSplashScreen tSplashScreen(tLogo);
        tSplashScreen.show();
        Thread::Suspend(2 * 1000 * 1000);
    #endif

    QStringList tArguments = QCoreApplication::arguments();

    if (tArguments.contains("-DebugLevel=Error"))
    {
        LOGGER.Init(LOG_ERROR);
    }else
        if (tArguments.contains("-DebugLevel=Info"))
        {
            LOGGER.Init(LOG_INFO);
        }else
            if (tArguments.contains("-DebugLevel=Verbose"))
            {
                LOGGER.Init(LOG_VERBOSE);
            }else
            {
                #ifdef RELEASE_VERSION
                    LOGGER.Init(LOG_ERROR);
                #else
                    LOGGER.Init(LOG_VERBOSE);
                #endif
            }
    showMood();
    LOGEX(MainWindow, LOG_VERBOSE, "Creating Qt main window");

    MainWindow *tMainWindow = new MainWindow(tAbsBinPath);

    LOGEX(MainWindow, LOG_VERBOSE, "Showing Qt main window");
    tMainWindow->show();

    #ifdef RELEASE_VERSION
        LOGEX(MainWindow, LOG_VERBOSE, "Showing splash screen");
        tSplashScreen.finish(tMainWindow);
    #endif

    LOGEX(MainWindow, LOG_VERBOSE, "Executing Qt main window");
    tApp->exec();

    return 0;
}
