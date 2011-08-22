/*
 * Name:    WaveOut.h
 * Purpose: abstract wave out
 * Author:  Thomas Volkert
 * Since:   2010-11-30
 * Version: $Id$
 */

#ifndef _SOUNDOUT_WAVE_OUT_
#define _SOUNDOUT_WAVE_OUT_

#include <MediaSource.h>
#include <PacketStatistic.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

class WaveOut:
    public Homer::Monitor::PacketStatistic
{
public:
    WaveOut(std::string pName = "");

    /// The destructor
    virtual ~WaveOut();

    /* device interface */
    virtual bool SelectDevice(std::string pDeviceName = "");

    // playback control
    virtual void StopPlayback();

public:
    /* abstract interface which has to be implemented by derived classes */

    /* open/close */
    virtual bool OpenWaveOutDevice(int pSampleRate = 44100, bool pStereo = true) = 0;
    virtual bool CloseWaveOutDevice() = 0;
    /* device interface */
    virtual void getAudioDevices(Homer::Multimedia::AudioDevicesList &pAList) = 0;
    /* playback control */
    virtual bool WriteChunk(void* pChunkBuffer, int pChunkSize = 4096) = 0;

protected:
    /* device state */
    bool                mWaveOutOpened;
    bool                mPlaybackStopped;
    int                 mChunkNumber;
    /* device parameters */
    int                 mSampleRate;
    bool                mStereo;
    /* device handling */
    std::string         mDesiredDevice;
    std::string         mCurrentDevice;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
