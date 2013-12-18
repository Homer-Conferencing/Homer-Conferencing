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
    SetOutgoingStream();
    mPlaybackStopped = true;
    mWaveOutOpened = false;
    mThreadNameAssigned = false;
    mDesiredDevice = "";
    mCurrentDevice = "";
    mCurrentDeviceName = "";
    mAudioChannels = 2;
    mVolume = 100;
    mFilePlaybackSource = NULL;
    mSampleFifo = NULL;
    mPlaybackGaps = 0;
    mPlaybackChunks = 0;
    mFilePlaybackNeeded = false;

    LOG(LOG_VERBOSE, "Going to allocate playback FIFO");
    mPlaybackFifo = new MediaFifo(MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE, "WaveOut");

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
	LOG(LOG_VERBOSE, "Going to release playback FIFO");
    delete mPlaybackFifo;

    LOG(LOG_VERBOSE, "Going to release file playback buffer");
    free(mFilePlaybackBuffer);

    // free fifo buffer
    av_fifo_free(mSampleFifo);
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

bool WaveOut::SelectDevice(string pDeviceName)
{
    AudioDevices::iterator tAIt;
    AudioDevices tAList;
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
            mCurrentDeviceName = tAIt->Name;
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
    if (!mThreadNameAssigned)
    {
        mHaveToAssignThreadName = true;
        mThreadNameAssigned = true;
    }

    return true;
}

bool WaveOut::IsPlaying()
{
    bool tResult = false;

    // we are still playing some sound?
    if (!mPlaybackStopped)
        tResult = true;

    mOpenNewFile.lock();

    // we were triggered to play a new file?
    if ((mOpenNewFileAsap) || (mFilePlaybackLoops > 0))
        tResult = true;

    mOpenNewFile.unlock();

	 return tResult;
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
        mFilePlaybackCondition.Signal();
    }

    return true;
}

string WaveOut::CurrentFile()
{
	return mFilePlaybackFileName;
}

string WaveOut::CurrentDeviceName()
{
    if(mCurrentDevice != "")
        return mCurrentDeviceName;
    else
        return "auto";
}

int WaveOut::GetQueueUsage()
{
    if (mPlaybackFifo != NULL)
        return mPlaybackFifo->GetUsage();
    else
        return 0;
}

int WaveOut::GetQueueSize()
{
    if (mPlaybackFifo != NULL)
        return mPlaybackFifo->GetSize();
    else
        return 0;
}

void WaveOut::ClearQueue()
{
    if (mPlaybackFifo != NULL)
        mPlaybackFifo->ClearFifo();
}

void WaveOut::LimitQueue(int pNewSize)
{
    if (mPlaybackFifo != NULL)
    {
        while (mPlaybackFifo->GetUsage() > pNewSize)
        {
            char *tBuffer;
            int tBufferSize;
            int64_t tChunkNumber;
            int tEntryId = mPlaybackFifo->ReadFifoExclusive(&tBuffer, tBufferSize, tChunkNumber);
            mPlaybackFifo->ReadFifoExclusiveFinished(tEntryId);
        }
    }
}

int64_t WaveOut::GetPlaybackGapsCounter()
{
    return mPlaybackGaps;
}

void WaveOut::AdjustVolume(void *pBuffer, int pBufferSize)
{
    if (mVolume != 100)
    {
        //LOG(LOG_WARN, "Got %d bytes and will adapt volume", pChunkSize);
        short int *tSamples = (short int*)pBuffer;
        for (int i = 0; i < pBufferSize / 2; i++)
        {
            int tNewSample = (int)tSamples[i] * mVolume / 100;
            if (tNewSample < -32767)
                tNewSample = -32767;
            if (tNewSample >  32767)
                tNewSample =  32767;
            //LOG(LOG_WARN, "Entry %d from %d to %d", i, (int)tSamples[i], (int)tNewSample);
            tSamples[i] = (short int)tNewSample;
        }
    }
}

void WaveOut::StopFilePlayback()
{
    // terminate possibly running main loop for file based playback
    if (mFilePlaybackNeeded)
    {
        LOG(LOG_VERBOSE, "Stopping file based playback");

        mFilePlaybackNeeded = false;
        mFilePlaybackCondition.Signal();
        LOG(LOG_VERBOSE, "..loopback wake-up signal sent");
        StopThread(3000);
        LOG(LOG_VERBOSE, "..playback thread stopped");
    }
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
        SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(WAVEOUT)");

        Play();
    }

    mOpenNewFile.unlock();
    return tResult;
}

bool WaveOut::WriteChunk(void* pChunkBuffer, int pChunkSize)
{
    mPlayMutex.lock();

    if (!mWaveOutOpened)
    {
        // unlock grabbing
        mPlayMutex.unlock();

        LOG(LOG_ERROR, "Tried to play while WaveOut device is closed");

        return false;
    }

    #ifdef WOPA_AUTO_START_PLAYBACK
        if (mPlaybackStopped)
        {
            // unlock grabbing
            mPlayMutex.unlock();

            LOG(LOG_VERBOSE, "Will automatically start the audio stream");

            Play();

            mPlayMutex.lock();
        }
    #endif

    AdjustVolume(pChunkBuffer, pChunkSize);

    #ifdef WOPA_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Got %d samples for audio output stream", pChunkSize / 4);
    #endif

    if (pChunkSize / (2 /* 16 bits per sample */ * mAudioChannels) != MEDIA_SOURCE_SAMPLES_PER_BUFFER)
    {
        #ifdef WOPA_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Will use FIFO because input chunk has wrong size, need %d samples per chunk buffer", MEDIA_SOURCE_SAMPLES_PER_BUFFER);
        #endif
        if (av_fifo_realloc2(mSampleFifo, av_fifo_size(mSampleFifo) + pChunkSize) < 0)
        {
            LOG(LOG_ERROR, "Reallocation of FIFO audio buffer failed");
            return false;
        }

        // write new samples into fifo buffer
        av_fifo_generic_write(mSampleFifo, pChunkBuffer, pChunkSize, NULL);

        char tAudioBuffer[MEDIA_SOURCE_SAMPLES_BUFFER_SIZE];
        int tAudioBufferSize;
        while (av_fifo_size(mSampleFifo) >=MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)
        {
            tAudioBufferSize = MEDIA_SOURCE_SAMPLES_BUFFER_SIZE;

            // read sample data from the fifo buffer
            HM_av_fifo_generic_read(mSampleFifo, (void*)tAudioBuffer, tAudioBufferSize);

            #ifdef WOPA_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", tAudioBufferSize / (2 /* 16 bits per sample */ * mAudioChannels));
            #endif

            DoWriteChunk((char*)tAudioBuffer, tAudioBufferSize);

            // log statistics about raw PCM audio data stream
            AnnouncePacket(tAudioBufferSize);
        }
    }else
    {
        #ifdef WOPA_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Writing %d samples to audio output stream", pChunkSize / (2 /* 16 bits per sample */ * mAudioChannels));
        #endif

            DoWriteChunk((char*)pChunkBuffer, pChunkSize);

        // log statistics about raw PCM audio data stream
        AnnouncePacket(pChunkSize);
    }

    mPlayMutex.unlock();
    return true;
}

void WaveOut::DoWriteChunk(char *pChunkBuffer, int pChunkSize)
{
    mPlaybackFifo->WriteFifo(pChunkBuffer, pChunkSize, ++mPlaybackChunks);
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
				#ifdef WO_DEBUG_FILE
					LOG(LOG_VERBOSE, "Sending audio chunk %d of %d bytes from file to playback device", tSampleNumber, tSamplesSize);
				#endif

                // wait
				// HINT: we use 2 additional zero buffers after EOF was detected!
                while ((GetQueueUsage() > MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE - 4) && (MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE > 4))
                {
                    #ifdef WO_DEBUG_FILE
                        LOG(LOG_VERBOSE, "Playback FIFO is filled, waiting some time");
                    #endif
                    Thread::Suspend(10 * 1000); //TODO: use a condition here
                }

                WriteChunk(mFilePlaybackBuffer, tSamplesSize);
			}

			// if we have reached EOF then we wait until next file is scheduled for playback
			if (tSampleNumber == GRAB_RES_EOF)
			{
				// add 2 more zero buffers to audio output queue
				char *tZeroBuffer = (char*)malloc(MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
				memset(tZeroBuffer, 0, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
				WriteChunk(tZeroBuffer, MEDIA_SOURCE_SAMPLES_BUFFER_SIZE);
				free(tZeroBuffer);

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
					mFilePlaybackCondition.Wait();
					LOG(LOG_VERBOSE, "Continuing after last file based playback has finished");
					if ((!mOpenNewFileAsap) && (!mPlaybackStopped))
					    LOG(LOG_ERROR, "Error in state machine of file based audio playback thread");
					Play();
				}
			}
        }else
        {
			// wait for next trigger
			mFilePlaybackCondition.Wait();
            LOG(LOG_VERBOSE, "Continuing after last file based playback was invalid");
        }
	}

    LOG(LOG_WARN, "End of thread for file based audio playback reached, playback needed: %d", mFilePlaybackNeeded);

    // reset the state variables
    mOpenNewFileAsap = false;
    mFilePlaybackLoops = 0;

    return NULL;
}

}} //namespaces
