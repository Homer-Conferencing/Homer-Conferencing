/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: ffmpeg based memory video source
 * Author:  Thomas Volkert
 * Since:   2011-05-05
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_MEM_
#define _MULTIMEDIA_MEDIA_SOURCE_MEM_

#include <Header_Ffmpeg.h>
#include <MediaFifo.h>
#include <MediaSource.h>
#include <RTP.h>

#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSMEM_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

// size of one single fragment of a frame packet
#define MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE                8*1024 // 8 kB (for jumbo packets!)

#define MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT 	            512 // 512 entries á 8 kB

///////////////////////////////////////////////////////////////////////////////

struct MediaInputQueueEntry
{
	char	*Data;
	int		Size;
};

///////////////////////////////////////////////////////////////////////////////

class MediaSourceMem :
    public MediaSource, public RTP
{
public:
    /// The default constructor
    MediaSourceMem(bool pRtpActivated = true);
    /// The destructor.
    virtual ~MediaSourceMem();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual int GetChunkDropCounter();
    virtual int GetFragmentBufferCounter();
    virtual int GetFragmentBufferSize();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();

    /* recording */
    virtual bool SupportsRecording();

    /* relaying */
    virtual bool SupportsRelaying();

    virtual bool SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset = false, bool pRtpActivated = true);

    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

    virtual void WriteFragment(char *pBuffer, int pBufferSize);

protected:
    static int GetNextPacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);
    virtual void ReadFragment(char *pData, ssize_t &pDataSize);

    unsigned long       mPacketNumber;
    enum CodecID        mStreamCodecId;
    char                *mStreamPacketBuffer;
    char                *mFragmentBuffer;
    int					mResXLastGrabbedFrame, mResYLastGrabbedFrame;
    bool                mRtpActivated;
    bool                mOpenInputStream;
    int                 mWrappingHeaderSize;
    MediaFifo           *mDecoderFifo;
    int                 mPacketStatAdditionalFragmentSize; // used to adapt packet statistic to additional fragment header, which is used for TCP transmission
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
