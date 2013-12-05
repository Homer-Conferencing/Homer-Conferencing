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

AudioPlayback::AudioPlayback()
{
    LOG(LOG_VERBOSE, "Created");
    mWaveOut = NULL;
}

AudioPlayback::~AudioPlayback()
{

    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void AudioPlayback::OpenPlaybackDevice(QString pOutputName)
{
    LOG(LOG_VERBOSE, "Going to open playback device");

    if (CONF.AudioOutputEnabled())
    {
        #if defined(WINDOWS)
            LOG(LOG_VERBOSE, "Opening PortAudio based playback");
            mWaveOut = new WaveOutPortAudio("WaveOut-" + pOutputName.toStdString(), CONF.GetLocalAudioSink().toStdString());
		#endif
		#if defined(LINUX)
            #if FEATURE_PULSEAUDIO
                if (!WaveOutPulseAudio::PulseAudioAvailable())
                {
                    LOG(LOG_VERBOSE, "Opening PortAudio based playback");
                    mWaveOut = new WaveOutPortAudio("WaveOut-" + pOutputName.toStdString(), CONF.GetLocalAudioSink().toStdString());
                }else
                {
                    LOG(LOG_VERBOSE, "Opening PulseAudio based playback");
                    mWaveOut = new WaveOutPulseAudio("WaveOut-" + pOutputName.toStdString(), CONF.GetLocalAudioSink().toStdString());
                }
            #else
                LOG(LOG_VERBOSE, "Opening PortAudio based playback");
                mWaveOut = new WaveOutPortAudio("WaveOut-" + pOutputName.toStdString(), CONF.GetLocalAudioSink().toStdString());
            #endif
		#endif
		#if defined(APPLE)
            LOG(LOG_VERBOSE, "Opening SDL based playback");
           mWaveOut = new WaveOutSdl("WaveOut: " + pOutputName.toStdString(), CONF.GetLocalAudioSink().toStdString());
        #endif
        if (mWaveOut != NULL)
        {
            if (!mWaveOut->OpenWaveOutDevice())
            {
            	LOG(LOG_ERROR, "Couldn't open wave out device");
            	delete mWaveOut;
            	mWaveOut = NULL;
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
