/*
 * Name:    MediaSink.cpp
 * Purpose: Implementation of an abstract media sink
 * Author:  Thomas Volkert
 * Since:   2009-01-06
 * Version: $Id: MediaSink.cpp,v 1.6 2010/12/11 00:09:29 chaos Exp $
 */

#include <MediaSink.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSink::MediaSink(enum MediaSinkType pType):
    PacketStatistic()
{
    SetOutgoingStream();
    mPacketNumber = 0;
    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO);
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO);
            break;
        default:
            break;
    }
}

MediaSink::~MediaSink()
{
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
