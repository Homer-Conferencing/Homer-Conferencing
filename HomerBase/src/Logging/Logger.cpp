/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of the core logger
 * Author:  Thomas Volkert
 * Since:   2009-05-19
 */

/*
 * Hint: The logger doesn't check for a valid instance in every function.
 *		 It is assumed that the macros of the header file are used for logging.
 */

#include <Logger.h>
#include <HBReflection.h>
#include <HBTime.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <Header_Windows.h>

namespace Homer { namespace Base {

#ifdef WINDOWS
#ifndef __MINGW32__
// use secured vsprintf of MS Win
#define vsprintf vsprintf_s
#define strcpy strcpy_s
#endif
#endif

///////////////////////////////////////////////////////////////////////////////

using namespace std;

Logger sLogger;
bool sLoggerReady = false;

///////////////////////////////////////////////////////////////////////////////

Logger::Logger()
{
    mRegisteredSinks = 0;
    mLogLevel = LOG_ERROR;
    mLastMessageLogLevel = LOG_ERROR;
    mLastSource = "";
    mLastLine = 0;
    mRepetitionCount = 0;
	sLoggerReady = true;
    mLogSinkConsole = new LogSinkConsole();
}

Logger::~Logger()
{
    sLoggerReady = false;
}

Logger& Logger::GetInstance()
{
	return sLogger;
}

void Logger::RegisterLogSink(LogSink* pLogSink)
{
    if( !sLoggerReady)
    {
    	printf("Tried to register log sink when logger isn't available yet\n");
    	return;
    }

    LogSinksList::iterator tIt;
    bool tFound = false;
    string tId = pLogSink->GetId();

    if (tId == "")
    {
        LOG(LOG_ERROR, "Sink is ignored because its id is undefined");
        return;
    }

    printf("Registering log sink: %s\n", tId.c_str());

    // lock
    mLogSinksMutex.lock();

    for (tIt = mLogSinks.begin(); tIt != mLogSinks.end(); tIt++)
    {
        if ((*tIt)->GetId() == tId)
        {
            printf("Sink already registered\n");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    {
        mLogSinks.push_back(pLogSink);
        mRegisteredSinks++;
    }

    // unlock
    mLogSinksMutex.unlock();
}

void Logger::UnregisterLogSink(LogSink* pLogSink)
{
    if( !sLoggerReady)
    {
        printf("Tried to unregister log sink when logger isn't available yet\n");
        return;
    }

    LogSinksList::iterator tIt;
    bool tFound = false;
    string tId = pLogSink->GetId();

    if (tId == "")
    {
        LOG(LOG_ERROR, "Sink is ignored because its id is undefined");
        return;
    }

    printf("Unregistering log sink: %s\n", tId.c_str());

    // lock
    mLogSinksMutex.lock();

    for (tIt = mLogSinks.begin(); tIt != mLogSinks.end(); tIt++)
    {
        //printf("Comparing log sinks: %s = %s?\n", (*tIt)->GetId().c_str(), tId.c_str());
        if ((*tIt)->GetId() == tId)
        {
            printf("Sink found, deleting it..\n");
            // remove registration of log sink object
            tFound = true;
            mLogSinks.erase(tIt);
            mRegisteredSinks--;
            break;
        }
    }

    // unlock
    mLogSinksMutex.unlock();

    if (tFound)
    {// sink found
        LOG(LOG_VERBOSE, "..unregistered");
    }else
    {// we haven't found the sink

    }
}

///////////////////////////////////////////////////////////////////////////////

void Logger::RelayMessageToLogSinks(int pLevel, string pTime, string pSource, int pLine, string pMessage)
{
    if( !sLoggerReady)
    {
    	printf("Tried to relay log message when logger isn't available yet\n");
    	return;
    }

    // first debug output to console
    mLogSinkConsole->ProcessMessage(pLevel, pTime, pSource, pLine, pMessage);

    LogSinksList::iterator tIt;

    // lock
    mLogSinksMutex.lock();

    if (mLogSinks.size() > 0)
    {
        for (tIt = mLogSinks.begin(); tIt != mLogSinks.end(); tIt++)
        {
            (*tIt)->ProcessMessage(pLevel, pTime, pSource, pLine, pMessage);
        }
    }

    // unlock
    mLogSinksMutex.unlock();
}

void Logger::SetColoring(bool pState)
{
    mLogSinkConsole->SetColoring(pState);
}

void Logger::AddMessage(int pLevel, const char *pSource, int pLine, const char* pFormat, ...)
{
    if( !sLoggerReady)
    {
    	printf("LOGGER: tried to log message from %s(%d) when logger instance isn't valid\n", pSource, pLine);
    	return;
    }

    if ((pLevel > mLogLevel /* we won't produce an output in the console */) && (mRegisteredSinks == 0 /* only the console would get the message */))
    {// nothing to do

        // return immediately
        return;
    }

    va_list tVArgs;
    char tMessageBuffer[4 * 1024];

    va_start(tVArgs, pFormat);
    vsprintf(tMessageBuffer, pFormat, tVArgs);
    va_end(tVArgs);

    string tFinalSource, tFinalTime, tFinalMessage;
    int tHour, tMin, tSec;
    Time::GetNow(0, 0, 0, &tHour, &tMin, &tSec);
    tFinalTime = (tHour < 10 ? "0" : "") + toString(tHour) + ":" + (tMin < 10 ? "0" : "") + toString(tMin) + "." + (tSec < 10 ? "0" : "") + toString(tSec);
    tFinalSource = toString(pSource);
    tFinalMessage = toString(tMessageBuffer);

    // lock
    if (mLoggerMutex.lock(250))
    {
        if ((mLastMessage != tFinalMessage) || (mLastSource != tFinalSource) || (mLastLine != pLine))
        {
            if (mRepetitionCount)
            {
                RelayMessageToLogSinks(mLastMessageLogLevel, tFinalTime, mLastSource, mLastLine, "        LAST MESSAGE WAS REPEATED " + toString(mRepetitionCount) + " TIME(S)");
                mRepetitionCount = 0;
            }

            RelayMessageToLogSinks(pLevel, tFinalTime, tFinalSource, pLine, tFinalMessage);

            mLastMessageLogLevel = pLevel;
            mLastSource = tFinalSource;
            mLastMessage = tFinalMessage;
            mLastLine = pLine;
        }else
            mRepetitionCount++;

        // unlock
        mLoggerMutex.unlock();
    }else
    {
        if ((pLevel <= mLogLevel) && (pLevel > LOG_OFF))
        {
        	printf("LOGGER: system load is high, skipped locking at %s for %s(%d) and message \"%s\", will ignore this.\n", tFinalTime.c_str(), pSource, pLine, tFinalMessage.c_str());
        }
    }
}

void Logger::Init(int pLevel)
{
    SetLogLevel(pLevel);
}

void Logger::Deinit()
{
}

void Logger::SetLogLevel(int pLevel)
{
    if( !sLoggerReady)
    {
    	printf("Tried to set log level when logger isn't available yet\n");
    	return;
    }

	printf("Setting log level to %d\n", pLevel);
    switch(pLevel)
    {
        case LOG_ERROR:
                mLogLevel = pLevel;
                break;
        case LOG_WARN:
                LOG(LOG_INFO, "Set log level to: WARN");
                mLogLevel = pLevel;
                break;
        case LOG_INFO:
                LOG(LOG_INFO, "Set log level to: INFO");
                mLogLevel = pLevel;
                break;
        case LOG_VERBOSE:
                LOG(LOG_INFO, "Set log level to: VERBOSE");
                mLogLevel = pLevel;
                break;
        case LOG_WORLD:
                LOG(LOG_INFO, "Set log level to: WORLD");
                mLogLevel = pLevel;
                break;
        default:
                mLogLevel = LOG_OFF;
                break;
    }
    mLogSinkConsole->SetLogLevel(pLevel);
}

int Logger::GetLogLevel()
{
	if( !sLoggerReady)
    {
    	printf("Tried to get log level when logger isn't available yet\n");
		return LOG_VERBOSE;
    }

	return mLogLevel;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
