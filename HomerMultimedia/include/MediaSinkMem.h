/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: memory based media sink which supports RTP
 * Since:   2012-06-07
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_MEM_
#define _MULTIMEDIA_MEDIA_SINK_MEM_

#include <string>

#include <MediaFifo.h>
#include <MediaSink.h>
#include <RTP.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSIM_DEBUG_PACKETS

//#define MSIM_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////

class MediaSinkMem:
    public MediaSink, public RTP
{

public:
    MediaSinkMem(std::string pMediaId, enum MediaSinkType pType, bool pRtpActivated);

    virtual ~MediaSinkMem();

    virtual void ProcessPacket(AVPacket *pAVPacket, AVStream *pStream = NULL, std::string pStreamName = "");
    virtual void UpdateSynchronization(int64_t pReferenceNtpTimestamp, int64_t pReferenceFrameTimestamp);

    virtual int GetFragmentBufferCounter();
    virtual int GetFragmentBufferSize();

    virtual void ReadFragment(char *pData, int &pDataSize, int64_t &pFragmentNumber);
    virtual void StopProcessing();

protected:
    virtual void WriteFragment(char* pData, unsigned int pSize, int64_t pFragmentNumber);

    /* RTP stream handling */
    virtual bool OpenStreamer(AVStream *pStream, std::string pStreamName);
    virtual bool CloseStreamer();

protected:
    /* timstampes from higher layer */
    int64_t             mLastPacketPts;
    /* target */
    std::string         mTargetHost;
    unsigned int        mTargetPort;
    /* RTP stream handling */
    bool                mRtpActivated;
    int64_t             mIncomingAVStreamStartPts;
    int64_t             mIncomingAVStreamLastPts;
    bool                mIncomingFirstPacket;
    enum AVCodecID      mIncomingAVStreamCodecID;
    AVStream*           mIncomingAVStream;
    AVCodecContext*     mIncomingAVStreamCodecContext;
    /* general stream handling */
    bool                mWaitUntillFirstKeyFrame;
    /* queue handling */
    MediaFifo           *mSinkFifo;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
