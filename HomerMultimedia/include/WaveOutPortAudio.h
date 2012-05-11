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
 * Purpose: wave out based on PortAudio
 * Author:  Thomas Volkert
 * Since:   2012-05-07
 */

#ifndef _MULTIMEDIA_WAVE_OUT_PORT_AUDIO_
#define _MULTIMEDIA_WAVE_OUT_PORT_AUDIO_

#include <Header_PortAudio.h>
#include <MediaSourceFile.h>
#include <WaveOut.h>
#include <MediaFifo.h>
#include <HBThread.h>
#include <HBCondition.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define WOPA_DEBUG_PACKETS
//#define WOPA_DEBUG_HANDLER
//#define WOPA_DEBUG_FILE

///////////////////////////////////////////////////////////////////////////////

class WaveOutPortAudio:
    public WaveOut, Thread
{
public:
    WaveOutPortAudio(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutPortAudio();

    // playback control
    virtual bool Play();
    virtual void Stop();
    virtual bool PlayFile(std::string pFileName);

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioDevicesList &pAList);
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096);

private:
    static int PlayAudioHandler(const void *pInputBuffer, void *pOutputBuffer, unsigned long pInputSize, const PaStreamCallbackTimeInfo* pTimeInfo, PaStreamCallbackFlags pStatus, void *pUserData);
    void AssignThreadName();

    /* main loop to play audio file */
    virtual void* Run(void* pArgs = NULL);
    bool DoOpenNewFile();

    bool                mHaveToAssignThreadName;
    /* playback */
    PaStream            *mStream;
    MediaFifo           *mPlaybackFifo; // needed as FIFO buffer with prepared audio buffers for playback, avoid expensive operations like malloc/free (used when using AVFifoBuffer)
    AVFifoBuffer        *mSampleFifo; // needed to create audio buffers of fixed size (4096 bytes)
    /* playback of file */
    std::string         mCurrentFile;
    bool                mOpenNewFileAsap;
    bool                mFilePlaybackNeeded;
    Mutex               mOpenNewFile;
    MediaSourceFile     *mFilePlaybackSource;
    char                *mFilePlaybackBuffer;
    Condition           mFilePlaybackCondition;
    /* portaudio init. */
    static Mutex        mPaInitMutex;
    static bool         mPaInitiated;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
