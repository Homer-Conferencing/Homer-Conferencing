/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
