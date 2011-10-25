/*
 * Name:    MediaSinkFile.cpp
 * Purpose: Implementation of a file based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2010-04-17
 * Version: $Id: MediaSinkFile.cpp,v 1.8 2011/08/03 21:54:16 chaos Exp $
 */

#include <Header_Ffmpeg.h>
#include <MediaSinkFile.h>
#include <MediaSourceNet.h>
#include <PacketStatistic.h>
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSinkFile::MediaSinkFile(string pSinkFile, enum MediaSinkType pType):
    MediaSinkNet(pSinkFile, 0, SOCKET_UDP, pType, false)
{
    mSinkFile = pSinkFile;
    // overwrite id from MediaSinkNet
    mMediaId = pSinkFile;
    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO, PACKET_TYPE_RAW);
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO, PACKET_TYPE_RAW);
            break;
        default:
            break;
    }
}

MediaSinkFile::~MediaSinkFile()
{
}

void MediaSinkFile::SendFragment(char* pPacketData, unsigned int pPacketSize, unsigned int pHeaderSize)
{
    #ifdef MSIF_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Storing packet number %6ld at %p with size %4u(%3u header) in file %s", ++mPacketNumber, pPacketData, pPacketSize, pHeaderSize, mSinkFile.c_str());

        // if RTP activated then reparse the current packet and print the content
        if ((mRtpActivated) && (pHeaderSize > 0))
        {
            char *tPacketData = pPacketData;
            unsigned int tPacketSize = pPacketSize;
            RtpParse(tPacketData, tPacketSize, mCurrentStream->codec->codec_id);
        }
    #endif
    AnnouncePacket(pPacketSize);

    FILE *tFile = fopen(mSinkFile.c_str(), "a+");
    if (tFile == NULL)
    {
        LOG(LOG_ERROR, "Couldn't write to file %s", mSinkFile.c_str());
        return;
    }

    size_t tSent = fwrite((void*)pPacketData, 1, (size_t) pPacketSize, tFile);
    fclose(tFile);

    if (tSent < pPacketSize)
        LOG(LOG_ERROR, "Insufficient data was written to file %s (%u < %u)", mSinkFile.c_str(), tSent, pPacketSize);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
