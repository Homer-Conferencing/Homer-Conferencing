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
 * Purpose: logo video source
 * Author:  Thomas Volkert
 * Since:   2012-10-25
 */

#ifndef _MEDIA_SOURCE_LOGO_
#define _MEDIA_SOURCE_LOGO_

#include <MediaSource.h>

#include <QWidget>
#include <QTime>
#include <QMutex>
#include <QWaitCondition>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

#define MEDIA_SOURCE_HOMER_LOGO		               "Homer-Conferencing logo"

// the following de/activates debugging of packets
//#define MSD_DEBUG_PACKETS

#define MSD_BYTES_PER_PIXEL                         4 //RGBX
#define MIN_GRABBING_FPS							5

///////////////////////////////////////////////////////////////////////////////

class MediaSourceLogo:
    public MediaSource
{
public:
	MediaSourceLogo(std::string pDesiredDevice = "");

    virtual ~MediaSourceLogo();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();
    virtual bool HasVariableVideoOutputFrameRate();

    /* device control */
    virtual void getVideoDevices(VideoDevices &pVList);

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:

    void                *mLogoRawPicture;
    QTime               mLastTimeGrabbed;
    QList<int64_t>		mFrameTimestamps;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
