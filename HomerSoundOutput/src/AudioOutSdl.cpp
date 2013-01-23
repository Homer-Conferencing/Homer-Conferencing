/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
 * Copyright (C) 2009-2009 Stefan Koegel
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
 * Purpose: Implementation of an universal SDL based audio output
 * Author:  Stefan Koegel, Thomas Volkert
 * Since:   2009-03-18
 */

#include <Header_SdlMixer.h>
#include <AudioOutSdl.h>
#include <Logger.h>
#include <map>
#include <string>
#include <stdlib.h>

namespace Homer { namespace SoundOutput {

///////////////////////////////////////////////////////////////////////////////

using namespace std;

AudioOutSdl AudioOutSdl::sAudioOut;

///////////////////////////////////////////////////////////////////////////////

AudioOutSdl::AudioOutSdl()
{
}

AudioOutSdl::~AudioOutSdl()
{
}

AudioOutSdl& AudioOutSdl::getInstance()
{
    return sAudioOut;
}

///////////////////////////////////////////////////////////////////////////////

/*
 *  remarks: open function should be called only once because we don't use a mutex for "mAudioOutOpened"
 *           the same for close function
 */
bool AudioOutSdl::OpenPlaybackDevice(int pSampleRate, bool pStereo, string pDriver, string pDevice)
{
    int tReqChunksize = 1024, tReqChannels;
    int tGotSampleRate, tGotChannels;
    Uint16 tGotSampleFormat;

    mAudioOutOpened = false;

    tReqChannels = pStereo?2:1;

    if(SDL_InitSubSystem(SDL_INIT_AUDIO) == -1)
        LOG(LOG_ERROR, "SDL Audio subsystem could not be started");

    // set explicit env. variable for output driver
    #ifdef LINUX
        if (pDriver != "auto")
        setenv("SDL_AUDIODRIVER", pDriver.c_str(), 1);
    #endif
    #ifdef WINDOWS
        if (pDriver != "auto")
            putenv(("SDL_AUDIODRIVER=" + pDriver).c_str());
    #endif

    ///SDL 1.3
    /*
    //void my_audio_callback(void *userdata, Uint8 *stream, int len);

    SDL_AudioSpec tDesired;
    tDesired.freq = pSampleRate;
    tDesired.format = pSampleFormat;
    tDesired.channels = channels;
    tDesired.samples = chunksize;
    tDesired.callback = my_audio_callback;
    tDesired.userdata = NULL;

    SDL_AudioSpec tObtained;

    const char* tDevice;
    if (pDevice == "auto")
        tDevice = NULL;

    int isCapture = 0;

    int tReturnedDevice = SDL_OpenAudioDevice(tDevice, isCapture, &tDesired, &tObtained, 0);
    if ( tReturnedDevice == 0){
        cerr << "SDL Audio could not be started!" << SDL_GetError() << "\n";
        return false;
    }

    if (pSampleFormat == MIX_DEFAULT_FORMAT){
        if (tDesired.freq != tObtained.freq || tDesired.channels != tObtained.channels){
            SDL_CloseAudioDevice(tReturnedDevice);
            cerr << "Unable to open audio with your specifications!\n";
            return false;
        }
    } else{
        if (tDesired.freq != tObtained.freq || tDesired.channels != tObtained.channels || tDesired.format != tObtained.format){
            SDL_CloseAudioDevice(tReturnedDevice);
            cerr << "Unable to open audio with your specifications!\n";
            return false;
        }
    }
    */

    // open audio device itself
    if (Mix_OpenAudio(pSampleRate, MIX_DEFAULT_FORMAT, tReqChannels, tReqChunksize) == -1)
    {
        LOG(LOG_ERROR, "Error when opening SDL audio mixer");
        return false;
    }

    if (Mix_QuerySpec(&tGotSampleRate, &tGotSampleFormat, &tGotChannels) == 0)
    {
        LOG(LOG_ERROR, "Error when calling MixQuerySpec because of: %s", Mix_GetError());
        return false;
    }

    if ((tGotSampleRate != pSampleRate) || (tGotChannels != tReqChannels) || (tGotSampleFormat != MIX_DEFAULT_FORMAT))
    {
        ClosePlaybackDevice();
        LOG(LOG_ERROR, "Requested and delivered specifications differ");
        LOG(LOG_ERROR, "    ..frequency-req: %d got: %d", pSampleRate, tGotSampleRate);
        LOG(LOG_ERROR, "    ..channels-req: %d got: %d", tReqChannels, tGotChannels);
        LOG(LOG_ERROR, "    ..format-req: %d got: %d", MIX_DEFAULT_FORMAT, tGotSampleFormat);
        return false;
    }

    mChannels = Mix_AllocateChannels(DEFAULT_CHANNEL_COUNT);

    char tDriverNameBuffer[64];

    if (SDL_AudioDriverName(tDriverNameBuffer, 64) == NULL)
    {
        ClosePlaybackDevice();
        LOG(LOG_ERROR, "No audio output driver initiated");
        return false;
    }

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..driver: %s", tDriverNameBuffer);
    LOG(LOG_INFO, "    ..frequency: %d", tGotSampleRate);
    LOG(LOG_INFO, "    ..channels: %d", tGotChannels);
    LOG(LOG_INFO, "    ..format: %d", tGotSampleFormat);
    LOG(LOG_INFO, "    ..mix channels: %d", mChannels);

    // init channel map
    for (int i = 0; i < mChannels; i++)
    {
        ChannelEntry *tChannelEntry = new ChannelEntry();
        tChannelEntry->LastChunk = NULL;
        tChannelEntry->Assigned = false;
        tChannelEntry->IsPlaying = false;
        tChannelEntry->CallbackRecursionCounter = 0;

        // add this channel descriptor to the channel map (without mutex locking! -> run before anything else)
        mChannelMap[i] = tChannelEntry;
    }

    // set player callback function
    Mix_ChannelFinished(PlayerCallBack);

	#if (((SDL_MIXER_MAJOR_VERSION << 16) + (SDL_MIXER_MINOR_VERSION << 8) + SDL_MIXER_PATCHLEVEL) > ((1 << 16) + (2 << 8) + 10))

		// give info about available chunk decoders
		int tNumDecoders = Mix_GetNumChunkDecoders();
		LOG(LOG_INFO, "There are %d available chunk decoders..", tNumDecoders);
		for(int i = 0; i < tNumDecoders; ++i)
			LOG(LOG_INFO, "  ..chunk decoder[%d]: %s", i, Mix_GetChunkDecoder(i));

		// give info about available music decoders
		tNumDecoders = Mix_GetNumMusicDecoders();
		LOG(LOG_INFO, "There are %d available music decoders..", tNumDecoders);
		for(int i = 0; i < tNumDecoders; ++i)
			LOG(LOG_INFO, "  ..music decoder[%d]: %s", i, Mix_GetMusicDecoder(i));
	#endif

    mAudioOutOpened = true;

    return true;
}

void AudioOutSdl::ClosePlaybackDevice()
{
    if (mAudioOutOpened)
    {
        Mix_ChannelFinished(NULL);

        mAudioOutOpened = false;

        //HINT: Ignore this request in Windows because it uses different heaps to allocate
        //		when we deallocate perhaps Windows mixes the heaps and our app. crashes to hell -> it's better to lose some memory than go to binary hell
        //see http://mail-archives.apache.org/mod_mbox/xerces-c-dev/200004.mbox/%3C1DBD6F6FF0F9D311BD4000A0C9979E3201A4C7@cvo1.cvo.roguewave.com%3E
		#ifndef WINDOWS
			for (int i = mChannels; i != 0; --i)
				delete mChannelMap[i];
		#endif
        mChannelMap.clear();

        // stop all channels
        Mix_HaltMusic();

        // close SDL_mixer
        //Mix_CloseAudio();
        //SDL_QuitSubSystem(SDL_INIT_AUDIO);

        LOG(LOG_INFO, "closed");
    }
}

int AudioOutSdl::AllocateChannel()
{
    int tResult = -1;

    if (!mAudioOutOpened)
        return tResult;

    for (int i = 0; (i < mChannels) && (tResult == -1); i++)
    {
        ChannelEntry* tChannelDesc = mChannelMap[i];

        // lock
        tChannelDesc->mMutex.lock();

        if (!tChannelDesc->Assigned)
        {
            tChannelDesc->Assigned = true;
            tChannelDesc->Chunks.clear();
            tChannelDesc->IsPlaying = false;
            tResult = i;
        }

        // unlock
        tChannelDesc->mMutex.unlock();
    }

    if (tResult != -1)
        LOG(LOG_INFO, "Allocated audio output channel: %d", tResult);
    else
        LOG(LOG_ERROR, "No free audio output channel found");

    return tResult;
}

void AudioOutSdl::ReleaseChannel(int pChannel)
{
    if (pChannel == -1)
        return;

    if (!mAudioOutOpened)
        return;

    ChannelEntry* tChannelDesc = mChannelMap[pChannel];

    // lock
    tChannelDesc->mMutex.lock();

    if (tChannelDesc->Assigned)
    {
        // unassign the channel
        tChannelDesc->Assigned = false;

        Mix_Chunk* tLastChunk = (Mix_Chunk*)tChannelDesc->LastChunk;

        // reset and free possible current buffer
        if (tLastChunk != NULL)
        {

            // free buffer memory of old chunk
            Mix_FreeChunk(tLastChunk);

            // reset pointer
            tChannelDesc->LastChunk = NULL;
        }

        ClearChunkListInternal(pChannel);
    }

    // unlock
    tChannelDesc->mMutex.unlock();

    Stop(pChannel);

    LOG(LOG_INFO, "Released audio output channel: %d", pChannel);
}

void AudioOutSdl::ClearChunkList(int pChannel)
{
    if (!mAudioOutOpened)
        return;

    ChannelEntry* tChannelDesc = mChannelMap[pChannel];

    tChannelDesc->mMutex.lock();

    ClearChunkListInternal(pChannel);

    tChannelDesc->mMutex.unlock();
}

void AudioOutSdl::ClearChunkListInternal(int pChannel)
{
    ChannelEntry* tChannelDesc = mChannelMap[pChannel];

	#ifdef DEBUG_AUDIO_OUT_SDL
		LOG(LOG_VERBOSE, "Clearing chunk list internally");
	#endif

    // free chunk list
    std::list<void*>::iterator tItEnd = tChannelDesc->Chunks.end();
    for (std::list<void*>::iterator tIt = tChannelDesc->Chunks.begin(); tIt != tItEnd; tIt++)
    {
        Mix_Chunk* tChunk = (Mix_Chunk*)(*tIt);

        // free buffer memory of chunk
        Mix_FreeChunk(tChunk);
    }
    tChannelDesc->Chunks.clear();

	#ifdef DEBUG_AUDIO_OUT_SDL
		LOG(LOG_VERBOSE, "Clearing of chunk list finished");
	#endif
}

bool AudioOutSdl::Play(int pChannel)
{
    Mix_Chunk* tChunk = NULL;
    bool tResult = false;

    if (pChannel == -1)
    {
    	LOG(LOG_WARN, "Given channel ID is invalid");
    	return false;
    }

    if (!mAudioOutOpened)
    {
    	LOG(LOG_ERROR, "Tried to output audio data when playback is closed");
    	return false;
    }

	#ifdef DEBUG_AUDIO_OUT_SDL
		LOG(LOG_VERBOSE, "Starting playback on channel %d", pChannel);
	#endif
    if (mChannelMap.find(pChannel) != mChannelMap.end())
    {
        ChannelEntry* tChannelDesc = mChannelMap[pChannel];

		#ifdef DEBUG_AUDIO_OUT_SDL
			LOG(LOG_VERBOSE, "Locking channel descriptor");
		#endif
        // lock
        tChannelDesc->mMutex.lock();

		#ifdef DEBUG_AUDIO_OUT_SDL
			LOG(LOG_VERBOSE, "Channel descriptor locked");
		#endif

        if (!tChannelDesc->IsPlaying)
        {
            // play new chunk
            if ((tChannelDesc->Chunks.size() > 0) && (tChannelDesc->Assigned))
            {
                // get new chunk for playing
                tChunk = (Mix_Chunk*)tChannelDesc->Chunks.front();
                tChannelDesc->Chunks.pop_front();

                // unlock to prevent deadlock with SDL_lock
                tChannelDesc->mMutex.unlock();

                // play the new chunk (0 loops)
				#ifdef DEBUG_AUDIO_OUT_SDL
                	LOG(LOG_VERBOSE, "Starting playback on SDL channel %d", pChannel);
				#endif

                int tGotChannel = Mix_PlayChannel(pChannel, tChunk, 0);

				#ifdef DEBUG_AUDIO_OUT_SDL
					LOG(LOG_VERBOSE, "Playback on SDL channel %d started", pChannel);
				#endif

				// lock
                tChannelDesc->mMutex.lock();

                if (tGotChannel != pChannel)
                {
                    LOG(LOG_ERROR, "Callback failed, unable to play chunk because of: %s", Mix_GetError());
                    ClearChunkListInternal(pChannel);
                }else
                    tChannelDesc->IsPlaying = true;
            }

	        Mix_Chunk* tLastChunk = (Mix_Chunk*)tChannelDesc->LastChunk;
	        
            // free memory
            if (tLastChunk != NULL)
            {
                // free buffer memory of old chunk
                Mix_FreeChunk(tLastChunk);
            }

            // update the current chunk pointer
            tChannelDesc->LastChunk = tChunk;
        }else
        {
			#ifdef DEBUG_AUDIO_OUT_SDL
        		LOG(LOG_VERBOSE, "Channel %d is already playing", pChannel);
			#endif
        }

        // were we successful?
        tResult = tChannelDesc->IsPlaying;

        // unlock
        tChannelDesc->mMutex.unlock();
    }else
    {
		LOG(LOG_WARN, "Channel %d is unknown", pChannel);
    }

    return tResult;
}

bool AudioOutSdl::Stop(int pChannel)
{
    if (pChannel == -1)
        return false;

    if (!mAudioOutOpened)
        return false;

	#ifdef DEBUG_AUDIO_OUT_SDL
		LOG(LOG_VERBOSE, "Stopping playback on SDL channel %d", pChannel);
	#endif

    //Mix_HaltChannel(pChannel);
	#ifdef DEBUG_AUDIO_OUT_SDL
		LOG(LOG_VERBOSE, "..ClearChunkListInternal() on channel %d", pChannel);
	#endif
    ClearChunkListInternal(pChannel);

    ChannelEntry* tChannelDesc = mChannelMap[pChannel];

    // lock
    tChannelDesc->mMutex.lock();

    tChannelDesc->IsPlaying = false;

    // unlock
    tChannelDesc->mMutex.unlock();

	#ifdef DEBUG_AUDIO_OUT_SDL
    	LOG(LOG_VERBOSE, "Stopping of playback on SDL channel %d finished", pChannel);
	#endif

    return true;
}

int AudioOutSdl::SetVolume(int pChannel, int pVolume)
{
    if (!mAudioOutOpened)
        return 0;
    else
        return Mix_Volume(pChannel, pVolume);
}

bool AudioOutSdl::Enqueue(int pChannel, void *pBuffer, int pBufferSize, bool pLimitBucket)
{
    if (pChannel == -1)
        return false;

    if (!mAudioOutOpened)
        return false;

    if(pBufferSize <= 0)
    {
        LOG(LOG_ERROR, "Invalid buffer size %d, skipping this chunk", pBufferSize);
        return false;
    }

    void *tBuffer = malloc(pBufferSize);
    if (tBuffer == NULL)
    {
        LOG(LOG_ERROR, "Can not enqueue because an out of memory occurred");
        return false;
    }

    // copy buffer, hence the original one can be freed within the upper application
    memcpy(tBuffer, pBuffer, pBufferSize);

    Mix_Chunk *tNewChunk = Mix_QuickLoad_RAW((Uint8 *)tBuffer, (Uint32)pBufferSize);
    if (tNewChunk == NULL)
    {
        LOG(LOG_ERROR, "AudioOutSdl-Error when loading sample for audio output because of: %s", Mix_GetError());
        return false;
    }

    if (mChannelMap.find(pChannel) != mChannelMap.end())
    {
        ChannelEntry* tChannelDesc = mChannelMap[pChannel];

        // lock
        tChannelDesc->mMutex.lock();

        // check for queue limit
        if ((pLimitBucket) && (tChannelDesc->Chunks.size() > AUDIO_BUFFER_QUEUE_LIMIT))
        {
            LOG(LOG_WARN, "AudioOutSdl-Latency too high, dropping audio samples");
            // free buffer memory of new chunk
            free(tNewChunk->abuf);
            // free memory of new chunk
            free(tNewChunk);
        }else
        {
            #ifdef DEBUG_AUDIO_OUT_SDL
                LOG(LOG_VERBOSE, "Channel %d, queue size: %d", pChannel, (int)tChannelDesc->Chunks.size());
            #endif
            // add new chunk to queue
            tChannelDesc->Chunks.push_back(tNewChunk);
        }

        // unlock
        tChannelDesc->mMutex.unlock();
    }

    return true;
}

AudioOutInfo AudioOutSdl::QueryAudioOutDevices()
{
    AudioOutInfo tAudioOutInfo;
    list<string> tDrivers;
    list<string> tDevices;
    string tCurDriver = "";
    char tDriverNameBuffer[1024];

    if (SDL_AudioDriverName(tDriverNameBuffer, 1024) != NULL)
        tCurDriver = tDriverNameBuffer;

    ///SDL 1.3

    // get available drivers
    //int n = SDL_GetNumAudioDrivers();
    /*if (n == 0) {
        tDrivers.push_back("No built-in audio drivers");
    } else {
        for (int i = 0; i < n; ++i) {
            tDrivers.push_back(SDL_GetAudioDriver(i));
        }
    }

    int iscapture = 0;
    int m = SDL_GetNumAudioDevices(iscapture);

    if (m == -1)
        tDevices.push_back("Driver can't detect specific devices.");
    else if (m == 0)
        tDevices.push_back("No output devices found");
    else {
        for (int i = 0; i < m; i++) {
            tDevices.push_back(SDL_GetAudioDeviceName(i, iscapture));
        }
    }

    tCurrentDriver = SDL_GetCurrentAudioDriver();
    */

    tAudioOutInfo.Drivers = tDrivers;
    tAudioOutInfo.Devices = tDevices;
    tAudioOutInfo.CurDriver = tCurDriver;

    return tAudioOutInfo;
}

///////////////////////////////////////////////////////////////////////////////
// static call back function:
//      function plays next chunk
void AudioOutSdl::PlayerCallBack(int pChannel)
{
    if (pChannel == -1)
        return;

    Mix_Chunk* tChunk = NULL;

	#ifdef DEBUG_AUDIO_OUT_SDL
    	LOGEX(AudioOutSdl, LOG_WARN, "Got request for audio data for channel %d from SDL", pChannel);
	#endif

    if (!AUDIOOUTSDL.mAudioOutOpened)
        return;

    if (AUDIOOUTSDL.mChannelMap.find(pChannel) != AUDIOOUTSDL.mChannelMap.end())
    {
        ChannelEntry* tChannelDesc = AUDIOOUTSDL.mChannelMap[pChannel];
        if (tChannelDesc->CallbackRecursionCounter == 0)
        {
			// lock
			if (!tChannelDesc->mMutex.tryLock(5000))
			{
				LOGEX(AudioOutSdl, LOG_WARN, "System is so terribly slow?, PlayerCallBack couldn't lock the mutex for channel %d", pChannel);
				return;
			}

			tChannelDesc->CallbackRecursionCounter++;
			tChannelDesc->IsPlaying = false;

			// play new chunk
			if ((tChannelDesc->Chunks.size() > 0) && (tChannelDesc->Assigned))
			{
				// get new chunk for playing
				tChunk = (Mix_Chunk*)tChannelDesc->Chunks.front();
				if (tChunk != NULL)
				{
					tChannelDesc->Chunks.pop_front();

					// play the new chunk (0 loops)
					int tGotChannel = Mix_PlayChannel(pChannel, tChunk, 0);

					if (tGotChannel == -1)
					{
						LOGEX(AudioOutSdl, LOG_ERROR, "Callback failed, unable to play chunk because of: %s", Mix_GetError());
						AUDIOOUTSDL.ClearChunkListInternal(pChannel);
					}else
						tChannelDesc->IsPlaying = true;
				}
			}

			Mix_Chunk* tLastChunk = (Mix_Chunk*)tChannelDesc->LastChunk;

			// update the current chunk pointer
			tChannelDesc->LastChunk = (void*)tChunk;

			// unlock
			tChannelDesc->mMutex.unlock();

			Mix_FreeChunk(tLastChunk);
			tChannelDesc->CallbackRecursionCounter--;
        }else
        	LOGEX(AudioOutSdl, LOG_WARN, "Callback recursion detected, will break recursion here at depth of 1");
	}
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
