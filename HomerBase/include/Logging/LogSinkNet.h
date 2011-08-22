/*
 * Name:    LogSinkNet.h
 * Purpose: header file for log sink for net outputs
 * Author:  Thomas Volkert
 * Since:   2011-07-29
 * Version: $Id$
 */

#ifndef _LOGGER_LOG_SINK_NET_
#define _LOGGER_LOG_SINK_NET_

#include <string>
#include <LogSink.h>
#include <HBSocket.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSinkNet:
    public LogSink
{
public:

    LogSinkNet(std::string pTargetHost, unsigned short pTargetPort);

    /// The destructor
    virtual ~LogSinkNet();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

private:
    std::string         mTargetHost;
    unsigned short      mTargetPort;
    Homer::Base::Socket *mDataSocket;
    bool                mBrokenPipe;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
