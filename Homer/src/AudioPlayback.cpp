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
 * Purpose: Implementation of an audio playback
 * Since:   2012-08-16
 */

#include <WaveOutPortAudio.h>
#include <WaveOutPulseAudio.h>
#include <WaveOutSdl.h>
#include <Configuration.h>
#include <AudioPlayback.h>
#include <Logger.h>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

AudioPlayback::AudioPlayback(QString pOutputName)
{
    LOG(LOG_VERBOSE, "Created");
    mWaveOut = NULL;
    mCurrentOutputDeviceName = "";
    mOutputName = pOutputName;
    mPlayedChunks = 0;
}

AudioPlayback::~AudioPlayback()
{

    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

AudioDevices AudioPlayback::GetAudioOutputDevices()
{
    AudioDevices tResult;

    if(mWaveOut != NULL)
        mWaveOut->getAudioDevices(tResult);

    return tResult;
}

QString AudioPlayback::CurrentAudioOutputDevice()
{
    QString tResult = "";

    if (mWaveOut != NULL)
        tResult = QString(mWaveOut->CurrentDeviceName().c_str());

    return tResult;
}

void AudioPlayback::SetAudioOutputDevice(QString pDeviceName)
{
    LOG(LOG_VERBOSE, "Selecting audio output device \"%s\"", pDeviceName.toStdString().c_str());

    if(mCurrentOutputDeviceName != pDeviceName)
    {
        mOutputMutex.lock();

        bool tOldWasPlaying = mWaveOut->IsPlaying();
        ClosePlaybackDevice();

        OpenPlaybackDevice(pDeviceName);
        if(tOldWasPlaying)
            mWaveOut->Play();

        mOutputMutex.unlock();
    }
}

void AudioPlayback::PlayAudioChunk(void* pChunkBuffer, int pChunkSize)
{
    mOutputMutex.lock();

    if(mWaveOut != NULL)
    {
        //LOG(LOG_VERBOSE, "Playing %d bytes of audio buffer %d", pChunkSize, mPlayedChunks);
        mPlayedChunks++;
        mWaveOut->WriteChunk(pChunkBuffer, pChunkSize);
    }

    mOutputMutex.unlock();
}

void AudioPlayback::OpenPlaybackDevice(QString pDeviceName, QString pOutputName)
{
    LOG(LOG_VERBOSE, "Going to open playback device");

    if(pOutputName != "")
        mOutputName = pOutputName;

    if(pDeviceName == "")
        pDeviceName = CONF.GetLocalAudioSink();

    if (CONF.AudioOutputEnabled())
    {
        #if defined(WINDOWS)
            LOG(LOG_VERBOSE, "Opening PortAudio based playback");
            mWaveOut = new WaveOutPortAudio("WaveOut-" + mOutputName.toStdString(), pDeviceName.toStdString());
		#endif
		#if defined(LINUX)
            #if FEATURE_PULSEAUDIO
                if (!WaveOutPulseAudio::PulseAudioAvailable())
                {
                    LOG(LOG_VERBOSE, "Opening PortAudio based playback");
                    mWaveOut = new WaveOutPortAudio("WaveOut-" + mOutputName.toStdString(), pDeviceName.toStdString());
                }else
                {
                    LOG(LOG_VERBOSE, "Opening PulseAudio based playback");
                    mWaveOut = new WaveOutPulseAudio("WaveOut-" + mOutputName.toStdString(), pDeviceName.toStdString());
                }
            #else
                LOG(LOG_VERBOSE, "Opening PortAudio based playback");
                mWaveOut = new WaveOutPortAudio("WaveOut-" + mOutputName.toStdString(), pDeviceName.toStdString());
            #endif
		#endif
		#if defined(APPLE)
            LOG(LOG_VERBOSE, "Opening SDL based playback");
           mWaveOut = new WaveOutSdl("WaveOut: " + mOutputName.toStdString(), pDeviceName.toStdString());
        #endif
        if (mWaveOut != NULL)
        {
            if (!mWaveOut->OpenWaveOutDevice())
            {
            	LOG(LOG_ERROR, "Couldn't open wave out device");
            	delete mWaveOut;
            	mWaveOut = NULL;
            }else{
                mCurrentOutputDeviceName = pDeviceName;
            }
        }else
            LOG(LOG_ERROR, "Couldn't create wave out object");
    }
    LOG(LOG_VERBOSE, "Finished to open playback device");
}

void AudioPlayback::ClosePlaybackDevice()
{
    LOG(LOG_VERBOSE, "Going to close playback device at %p", mWaveOut);

    if (mWaveOut != NULL)
    {
        // close the audio out
        delete mWaveOut;
    }

    LOG(LOG_VERBOSE, "Finished to close playback device");
}

bool AudioPlayback::StartAudioPlayback(QString pFileName, int pLoops)
{
    bool tResult = false;

    if (mWaveOut == NULL)
    {
        LOG(LOG_WARN, "Cannot play sound file %s because wave out is not available", pFileName.toStdString().c_str());
        return tResult;
    }

    if (pFileName == "")
    {
        LOG(LOG_WARN, "Cannot play sound file because its name is empty");
        return tResult;
    }

    LOG(LOG_VERBOSE, "Starting playback of file: %s", pFileName.toStdString().c_str());

    if (!mWaveOut->PlayFile(pFileName.toStdString(), pLoops))
        LOG(LOG_ERROR, "Was unable to play the file \"%s\".", pFileName.toStdString().c_str());
    else
        tResult = true;

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
