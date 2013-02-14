/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: video scaler
 * Author:  Thomas Volkert
 * Since:   2012-08-09
 */

#ifndef _MULTIMEDIA_VIDEO_SCALER_
#define _MULTIMEDIA_VIDEO_SCALER_

#include <Header_Ffmpeg.h>
#include <HBMutex.h>
#include <HBThread.h>
#include <MediaFifo.h>
#include <RTP.h>

#include <vector>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of sent packets
//#define VS_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class VideoScaler:
    public Thread, public MediaFifo
{
public:
    VideoScaler(std::string pName);

    virtual ~VideoScaler();

    void StartScaler(int pInputQueueSize, int pSourceResX, int pSourceResY, enum PixelFormat pSourcePixelFormat, int pTargetResX, int pTargetResY, enum PixelFormat pTargetPixelFormat);
    void StopScaler();

    virtual void WriteFifo(char* pBuffer, int pBufferSize, int64_t pFrameTimestamp);
    virtual void ReadFifo(char *pBuffer, int &pBufferSize, int64_t &pFrameTimestamp); // memory copy, returns entire memory
    virtual void ClearFifo();

    virtual int GetEntrySize();
    virtual int GetUsage();
    virtual int GetSize();

    virtual void ChangeInputResolution(int pResX, int pResY);

private:
    // avoids memory copy, returns a pointer to memory
    int ReadFifoExclusive(char **pBuffer, int &pBufferSize, int64_t &pFrameTimestamp); // return -1 if internal FIFO isn't available yet
    void ReadFifoExclusiveFinished(int pEntryPointer);

    virtual void* Run(void* pArgs = NULL); // video scaler main loop

    std::string			mName;
    Mutex               mInputFifoMutex;
    MediaFifo           *mInputFifo;
    Mutex               mScalingThreadMutex; // we use this to avoid concurrent access to input FIFO/scaler context by ChangeInputResolution() and scaler-thread
    MediaFifo           *mOutputFifo;
    Mutex               mOutputFifoMutex;
    bool                mScalerNeeded;
    int                 mSourceResX;
    int                 mSourceResY;
    enum PixelFormat    mSourcePixelFormat;
    int                 mTargetResX;
    int                 mTargetResY;
    enum PixelFormat    mTargetPixelFormat;
    int                 mQueueSize;
    int                 mChunkNumber;
    SwsContext          *mVideoScalerContext;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
