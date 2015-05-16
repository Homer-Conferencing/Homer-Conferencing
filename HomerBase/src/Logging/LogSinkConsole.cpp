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
 * Purpose: Implementation of log sink for console output
 * Since:   2011-07-27
 */

#include <LogSinkConsole.h>
#include <Logger.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <Header_Windows.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

#ifdef WINDOWS
HANDLE sConsoleHandle;
#endif

///////////////////////////////////////////////////////////////////////////////

LogSinkConsole::LogSinkConsole()
{
    mLogSinkId = "CONSOLE: standard out";
    mColoring = true;
	#ifdef WINDOWS
		sConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	#endif
}

LogSinkConsole::~LogSinkConsole()
{
	// reset to default color values
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		printf("\033[01;37m \n");
	#endif
	#ifdef WINDOWS
    	SetConsoleTextAttribute(sConsoleHandle, 7);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
void LogSinkConsole::SetColoring(bool pState)
{
    mColoring = pState;
}

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

        Foreground Colors
      30    Black
      31    Red
      32    Green
      33    Yellow
      34    Blue
      35    Magenta
      36    Cyan
      37    White

        Background Colors
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
    if ((pLevel <= LOGGER.GetLogLevel()) && (pLevel > LOG_OFF))
    {
        if(mColoring)
        {
            #if defined(LINUX) || defined(APPLE) || defined(BSD)
                switch(pLevel)
                {
                    case LOG_ERROR:
                                printf("\033[22;36m(%s)\033[22;31m ERROR:   %s(%d):\033[01;31m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                                break;
                    case LOG_WARN:
                                printf("\033[22;36m(%s)\033[22;33m WARN:    %s(%d):\033[01;33m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                                break;
                    case LOG_INFO:
                                printf("\033[22;36m(%s)\033[01;30m INFO:    %s(%d):\033[01;37m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                                break;
                    case LOG_VERBOSE:
                                printf("\033[22;36m(%s)\033[01;30m VERBOSE: %s(%d):\033[01;37m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                                break;
                    case LOG_WORLD:
								printf("\033[22;36m(%s)\033[01;30m WORLD: %s(%d):\033[01;37m %s\033[00;22;35m\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
								break;
                }
            #endif
            #ifdef WINDOWS
                SetConsoleTextAttribute(sConsoleHandle, 3);
                printf("(%s) ", pTime.c_str());
                switch(pLevel)
                {
                    case LOG_ERROR:
                                SetConsoleTextAttribute(sConsoleHandle, 4);
                                printf("ERROR:   %s(%d): ", pSource.c_str(), pLine);
                                SetConsoleTextAttribute(sConsoleHandle, 12);
                                break;
                    case LOG_WARN:
                                SetConsoleTextAttribute(sConsoleHandle, 6);
                                printf("WARN:    %s(%d): ", pSource.c_str(), pLine);
                                SetConsoleTextAttribute(sConsoleHandle, 14);
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
                    case LOG_WORLD:
                                SetConsoleTextAttribute(sConsoleHandle, 8);
                                printf("WORLD: %s(%d): ", pSource.c_str(), pLine);
                                SetConsoleTextAttribute(sConsoleHandle, 15);
                                break;
                }
                printf("%s\n", pMessage.c_str());
                SetConsoleTextAttribute(sConsoleHandle, 7);
            #endif
        }else
        {
            switch(pLevel)
            {
                case LOG_ERROR:
                            printf("(%s) ERROR:   %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_WARN:
                            printf("(%s) WARN:    %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_INFO:
                            printf("(%s) INFO:    %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_VERBOSE:
                            printf("(%s) VERBOSE: %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
                case LOG_WORLD:
                            printf("(%s) WORLD: %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                            break;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
