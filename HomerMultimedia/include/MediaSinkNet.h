/*
 * Name:    MediaSinkNet.h
 * Purpose: network based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2009-12-27
 * Version: $Id: MediaSinkNet.h,v 1.19 2011/09/09 10:54:57 chaos Exp $
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_NET_
#define _MULTIMEDIA_MEDIA_SINK_NET_

#include <Header_Ffmpeg.h>
#include <HBSocket.h>
#include <MediaSink.h>
#include <RTP.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSIN_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSinkNet:
    public MediaSink, public RTP
{

public:
    MediaSinkNet(std::string pTargetHost, unsigned int pTargetPort, enum TransportType pSocketType = SOCKET_UDP, enum MediaSinkType pType = MEDIA_SINK_UNKNOWN, bool pRtpActivated = true);

    virtual ~MediaSinkNet();

    virtual void ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream = NULL);
    static std::string CreateId(std::string pHost, std::string pPort, enum TransportType pSocketTransportType = SOCKET_TRANSPORT_TYPE_INVALID, bool pRtpActivated = true);

protected:
    /* automatic state handling */
    virtual bool OpenStreamer(AVStream *pStream);
    virtual bool CloseStreamer();
    /* sending one single fragment of an (rtp) packet stream */
    virtual void SendFragment(char* pPacketData, unsigned int pPacketSize, unsigned int pHeaderSize);

    bool                mRtpActivated;
    AVStream            *mCurrentStream;

private:
    std::string         mTargetHost;
    unsigned int        mTargetPort;
    std::string         mCodec;
    Socket              *mDataSocket;
    bool                mStreamerOpened;
    bool                mBrokenPipe;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
