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
 * Purpose: Implementation of a ffmpeg based local file source
 * Author:  Thomas Volkert
 * Since:   2009-02-24
 */

//HINT: This file based media source uses the entire frame buffer during grabbing.

#include <MediaSourceFile.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>
#include <HBCondition.h>
#include <HBSystem.h>

#include <algorithm>
#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

// 33 ms delay for 30 fps -> rounded to 35 ms
#define MSF_FRAME_DROP_THRESHOLD                           0 //in us, 0 deactivates frame dropping

// above which threshold value should we execute a hard file seeking? (below this threshold we do soft seeking by adjusting RT grabbing)
#define MSF_SEEK_WAIT_THRESHOLD                            1.5 // seconds

// how much time do we want to buffer at maximum?
#define MSF_FRAME_INPUT_QUEUE_MAX_TIME                     ((System::GetTargetMachineType() != "x86") ? 3.0 : 0.5) // 0.5 seconds for 32 bit targets with limit of 4 GB ram, 3.0 seconds for 64 bit targets

///////////////////////////////////////////////////////////////////////////////

MediaSourceFile::MediaSourceFile(string pSourceFile, bool pGrabInRealTime):
    MediaSourceMem("FILE: " + pSourceFile, false)
{
	mLastDecoderFilePosition = 0;
    mDecoderFrameBufferTimeMax = MSF_FRAME_INPUT_QUEUE_MAX_TIME;
    mDecoderFramePreBufferTime = mDecoderFrameBufferTimeMax; // for file based media sources we use the entire frame buffer
    mSourceType = SOURCE_FILE;
    mDesiredDevice = pSourceFile;
    mGrabberProvidesRTGrabbing = pGrabInRealTime;
    mCurrentDeviceName = mDesiredDevice;
}

MediaSourceFile::~MediaSourceFile()
{
    LOG(LOG_VERBOSE, "Destroying %s media source file for %s", GetMediaTypeStr().c_str(), mDesiredDevice.c_str());
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

///////////////////////////////////////////////////////////////////////////////

void MediaSourceFile::getVideoDevices(VideoDevices &pVList)
{
    VideoDeviceDescriptor tDevice;

    tDevice.Name = mDesiredDevice;
    tDevice.Card = mDesiredDevice;
    tDevice.Desc = "file source: \"" + mDesiredDevice + "\"";

    pVList.push_back(tDevice);
}

void MediaSourceFile::getAudioDevices(AudioDevices &pAList)
{
    AudioDeviceDescriptor tDevice;

    tDevice.Name = mDesiredDevice;
    tDevice.Card = mDesiredDevice;
    tDevice.Desc = "file source: \"" + mDesiredDevice + "\"";
    tDevice.IoType = "Input/Output";

    pAList.push_back(tDevice);
}

bool MediaSourceFile::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    mMediaType = MEDIA_VIDEO;

    if (pFps > 29.97)
        pFps = 29.97;
    if (pFps < 5)
        pFps = 5;

    LOG(LOG_VERBOSE, "Try to open video stream from file \"%s\"..", mDesiredDevice.c_str());

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(FILE)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    if (!OpenInput(mDesiredDevice.c_str(), NULL, NULL))
    	return false;

    if (!DetectAllStreams())
    	return false;

    if (!SelectStream())
    	return false;

    if (!OpenDecoder())
    	return false;

	if (SupportsSeeking())
    {
    	int tResult = 0;
        if((tResult = avformat_seek_file(mFormatContext, -1, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_ANY)) < 0)
        {
            LOG(LOG_WARN, "Couldn't seek to the start of video stream because \"%s\".", strerror(AVUNERROR(tResult)));
        }
    }

    // allocate software scaler context
    LOG(LOG_VERBOSE, "Going to create scaler context..");

    MarkOpenGrabDeviceSuccessful();

    StartDecoder();

    // we do not need the fragment queue
    delete mDecoderFragmentFifo;
    mDecoderFragmentFifo = NULL;

    return true;
}

bool MediaSourceFile::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
	int tResult = 0;

	mMediaType = MEDIA_AUDIO;
    mOutputAudioChannels = pChannels;
    mOutputAudioSampleRate = pSampleRate;
    mOutputAudioFormat = AV_SAMPLE_FMT_S16; // assume we always want signed 16 bit

    LOG(LOG_VERBOSE, "Try to open audio stream from file \"%s\"..", mDesiredDevice.c_str());

    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(FILE)");

    // set category for packet statistics
    ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);

    // ffmpeg uses mmst:// instead of mms://
    if (mDesiredDevice.compare(0, string("mms://").size(), "mms://") == 0)
    {
    	LOG(LOG_WARN, "Replacing mms:// by mmst:// in %s", mDesiredDevice.c_str());
		string tNewDesiredDevice = "mmst://" + mDesiredDevice.substr(6, mDesiredDevice.size() - 6);
		mDesiredDevice = tNewDesiredDevice;
    }

    if (!OpenInput(mDesiredDevice.c_str(), NULL, NULL))
    	return false;

    mCurrentDevice = mDesiredDevice;
    mCurrentDeviceName = mDesiredDevice;

    if (!DetectAllStreams())
    	return false;

    // enumerate all audio streams and store them as possible input channels
    // find correct audio stream, depending on the desired input channel
    string tEntry;
    AVDictionaryEntry *tDictEntry;
    int tAudioStreamCount = 0;
    LOG(LOG_VERBOSE, "Probing for multiple audio input channels for device %s", mCurrentDevice.c_str());
    mInputChannels.clear();
    mMediaStreamIndex = -1;
    for (int i = 0; i < (int)mFormatContext->nb_streams; i++)
    {
        if (mFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
        	if (mFormatContext->streams[i]->codec->channels < 7)
        	{
				string tLanguage = "";
				string tTitle = "";
				AVCodec *tCodec = NULL;
				tCodec = avcodec_find_decoder(mFormatContext->streams[i]->codec->codec_id);
				tDictEntry = NULL;

				// get the language of the stream
				tDictEntry = HM_av_dict_get(mFormatContext->streams[i]->metadata, "language", NULL);
				if (tDictEntry != NULL)
				{
					if (tDictEntry->value != NULL)
					{
						tLanguage = string(tDictEntry->value);
						std::transform(tLanguage.begin(), tLanguage.end(), tLanguage.begin(), ::toupper);
						LOG(LOG_VERBOSE, "Language found: %s", tLanguage.c_str());
					}
				}

				// get the title of the stream
				tDictEntry = HM_av_dict_get(mFormatContext->streams[i]->metadata, "title", NULL);
				if (tDictEntry != NULL)
				{
					if (tDictEntry->value != NULL)
					{
						tTitle = string(tDictEntry->value);
						LOG(LOG_VERBOSE, "Title found: %s", tTitle.c_str());
					}
				}

				LOG(LOG_VERBOSE, "Desired audio input channel: %d, current found audio input channel: %d", mDesiredInputChannel, tAudioStreamCount);
				if(tAudioStreamCount == mDesiredInputChannel)
				{
					mMediaStreamIndex = i;
					LOG(LOG_VERBOSE, "Using audio input channel %d in stream %d for grabbing", mDesiredInputChannel, i);

					// Dump information about device file
					av_dump_format(mFormatContext, i, "MediaSourceFile(audio)", false);
				}

				tAudioStreamCount++;

				if ((tLanguage != "") || (tTitle != ""))
					tEntry = tLanguage + ": " + tTitle + " (" + toString(tCodec->name) + ", " + toString(mFormatContext->streams[i]->codec->channels) + " ch, " + toString(mFormatContext->streams[i]->codec->bit_rate / 1000) + " kbit/s)";
				else
					tEntry = "Audio " + toString(tAudioStreamCount) + " (" + toString(tCodec->name) + ", " + toString(mFormatContext->streams[i]->codec->channels) + " ch, " + toString(mFormatContext->streams[i]->codec->bit_rate / 1000) + " kbit/s)";
				LOG(LOG_VERBOSE, "Found audio stream: %s", tEntry.c_str());
				int tDuplicates = 0;
				for(int i = 0; i < (int)mInputChannels.size(); i++)
				{
					if (mInputChannels[i].compare(0, tEntry.size(), tEntry) == 0)
						tDuplicates++;
				}
				if (tDuplicates > 0)
					tEntry+= "-" + toString(tDuplicates + 1);
				mInputChannels.push_back(tEntry);
        	}else
        	{
        		LOG(LOG_ERROR, "Detected unsupported audio channel setup with %d channels, will ignore this audio stream", mFormatContext->streams[i]->codec->channels);
        	}
		}
    }
    if (mMediaStreamIndex == -1)
    {
        LOG(LOG_ERROR, "Couldn't find an audio stream");
        // Close the audio file
        HM_avformat_close_input(mFormatContext);
        return false;
    }

    mCurrentInputChannel = mDesiredInputChannel;

    if (!OpenDecoder())
    	return false;

	if (!OpenFormatConverter())
		return false;
		
    // fix frame size of 0 for some audio codecs and use default caudio capture frame size instead to allow 1:1 transformation
    // old PCM implementation delivered often a frame size of 1
    // some audio codecs (excl. MP3) deliver a frame size of 0
    // ==> we use 32 as threshold to catch most of the inefficient values for the frame size
//    if (mCodecContext->frame_size < 32)
//        mCodecContext->frame_size = 1024;

	if (SupportsSeeking())
	{
		if((tResult = avformat_seek_file(mFormatContext, -1, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_ANY)) < 0)
		{
			LOG(LOG_WARN, "Couldn't seek to the start of audio stream because \"%s\".", strerror(AVUNERROR(tResult)));
		}
	}

    MarkOpenGrabDeviceSuccessful();

    StartDecoder();

    // we do not need the fragment queue
    delete mDecoderFragmentFifo;
    mDecoderFragmentFifo = NULL;

    return true;
}

bool MediaSourceFile::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s stream from file %s", GetMediaTypeStr().c_str(), mCurrentDevice.c_str());

    tResult = MediaSourceMem::CloseGrabDevice();

    LOG(LOG_VERBOSE, "...%s stream from file %s closed", GetMediaTypeStr().c_str(), mCurrentDevice.c_str());

    return tResult;
}

bool MediaSourceFile::SupportsRecording()
{
    return true;
}

bool MediaSourceFile::SupportsSeeking()
{
    return ((!InputIsPicture()) && (GetSeekEnd() > 0));
}

float MediaSourceFile::GetSeekEnd()
{
    float tResult = 0;
    if ((mFormatContext != NULL) && (mFormatContext->duration != (int64_t)AV_NOPTS_VALUE))
        tResult = ((float)mFormatContext->duration / AV_TIME_BASE);

    // result is in seconds
    return tResult;
}

bool MediaSourceFile::Seek(float pSeconds, bool pOnlyKeyFrames)
{
    int tResult = false;
    int tRes = 0;

    float tSeekEnd = GetSeekEnd();
    float tNumberOfFrames = mNumberOfFrames;

    //HINT: we need the original PTS values from the file
    // correcting PTS value by a possible start PTS value (offset)
    if (mSourceStartPts > 0)
    {
        //LOG(LOG_VERBOSE, "Correcting PTS value by offset: %.2f", (float)mSourceStartPts);
        tNumberOfFrames += mSourceStartPts;
    }

    if (pSeconds <= 0)
        pSeconds = 0;
    if (pSeconds > tSeekEnd)
        pSeconds = tSeekEnd;

    // if we have a picture as input we cannot seek but we pretend a successful seeking
    if (!SupportsSeeking())
    {
        LOG(LOG_WARN, "Seeking not supported for the file \"%s\"", mCurrentDevice.c_str());
        return true;
    }

    // lock grabbing
    mGrabMutex.lock();

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        //LOG(LOG_ERROR, "Tried to seek while %s file is closed", GetMediaTypeStr().c_str());
        return false;
    }

    double tFrameIndex;
    if (pSeconds <= 0)
        tFrameIndex = 0;
    else
        tFrameIndex = (mSourceStartPts + (double)pSeconds * mFrameRate); // later the value for mCurrentFrameIndex will be changed to the same value like tAbsoluteTimestamp
    float tTimeDiff = pSeconds - GetSeekPos();

    //LOG(LOG_VERBOSE, "Rel: %ld Abs: %ld", tRelativeTimestamp, tAbsoluteTimestamp);

    if ((tFrameIndex >= 0) && (tFrameIndex < tNumberOfFrames))
    {
        // seek only if it is necessary
        if (pSeconds != GetSeekPos())
        {
            mDecoderNeedWorkConditionMutex.lock();

            if ((!mGrabberProvidesRTGrabbing) || ((tTimeDiff > MSF_SEEK_WAIT_THRESHOLD) || (tTimeDiff < -MSF_SEEK_WAIT_THRESHOLD)))
            {
                float tTargetTimestamp = pSeconds * AV_TIME_BASE;
                if (mFormatContext->start_time != (int64_t)AV_NOPTS_VALUE)
                {
                    LOG(LOG_VERBOSE, "Seeking: format context describes an additional start offset of %ld", mFormatContext->start_time);
                    tTargetTimestamp += mFormatContext->start_time;
                }

                LOG(LOG_VERBOSE, "%s-SEEKING from %5.2f sec. (pts %.2f) to %5.2f sec. (pts %.2f, ts: %.2f), max. sec.: %.2f (pts %.2f), source start pts: %.2f", GetMediaTypeStr().c_str(), GetSeekPos(), mGrabberCurrentFrameIndex, pSeconds, tFrameIndex, tTargetTimestamp, tSeekEnd, tNumberOfFrames, mSourceStartPts);

                int tSeekFlags = (pOnlyKeyFrames ? 0 : AVSEEK_FLAG_ANY) | AVSEEK_FLAG_FRAME | (tFrameIndex < mGrabberCurrentFrameIndex ? AVSEEK_FLAG_BACKWARD : 0);
                mDecoderTargetFrameIndex = (int64_t)tFrameIndex;

                // VIDEO: trigger a seeking until next key frame
                if (mMediaType == MEDIA_VIDEO)
                {
                    mDecoderWaitForNextKeyFrame = true;
                    mDecoderWaitForNextKeyFramePackets = true;
                }

                tRes = (avformat_seek_file(mFormatContext, -1, INT64_MIN, tTargetTimestamp, INT64_MAX, 0) >= 0);
                if (tRes < 0)
                {
                    LOG(LOG_ERROR, "Error during absolute seeking in %s source file because \"%s\"", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tResult)));
                    tResult = false;
                }else
                {
                    LOG(LOG_VERBOSE, "Seeking in %s file to frame index %.2f was successful, current dts is %ld", GetMediaTypeStr().c_str(), (float)tFrameIndex, mFormatContext->streams[mMediaStreamIndex]->cur_dts);
                    mGrabberCurrentFrameIndex = tFrameIndex;

                    // seeking was successful
                    tResult = true;
                }

                mDecoderFifo->ClearFifo();
                mDecoderMetaDataFifo->ClearFifo();
                mDecoderNeedWorkCondition.SignalAll();

                // trigger a avcodec_flush_buffers()
                mDecoderFlushBuffersAfterSeeking = true;

                // trigger a RT playback calibration after seeking
                mDecoderRecalibrateRTGrabbingAfterSeeking = true;
            }else
            {
                LOG(LOG_VERBOSE, "WAITING for %2.2f sec., SEEK/WAIT threshold is %2.2f", tTimeDiff, MSF_SEEK_WAIT_THRESHOLD);
                LOG(LOG_VERBOSE, "%s-we are at frame %.2f and we should be at frame %.2f", GetMediaTypeStr().c_str(), (float)mGrabberCurrentFrameIndex, (float)tFrameIndex);

                // simulate a frame index
                mGrabberCurrentFrameIndex = tFrameIndex;

                // seek by adjusting the start time of RT grabbing
                mSourceStartTimeForRTGrabbing = mSourceStartTimeForRTGrabbing - tTimeDiff/* in us */ * 1000 * 1000;

                // everything is fine
                tResult = true;
            }

            // reset EOF marker
            if (mEOFReached)
            {
                LOG(LOG_VERBOSE, "Reseting EOF marker for %s stream", GetMediaTypeStr().c_str());
                mEOFReached = false;
            }

            mDecoderNeedWorkConditionMutex.unlock();
        }else
        {
            LOG(LOG_VERBOSE, "%s-seeking in file skipped because position is already the desired one", GetMediaTypeStr().c_str());
            tResult = true;

            // trigger a RT playback calibration after seeking
            mDecoderRecalibrateRTGrabbingAfterSeeking = true;
        }
    }else
        LOG(LOG_ERROR, "Seek position PTS=%.2f(%.2f) is out of range (0 - %.2f) for %s file, fps: %.2f, start offset: %.2f", (float)tFrameIndex, pSeconds, tNumberOfFrames, GetMediaTypeStr().c_str(), mFrameRate, (float)mSourceStartPts);

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

bool MediaSourceFile::SeekRelative(float pSeconds, bool pOnlyKeyFrames)
{
    bool tResult = false;

    float tTargetPos = GetSeekPos() + pSeconds;
    if (tTargetPos < 0)
        tTargetPos = 0;
    if (tTargetPos > GetSeekEnd())
        tTargetPos = GetSeekEnd();

    tResult = Seek(tTargetPos, pOnlyKeyFrames);

    LOG(LOG_VERBOSE, "Seeking relative %.2f seconds to absolute position %.2f seconds resulted with state: %d", pSeconds, tTargetPos, tResult);

    return tResult;
}

int64_t MediaSourceFile::GetSynchronizationTimestamp()
{
    int64_t tResult = 0;

    /******************************************
     * The following lines do the following:
     *   - use the current play-out position (given in seconds!) and transform it into micro seconds
     *   - return the calculated play-out time as synchronization timestamp in micro seconds
     *
     *   - when this approach is applied for both the video and audio stream (which is grabbed from the same local/remote file!),
     *     a time difference can be derived which corresponds to the A/V drift in micro seconds of the video and audio playback on the local(!) machine
     ******************************************/
    // we return the file position in us to allow A/V synchronization based on a file index
    tResult = GetSeekPos() /* in seconds */ * AV_TIME_BASE;

    return tResult;
}

bool MediaSourceFile::TimeShift(int64_t pOffset)
{
    LOG(LOG_VERBOSE, "Shifting %s time by: %ld", GetMediaTypeStr().c_str(), pOffset);
    float tCurPos = GetSeekPos();
    float tOffsetSeconds = (float)pOffset / AV_TIME_BASE;
    float tTargetPos = tCurPos + tOffsetSeconds;
    LOG(LOG_VERBOSE, "Seeking in %s file from %.2fs to %.2fs, offset: %.2fs", GetMediaTypeStr().c_str(), tCurPos, tTargetPos, tOffsetSeconds);
    return Seek(tTargetPos, false);
}

float MediaSourceFile::GetSeekPos()
{
    float tResult = 0;
    float tSeekEnd = GetSeekEnd();
	double tCurrentFrameIndex = mGrabberCurrentFrameIndex;

	if (!SupportsSeeking())
		return 0;

	if (tCurrentFrameIndex > 0)
	{
        //HINT: we need the corrected PTS values and the one from the file
        // correcting PTS value by a possible start PTS value (offset)
        if (mSourceStartPts > 0)
        {
            //LOG(LOG_VERBOSE, "Correcting PTS value by offset: %.2f", (float)mSourceStartPts);
            tCurrentFrameIndex -= mSourceStartPts;
        }

        if (mNumberOfFrames > 0)
        {
            //LOG(LOG_VERBOSE, "Rel. progress: %.2f %.2f", tCurrentFrameIndex, mNumberOfFrames);
            float tRelProgress = tCurrentFrameIndex / mNumberOfFrames;
            if (tRelProgress <= 1.0)
                tResult = (tSeekEnd * tRelProgress);
            else
                tResult = tSeekEnd;
        }
	}

	//LOG(LOG_VERBOSE, "Resulting %s file position: %.2f", GetMediaTypeStr().c_str(), tResult);

    return tResult;
}

bool MediaSourceFile::SupportsMultipleInputStreams()
{
    if ((mMediaType == MEDIA_AUDIO) && (mInputChannels.size() > 1))
        return true;
    else
        return false;
}

bool MediaSourceFile::SelectInputStream(int pIndex)
{
    bool tResult = true;

    LOG(LOG_WARN, "Selecting input channel: %d of %d max. channels, current is %d", pIndex, (int)mInputChannels.size(), mCurrentInputChannel);

    if (mCurrentInputChannel != pIndex)
    {
        mDesiredInputChannel = pIndex;

        float tCurPos = GetSeekPos();
        tResult = Reset();
        tResult &= Seek(tCurPos, false);
    }else
        LOG(LOG_VERBOSE, "Desired input channel %d is already selected, skipping %s source reset", pIndex, GetMediaTypeStr().c_str());

    return tResult;
}

string MediaSourceFile::CurrentInputStream()
{
    AVCodec *tCodec;
    string tResult = "";
    vector<string>::iterator tIt;

    int tCount = 0;

    // lock grabbing
    mGrabMutex.lock();

    if(mMediaSourceOpened)
    {
        for (tIt = mInputChannels.begin(); tIt != mInputChannels.end(); tIt++)
        {
            if (tCount == mCurrentInputChannel)
            {
                tResult = (*tIt);
                break;
            }
            tCount++;
        }
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

vector<string> MediaSourceFile::GetInputStreams()
{
    vector<string> tResult;

    // lock grabbing
    mGrabMutex.lock();

    if(mMediaSourceOpened)
    {
        tResult = mInputChannels;
    }

    // unlock grabbing
    mGrabMutex.unlock();

    return tResult;
}

void MediaSourceFile::CalibrateRTGrabbing()
{
    // adopt the stored pts value which represent the start of the media presentation in real-time useconds
    float  tRelativeFrameIndex = mGrabberCurrentFrameIndex - mSourceStartPts;
    double tRelativeTime = (int64_t)((double)AV_TIME_BASE * tRelativeFrameIndex / GetFrameRate());
    #ifdef MSMEM_DEBUG_CALIBRATION
        LOG(LOG_WARN, "Calibrating %s RT playback, current frame: %.2f, source start: %.2f, RT ref. time: %.2f->%.2f(diff: %.2f)", GetMediaTypeStr().c_str(), (float)mGrabberCurrentFrameIndex, (float)mSourceStartPts, mSourceStartTimeForRTGrabbing, (float)av_gettime() - tRelativeTime, (float)av_gettime() - tRelativeTime -mSourceStartTimeForRTGrabbing);
    #endif
    mSourceStartTimeForRTGrabbing = av_gettime() - tRelativeTime; //HINT: no "+ mDecoderFramePreBufferTime * AV_TIME_BASE" here because we start playback immediately
    #ifdef MSMEM_DEBUG_CALIBRATION
        LOG(LOG_WARN, "Calibrating %s RT playback: new PTS start: %.2f, rel. frame index: %.2f, rel. time: %.2f ms", GetMediaTypeStr().c_str(), mSourceStartTimeForRTGrabbing, tRelativeFrameIndex, (float)(tRelativeTime / 1000));
    #endif
}

void MediaSourceFile::StartDecoder()
{
	// setting last decoder file position
	//HINT: we can not use Seek() because this would lead to recursion
	if (SupportsSeeking())
	{
		LOG(LOG_VERBOSE, "Seeking to last %s decoder position: %.2f", GetMediaTypeStr().c_str(), mLastDecoderFilePosition);
		int tRes = (avformat_seek_file(mFormatContext, -1, INT64_MIN, mFormatContext->start_time + mLastDecoderFilePosition * AV_TIME_BASE, INT64_MAX, 0) >= 0);
		if (tRes < 0)
			LOG(LOG_ERROR, "Error during absolute seeking in %s source file because \"%s\"", GetMediaTypeStr().c_str(), strerror(AVUNERROR(tRes)));
	}
    MediaSourceMem::StartDecoder();
}

void MediaSourceFile::StopDecoder()
{
	// storing current decoder file position
	mLastDecoderFilePosition = GetSeekPos();

	MediaSourceMem::StopDecoder();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
