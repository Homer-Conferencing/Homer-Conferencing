/*****************************************************************************
 *
 * Copyright (C) 2013 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: wave out based on PulseAudio
 * Since:   2013-02-09
 */

#if defined(LINUX)
#ifndef _MULTIMEDIA_WAVE_OUT_PULSE_AUDIO_
#define _MULTIMEDIA_WAVE_OUT_PULSE_AUDIO_

#include <WaveOut.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define WOPUA_DEBUG_PACKETS
//#define WOPUA_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////

class WaveOutPulseAudio:
    public WaveOut
{
public:
    WaveOutPulseAudio(std::string pOutputName, std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutPulseAudio();

    // playback control
    virtual bool Play();
    virtual void Stop();

    static bool PulseAudioAvailable();

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, int pOutputChannels = 2);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(AudioDevices &pAList);

private:
    virtual void DoWriteChunk(char *pChunkBuffer, int pChunkSize);

    struct pa_simple         *mOutputStream;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
