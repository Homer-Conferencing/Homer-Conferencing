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
 * Purpose: abstract wave out
 * Author:  Thomas Volkert
 * Since:   2010-11-30
 */

#ifndef _MULTIMEDIA_WAVE_OUT_
#define _MULTIMEDIA_WAVE_OUT_

#include <MediaSource.h>
#include <MediaFifo.h>
#include <MediaSourceFile.h>
#include <PacketStatistic.h>
#include <HBThread.h>
#include <HBCondition.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

class WaveOut:
    public Homer::Monitor::PacketStatistic, public Thread
{
public:
    WaveOut(std::string pName = "");

    /// The destructor
    virtual ~WaveOut();

    /* device interface */
    virtual bool SelectDevice(std::string pDeviceName = "");

    // playback control
    virtual bool Play();
    virtual void Stop();
    virtual bool IsPlaying();
    virtual bool PlayFile(std::string pFileName, int pLoops = 1);
    virtual std::string CurrentFile();

    /* volume control */
    virtual int GetVolume(); // range: 0-200 %
    virtual void SetVolume(int pValue);

public:
    /* abstract interface which has to be implemented by derived classes */

    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true) = 0;
    virtual bool CloseWaveOutDevice() = 0;
    /* device interface */
    virtual void getAudioDevices(AudioDevices &pAList) = 0;
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096) = 0;

private:
    /* playback of file */
    virtual void* Run(void* pArgs = NULL);
    bool DoOpenNewFile();

protected:
    virtual void AssignThreadName();

    /* file based playback - thread naming */
    bool                mHaveToAssignThreadName;
    /* device state */
    bool                mWaveOutOpened;
    bool                mPlaybackStopped;
    Mutex				mPlayMutex;
    /* device parameters */
    int                 mSampleRate;
    bool                mStereo;
    int                 mVolume;
    /* device handling */
    std::string         mDesiredDevice;
    std::string         mCurrentDevice;
    /* playback */
    AVFifoBuffer        *mSampleFifo; // needed to create audio buffers of fixed size (4096 bytes)
    MediaFifo           *mPlaybackFifo; // needed as FIFO buffer with prepared audio buffers for playback, avoid expensive operations like malloc/free (used when using AVFifoBuffer)
    /* playback of file */
    std::string         mFilePlaybackFileName;
    bool                mOpenNewFileAsap;
    bool                mFilePlaybackNeeded;
    Mutex               mOpenNewFile;
    MediaSourceFile     *mFilePlaybackSource;
    int					mFilePlaybackLoops;
    char                *mFilePlaybackBuffer;
    Condition           mFilePlaybackCondition;};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
