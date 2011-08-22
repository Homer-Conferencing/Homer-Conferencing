/*
 * Name:    LogSinkConsole.cpp
 * Purpose: Implementation of log sink for console output
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#include <LogSinkConsole.h>
#include <Logger.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <Windows.h>
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

#ifdef WIN32
HANDLE sConsoleHandle;
#endif

///////////////////////////////////////////////////////////////////////////////

LogSinkConsole::LogSinkConsole()
{
    mLogLevel = LOG_ERROR;
    mMediaId = "CONSOLE: standard out";
	#ifdef WIN32
		sConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	#endif
}

LogSinkConsole::~LogSinkConsole()
{
}

///////////////////////////////////////////////////////////////////////////////
/*
    LINUX COLORING
    --------------

    from http://www.termsys.demon.co.uk/vtansi.htm:

    Set Attribute Mode  <ESC>[{attr1};...;{attrn}m

    * Sets multiple display attribute settings. The following lists standard attributes:

      0 Reset all attributes
      1 Bright
      2 Dim
      4 Underscore
      5 Blink
      7 Reverse
      8 Hidden

        Foreground Colours
      30    Black
      31    Red
      32    Green
      33    Yellow
      34    Blue
      35    Magenta
      36    Cyan
      37    White

        Background Colours
      40    Black
      41    Red
      42    Green
      43    Yellow
      44    Blue
      45    Magenta
      46    Cyan
      47    White


    examples:
            \033[22;30m - black
            \033[22;31m - red
            \033[22;32m - green
            \033[22;33m - brown
            \033[22;34m - blue
            \033[22;35m - magenta
            \033[22;36m - cyan
            \033[22;37m - gray
            \033[01;30m - dark gray
            \033[01;31m - light red
            \033[01;32m - light green
            \033[01;33m - yellow
            \033[01;34m - light blue
            \033[01;35m - light magenta
            \033[01;36m - light cyan
            \033[01;37m - white

 */

void LogSinkConsole::ProcessMessage(int pLevel, string pTime, string pSource, int pLine, string pMessage)
{
    if ((pLevel <= mLogLevel) && (pLevel > LOG_OFF))
    {
        #ifdef LINUX
            switch(pLevel)
            {
                case LOG_ERROR:
                            printf("\033[22;36m(%s)\033[22;31m ERROR:   %s(%d):\033[01;31m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_INFO:
                            printf("\033[22;36m(%s)\033[01;30m INFO:    %s(%d):\033[01;37m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_VERBOSE:
                            printf("\033[22;36m(%s)\033[01;30m VERBOSE: %s(%d):\033[01;37m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
            }
        #endif
        #ifdef WIN32
			SetConsoleTextAttribute(sConsoleHandle, 3);
            printf("(%s) ", pTime.c_str());
            switch(pLevel)
            {
                case LOG_ERROR:
                            SetConsoleTextAttribute(sConsoleHandle, 4);
                            printf("ERROR:   %s(%d): ", pSource.c_str(), pLine);
                            SetConsoleTextAttribute(sConsoleHandle, 12);
                            break;
                case LOG_INFO:
                            SetConsoleTextAttribute(sConsoleHandle, 8);
                            printf("INFO:    %s(%d): ", pSource.c_str(), pLine);
                            SetConsoleTextAttribute(sConsoleHandle, 15);
                            break;
                case LOG_VERBOSE:
                            SetConsoleTextAttribute(sConsoleHandle, 8);
                            printf("VERBOSE: %s(%d): ", pSource.c_str(), pLine);
                            SetConsoleTextAttribute(sConsoleHandle, 15);
                            break;
            }
            printf("%s\n", pMessage.c_str());
            SetConsoleTextAttribute(sConsoleHandle, 7);
        #endif
    }
}

void LogSinkConsole::SetLogLevel(int pLevel)
{
	printf("Setting console log level to %d\n", pLevel);
	mLogLevel = pLevel;
}

int LogSinkConsole::GetLogLevel()
{
	return mLogLevel;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
