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
 * Purpose: Implementation of a desktop video source
 * Author:  Thomas Volkert
 * Since:   2010-02-15
 */

#include <MediaSourceDesktop.h>
#include <MediaSource.h>
#include <PacketStatistic.h>
#include <ProcessStatisticService.h>
#include <Logger.h>
#include <HBThread.h>
#include <Dialogs/SegmentSelectionDialog.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QPainter>
#include <QPaintDevice>
#include <QWidget>
#include <QTime>
#include <QWaitCondition>
#include <string.h>
#include <Snippets.h>
#ifdef APPLE
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace Homer::Multimedia;
using namespace Homer::Base;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

MediaSourceDesktop::MediaSourceDesktop(string pDesiredDevice):
    MediaSource("QT: local capture")
{
    // set category for packet statistics
    ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);

    mWidget = NULL;
    mScreenshot = NULL;

    // reset grabbing offset values
    mGrabOffsetX = 0;
    mGrabOffsetY = 0;
    mSourceResX = 352;
    mSourceResY = 288;
    mLastTimeGrabbed = QTime(0, 0, 0, 0);

    bool tNewDeviceSelected = false;
    SelectDevice(pDesiredDevice, MEDIA_VIDEO, tNewDeviceSelected);
    if (!tNewDeviceSelected)
    {
        LOG(LOG_INFO, "Haven't selected new Qt-desktop device when creating source object");
    }
}

MediaSourceDesktop::~MediaSourceDesktop()
{
    if (mMediaSourceOpened)
        CloseGrabDevice();
}

void MediaSourceDesktop::getVideoDevices(VideoDevicesList &pVList)
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
    tDevice.Name = "DESKTOP: screen segment";
    tDevice.Card = "segment";
	#ifdef APPLE
    	tDevice.Desc = "OSX Cocoa based screen segment capturing";
	#else
		tDevice.Desc = "Qt based screen segment capturing";
	#endif
	if (tFirstCall)
        LOG(LOG_VERBOSE, "Found video device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
    pVList.push_back(tDevice);

    QDesktopWidget *tDesktop = QApplication::desktop();
    if (tDesktop != NULL)
    {
        if (tFirstCall)
        {
            LOG(LOG_VERBOSE, "Desktop found..");
            LOG(LOG_VERBOSE, "  ..resolution: %d * %d", tDesktop->width(), tDesktop->height());
            LOG(LOG_VERBOSE, "  ..screens: %d", tDesktop->numScreens());
            LOG(LOG_VERBOSE, "  ..virtualized: %d", tDesktop->isVirtualDesktop());
        }

        for (int i = 0; i < tDesktop->numScreens(); i++)
        {
            QWidget *tScreen = tDesktop->screen(i);

            string tScreenName = "screen ";
            tScreenName += char(i + 48);
            tDevice.Name = "DESKTOP: ";
            tDevice.Name += tScreenName;
            tDevice.Card = tScreenName;
			#ifdef APPLE
            	tDevice.Desc = "OSX Cocoa based capturing from screen " + toString(i) + " with resolution " + toString(tScreen->width()) + "*" + toString(tScreen->height()) + " pixels";
			#else
            	tDevice.Desc = "Qt based capturing from screen " + toString(i) + " with resolution " + toString(tScreen->width()) + "*" + toString(tScreen->height()) + " pixels";
			#endif

            if (tFirstCall)
            {
                LOG(LOG_VERBOSE, "  ..screen %d with resolution: %d * %d", i, tScreen->width(), tScreen->height());
                LOG(LOG_VERBOSE, "Found video device: %s (card: %s)", tDevice.Name.c_str(), tDevice.Card.c_str());
            }

            pVList.push_back(tDevice);
        }
    }

    tFirstCall = false;
}

void MediaSourceDesktop::StopGrabbing()
{
	MediaSource::StopGrabbing();
	mWaitConditionScreenshotUpdated.wakeAll();
}

string MediaSourceDesktop::GetCodecName()
{
    return "Raw";
}

string MediaSourceDesktop::GetCodecLongName()
{
    return "Raw";
}

///////////////////////////////////////////////////////////////////////////////

bool MediaSourceDesktop::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    int tScreenId = -1;

    LOG(LOG_VERBOSE, "Trying to open the video source");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type detected");
        return false;
    }

    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(Desktop)");

    if (mMediaSourceOpened)
        return false;

    if ((mDesiredDevice == "auto") || (mDesiredDevice == "segment"))
    {// screen segment
        mWidget = QApplication::desktop()->screen(0); //per default support only grabbing from screen 0
        mSourceResX = pResX;
        mSourceResY = pResY;
    }else
    {// screen 0/1/n
        if ((mDesiredDevice != "") && (mDesiredDevice.substr(0, 7) == "screen "))
            tScreenId = mDesiredDevice.substr(7,1).c_str()[0] - 48;

        if (tScreenId == -1)
        {
            LOG(LOG_ERROR, "Selected screen \"%s\" not found", mDesiredDevice.c_str());
            return false;
        }

        LOG(LOG_VERBOSE, "Try to access screen %d", tScreenId);

        mWidget = QApplication::desktop()->screen(tScreenId);

        mSourceResX = mWidget->width();
        mSourceResY = mWidget->height();
    }

    mCurrentDevice = mDesiredDevice;

    // don't allow too slow grabbing
    if (pFps < MIN_GRABBING_FPS)
    	pFps = MIN_GRABBING_FPS;

	mFrameRate = pFps;
    mScreenshot = malloc(mTargetResX * mTargetResY * 4 * sizeof(char));
    if (mScreenshot == NULL)
    {
        LOG(LOG_ERROR, "Buffer allocation failed");
        return false;
    }

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
    mLastTimeGrabbed == QTime(0, 0, 0, 0);
    mSourceStartPts = -1;
    mChunkNumber = 0;
    mMediaType = MEDIA_VIDEO;
    mMediaSourceOpened = true;

    return true;
}

bool MediaSourceDesktop::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    LOG(LOG_ERROR, "Wrong media type");
    return false;
}

bool MediaSourceDesktop::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close");

    if (mMediaType == MEDIA_AUDIO)
    {
        LOG(LOG_ERROR, "Wrong media type");
        return false;
    }

    mMutexGrabberActive.lock();

    if (mMediaSourceOpened)
    {
        StopRecording();

        mMediaSourceOpened = false;

        mWidget = NULL;

        // free internal buffer
        free(mScreenshot);
        mScreenshot = NULL;

        LOG(LOG_INFO, "...closed");

        tResult = true;
    }else
        LOG(LOG_INFO, "...wasn't open");

    mGrabbingStopped = false;
    mScreenshotUpdated = false;
    mMediaType = MEDIA_UNKNOWN;

    mMutexGrabberActive.unlock();

    return tResult;
}

void MediaSourceDesktop::DoSetVideoGrabResolution(int pResX, int pResY)
{
    LOG(LOG_VERBOSE, "Setting desktop grab resolution to %d*%d", pResX, pResY);

    mMutexScreenshot.lock();

    free(mScreenshot);
    mSourceResX = pResX;
    mSourceResY = pResY;
    mScreenshot = malloc(mSourceResX * mSourceResY * 4 * sizeof(char));

    mMutexScreenshot.unlock();
}

bool MediaSourceDesktop::SupportsRecording()
{
	return true;
}

void MediaSourceDesktop::CreateScreenshot()
{
    AVFrame             *tRGBFrame;

    mMutexGrabberActive.lock();

    if (!mMediaSourceOpened)
    {
    	mMutexGrabberActive.unlock();
    	return;
    }

    if (mWidget == NULL)
    {
    	LOG(LOG_ERROR, "Capture widget is invalid");
    	mMutexGrabberActive.unlock();
    	return;
    }

    QTime tCurrentTime = QTime::currentTime();
    int tTimeDiff = mLastTimeGrabbed.msecsTo(tCurrentTime);

    //### skip capturing when we are too slow
    if (tTimeDiff < 1000 / (mFrameRate + 0.5 /* some tolerance! */))
    {
        #ifdef MSD_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Screen capturing skipped because system is too fast");
        #endif
		mMutexGrabberActive.unlock();
    	return;
    }

    if (mLastTimeGrabbed == QTime(0, 0, 0, 0))
    {
        mLastTimeGrabbed = tCurrentTime;
        mMutexGrabberActive.unlock();
        return;
    }else
        mLastTimeGrabbed = tCurrentTime;

    //### skip capturing when we are too slow
    if (tTimeDiff > 1000 / MIN_GRABBING_FPS)
    {
    	LOG(LOG_WARN, "Screen capturing skipped because system is too busy");
    	mMutexGrabberActive.unlock();
    	return;
    }

    //####################################################################
    //### do the grabbing and scaling stuff
    //####################################################################
    QPixmap tSourcePixmap;
    // screen capturing
	#ifdef APPLE
		CGImageRef tOSXWindowImage = CGWindowListCreateImage(CGRectInfinite, kCGWindowListOptionOnScreenOnly, mWidget->winId(), kCGWindowImageDefault);
		tSourcePixmap = QPixmap::fromMacCGImageRef(tOSXWindowImage).copy(mGrabOffsetX, mGrabOffsetY, mSourceResX, mSourceResY);
	#else
		tSourcePixmap = QPixmap::grabWindow(mWidget->winId(), mGrabOffsetX, mGrabOffsetY);
		if((tSourcePixmap.width() != mSourceResX) || (tSourcePixmap.height() != mSourceResY))
		{
			#ifdef MSD_DEBUG_PACKETS
				LOG(LOG_VERBOSE, "Clip the source pixmap from %d*%d to %d*%d", tSourcePixmap.width(), tSourcePixmap.height(), mSourceResX, mSourceResY);
			#endif
			tSourcePixmap = tSourcePixmap.copy(0, 0, mSourceResX, mSourceResY);
		}
	#endif

    if(!tSourcePixmap.isNull())
    {
		// record screenshot via ffmpeg
		if (mRecording)
		{
			if ((tRGBFrame = AllocFrame()) == NULL)
			{
				LOG(LOG_ERROR, "Unable to allocate memory for RGB frame");
			}else
			{
				void *tOriginalScreenShotData = malloc(mSourceResX * mSourceResY * 4 * sizeof(char));
				QImage tSourceImage = QImage((unsigned char*)tOriginalScreenShotData, mSourceResX, mSourceResY, QImage::Format_RGB32);
				QPainter *tSourcePainter = new QPainter(&tSourceImage);
				tSourcePainter->drawPixmap(0, 0, tSourcePixmap);
				delete tSourcePainter;

				// Assign appropriate parts of buffer to image planes in tRGBFrame
				FillFrame(tRGBFrame, tOriginalScreenShotData, PIX_FMT_RGB32, mSourceResX, mSourceResY);

				// set frame number in corresponding entries within AVFrame structure
				tRGBFrame->pts = mChunkNumber + 1;
				tRGBFrame->coded_picture_number = mChunkNumber + 1;
				tRGBFrame->display_picture_number = mChunkNumber + 1;

				// emulate set FPS
				tRGBFrame->pts = FpsEmulationGetPts();

				// re-encode the frame and write it to file
				RecordFrame(tRGBFrame);
			}
		}

		// get the scaled version of the capture screen segment
		QPixmap tTargetPixmap = tSourcePixmap.scaled(mTargetResX, mTargetResY);

		// lock screenshot buffer
		mMutexScreenshot.lock();
		QImage tTargetImage = QImage((unsigned char*)mScreenshot, mTargetResX, mTargetResY, QImage::Format_RGB32);
		QPainter *tTargetPainter = new QPainter(&tTargetImage);
		tTargetPainter->drawPixmap(0, 0, tTargetPixmap);
		delete tTargetPainter;
		mScreenshotUpdated = true;
		// notify consumer about new screenshot
		mWaitConditionScreenshotUpdated.wakeAll();
		// unlock screenshot buffer again
		mMutexScreenshot.unlock();
    }else
    	LOG(LOG_ERROR, "Source pixmap is invalid");

    mMutexGrabberActive.unlock();
}

int MediaSourceDesktop::GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk)
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

    // additional Qt based lock for QWaitCondition
    mMutexScreenshot.lock();

    // waiting for new data
    mWaitConditionScreenshotUpdated.wait(&mMutexScreenshot);

    // was wait interrupted because of a call to StopGrabbing ?
	if (mGrabbingStopped)
	{
	    // unlock again and enable new screenshots
	    mMutexScreenshot.unlock();

	    // unlock grabbing
		mGrabMutex.unlock();
		return -1;
	}

    memcpy(pChunkBuffer, mScreenshot, mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL);
    mScreenshotUpdated = false;

    // unlock again and enable new screenshots
    mMutexScreenshot.unlock();

    // unlock grabbing
    mGrabMutex.unlock();

    // return size of decoded frame
    pChunkSize = mTargetResX * mTargetResY * MSD_BYTES_PER_PIXEL;

    AnnouncePacket(pChunkSize);

    return ++mChunkNumber;
}

GrabResolutions MediaSourceDesktop::GetSupportedVideoGrabResolutions()
{
    VideoFormatDescriptor tFormat;

    mSupportedVideoFormats.clear();

    tFormat.Name="CIF";        //      352 × 288
    tFormat.ResX = 352;
    tFormat.ResY = 288;
    mSupportedVideoFormats.push_back(tFormat);

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

    tFormat.Name="Desktop";
    tFormat.ResX = QApplication::desktop()->width();
    tFormat.ResY = QApplication::desktop()->height();
    mSupportedVideoFormats.push_back(tFormat);

    tFormat.Name="Original";
    tFormat.ResX = mSourceResX;
    tFormat.ResY = mSourceResY;
    mSupportedVideoFormats.push_back(tFormat);

    return mSupportedVideoFormats;
}

///////////////////////////////////////////////////////////////////////////////

void MediaSourceDesktop::SelectSegment(QWidget *pParent)
{
	if (mRecording)
	{
		DoShowInfo(GetObjectNameStr(this).c_str(), __LINE__, pParent, "Recording active", "Desktop capture settings cannot be changed if recording is active");
		return;
	}
    int tOldGrabOffsetX = mGrabOffsetX;
    int tOldGrabOffsetY = mGrabOffsetY;
    int tOldSourceResX = mSourceResX;
    int tOldSourceResY = mSourceResY;

    SegmentSelectionDialog *tSelectDialog = new SegmentSelectionDialog(pParent, this);
    if (tSelectDialog->exec() != QDialog::Accepted)
    {
        LOG(LOG_VERBOSE, "Resetting to original segment capture configuration");
        mGrabOffsetX = tOldGrabOffsetX;
        mGrabOffsetY = tOldGrabOffsetY;
        mSourceResX = tOldSourceResX;
        mSourceResY = tOldSourceResY;
    }
    delete tSelectDialog;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
