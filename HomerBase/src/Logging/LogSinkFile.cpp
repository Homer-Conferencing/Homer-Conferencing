/*
 * Name:    LogSinkFile.cpp
 * Purpose: Implementation of log sink for file output
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#include <LogSinkFile.h>
#include <Logger.h>

#include <string.h>
#include <stdio.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

///////////////////////////////////////////////////////////////////////////////

LogSinkFile::LogSinkFile(string pFileName)
{
    mFileName = pFileName;
    mMediaId = "FILE: " + mFileName;
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
