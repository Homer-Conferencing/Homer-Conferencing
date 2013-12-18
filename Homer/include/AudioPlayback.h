/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: audio playback
 * Since:   2012-08-16
 */

#ifndef _AUDIO_PLAYBACK_
#define _AUDIO_PLAYBACK_

#include <WaveOut.h>

#include <QString>
#include <QMutex>

#include <string>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class AudioPlayback
{
public:
    AudioPlayback(QString pOutputName);

    virtual ~AudioPlayback();

    Homer::Multimedia::AudioDevices GetAudioOutputDevices();
    QString CurrentAudioOutputDevice();
    void SetAudioOutputDevice(QString pDeviceName);

    void PlayAudioChunk(void* pChunkBuffer, int pChunkSize = 4096);

protected:
    virtual void OpenPlaybackDevice(QString pDeviceName = "", QString pOutputName = "");
    virtual void ClosePlaybackDevice();

    bool StartAudioPlayback(QString pFileName, int pLoops = 1);

    QString                     mOutputName;
    QString                     mCurrentOutputDeviceName;
    QMutex                      mOutputMutex;
    int                         mPlayedChunks;

    /* playback */
    Homer::Multimedia::WaveOut *mWaveOut;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
