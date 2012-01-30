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
 * Purpose: Implementation of log sink for file output
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 */

#include <LogSinkFile.h>
#include <Logger.h>

#include <Header_Windows.h>

#include <string.h>
#include <stdio.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

LogSinkFile::LogSinkFile(string pFileName)
{
    mFileName = pFileName;
    mLogSinkId = "FILE: " + mFileName;
    ProcessMessage(LOG_VERBOSE, "", "", 0, "");
    ProcessMessage(LOG_VERBOSE, "", "", 0, "======================================");
    ProcessMessage(LOG_VERBOSE, "", "", 0, "========> LOGGING START POINT <=======");
    ProcessMessage(LOG_VERBOSE, "", "", 0, "======================================");
    printf("Logging debug output to file \"%s\"\n", pFileName.c_str());
}

LogSinkFile::~LogSinkFile()
{
}

///////////////////////////////////////////////////////////////////////////////

void LogSinkFile::ProcessMessage(int pLevel, string pTime, string pSource, int pLine, string pMessage)
{

    FILE *tFile = fopen((__const char *)mFileName.c_str(), "a");
    if (tFile != NULL)
    {
        switch(pLevel)
        {
            case LOG_ERROR:
                    fprintf(tFile, "(%s) ERROR:   %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                    break;
            case LOG_WARN:
                    fprintf(tFile, "(%s) WARN:   %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                    break;
            case LOG_INFO:
                    fprintf(tFile, "(%s) INFO:    %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                    break;
            case LOG_VERBOSE:
                    fprintf(tFile, "(%s) VERBOSE: %s(%d): %s\n", pTime.c_str(), pSource.c_str(), pLine, pMessage.c_str());
                    break;
        }
        fclose(tFile);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
