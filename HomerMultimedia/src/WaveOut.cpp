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
 * Purpose: Wave output base class
 * Author:  Thomas Volkert
 * Since:   2010-12-11
 */

#include <ProcessStatisticService.h>
#include <WaveOut.h>
#include <Logger.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

WaveOut::WaveOut(string pName):
    PacketStatistic(pName), Thread()
{
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);
    mPlaybackStopped = true;
    mWaveOutOpened = false;
    mDesiredDevice = "";
    mCurrentDevice = "";
    mVolume = 100;
    mFilePlaybackSource = NULL;
    mSampleFifo = NULL;
    mFilePlaybackNeeded = false;

    LOG(LOG_VERBOSE, "Going to allocate playback FIFO");
    mPlaybackFifo = new MediaFifo(MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE, "WaveOutPortAudio");

    LOG(LOG_VERBOSE, "Going to allocate file playback buffer");
    mFilePlaybackBuffer = (char*)malloc(MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);

    // init fifo buffer
    LOG(LOG_VERBOSE, "Going to allocate sample size FIFO");
    mSampleFifo = HM_av_fifo_alloc(MEDIA_SOURCE_SAMPLES_BUFFER_SIZE * 4);
    if (mSampleFifo == NULL)
    	LOG(LOG_ERROR, "Sample size FIFO is invalid");
}

WaveOut::~WaveOut()
{
    delete mPlaybackFifo;

    LOG(LOG_VERBOSE, "Going to release file playback buffer");
    free(mFilePlaybackBuffer);

    // free fifo buffer
    av_fifo_free(mSampleFifo);
}

///////////////////////////////////////////////////////////////////////////////

bool WaveOut::SelectDevice(string pDeviceName)
{
    AudioDevicesList::iterator tAIt;
    AudioDevicesList tAList;
    string tOldDesiredDevice = mDesiredDevice;
    bool tNewSelection = false;
    bool tAutoSelect = false;

    LOG(LOG_INFO, "Selecting new device..");

    if ((pDeviceName == "auto") || (pDeviceName == ""))
        tAutoSelect = true;

    getAudioDevices(tAList);
    for (tAIt = tAList.begin(); tAIt != tAList.end(); tAIt++)
    {
        if ((pDeviceName == tAIt->Name) || (tAutoSelect))
        {
            mDesiredDevice = tAIt->Card;
            if (mDesiredDevice != mCurrentDevice)
                LOG(LOG_INFO, "..audio device selected: %s", mDesiredDevice.c_str());
            else
                LOG(LOG_INFO, "..audio device re-selected: %s", mDesiredDevice.c_str());
            tNewSelection = true;
        }
    }

    if ((!tNewSelection) && (((pDeviceName == "auto") || (pDeviceName == "automatic") || pDeviceName == "")))
    {
        LOG(LOG_INFO, "..device selected: auto detect (card: auto)");
        mDesiredDevice = "";
    }

    LOG(LOG_VERBOSE, "WaveOut should be reseted: %d", tNewSelection);

    return tNewSelection;
}

void WaveOut::Stop()
{
	LOG(LOG_VERBOSE, "Mark stream as stopped");
    mPlaybackStopped = true;
    mFilePlaybackLoops = 0;
}

bool WaveOut::Play()
{
	LOG(LOG_VERBOSE, "Mark stream as started");
    mPlaybackStopped = false;
    mHaveToAssignThreadName = true;

    return true;
}

bool WaveOut::IsPlaying()
{
	return !mPlaybackStopped;
}

int WaveOut::GetVolume()
{
    return mVolume;
}

void WaveOut::SetVolume(int pValue)
{
	LOG(LOG_VERBOSE, "Setting volume to %d \%", pValue);
    if (pValue < 0)
        pValue = 0;
    if(pValue > 300)
        pValue = 300;
    mVolume = pValue;
}

bool WaveOut::PlayFile(string pFileName, int pLoops)
{
    LOG(LOG_VERBOSE, "Try to play file: %s", pFileName.c_str());

    if (!mWaveOutOpened)
    {
        LOG(LOG_VERBOSE, "Playback device wasn't opened yet");
        return false;
    }

    mOpenNewFile.lock();

    mFilePlaybackLoops = pLoops;
    mFilePlaybackFileName = pFileName;
    mOpenNewFileAsap = true;

    mOpenNewFile.unlock();

    if (!mFilePlaybackNeeded)
    {
        // start the playback thread for the first time
        LOG(LOG_VERBOSE, "Starting thread for file based audio playback");
        StartThread();
    }else
    {
        // send wake up
        LOG(LOG_VERBOSE, "Sending thread for file based audio playback a wake up signal");
        mFilePlaybackCondition.SignalAll();
    }

    return true;
}

string WaveOut::CurrentFile()
{
	return mFilePlaybackFileName;
}

void WaveOut::AssignThreadName()
{
    if (mHaveToAssignThreadName)
    {
        SVC_PROCESS_STATISTIC.AssignThreadName("WaveOut-File");
        mHaveToAssignThreadName = false;
    }
}

bool WaveOut::DoOpenNewFile()
{
    bool tResult = true;

    LOG(LOG_VERBOSE, "Doing DoOpenNewFile() now..");
    LOG(LOG_VERBOSE, "Try to play file: %s", mFilePlaybackFileName.c_str());

    mOpenNewFile.lock();

    mOpenNewFileAsap = false;

    if (mFilePlaybackSource != NULL)
    {
        Stop();
        mFilePlaybackSource->CloseGrabDevice();
        delete mFilePlaybackSource;
    }

    LOG(LOG_VERBOSE, "..clearing FIFO buffer");
    mPlaybackFifo->ClearFifo();

    mFilePlaybackSource = new MediaSourceFile(mFilePlaybackFileName);
    if (!mFilePlaybackSource->OpenAudioGrabDevice())
    {
        LOG(LOG_ERROR, "Couldn't open audio file for playback");
        delete mFilePlaybackSource;
        mFilePlaybackSource = NULL;
        tResult = false;
    }else
    {
        SVC_PROCESS_STATISTIC.AssignThreadName("Playback-File");

        Play();
    }

    mOpenNewFile.unlock();
    return tResult;
}

void* WaveOut::Run(void* pArgs)
{
    int tSamplesSize;
    int tSampleNumber = -1, tLastSampleNumber = -1;

    mFilePlaybackNeeded = true;

    LOG(LOG_VERBOSE, "Starting main loop for file based playback");
    while(mFilePlaybackNeeded)
    {
        if (mOpenNewFileAsap)
            DoOpenNewFile();

        if (mFilePlaybackSource != NULL)
        {
			// get new samples from audio file
			tSamplesSize = MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE + FF_INPUT_BUFFER_PADDING_SIZE;
			tSampleNumber = mFilePlaybackSource->GrabChunk(mFilePlaybackBuffer, tSamplesSize);

			if ((tSampleNumber >= 0) && (!mPlaybackStopped))
			{
				#ifdef WOPA_DEBUG_FILE
					LOG(LOG_VERBOSE, "Sending audio chunk %d of %d bytes from file to playback device", tSampleNumber, tSamplesSize);
				#endif
				WriteChunk(mFilePlaybackBuffer, tSamplesSize);
			}

			// if we have reached EOF then we wait until next file is scheduled for playback
			if (tSampleNumber == GRAB_RES_EOF)
			{
				mFilePlaybackLoops--;

				// should we loop the file?
				if (mFilePlaybackLoops > 0)
				{// repeat file
					LOG(LOG_VERBOSE, "Looping %s, remaining loops: %d", mFilePlaybackFileName.c_str(), mFilePlaybackLoops - 1);
					mFilePlaybackSource->Seek(0);
				}else
				{// passive waiting until next trigger is received
					// wait until last chunk is played
					LOG(LOG_VERBOSE, "EOF reached, waiting for playback end");
					while(mPlaybackFifo->GetUsage() > 0)
						Suspend(50 * 1000);

					// stop playback
					Stop();

					// wait for next trigger
					mFilePlaybackCondition.Reset();
					mFilePlaybackCondition.Wait();
				}
			}
        }else
        {
			// wait for next trigger
			mFilePlaybackCondition.Reset();
			mFilePlaybackCondition.Wait();
        }
	}

    return NULL;
}

}} //namespaces
