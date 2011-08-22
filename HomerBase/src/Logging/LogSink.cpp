/*
 * Name:    LogSink.cpp
 * Purpose: Implementation of abstract log sink
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id: LogSink.cpp 6 2011-08-22 13:06:22Z silvo $
 */

#include <LogSink.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

LogSink::LogSink()
{
    mMediaId = "undefined";
}

LogSink::~LogSink()
{
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
