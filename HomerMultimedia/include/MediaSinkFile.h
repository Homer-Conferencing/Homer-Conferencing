/*
 * Name:    MediaSinkFile.h
 * Purpose: file based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2010-04-17
 * Version: $Id: MediaSinkFile.h,v 1.2 2010/10/27 16:00:53 chaos Exp $
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_FILE_
#define _MULTIMEDIA_MEDIA_SINK_FILE_

#include <string>

#include <MediaSinkNet.h>
#include <RTP.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSIF_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSinkFile:
    public MediaSinkNet
{

public:
    MediaSinkFile(std::string pSinkFile, enum MediaSinkType pType= MEDIA_SINK_UNKNOWN);

    virtual ~MediaSinkFile();

protected:
    virtual void SendFragment(char* pPacketData, unsigned int pPacketSize, unsigned int pHeaderSize);

private:
    std::string         mSinkFile;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
