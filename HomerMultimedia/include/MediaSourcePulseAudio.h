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
 * Purpose: PulseAudio capturing
 * Author:  Thomas Volkert
 * Since:   2013-02-09
 */

#if defined(LINUX)
#ifndef _MULTIMEDIA_MEDIA_SOURCE_PULSE_AUDIO_
#define _MULTIMEDIA_MEDIA_SOURCE_PULSE_AUDIO_

#include <MediaFifo.h>
#include <MediaSource.h>
#include <WaveOutPulseAudio.h>

#include <string.h>
struct PaStreamCallbackTimeInfo;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSPUA_DEBUG_PACKETS
//#define MSPUA_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////

#define MAX_PULSEAUDIO_DEVICES_IN_LIST					32

struct PulseAudioDeviceDescriptor
{
		char 		Name[512];
		char 		Description[256];
		uint32_t 	Index;
		uint8_t 	Initialized;
};

///////////////////////////////////////////////////////////////////////////////

class MediaSourcePulseAudio:
    public MediaSource
{
public:
    MediaSourcePulseAudio(std::string pDesiredDevice = "");

    ~MediaSourcePulseAudio();

    /* device control */
    virtual void getAudioDevices(AudioDevices &pAList);

    /* recording */
    virtual bool SupportsRecording();

    /* grabbing control */
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

    static int GetPulseAudioDevices(PulseAudioDeviceDescriptor *pInputDevicesList, PulseAudioDeviceDescriptor *pOutputDevicesList);    friend class WaveOutPulseAudio;
    static bool PulseAudioAvailable();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropFrame = false);

private:

	struct pa_simple 		*mInputStream;
};

///////////////////////////////////////////////////////////////////////////////

}} //namespaces

#endif
#endif
