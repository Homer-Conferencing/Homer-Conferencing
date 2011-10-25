/*
 * Name:    MediaSink.h
 * Purpose: abstract media sink
 * Author:  Thomas Volkert
 * Since:   2009-01-06
 * Version: $Id: MediaSink.h,v 1.3 2010/11/07 19:13:32 chaos Exp $
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_
#define _MULTIMEDIA_MEDIA_SINK_

#include <PacketStatistic.h>
#include <Header_Ffmpeg.h>

#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

enum MediaSinkType
{
    MEDIA_SINK_UNKNOWN = -1,
    MEDIA_SINK_VIDEO,
    MEDIA_SINK_AUDIO
};

class MediaSink:
    public Homer::Monitor::PacketStatistic
{

public:
    MediaSink(enum MediaSinkType pType= MEDIA_SINK_UNKNOWN);

    virtual ~MediaSink();

public:
    virtual void ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream) = 0;

    std::string GetId() { return mMediaId; }

protected:
    unsigned long       mPacketNumber;
    std::string         mMediaId;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
