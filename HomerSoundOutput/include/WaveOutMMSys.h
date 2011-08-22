/*
 * Name:    WaveOutMMSys.h
 * Purpose: wave out based on Windows multimedia system
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 * Version: $Id: WaveOutMMSys.h 11 2011-08-22 13:15:28Z silvo $
 */

#ifdef WIN32
#ifndef _SOUNDOUT_WAVE_OUT_MMSYS_
#define _SOUNDOUT_WAVE_OUT_MMSYS_

#include <WaveOut.h>
#include <Header_MMSys.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

//#define MMSYS_OUT_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class WaveOutMMSys:
    public WaveOut
{
public:
    WaveOutMMSys(std::string pDesiredDevice = "");

    /// The destructor
    virtual ~WaveOutMMSys();

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
    static void EventHandler(HWAVEOUT pPlaybackDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2);

    HANDLE          mPlaybackEvent;
    HWAVEOUT        mPlaybackHandle;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
