/*
 * Name:    LogSink.cpp
 * Purpose: Implementation of abstract log sink
 * Author:  Thomas Volkert
 * Since:   2011-07-27
 * Version: $Id$
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
