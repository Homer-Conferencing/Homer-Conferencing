/*
 * Name:    LogSinkFile.h
 * Purpose: header file for log sink for file outputs
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#ifndef _LOGGER_LOG_SINK_FILE_
#define _LOGGER_LOG_SINK_FILE_

#include <string>
#include <LogSink.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSinkFile:
    public LogSink
{
public:

    LogSinkFile(std::string pFileName);

    /// The destructor
    virtual ~LogSinkFile();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

private:
    std::string         mFileName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
