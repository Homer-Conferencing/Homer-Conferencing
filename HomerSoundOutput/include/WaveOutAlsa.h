/*
 * Name:    WaveOutAlsa.h
 * Purpose: wave out based on ALSA
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 * Version: $Id$
 */

#ifdef LINUX
#ifndef _SOUNDOUT_WAVE_OUT_ALSA_
#define _SOUNDOUT_WAVE_OUT_ALSA_

#include <WaveOut.h>
#include <Header_Alsa.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define WOA_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class WaveOutAlsa:
    public WaveOut
{
public:
    WaveOutAlsa(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutAlsa();

    // playback control
    virtual void StopPlayback();

public:
    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseWaveOutDevice();
    /* device interface */
    virtual void getAudioDevices(Homer::Multimedia::AudioDevicesList &pAList);
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096);

private:
    snd_pcm_t           *mPlaybackHandle;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
