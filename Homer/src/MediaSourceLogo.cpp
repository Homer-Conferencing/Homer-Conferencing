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
 * Purpose: Implementation of a logo video source
 * Author:  Thomas Volkert
 * Since:   2012-10-25
 */

#include <MediaSourceLogo.h>
#include <MediaSource.h>
#include <PacketStatistic.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>

#include <QPainter>
#include <QLinearGradient>
#include <QTime>
#include <string.h>
#include <Snippets.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Multimedia;
using namespace Homer::Base;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceLogo::MediaSourceLogo(string pDesiredDevice):
    MediaSource("Logo: local capture")
{
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    mLogoRawPicture = NULL;

    // reset grabbing offset values
    mSourceResX = 352;
    mSourceResY = 288;
    mRecorderChunkNumber = 0;
    mLastTimeGrabbed = QTime(0, 0, 0, 0);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new Homer-Conferencing logo device when creating source object");
    }
}

MediaSourceLogo::~MediaSourceLogo()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

void MediaSourceLogo::getVideoDevices(VideoDevices &pVList)
{
    static bool tFirstCall = true;
    VideoDeviceDescriptor tDevice;

    #ifdef MSD_DEBUG_PACKETS
        tFirstCall = true;
    #endif

    if (tFirstCall)
        LOG(LOG_VERBOSE, "Enumerating hardware..");

    //#############################
    //### screen segment
    //#############################
    tDevice.Name = MEDIA_SOURCE_HOMER_LOGO;
    tDevice.Card = "logo";
	tDevice.Desc = "Homer-Conferencing logo capturing";
	if (tFirstCall)
        LOG(LOG_VERBOSE, "Found video device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
    pVList.push_back(tDevice);

    tFirstCall = false;
}

string MediaSourceLogo::GetCodecName()
{
    return "Raw";
}

string MediaSourceLogo::GetCodecLongName()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceLogo::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
	QImage tLogo(":/images/VersionLogoHomer.png");

	int tScreenId = -1;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(Logo)");

    if (mMediaSourceOpened)
        return false;

    // scale logo pixmap if neccessary
    if ((tLogo.width() > mTargetResX) || (tLogo.height() > mTargetResY))
    	tLogo = tLogo.scaled(mTargetResX, mTargetResY, Qt::KeepAspectRatio);

    mCurrentDevice = mDesiredDevice;

    // don't allow too slow grabbing
    if (pFps < MIN_GRABBING_FPS)
    	pFps = MIN_GRABBING_FPS;

    mSourceResX = pResX;
    mSourceResY = pResY;
	mFrameRate = pFps;
    mLogoRawPicture = malloc(mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL * sizeof(char));
    if (mLogoRawPicture == NULL)
    {
        LOG(LOG_ERROR, "Buffer allocation failed");
        return false;
    }

    // create logo buffer
	QImage tTargetImage = QImage((unsigned char*)mLogoRawPicture, mTargetResX, mTargetResY, QImage::Format_RGB32);
	QLinearGradient tGradient(tTargetImage.width() / 2, 0, tTargetImage.width() / 2, tTargetImage.height());//0.5, 0, 0.5, 1);
	tGradient.setColorAt(0, QColor(8, 94, 129, 255));
	tGradient.setColorAt(0.5,QColor(118, 201, 225, 255));
	tGradient.setColorAt(1, QColor(255, 255, 255, 255));
	QPainter *tTargetPainter = new QPainter(&tTargetImage);
	tTargetPainter->fillRect(tTargetImage.rect(), tGradient);
	tTargetPainter->drawImage((mTargetResX - tLogo.width()) / 2, (mTargetResY - tLogo.height()) / 2, tLogo);
	delete tTargetPainter;

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..fps: %3.2f", mFrameRate);
    LOG(LOG_INFO, "    ..device: %s", mCurrentDevice.c_str());
    LOG(LOG_INFO, "    ..resolution: %d * %d", mSourceResX, mSourceResY);
    LOG(LOG_INFO, "    ..source frame size: %d", mSourceResX * mSourceResY * MSD_BYTES_PER_PIXEL);
    LOG(LOG_INFO, "    ..destination frame size: %d", mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL);

    //######################################################
    //### initiate local variables
    //######################################################
    FpsEmulationInit();
    mSourceStartPts = 0;
    mChunkNumber = 0;
    mMediaType = MEDIA_VIDEO;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourceLogo::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceLogo::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type");
        return false;
    }

    if (mMediaSourceOpened)
    {
        StopRecording();

        mMediaSourceOpened = false;

        // free internal buffer
        free(mLogoRawPicture);
        mLogoRawPicture = NULL;

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mMediaType = MEDIA_UNKNOWN;

    return tResult;
}

bool MediaSourceLogo::SupportsRecording()
{
	return true;
}

void MediaSourceLogo::StopRecording()
{
    if (mRecording)
    {
        MediaSource::StopRecording();
        mRecorderChunkNumber = 0;
    }
}

int MediaSourceLogo::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
{
    // lock grabbing
    mGrabMutex.lock();

    if (pChunkBuffer == NULL)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Tried to grab while chunk buffer doesn't exist");
        return -1;
    }

    if (!mMediaSourceOpened)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Tried to grab while video source is closed");
        return -1;
    }

    if (mGrabbingStopped)
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Tried to grab while video source is paused");
        return -1;
    }

    if ((pChunkSize != 0 /* the application doesn't give us the chunk size */) && (pChunkSize < mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL))
    {
        // unlock grabbing
        mGrabMutex.unlock();

        LOG(LOG_ERROR, "Tried to grab while chunk buffer is too small (given: %d needed: %d)", pChunkSize, mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL);
        return -1;
    }

    // was wait interrupted because of a call to StopGrabbing ?
	if (mGrabbingStopped)
	{
	    // unlock grabbing
		mGrabMutex.unlock();
		return -1;
	}

	// get the time since last successful grabbing
    QTime tCurrentTime = QTime::currentTime();
	int tTimeDiff = mLastTimeGrabbed.msecsTo(tCurrentTime);

	// calculate the time which corresponds to the request FPS
	int tNeededTimeDiff = 1000 / mFrameRate;

    if (tTimeDiff < tNeededTimeDiff)
    {// skip capturing when we are too fast
        #ifdef MSL_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Logo capturing delayed because system is too fast, time diff: %d, needed time diff: %d", tTimeDiff, tNeededTimeDiff);
        #endif
		Thread::Suspend((tNeededTimeDiff - tTimeDiff) * 1000);
    }

    // copy the logo to the destination buffer
    memcpy(pChunkBuffer, mLogoRawPicture, mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL);

    mLastTimeGrabbed = QTime::currentTime();

    // unlock grabbing
    mGrabMutex.unlock();

    // return size of decoded frame
    pChunkSize = mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL;

    AnnouncePacket(pChunkSize);

    return ++mChunkNumber;
}

GrabResolutions MediaSourceLogo::GetSupportedVideoGrabResolutions()
{
    VideoFormatDescriptor tFormat;

    mSupportedVideoFormats.clear();

    tFormat.Name="VGA";       //      640 * 480
    tFormat.ResX = 640;
    tFormat.ResY = 480;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="DVD";        //      720 × 576
    tFormat.ResX = 720;
    tFormat.ResY = 576;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="CIF9";       //     1056 × 864
    tFormat.ResX = 1056;
    tFormat.ResY = 864;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="SXGA";       //     1280 × 1024
    tFormat.ResX = 1280;
    tFormat.ResY = 1024;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="WXGA+";      //     1440 × 900
    tFormat.ResX = 1440;
    tFormat.ResY = 900;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="SXGA+";       //     1440 × 1050
    tFormat.ResX = 1440;
    tFormat.ResY = 1050;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="WUXGA";       //     1920 * 1200
    tFormat.ResX = 1920;
    tFormat.ResY = 1200;
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="Original";
    tFormat.ResX = 640;
    tFormat.ResY = 480;
    mSupportedVideoFormats.push_back(tFormat);

    return mSupportedVideoFormats;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
