/*
 * Name:    LogSink.h
 * Purpose: header file for abstract log sink
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
 */

#ifndef _LOGGER_LOG_SINK_
#define _LOGGER_LOG_SINK_

#include <string>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSink
{
public:

    LogSink();

    /// The destructor
    virtual ~LogSink();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage) = 0;

    std::string GetId() { return mMediaId; }

protected:
    std::string         mMediaId;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
