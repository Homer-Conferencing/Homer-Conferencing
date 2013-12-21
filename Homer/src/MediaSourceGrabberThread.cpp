/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a worker thread for video/audio grabbing
 * Since:   2012-08-08
 */

#include <Widgets/OverviewPlaylistWidget.h>
#include <MediaSourceGrabberThread.h>
#include <MediaSource.h>
#include <MediaSourceFile.h>
#include <MediaSourceMuxer.h>
#include <Logger.h>
#include <PacketStatistic.h>
#include <Configuration.h>
#include <Snippets.h>

#include <QInputDialog>
#include <QPalette>
#include <QImage>
#include <QMenu>
#include <QDockWidget>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QDir>
#include <QTime>
#include <QMutex>
#include <QPainter>
#include <QEvent>
#include <QApplication>
#include <QHostInfo>
#include <QStringList>
#include <QDesktopWidget>

#include <stdlib.h>
#include <vector>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;
using namespace Homer::Monitor;

// for debugging purposes: support video files
#define MEDIA_SOURCE_GRABBER_VIDEO_FILE

// for debugging purposes: support audio files
#define MEDIA_SOURCE_GRABBER_AUDIO_FILE

///////////////////////////////////////////////////////////////////////////////

MediaSourceGrabberThread::MediaSourceGrabberThread(QString pName, MediaSource *pMediaSource):
    QThread()
{
	mSyncClockMasterSource = NULL;
    mSyncClockAsap = false;
    mSetGrabResolutionAsap = false;
    mStartRecorderAsap = false;
    mStopRecorderAsap = false;
    mSetCurrentDeviceAsap = false;
    mSetInputStreamPreferencesAsap = false;
    mDesiredInputStream = 0;
    mPlayNewFileAsap = false;
    mResetMediaSourceAsap = false;
    mSeekAsap = false;
    mSeekPos = 0;
    mLastFrameNumber = 0;
    mSelectInputStreamAsap = false;
    mSourceAvailable = false;
    mEofReached = false;
    mTryingToOpenAFile = false;
    mPaused = false;
    mPausedPos = 0;
    if (pMediaSource == NULL)
        LOG(LOG_ERROR, "media source is NULL");
    mMediaSource = pMediaSource;
    mName = pName;
	if (pMediaSource->GetSourceType() == SOURCE_FILE)
		mDesiredFile = GetCurrentDevice();
	else
		mDesiredFile = "";
	mCurrentFile = mDesiredFile;
    blockSignals(true);
}

MediaSourceGrabberThread::~MediaSourceGrabberThread()
{
}

void MediaSourceGrabberThread::ResetSource()
{
    mResetMediaSourceAsap = true;
    mGrabbingCondition.wakeAll();
}

void MediaSourceGrabberThread::SetInputStreamPreferences(QString pCodec)
{
    mCodec = pCodec;
    mSetInputStreamPreferencesAsap = true;
    mGrabbingCondition.wakeAll();
}

void MediaSourceGrabberThread::SetStreamName(QString pName)
{
    mMediaSource->AssignStreamName(pName.toStdString());
}

QString MediaSourceGrabberThread::GetStreamName()
{
    return QString(mMediaSource->GetMediaSource()->GetStreamName().c_str());
}

QString MediaSourceGrabberThread::GetCurrentDevicePeer()
{
    return QString(mMediaSource->GetCurrentDevicePeerName().c_str());
}

QString MediaSourceGrabberThread::GetCurrentDevice()
{
    return QString(mMediaSource->GetCurrentDeviceName().c_str());
}

void MediaSourceGrabberThread::SetCurrentDevice(QString pName)
{
    if ((pName != "auto") && (pName != "") && (pName != "auto") && (pName != "automatic"))
    {
        mDeviceName = pName;
        mSetCurrentDeviceAsap = true;
        mGrabbingCondition.wakeAll();
    }
}

QString MediaSourceGrabberThread::GetDeviceDescription(QString pName)
{
    QString tResult = "";

    if (mMediaSource->GetMediaType() == MEDIA_VIDEO)
    {
        VideoDevices::iterator tIt;
        VideoDevices tVList;

        mMediaSource->getVideoDevices(tVList);
        for (tIt = tVList.begin(); tIt != tVList.end(); tIt++)
        {
            if (pName.toStdString() == tIt->Name)
            {
                // set the type prefix
                switch(tIt->Type)
                {
                    case VideoFile:
                            tResult = "[FILE]";
                            break;
                    case Camera:
                            tResult = "[CAMERA]";
                            break;
                    case Tv:
                            tResult = "[TV]";
                            break;
                    case SVideoComp:
                            tResult = "[S-VIDEO/COMP]";
                            break;
                    default:
                            tResult = "[GENERIC]";
                            break;
                }
                // add the detailed description
                tResult += ": " + QString(tIt->Desc.c_str());
                break;
            }
        }
    }else if (mMediaSource->GetMediaType() == MEDIA_AUDIO)
    {
        AudioDevices::iterator tIt;
        AudioDevices tVList;

        mMediaSource->getAudioDevices(tVList);
        for (tIt = tVList.begin(); tIt != tVList.end(); tIt++)
        {
            if (pName.toStdString() == tIt->Name)
            {
                // set the type prefix
                switch(tIt->Type)
                {
                    case AudioFile:
                            tResult = "[FILE]";
                            break;
                    case Microphone:
                            tResult = "[MICROPHONE]";
                            break;
                    case TvAudio:
                            tResult = "[TV]";
                            break;
                    default:
                            tResult = "[GENERIC]";
                            break;
                }
                // add the detailed description
                tResult += ": " + QString(tIt->Desc.c_str());
                break;
            }
        }
    }

    return tResult;
}

bool MediaSourceGrabberThread::PlayFile(QString pName)
{
	#ifndef MEDIA_SOURCE_GRABBER_VIDEO_FILE
		if (mMediaSource->GetMediaType() == MEDIA_VIDEO)
			return false;
	#endif
	#ifndef MEDIA_SOURCE_GRABBER_AUDIO_FILE
		if (mMediaSource->GetMediaType() == MEDIA_AUDIO)
			return false;
	#endif

    if (pName == "")
    {
    	LOG(LOG_VERBOSE, "Given file name was empty, setting old name: %s", mCurrentFile.toStdString().c_str());
    	pName = mCurrentFile;
    }else{
        PLAYLISTWIDGET.CheckAndRemoveFilePrefix(pName);
        pName = QString(pName.toLocal8Bit());
    }

    if (mMediaSource->GetMediaType() == MEDIA_VIDEO)
    {// video
        if (!OverviewPlaylistWidget::IsVideoFile(pName))
        {
            LOG(LOG_VERBOSE, "File %s is no %s file, skipping play", pName.toStdString().c_str(), mMediaSource->GetMediaTypeStr().c_str());
            HandlePlayFileError();
            return false;
        }
    }else if (mMediaSource->GetMediaType() == MEDIA_AUDIO)
    {// audio
        if (!OverviewPlaylistWidget::IsAudioFile(pName))
        {
            LOG(LOG_VERBOSE, "File %s is no %s file, skipping play", pName.toStdString().c_str(), mMediaSource->GetMediaTypeStr().c_str());
            HandlePlayFileError();
            return false;
        }
    }else
    {// unknown media type
        return false;
    }

	if ((mPaused) && (pName == mDesiredFile))
	{
		LOG(LOG_VERBOSE, "Continue playback of file: %s at pos.: %.2f", pName.toStdString().c_str(), mPausedPos);
		Seek(mPausedPos);
		mGrabbingStateMutex.lock();
		mPaused = false;
        mGrabbingStateMutex.unlock();
		mGrabbingCondition.wakeAll();
        mFrameTimestamps.clear();
        HandlePlayFileSuccess();
	}else
	{
		LOG(LOG_VERBOSE, "Trigger playback of file: %s", pName.toStdString().c_str());
		mDesiredFile = pName;
		mPlayNewFileAsap = true;
		mTryingToOpenAFile = true;
        mGrabbingCondition.wakeAll();
        HandlePlayFileSuccess();
	}
	return true;
}

void MediaSourceGrabberThread::PauseFile()
{
    if (mMediaSource->SupportsSeeking())
    {
        LOG(LOG_VERBOSE, "Triggered %s pause state at position: %.2f", mMediaSource->GetMediaTypeStr().c_str(), mPausedPos);
        mPausedPos = mMediaSource->GetSeekPos();
        mGrabbingStateMutex.lock();
        mPaused = true;
        mGrabbingStateMutex.unlock();
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, PauseFile() aborted");
}

bool MediaSourceGrabberThread::IsPaused()
{
    if ((mMediaSource != NULL) && (mMediaSource->SupportsSeeking()))
        return mPaused;
    else
        return false;
}

bool MediaSourceGrabberThread::IsPlayingFile()
{
    if ((mMediaSource != NULL) && (mMediaSource->SupportsSeeking()) && (mMediaSource->GetSourceType() == SOURCE_FILE))
        return true;
    else
        return false;
}

bool MediaSourceGrabberThread::IsSeeking()
{
    if ((mMediaSource != NULL) && (mMediaSource->SupportsSeeking()) && (mMediaSource->IsSeeking()))
        return true;
    else
        return false;
}

void MediaSourceGrabberThread::StopFile()
{
    if (mMediaSource->SupportsSeeking())
    {
        LOG(LOG_VERBOSE, "Trigger stop state");
        mPausedPos = 0;
        mGrabbingStateMutex.lock();
        mPaused = true;
        mGrabbingStateMutex.unlock();
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, StopFile() aborted");
}

bool MediaSourceGrabberThread::EofReached()
{
	return ((mEofReached) && (!mResetMediaSourceAsap) && (!mPlayNewFileAsap) && (!mSeekAsap) && (!mSetCurrentDeviceAsap));
}

QString MediaSourceGrabberThread::CurrentFile()
{
    if (IsPlayingFile())
    	return mCurrentFile;
    else
        return "";
}

bool MediaSourceGrabberThread::SupportsSeeking()
{
    if(mMediaSource != NULL)
        return mMediaSource->SupportsSeeking();
    else
        return false;
}

void MediaSourceGrabberThread::Seek(float pPos)
{
	LOG(LOG_VERBOSE, "Seeking to position: %.2f", pPos);
    mSeekPos = pPos;
    mSeekAsap = true;
    mGrabbingCondition.wakeAll();
}

float MediaSourceGrabberThread::GetSeekPos()
{
    return mMediaSource->GetSeekPos();
}

float MediaSourceGrabberThread::GetSeekEnd()
{
    float tResult = 0;

    tResult = mMediaSource->GetSeekEnd();

    return tResult;
}

void MediaSourceGrabberThread::SyncClock(MediaSource* pSource)
{
    if (pSource != NULL)
        LOG(LOG_VERBOSE, "Trigger clock synch. for %s with %s", mMediaSource->GetStreamName().c_str(), pSource->GetStreamName().c_str());
    else
        LOG(LOG_VERBOSE, "Trigger clock re-calibration for %s", mMediaSource->GetStreamName().c_str());
    if (mEofReached)
        LOG(LOG_WARN, "Try to synchronize while EOF was already reached");
    mSyncClockAsap = true;
    mSyncClockMasterSource = pSource;
    mGrabbingCondition.wakeAll();
}

bool MediaSourceGrabberThread::SupportsMultipleInputStreams()
{
    if (mMediaSource != NULL)
        return mMediaSource->SupportsMultipleInputStreams();
    else
        return false;
}

QString MediaSourceGrabberThread::GetCurrentInputStream()
{
    return QString(mMediaSource->CurrentInputStream().c_str());
}

void MediaSourceGrabberThread::SelectInputStream(int pIndex)
{
    if (pIndex != -1)
    {
        LOG(LOG_VERBOSE, "Will select new input channel %d after some short time", pIndex);
        mDesiredInputStream = pIndex;
        mSelectInputStreamAsap = true;
        mGrabbingCondition.wakeAll();
    }else
    {
        LOG(LOG_WARN, "Will not select new input channel -1, ignoring this request");
    }
}

QStringList MediaSourceGrabberThread::GetPossibleInputStreams()
{
    QStringList tResult;

    vector<string> tStreamList = mMediaSource->GetInputStreams();
    vector<string>::iterator tIt;
    for (tIt = tStreamList.begin(); tIt != tStreamList.end(); tIt++)
        tResult.push_back(QString((*tIt).c_str()));

    return tResult;
}

void MediaSourceGrabberThread::SetRelayActivation(bool pActive)
{
    if (mMediaSource->SupportsMuxing())
    { // source is a muxer
        MediaSourceMuxer *tMuxer = (MediaSourceMuxer*)mMediaSource;
        tMuxer->SetRelayActivation(pActive);
    }else
        LOG(LOG_VERBOSE, "Cannot set %s relay activation of a media source which is not a muxer", mMediaSource->GetMediaTypeStr().c_str());
}

QStringList MediaSourceGrabberThread::GetSourceMetaInfo()
{
    QStringList tAudioMetaInfo;

    if(mMediaSource != NULL)
    {
        int tEntries = mMediaSource->GetMetaData().size();
        if(tEntries > 0)
        {
            Homer::Multimedia::MetaData tSourceMetaData = mMediaSource->GetMetaData();
            //LOG(LOG_VERBOSE, "Found %d meta data entries");
            for(int i = 0; i < tEntries; i++)
            {
                //LOG(LOG_VERBOSE, "  ..entry: %s -> %s", tSourceMetaData[i].Key.c_str(), tSourceMetaData[i].Value.c_str());
                tAudioMetaInfo.push_back(QString(tSourceMetaData[i].Key.c_str()) + ": " + QString(tSourceMetaData[i].Value.c_str()));
            }
        }
    }

    return tAudioMetaInfo;
}

void MediaSourceGrabberThread::StartRecorder(std::string pSaveFileName, int pQuality)
{
    mSaveFileName = pSaveFileName;
    mSaveFileQuality = pQuality;
    mStartRecorderAsap = true;
    mGrabbingCondition.wakeAll();
}

void MediaSourceGrabberThread::StopRecorder()
{
    mStopRecorderAsap = true;
    mGrabbingCondition.wakeAll();
}

void MediaSourceGrabberThread::DoStartRecorder()
{
    mMediaSource->StartRecording(mSaveFileName, mSaveFileQuality);
    mStartRecorderAsap = false;
}

void MediaSourceGrabberThread::DoStopRecorder()
{
    mMediaSource->StopRecording();
    mStopRecorderAsap = false;
}

void MediaSourceGrabberThread::DoSelectInputStream()
{
    LOG(LOG_VERBOSE, "DoSelectInputStream now...");

    if(mDesiredInputStream == -1)
        return;

    // lock
    mDeliverMutex.lock();

    // restart frame grabbing device
    mSourceAvailable = mMediaSource->SelectInputStream(mDesiredInputStream);

    mResetMediaSourceAsap = false;
    mSelectInputStreamAsap = false;
    mPaused = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void MediaSourceGrabberThread::DoResetMediaSource()
{
    LOG(LOG_VERBOSE, "%s-DoResetMediaSource now...", mMediaSource->GetMediaTypeStr().c_str());
    // lock
    mDeliverMutex.lock();

    // delete old frame buffers
    DeinitFrameBuffers();

    // restart frame grabbing device
    mSourceAvailable = mMediaSource->Reset();
    if (!mSourceAvailable)
        LOG(LOG_WARN, "%s source is (temporary) not available after Reset() in DoResetMediaSource()", mMediaSource->GetMediaTypeStr().c_str());
    mResetMediaSourceAsap = false;
    mPaused = false;
    mFrameTimestamps.clear();

    // create new frame buffers
    InitFrameBuffers(Homer::Gui::MediaSourceGrabberThread::tr(MESSAGE_WAITING_FOR_DATA_AFTER_RESET));

    // unlock
    mDeliverMutex.unlock();

    LOG(LOG_VERBOSE, "..DoResetMediaSource finished");
}

void MediaSourceGrabberThread::DoSetInputStreamPreferences()
{
    LOG(LOG_VERBOSE, "DoSetInputStreamPreferences now...");

    // lock
    mDeliverMutex.lock();

    if (mMediaSource->SetInputStreamPreferences(mCodec.toStdString()))
    {
    	mSourceAvailable = mMediaSource->Reset();
        if (!mSourceAvailable)
            LOG(LOG_WARN, "%s source is (temporary) not available after Reset() in DoSetInputStreamPreferences()", mMediaSource->GetMediaTypeStr().c_str());
        mResetMediaSourceAsap = false;
    }

    mSetInputStreamPreferencesAsap = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void MediaSourceGrabberThread::DoSeek()
{
    LOG(LOG_VERBOSE, "%s-DoSeek now...", mMediaSource->GetMediaTypeStr().c_str());

    if (!mMediaSource->IsSeeking())
    {// no seeking running, we are allowed to start a new seek process
		// lock
		mDeliverMutex.lock();

		LOG(LOG_VERBOSE, "Seeking now to position %5.2f", mSeekPos);
		if (mMediaSource->SupportsSeeking())
		{
			mSourceAvailable = mMediaSource->Seek(mSeekPos);
			if(!mSourceAvailable)
			{
				LOG(LOG_WARN, "Source isn't available anymore after seeking");
			}
		}
		mEofReached = false;
		mSeekAsap = false;

		// unlock
		mDeliverMutex.unlock();
    }else
    {// another seek process is already running
        LOG(LOG_VERBOSE, "Delaying %s seek request", mMediaSource->GetMediaTypeStr().c_str());
    }
}

void MediaSourceGrabberThread::StopGrabber()
{
    LOG(LOG_VERBOSE, "Stopping %s-Grabber now...", mMediaSource->GetMediaTypeStr().c_str());
    mWorkerNeeded = false;

    int tSignalingRound = 0;
    while(isRunning())
    {
        if (tSignalingRound > 0)
            LOG(LOG_WARN, "Signaling attempt %d to stop %s %s grabber...", tSignalingRound, mMediaSource->GetMediaTypeStr().c_str(), mMediaSource->GetSourceTypeStr().c_str());
        tSignalingRound++;

        LOG(LOG_VERBOSE, "...setting %s grabbing-condition", mMediaSource->GetMediaTypeStr().c_str());
        mGrabbingCondition.wakeAll();
        LOG(LOG_VERBOSE, "...stopping %s source grabbing", mMediaSource->GetMediaTypeStr().c_str());
        mMediaSource->StopGrabbing();
        LOG(LOG_VERBOSE, "...%s-Grabber stopped", mMediaSource->GetMediaTypeStr().c_str());
        QThread::usleep(25 * 1000);
    }
}

void MediaSourceGrabberThread::CalculateFrameRate(float *pFrameRate)
{
    // calculate FPS
    if ((pFrameRate != NULL) && (mFrameTimestamps.size() > 1))
    {
        int64_t tCurrentTime = Time::GetTimeStamp();
        int64_t tMeasurementStartTime = mFrameTimestamps.first();
        int tMeasuredValues = mFrameTimestamps.size() - 1;
        double tMeasuredTimeDifference = ((double)tCurrentTime - tMeasurementStartTime) / 1000000;

        // now finally calculate the FPS as follows: "count of measured values / measured time difference"
        *pFrameRate = ((float)tMeasuredValues) / tMeasuredTimeDifference;
        #ifdef GRABBER_THREAD_DEBUG_FRAMES
            LOG(LOG_VERBOSE, "FPS: %f, interval %d, oldest %"PRId64"", *pFps, tMeasuredValues, tMeasurementStartTime);
        #endif
    }
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
