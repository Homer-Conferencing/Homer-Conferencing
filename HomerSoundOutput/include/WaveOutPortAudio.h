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

#ifndef _SOUNDOUT_WAVE_OUT_PORT_AUDIO_
#define _SOUNDOUT_WAVE_OUT_PORT_AUDIO_

#include <Header_Ffmpeg.h>
#include <Header_PortAudio.h>
#include <WaveOut.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
#define WOPA_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class WaveOutPortAudio:
    public WaveOut
{
public:
    WaveOutPortAudio(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutPortAudio();

    // playback control
    virtual bool Play();
    virtual void Stop();

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioOutDevicesList &pAList);
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096);

private:
    /* playback */
    PaStream            *mStream;
    AVFifoBuffer        *mSampleFifo;
    /* portaudio init. */
    static Mutex        mPaInitMutex;
    static bool         mPaInitiated;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
