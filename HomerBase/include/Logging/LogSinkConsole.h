/*
 * Name:    LogSinkConsole.h
 * Purpose: header file for log sink for console outputs
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#ifndef _LOGGER_LOG_SINK_CONSOLE_
#define _LOGGER_LOG_SINK_CONSOLE_

#include <LogSink.h>
#include <string>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSinkConsole:
    public LogSink
{
public:

    LogSinkConsole();

    /// The destructor
    virtual ~LogSinkConsole();

    void SetLogLevel(int pLevel);
    int GetLogLevel();
    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

private:
    int         mLogLevel;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
