/*
 * Name:    MediaSourceNet.h
 * Purpose: ffmpeg based network video source
 * Author:  Thomas Volkert
 * Since:   2008-12-16
 * Version: $Id: MediaSourceNet.h,v 1.31 2011/09/09 09:42:55 chaos Exp $
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_NET_
#define _MULTIMEDIA_MEDIA_SOURCE_NET_

#include <Header_Ffmpeg.h>
#include <HBSocket.h>
#include <HBThread.h>
#include <MediaSource.h>
#include <MediaSourceMem.h>
#include <RTP.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSN_DEBUG_PACKETS

// maximum number of acceptable continuous receive errors
#define MAX_RECEIVE_ERRORS                                  3

///////////////////////////////////////////////////////////////////////////////

class MediaSourceNet :
    public MediaSourceMem, public Thread
{
public:
    /// The constructor
    MediaSourceNet(Socket *pDataSocket, bool pRtpActivated = true);
    MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType, bool pRtpActivated = true);
    /// The destructor
    virtual ~MediaSourceNet();

    unsigned int getListenerPort();

    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);

    virtual void StopGrabbing();

    virtual void* Run(void* pArgs = NULL);
protected:

    char                *mPacketBuffer;
    unsigned int        mListenerPort;
    Socket              *mDataSocket;
    bool				mListenerRunning;
    bool                mListenerSocketOutside;
    int                 mReceiveErrors;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
