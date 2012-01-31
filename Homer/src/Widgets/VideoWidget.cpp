/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of the widget for video display
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 */

/*
         Concept:
                1.) draw hour glas via the palette's backgroundRole
                2.) stop hour glas when first frame is received
                3.) deactivate background drawing and auto. update via Qt
                        -> setAutoFillBackground(false);
                        -> setAttribute(Qt::WA_NoSystemBackground, true);
                        -> setAttribute(Qt::WA_PaintOnScreen, true);
                        -> setAttribute(Qt::WA_OpaquePaintEvent, true);
                4.) draw manually the widget's content, use frame data and background color

 */

#include <Widgets/VideoWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <ProcessStatisticService.h>
#include <MediaSource.h>
#include <MediaSourceFile.h>
#include <PacketStatistic.h>
#include <Logger.h>
#include <PacketStatistic.h>
#include <Dialogs/AddNetworkSinkDialog.h>
#include <Configuration.h>
#include <Meeting.h>
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
#include <list>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

#define VIDEO_OPEN_ERROR                (QEvent::User + 1001)
#define VIDEO_NEW_FRAME                 (QEvent::User + 1002)
#define VIDEO_NEW_SOURCE                (QEvent::User + 1003)
#define VIDEO_NEW_SOURCE_RESOLUTION     (QEvent::User + 1004)

class VideoEvent:
    public QEvent
{
public:
    VideoEvent(int pReason, QString pDescription):QEvent(QEvent::User), mReason(pReason), mDescription(pDescription)
    {

    }
    ~VideoEvent()
    {

    }
    int GetReason()
    {
        return mReason;
    }
    QString GetDescription(){
        return mDescription;
    }

private:
    int mReason;
    QString mDescription;
};

///////////////////////////////////////////////////////////////////////////////
VideoWidget::VideoWidget(QWidget* pParent):
    QWidget(pParent)
{
    mResX = 640;
    mResY = 480;
    mCurrentFrameNumber = 0;
    mLastFrameNumber = 0;
    mHourGlassAngle = 0;
    mHourGlassOffset = 0;
    mCustomEventReason = 0;
    mShowLiveStats = false;
    mRecorderStarted = false;
    mVideoPaused = false;
    mAspectRatio = ASPECT_RATIO_ORIGINAL;
    mVideoMirroredHorizontal = false;
    mVideoMirroredVertical = false;
    mCurrentApplicationFocusedWidget = NULL;
    mVideoWorker = NULL;
    mMainWindow = NULL;
    mAssignedAction = NULL;

    parentWidget()->hide();
    hide();
}

void VideoWidget::Init(QMainWindow* pMainWindow, MediaSource *pVideoSource, QMenu *pMenu, QString pActionTitle, QString pWidgetTitle, bool pVisible)
{
    mVideoSource = pVideoSource;
    mVideoTitle = pActionTitle;
    mMainWindow = pMainWindow;

    //####################################################################
    //### create the remaining necessary menu item
    //####################################################################

    if (pMenu != NULL)
    {
        mAssignedAction = pMenu->addAction(pActionTitle);
        mAssignedAction->setCheckable(true);
        mAssignedAction->setChecked(pVisible);
        QIcon tIcon;
        tIcon.addPixmap(QPixmap(":/images/Checked.png"), QIcon::Normal, QIcon::On);
        tIcon.addPixmap(QPixmap(":/images/Unchecked.png"), QIcon::Normal, QIcon::Off);
        mAssignedAction->setIcon(tIcon);
    }

    //####################################################################
    //### update GUI
    //####################################################################
    setWindowTitle(pWidgetTitle);
    if (mAssignedAction != NULL)
        connect(mAssignedAction, SIGNAL(triggered()), this, SLOT(ToggleVisibility()));
    SetResolutionFormat(CIF);
    if (mVideoSource != NULL)
    {
        mVideoWorker = new VideoWorkerThread(mVideoSource, this);
        mVideoWorker->start(QThread::TimeCriticalPriority);
    }

    mHourGlassTimer = new QTimer(this);
    connect(mHourGlassTimer, SIGNAL(timeout()), this, SLOT(ShowHourGlass()));
    mHourGlassTimer->start(250);
    LOG(LOG_VERBOSE, "Created hour glas timer with ID 0x%X", mHourGlassTimer->timerId());

    //####################################################################
    //### speedup video presentation by setting the following
    //####################################################################
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    setMinimumSize(352, 288);
    setMaximumSize(16777215, 16777215);
    SetVisible(pVisible);
    mNeedBackgroundUpdatesUntillNextFrame = true;
}

VideoWidget::~VideoWidget()
{
	// we are going to destroy mCurrentFrame -> stop repainting now!
	setUpdatesEnabled(false);

	if (mVideoWorker != NULL)
    {
    	mVideoWorker->StopGrabber();
    	if (!mVideoWorker->wait(250))
        {
            LOG(LOG_VERBOSE, "Going to force termination of worker thread");
            mVideoWorker->terminate();
        }

        if (!mVideoWorker->wait(5000))
        {
            LOG(LOG_ERROR, "Termination of VideoWorker-Thread timed out");
        }
    	delete mVideoWorker;
    }
    if (mAssignedAction != NULL)
        delete mAssignedAction;
}

///////////////////////////////////////////////////////////////////////////////

void VideoWidget::closeEvent(QCloseEvent* pEvent)
{
	// are we a fullscreen widget?
	if (windowState() & Qt::WindowFullScreen)
    {
        LOG(LOG_VERBOSE, "Got closeEvent in VideoWidget while it is in fullscreen mode, will forward this to main window");
        QApplication::postEvent(mMainWindow, new QCloseEvent());
    }else
    {
    	ToggleVisibility();
    }
}

void VideoWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QAction *tAction;

    GrabResolutions tGrabResolutions = mVideoSource->GetSupportedVideoGrabResolutions();
    GrabResolutions::iterator tIt;
    int tCurResX, tCurResY;
    list<string> tRegisteredVideoSinks = mVideoSource->ListRegisteredMediaSinks();
    list<string>::iterator tRegisteredVideoSinksIt;

    mVideoSource->GetVideoGrabResolution(tCurResX, tCurResY);

    QMenu tMenu(this);

    //###############################################################################
    //### SCREENSHOT
    //###############################################################################
    tAction = tMenu.addAction("Save picture");
    QIcon tIcon6;
    tIcon6.addPixmap(QPixmap(":/images/Photo.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon6);

    //###############################################################################
    //### RECORD
    //###############################################################################
    QIcon tIcon5;
    if (mRecorderStarted)
    {
        tAction = tMenu.addAction("Stop recording");
        tIcon5.addPixmap(QPixmap(":/images/Audio - Stop.png"), QIcon::Normal, QIcon::Off);
    }else
    {
        tAction = tMenu.addAction("Record video");
        tIcon5.addPixmap(QPixmap(":/images/Audio - Record.png"), QIcon::Normal, QIcon::Off);
    }
    tAction->setIcon(tIcon5);

    tMenu.addSeparator();

    //###############################################################################
    //### STREAM INFO
    //###############################################################################
    if (mShowLiveStats)
        tAction = tMenu.addAction("Hide stream info");
    else
        tAction = tMenu.addAction("Show stream info");
    QIcon tIcon4;
    tIcon4.addPixmap(QPixmap(":/images/Info.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon4);
    tAction->setCheckable(true);
    tAction->setChecked(mShowLiveStats);

    QList<QKeySequence> tIKeys;
    tIKeys.push_back(Qt::Key_I);
    tAction->setShortcuts(tIKeys);

    //###############################################################################
    //### "Full screen"
    //###############################################################################
    if (windowState() & Qt::WindowFullScreen)
    {
        tAction = tMenu.addAction("Window mode");
    }else
    {
        tAction = tMenu.addAction("Full screen");
    }
    QList<QKeySequence> tFSKeys;
    tFSKeys.push_back(Qt::Key_F);
    tFSKeys.push_back(Qt::Key_Escape);
    tAction->setShortcuts(tFSKeys);

    QIcon tIcon3;
    tIcon3.addPixmap(QPixmap(":/images/Computer.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon3);

    //###############################################################################
    //### ASPECT RATION
    //###############################################################################
    QMenu *tAspectRatioMenu = tMenu.addMenu("Aspect ratio");
    //###############################################################################
    //### "Keep aspect ratio"
    //###############################################################################

    tAction = tAspectRatioMenu->addAction("Original");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_ORIGINAL)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("1 : 1");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_1x1)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("4 : 3");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_4x3)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("5 : 4");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_5x4)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("16 : 9");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_16x9)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("16 : 10");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_16x10)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAction = tAspectRatioMenu->addAction("Full window");
    tAction->setCheckable(true);
    if (mAspectRatio == ASPECT_RATIO_WINDOW)
        tAction->setChecked(true);
    else
        tAction->setChecked(false);

    tAspectRatioMenu->setIcon(tIcon3);

    //###############################################################################
    //### RESOLUTIONS
    //###############################################################################
    QMenu *tResMenu = tMenu.addMenu("Source resolution");
    //###############################################################################
    //### add all possible resolutions which are reported by the media soruce
    //###############################################################################
    if (tGrabResolutions.size())
    {
        for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
        {
            QAction *tResAction = tResMenu->addAction(QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX).arg(tIt->ResY));
            tResAction->setCheckable(true);
            if ((tIt->ResX == tCurResX) && (tIt->ResY == tCurResY))
                tResAction->setChecked(true);
            else
                tResAction->setChecked(false);
        }
    }
    tResMenu->setIcon(tIcon3);

    //###############################################################################
    //### MIRRORING
    //###############################################################################
    if (mVideoMirroredHorizontal)
    {
        tAction = tMenu.addAction("Unmirror horizontally");
        tAction->setCheckable(true);
        tAction->setChecked(true);
    }else
    {
        tAction = tMenu.addAction("Mirror horizontally");
        tAction->setCheckable(true);
        tAction->setChecked(false);
    }
    tAction->setIcon(tIcon3);

    if (mVideoMirroredVertical)
    {
        tAction = tMenu.addAction("Unmirror vertically");
        tAction->setCheckable(true);
        tAction->setChecked(true);
    }else
    {
        tAction = tMenu.addAction("Mirror vertically");
        tAction->setCheckable(true);
        tAction->setChecked(false);
    }
    tAction->setIcon(tIcon3);

    //###############################################################################
    //### STREAM RELAY
    //###############################################################################
    if(mVideoSource->SupportsRelaying())
    {
        QMenu *tVideoSinksMenu = tMenu.addMenu("Relay stream");
        QIcon tIcon7;
        tIcon7.addPixmap(QPixmap(":/images/ArrowRightGreen.png"), QIcon::Normal, QIcon::Off);
        tVideoSinksMenu->setIcon(tIcon7);

        tAction =  tVideoSinksMenu->addAction("Add network sink");
        QIcon tIcon8;
        tIcon8.addPixmap(QPixmap(":/images/Plus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon8);

        QMenu *tRegisteredVideoSinksMenu = tVideoSinksMenu->addMenu("Registered sinks");
        QIcon tIcon9;
        tIcon9.addPixmap(QPixmap(":/images/ArrowRightGreen.png"), QIcon::Normal, QIcon::Off);
        tRegisteredVideoSinksMenu->setIcon(tIcon9);

        if (tRegisteredVideoSinks.size())
        {
            for (tRegisteredVideoSinksIt = tRegisteredVideoSinks.begin(); tRegisteredVideoSinksIt != tRegisteredVideoSinks.end(); tRegisteredVideoSinksIt++)
            {
                QAction *tSinkAction = tRegisteredVideoSinksMenu->addAction(QString(tRegisteredVideoSinksIt->c_str()));
                tSinkAction->setCheckable(true);
                tSinkAction->setChecked(true);
            }
        }
    }

    if(CONF.DebuggingEnabled())
    {
        //###############################################################################
        //### STREAM PAUSE/CONTINUE
        //###############################################################################
        QIcon tIcon10;
        if (mVideoPaused)
        {
            tAction = tMenu.addAction("Continue stream");
            tIcon10.addPixmap(QPixmap(":/images/Audio - Play.png"), QIcon::Normal, QIcon::Off);
        }else
        {
            tAction = tMenu.addAction("Pause stream");
            tIcon10.addPixmap(QPixmap(":/images/Audio - Pause.png"), QIcon::Normal, QIcon::Off);
        }
        tAction->setIcon(tIcon10);
    }

    tMenu.addSeparator();

    //###############################################################################
    //### RESET SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Reset source");
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/Reload.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);

    //###############################################################################
    //### CLOSE SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Close video");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/Close.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    //###############################################################################
    //### RESULTING REACTION
    //###############################################################################
    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Close video") == 0)
        {
            ToggleVisibility();
            return;
        }
        if (tPopupRes->text().compare("Reset source") == 0)
        {
            mVideoWorker->ResetSource();
            return;
        }
        if (tPopupRes->text().compare("Save picture") == 0)
        {
            SavePicture();
            return;
        }
        if (tPopupRes->text().compare("Stop recording") == 0)
        {
            StopRecorder();
            return;
        }
        if (tPopupRes->text().compare("Record video") == 0)
        {
            StartRecorder();
            return;
        }
        if (tPopupRes->text().compare("Mirror horizontally") == 0)
        {
            mVideoMirroredHorizontal = true;
            return;
        }
        if (tPopupRes->text().compare("Unmirror horizontally") == 0)
        {
            mVideoMirroredHorizontal = false;
            return;
        }
        if (tPopupRes->text().compare("Mirror vertically") == 0)
        {
            mVideoMirroredVertical = true;
            return;
        }
        if (tPopupRes->text().compare("Unmirror vertically") == 0)
        {
            mVideoMirroredVertical = false;
            return;
        }
        if (tPopupRes->text().compare("Show stream info") == 0)
        {
            mShowLiveStats = true;
            return;
        }
        if (tPopupRes->text().compare("Hide stream info") == 0)
        {
            mShowLiveStats = false;
            return;
        }
        if (tPopupRes->text().compare("Pause stream") == 0)
        {
            mVideoPaused = true;
            mVideoWorker->SetFrameDropping(true);
            return;
        }
        if (tPopupRes->text().compare("Continue stream") == 0)
        {
            mVideoPaused = false;
            mVideoWorker->SetFrameDropping(false);
            return;
        }
        if (tPopupRes->text().compare("Add network sink") == 0)
        {
            DialogAddNetworkSink();
            return;
        }
        if ((tPopupRes->text().compare("Full screen") == 0) || (tPopupRes->text().compare("Window mode") == 0))
        {
            ToggleFullScreenMode();
            return;
        }
        if (tPopupRes->text().compare("Full window") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_WINDOW;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("Original") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_ORIGINAL;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("1 : 1") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_1x1;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("4 : 3") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_4x3;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("5 : 4") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_5x4;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("16 : 9") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_16x9;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("16 : 10") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_16x10;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        for (tRegisteredVideoSinksIt = tRegisteredVideoSinks.begin(); tRegisteredVideoSinksIt != tRegisteredVideoSinks.end(); tRegisteredVideoSinksIt++)
        {
            if (tPopupRes->text().compare(QString(tRegisteredVideoSinksIt->c_str())) == 0)
            {
                mVideoSource->UnregisterGeneralMediaSink((*tRegisteredVideoSinksIt));
                return;
            }
        }

        if (tGrabResolutions.size())
        {
            //printf("%s\n", tPopupRes->text().toStdString().c_str());
            for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
            {
                //printf("to compare: |%s| |%s|\n", (QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX).arg(tIt->ResY)).toStdString().c_str(), tPopupRes->text().toStdString().c_str());
                if (tPopupRes->text().compare(QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX).arg(tIt->ResY)) == 0)
                {
                    mVideoWorker->SetGrabResolution(tIt->ResX, tIt->ResY);
                    SetResolution(tIt->ResX, tIt->ResY);
                }
            }
        }
    }
}

void VideoWidget::DialogAddNetworkSink()
{
    AddNetworkSinkDialog tANSDialog(this, mVideoSource);

    tANSDialog.exec();
}

void VideoWidget::ShowFrame(void* pBuffer, float pFps, int pFrameNumber)
{
	int tFrameOutputWidth = 0;
	int tFrameOutputHeight = 0;

	if (!isVisible())
        return;

    setUpdatesEnabled(false);

    //#############################################################
    //### get frame from media source and mirror it if selected
    //#############################################################
    QImage tCurrentFrame = QImage((unsigned char*)pBuffer, mResX, mResY, QImage::Format_RGB32);
    if (tCurrentFrame.isNull())
    {
        setUpdatesEnabled(true);
    	return;
    }else
    	mCurrentFrame = tCurrentFrame;

    if ((mVideoMirroredHorizontal) || (mVideoMirroredVertical))
    	mCurrentFrame = mCurrentFrame.mirrored(mVideoMirroredHorizontal, mVideoMirroredVertical);

    //#############################################################
    //### scale to the dimension of this video widget
    //#############################################################
    // keep aspect ratio if requested
	QTime tTime = QTime::currentTime();
	Qt::AspectRatioMode tAspectMode = Qt::IgnoreAspectRatio;
	switch(mAspectRatio)
	{
		case ASPECT_RATIO_WINDOW:
			tFrameOutputWidth = width();
			tFrameOutputHeight = height();
			break;
		case ASPECT_RATIO_ORIGINAL:
			tFrameOutputWidth = width();
			tFrameOutputHeight = height();
			tAspectMode = Qt::KeepAspectRatio;
			break;
		case ASPECT_RATIO_1x1:
			tFrameOutputWidth = mCurrentFrame.width();
			tFrameOutputHeight = tFrameOutputWidth; // adapt aspect ratio
			break;
		case ASPECT_RATIO_4x3:
			tFrameOutputWidth = mCurrentFrame.width();
			tFrameOutputHeight = (int)(tFrameOutputWidth / 1.33); // adapt aspect ratio
			break;
		case ASPECT_RATIO_5x4:
			tFrameOutputWidth = mCurrentFrame.width();
			tFrameOutputHeight = (int)(tFrameOutputWidth / 1.25); // adapt aspect ratio
			break;
		case ASPECT_RATIO_16x9:
			tFrameOutputWidth = mCurrentFrame.width();
			tFrameOutputHeight = (int)(tFrameOutputWidth / 1.77); // adapt aspect ratio
			break;
		case ASPECT_RATIO_16x10:
			tFrameOutputWidth = mCurrentFrame.width();
			tFrameOutputHeight = (int)(tFrameOutputWidth / 1.6); // adapt aspect ratio
			break;
	}

	// resize frame to best fitting size, related to video widget
	if ((mAspectRatio != ASPECT_RATIO_WINDOW) && (mAspectRatio != ASPECT_RATIO_ORIGINAL))
	{
		float tRatio = (float)width() / tFrameOutputWidth;
		int tNewFrameOutputWidth = width();
		int tNewFrameOutputHeight = (int)(tRatio * tFrameOutputHeight);
		if(tNewFrameOutputHeight > height())
		{
			tRatio = (float)height() / tFrameOutputHeight;
			tFrameOutputHeight = height();
			tFrameOutputWidth = (int)(tRatio * tFrameOutputWidth);
		}else
		{
			tFrameOutputHeight = tNewFrameOutputHeight;
			tFrameOutputWidth = tNewFrameOutputWidth;
		}
	}

	mCurrentFrame = mCurrentFrame.scaled(tFrameOutputWidth, tFrameOutputHeight, tAspectMode, CONF.GetSmoothVideoPresentation() ? Qt::SmoothTransformation : Qt::FastTransformation);
	tFrameOutputWidth = mCurrentFrame.width();
	tFrameOutputHeight = mCurrentFrame.height();

    int tTimeDiff = QTime::currentTime().msecsTo(tTime);
    // did we spend too much time with transforming the image?
    if ((CONF.GetSmoothVideoPresentation()) && (tTimeDiff > 1000 / 3)) // at least we assume 3 FPS!
    {
        CONF.SetSmoothVideoPresentation(false);
        ShowInfo("System too busy", "Your system is too busy to do smooth transformation. Fast transformation will be used from now.");
    }

    QImage *tShownFrame = &mCurrentFrame;
    QPainter *tPainter = new QPainter(tShownFrame);

    //#############################################################
    //### draw statistics
    //#############################################################
    if (mShowLiveStats)
    {
		QString tAspectRatio = "";
    	switch(mAspectRatio)
		{
			case ASPECT_RATIO_ORIGINAL:
				tAspectRatio = "Original";
				break;
			case ASPECT_RATIO_WINDOW:
				tAspectRatio = "Window";
				break;
			case ASPECT_RATIO_1x1:
				tAspectRatio = "1 : 1";
				break;
			case ASPECT_RATIO_4x3:
				tAspectRatio = "4 : 3";
				break;
			case ASPECT_RATIO_5x4:
				tAspectRatio = "5 : 4";
				break;
			case ASPECT_RATIO_16x9:
				tAspectRatio = "16 : 9";
				break;
			case ASPECT_RATIO_16x10:
				tAspectRatio = "16 : 10";
				break;
		}

		int tHour = 0, tMin = 0, tSec = 0, tTime = mVideoSource->GetSeekPos();
        tSec = tTime % 60;
        tTime /= 60;
        tMin = tTime % 60;
        tHour = tTime / 60;

        int tMaxHour = 0, tMaxMin = 0, tMaxSec = 0, tMaxTime = mVideoSource->GetSeekEnd();
        tMaxSec = tMaxTime % 60;
        tMaxTime /= 60;
        tMaxMin = tMaxTime % 60;
        tMaxHour = tMaxTime / 60;

        QFont tFont = QFont("Tahoma", 12, QFont::Bold);
        tFont.setFixedPitch(true);
        tPainter->setRenderHint(QPainter::TextAntialiasing, true);
        tPainter->setFont(tFont);

        int tSourceResX = 0, tSourceResY = 0;
        mVideoSource->GetVideoSourceResolution(tSourceResX, tSourceResY);

        tPainter->setPen(QColor(Qt::darkRed));
        tPainter->drawText(5, 41, " Source: " + mVideoWorker->GetCurrentDevice());
        tPainter->drawText(5, 61, " Frame: " + QString("%1").arg(pFrameNumber) + (mVideoSource->GetChunkDropConter() ? (" (" + QString("%1").arg(mVideoSource->GetChunkDropConter()) + " dropped)") : ""));
        tPainter->drawText(5, 81, " Fps: " + QString("%1").arg(pFps, 4, 'f', 2, ' '));
        tPainter->drawText(5, 101, " Codec: " + QString((mVideoSource->GetCodecName() != "") ? mVideoSource->GetCodecName().c_str() : "unknown") + " (" + QString("%1").arg(tSourceResX) + "*" + QString("%1").arg(tSourceResY) + ")");
        tPainter->drawText(5, 121, " Output: " + QString("%1").arg(tFrameOutputWidth) + "*" + QString("%1").arg(tFrameOutputHeight) + " (" + tAspectRatio + ")");
        if (mVideoSource->SupportsSeeking())
            tPainter->drawText(5, 141, " Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0'));

        tPainter->setPen(QColor(Qt::red));
        tPainter->drawText(4, 40, " Source: " + mVideoWorker->GetCurrentDevice());
        tPainter->drawText(4, 60, " Frame: " + QString("%1").arg(pFrameNumber) + (mVideoSource->GetChunkDropConter() ? (" (" + QString("%1").arg(mVideoSource->GetChunkDropConter()) + " dropped)") : ""));
        tPainter->drawText(4, 80, " Fps: " + QString("%1").arg(pFps, 4, 'f', 2, ' '));
        tPainter->drawText(4, 100, " Codec: " + QString((mVideoSource->GetCodecName() != "") ? mVideoSource->GetCodecName().c_str() : "unknown") + " (" + QString("%1").arg(tSourceResX) + "*" + QString("%1").arg(tSourceResY) + ")");
        tPainter->drawText(4, 120, " Output: "  + QString("%1").arg(tFrameOutputWidth) + "*" + QString("%1").arg(tFrameOutputHeight) + " (" + tAspectRatio + ")");
        if (mVideoSource->SupportsSeeking())
			tPainter->drawText(4, 140, " Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0'));
    }

    //#############################################################
    //### draw record icon
    //#############################################################
    int tMSecs = QTime::currentTime().msec();
    if ((mRecorderStarted) and (tMSecs % 500 < 250))
    {
        QPixmap tPixmap = QPixmap(":/images/Audio - Record.png");
        tPainter->drawPixmap(10, 10, tPixmap);
    }

    //#############################################################
    //### draw pause icon
    //#############################################################
    if ((mVideoPaused) and (tMSecs % 500 < 250))
    {
        QPixmap tPixmap = QPixmap(":/images/Audio - Paused.png");
        tPainter->drawPixmap(30, 10, tPixmap);
    }

    delete tPainter;
    setUpdatesEnabled(true);

    if(mNeedBackgroundUpdatesUntillNextFrame)
    {
    	mNeedBackgroundUpdatesUntillNextFrame = false;
    	mNeedBackgroundUpdate = true;
    }
}

void VideoWidget::ShowHourGlass()
{
    if (!isVisible())
        return;

    setUpdatesEnabled(false);

    int tWidth = width(), tHeight = height();

    //printf("Res: %d %d\n", mResX, mResY);
    mCurrentFrame = QImage(tWidth, tHeight, QImage::Format_RGB32);
    mCurrentFrame.fill(QColor(Qt::darkGray).rgb());

    QPixmap tPixmap = QPixmap(":/images/Sandglass1.png");
    if (!tPixmap.isNull())
    	tPixmap = tPixmap.scaledToHeight(40, Qt::SmoothTransformation);

    QImage *tShownFrame = &mCurrentFrame;
    QPainter *tPainter1 = new QPainter(tShownFrame);
    tPainter1->setRenderHint(QPainter::Antialiasing);

    tPainter1->save();
    tPainter1->translate((qreal)(tWidth / 2), (qreal)(tHeight / 2));

    int tHourGlassOffsetMax = 20 < (tHeight / 2 - 20) ? 20 : (tHeight / 2 - 20);
    if ((mHourGlassOffset += 2) > tHourGlassOffsetMax * 2)
        mHourGlassOffset = 0;
    if (mHourGlassOffset <= tHourGlassOffsetMax)
        tPainter1->translate(0, mHourGlassOffset);
    else
        tPainter1->translate(0, tHourGlassOffsetMax * 2 - mHourGlassOffset);

    if ((mHourGlassAngle += 10) > 360)
        mHourGlassAngle = 0;
    tPainter1->rotate(mHourGlassAngle);

    tPainter1->translate((qreal)-(tPixmap.width() / 2), (qreal)-(tPixmap.height() / 2));
    tPainter1->drawPixmap(0, 0, tPixmap);
    tPainter1->restore();

    delete tPainter1;
    setUpdatesEnabled(true);
}

void VideoWidget::InformAboutNewFrame()
{
    QApplication::postEvent(this, new VideoEvent(VIDEO_NEW_FRAME, ""));
}

void VideoWidget::InformAboutOpenError(QString pSourceName)
{
    QApplication::postEvent(this, new VideoEvent(VIDEO_OPEN_ERROR, pSourceName));
}

void VideoWidget::InformAboutNewSource()
{
    QApplication::postEvent(this, new VideoEvent(VIDEO_NEW_SOURCE, ""));
}

void VideoWidget::InformAboutNewSourceResolution()
{
    QApplication::postEvent(this, new VideoEvent(VIDEO_NEW_SOURCE_RESOLUTION, ""));
}

void VideoWidget::SetOriginalResolution()
{
    GrabResolutions tGrabResolutions = mVideoSource->GetSupportedVideoGrabResolutions();
    GrabResolutions::iterator tIt;
    if (tGrabResolutions.size())
    {
        for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
        {
            //LOG(LOG_ERROR, "Res: %s", tIt->Name.c_str());
            if (tIt->Name == "Original")
            {
                mVideoWorker->SetGrabResolution(tIt->ResX, tIt->ResY);
                SetResolution(tIt->ResX, tIt->ResY);
            }
        }
    }
}

VideoWorkerThread* VideoWidget::GetWorker()
{
    return mVideoWorker;
}

void VideoWidget::SetResolution(int mX, int mY)
{
    setUpdatesEnabled(false);
    if ((mResX != mX) || (mResY != mY))
    {
        mResX = mX;
        mResY = mY;
        setMinimumSize(mResX, mResY);
        if (windowState() != Qt::WindowFullScreen)
            resize(mResX, mResY);
    }
    setUpdatesEnabled(true);
	mNeedBackgroundUpdatesUntillNextFrame = true;
}

void VideoWidget::SetResolutionFormat(VideoFormat pFormat)
{
    int tResX;
    int tResY;

    MediaSource::VideoFormat2Resolution(pFormat, tResX, tResY);
    SetResolution(tResX, tResY);
}

void VideoWidget::ShowFullScreen()
{
	// get screen that contains the largest part of he VideoWidget
	QDesktopWidget *tDesktop = new QDesktopWidget();
	int tScreenNumber = tDesktop->screenNumber(this);

	// get screen geometry
	QRect tScreenRes = QApplication::desktop()->screenGeometry(tScreenNumber);

	// prepare and set fullscreen on the corresponding screen
	move(QPoint(tScreenRes.x(), tScreenRes.y()));
	resize(tScreenRes.width(), tScreenRes.height());
	showFullScreen();
	delete tDesktop;
}

void VideoWidget::ToggleFullScreenMode()
{
    setUpdatesEnabled(false);
    LOG(LOG_VERBOSE, "Found window state: %d", (int)windowState());
    if (windowState() & Qt::WindowFullScreen)
    {
        setWindowFlags(windowFlags() ^ Qt::Window);
        showNormal();
    }else
    {
        setWindowFlags(windowFlags() | Qt::Window);
        ShowFullScreen();
    }
    setUpdatesEnabled(true);
	mNeedBackgroundUpdatesUntillNextFrame = true;
}

void VideoWidget::ToggleVisibility()
{
    if (isVisible())
        SetVisible(false);
    else
        SetVisible(true);
}

void VideoWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        mVideoWorker->SetFrameDropping(false);
        move(mWinPos);
        parentWidget()->show();
        show();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(true);

    }else
    {
        mVideoWorker->SetFrameDropping(true);
        mWinPos = pos();
        parentWidget()->hide();
        hide();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(false);
    }
}

void VideoWidget::SavePicture()
{
    QString tFileName = QFileDialog::getSaveFileName(this,
                                                     "Save picture",
                                                     CONF.GetDataDirectory() + "/Homer-Snapshot.png",
                                                     "Windows Bitmap (*.bmp);;"\
                                                     "Joint Photographic Experts Group (*.jpg);;"\
                                                     "Portable Graymap (*.pgm);;"\
                                                     "Portable Network Graphics (*.png);;"\
                                                     "Portable Pixmap (*.ppm);;"\
                                                     "X11 Bitmap (*.xbm);;"\
                                                     "X11 Pixmap (*.xpm)",
                                                     &*(new QString("Portable Network Graphics (*.png)")),
                                                     QFileDialog::DontUseNativeDialog);

    if (tFileName.isEmpty())
        return;

    CONF.SetDataDirectory(tFileName.left(tFileName.lastIndexOf('/')));

    mCurrentFrame.setText("Description", "Homer-Snapshot-" + mVideoTitle);
    LOG(LOG_VERBOSE, "Going to save screenshot to %s", tFileName.toStdString().c_str());
    mCurrentFrame.save(tFileName);
}

void VideoWidget::StartRecorder()
{
    QString tFileName = OverviewPlaylistWidget::LetUserSelectVideoSaveFile(this, "Set file name for video recording");

    if (tFileName.isEmpty())
        return;

    // get the quality value from the user
    bool tAck = false;
    QStringList tPosQuals;
    for (int i = 1; i < 11; i++)
        tPosQuals.append(QString("%1").arg(i * 10));
    QString tQualityStr = QInputDialog::getItem(this, "Select recording quality", "Record with quality:                                      ", tPosQuals, 9, false, &tAck);

    if (!tAck)
        return;

    // convert QString to int
    bool tConvOkay = false;
    int tQuality = tQualityStr.toInt(&tConvOkay, 10);
    if (!tConvOkay)
    {
        LOG(LOG_ERROR, "Error while converting QString to int");
        return;
    }

    if(tQualityStr.isEmpty())
        return;

    // finally start the recording
    mVideoWorker->StartRecorder(tFileName.toStdString(), tQuality);

    //record source data
    mRecorderStarted = true;
}

void VideoWidget::StopRecorder()
{
    mVideoWorker->StopRecorder();
    mRecorderStarted = false;
}

void VideoWidget::paintEvent(QPaintEvent *pEvent)
{
    QPainter tPainter(this);
    QColor tBackgroundColor;

    // selected background color depends on the window state
    if (windowState() & Qt::WindowFullScreen)
        tBackgroundColor = QColor(Qt::black);
    else
        tBackgroundColor = QApplication::palette().brush(backgroundRole()).color();

    // wait until valid new frame is available to be drawn
    if (mCurrentFrame.isNull())
    {
        tPainter.fillRect(0, 0, width(), height(), tBackgroundColor);
        return;
    }

    // force background update as long as we don't have the focus -> we are maybe covered by other applications GUI
    // force background update if focus has changed
    QWidget *tWidget = QApplication::focusWidget();
    if ((tWidget == NULL) || (tWidget != mCurrentApplicationFocusedWidget))
    	mNeedBackgroundUpdatesUntillNextFrame = true;
    mCurrentApplicationFocusedWidget = tWidget;

    if ((mNeedBackgroundUpdate) || (mNeedBackgroundUpdatesUntillNextFrame))
    {
        //### calculate background surrounding the current frame
        int tFrameWidth = width() - mCurrentFrame.width();
        if (tFrameWidth > 0)
        {

            tPainter.fillRect(0, 0, tFrameWidth / 2, height(), tBackgroundColor);
            tPainter.fillRect(mCurrentFrame.width() + tFrameWidth / 2, 0, tFrameWidth / 2 + 1, height(), tBackgroundColor);
        }

        int tFrameHeight = height() - mCurrentFrame.height();
        if (tFrameHeight > 0)
        {
            tPainter.fillRect(tFrameWidth / 2, 0, mCurrentFrame.width(), tFrameHeight / 2, tBackgroundColor);
            tPainter.fillRect(tFrameWidth / 2, mCurrentFrame.height() + tFrameHeight / 2, mCurrentFrame.width(), tFrameHeight / 2 + 1, tBackgroundColor);
        }
        mNeedBackgroundUpdate = false;
    }

    // draw only fitting new frames (otherwise we could have a race condition and a too big frame which might be drawn)
    if ((mCurrentFrame.width() <= width()) && (mCurrentFrame.height() <= height()))
        tPainter.drawImage((width() - mCurrentFrame.width()) / 2, (height() - mCurrentFrame.height()) / 2, mCurrentFrame);
    pEvent->accept();
}

void VideoWidget::resizeEvent(QResizeEvent *pEvent)
{
	// user triggered resize event?
//	if (QApplication::mouseButtons() & Qt::LeftButton)
//		setMinimumSize(192, 96);

	setUpdatesEnabled(false);
    QWidget::resizeEvent(pEvent);
    mNeedBackgroundUpdatesUntillNextFrame = true;
    pEvent->accept();
    setUpdatesEnabled(true);
}

void VideoWidget::keyPressEvent(QKeyEvent *pEvent)
{
	if ((pEvent->key() == Qt::Key_Escape) && (windowState() == Qt::WindowFullScreen))
	{
        setWindowFlags(windowFlags() ^ Qt::Window);
        showNormal();
	}
    if (pEvent->key() == Qt::Key_F)
    {
        ToggleFullScreenMode();
    }
    if (pEvent->key() == Qt::Key_I)
    {
        if (mShowLiveStats)
        	mShowLiveStats = false;
        else
        	mShowLiveStats = true;
    }
    if (pEvent->key() == Qt::Key_A)
    {
    	mAspectRatio++;
    	if(mAspectRatio > ASPECT_RATIO_16x10)
    		mAspectRatio = ASPECT_RATIO_ORIGINAL;
    	mNeedBackgroundUpdatesUntillNextFrame = true;
    }
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *pEvent)
{
    ToggleFullScreenMode();
    pEvent->accept();
}

void VideoWidget::customEvent(QEvent *pEvent)
{
    void* tFrame;
    float tFps;

    // make sure we have a user event here
    if (pEvent->type() != QEvent::User)
    {
        pEvent->ignore();
        return;
    }

    VideoEvent *tVideoEvent = (VideoEvent*)pEvent;

    switch(tVideoEvent->GetReason())
    {
        case VIDEO_NEW_FRAME:
            tVideoEvent->accept();
            if (isVisible())
            {

                mLastFrameNumber = mCurrentFrameNumber;
                // hint: we don't have to synchronize with resolution changes because Qt has only one synchronous working event loop!
                mCurrentFrameNumber = mVideoWorker->GetCurrentFrame(&tFrame, &tFps);
                if (mCurrentFrameNumber != -1)
                {
                    // make sure there is no hour glass anymore
                    if (mHourGlassTimer->isActive())
                    {
                        LOG(LOG_VERBOSE, "Deactivating hour glass because first frame was received");

                        mHourGlassTimer->stop();

                        //#############################################################################
                        //### deactivate background painting and speedup video presentation
                        //### each future painting task will be managed by our own paintEvent function
                        //#############################################################################
                        setAutoFillBackground(false);
                        setAttribute(Qt::WA_NoSystemBackground, true);
                        setAttribute(Qt::WA_PaintOnScreen, true);
                        setAttribute(Qt::WA_OpaquePaintEvent, true);
                        mNeedBackgroundUpdatesUntillNextFrame = true;
                    }

                    // display the current video frame
                    ShowFrame(tFrame, tFps, mCurrentFrameNumber);
                    //printf("VideoWidget-Frame number: %d\n", mCurrentFrameNumber);
                    // do we have a frame order problem?
                    if ((mLastFrameNumber > mCurrentFrameNumber) && (mCurrentFrameNumber  > 9 /* -1 means error, 1 is received after every reset, use "9" because of possible latencies */))
                        LOG(LOG_ERROR, "Frame ordering problem detected! [%d->%d]", mLastFrameNumber, mCurrentFrameNumber);
                    //if (tlFrameNumber == tFrameNumber)
                        //printf("VideoWidget-unnecessary frame grabbing detected!\n");
                }
            }
            break;
        case VIDEO_OPEN_ERROR:
            tVideoEvent->accept();
            if (tVideoEvent->GetDescription() != "")
                ShowWarning("Video source not available", "The selected video source \"" + tVideoEvent->GetDescription() + "\" is not available. Please, select another one!");
            else
            	ShowWarning("Video source not available", "The selected video source auto detection was not successful. Please, connect an additional video device to your hardware!");
            break;
        case VIDEO_NEW_SOURCE:
            tVideoEvent->accept();
            SetOriginalResolution();
            if(!mHourGlassTimer->isActive())
            {
                LOG(LOG_VERBOSE, "Reactivating hour glas timer");
                mHourGlassTimer->start(250);
            }
            break;
        case VIDEO_NEW_SOURCE_RESOLUTION:
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            break;
        default:
            break;
    }
    mCustomEventReason = 0;
}

VideoWorkerThread::VideoWorkerThread(MediaSource *pVideoSource, VideoWidget *pVideoWidget):
    QThread()
{
    mSetGrabResolutionAsap = false;
    mResetVideoSourceAsap = false;
    mStartRecorderAsap = false;
    mStopRecorderAsap = false;
    mSetCurrentDeviceAsap = false;
    mSetInputStreamPreferencesAsap = false;
    mPlayNewFileAsap = false;
    mSourceAvailable = false;
    mEofReached = false;
    mPaused = false;
    mPausedPos = 0;
    mDesiredFile = "";
    mResX = 352;
    mResY = 288;
    if (pVideoSource == NULL)
        LOG(LOG_ERROR, "Video source is NULL");
    mVideoSource = pVideoSource;
    mVideoWidget = pVideoWidget;
    blockSignals(true);
    mFrameCurrentIndex = FRAME_BUFFER_SIZE - 1;
    mFrameGrabIndex = 0;
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        mFrame[i] = mVideoSource->AllocChunkBuffer(mFrameSize[i], MEDIA_VIDEO);
        mFrameNumber[i] = 0;
        InitFrameBuffer(i);
    }
    mDropFrames = false;
    mWorkerWithNewData = false;
}

VideoWorkerThread::~VideoWorkerThread()
{
	mWorkerWithNewData = false;
	for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
        mVideoSource->FreeChunkBuffer(mFrame[i]);
}

void VideoWorkerThread::InitFrameBuffer(int pBufferId)
{
    QImage tFrameImage = QImage((unsigned char*)mFrame[pBufferId], mResX, mResY, QImage::Format_RGB32);
    QPainter *tPainter = new QPainter(&tFrameImage);
    tPainter->setRenderHint(QPainter::TextAntialiasing, true);
    tPainter->fillRect(0, 0, mResX, mResY, QColor(Qt::darkGray));
    tPainter->setFont(QFont("Tahoma", 16));
    tPainter->setPen(QColor(Qt::black));
    tPainter->drawText(5, 70, "..no data");
    delete tPainter;
}

void VideoWorkerThread::SetFrameDropping(bool pDrop)
{
    mDropFrames = pDrop;
}

void VideoWorkerThread::SetGrabResolution(int mX, int mY)
{
    if ((mResX != mX) || (mResY != mY))
    {
        mResX = mX;
        mResY = mY;
        mSetGrabResolutionAsap = true;
    }
}

void VideoWorkerThread::ResetSource()
{
    mResetVideoSourceAsap = true;
}

void VideoWorkerThread::SetInputStreamPreferences(QString pCodec)
{
    mCodec = pCodec;
    mSetInputStreamPreferencesAsap = true;
}

void VideoWorkerThread::SetStreamName(QString pName)
{
    mVideoSource->AssignStreamName(pName.toStdString());
}

QString VideoWorkerThread::GetStreamName()
{
    return QString(mVideoSource->GetMediaSource()->GetStreamName().c_str());
}

QString VideoWorkerThread::GetCurrentDevice()
{
    return QString(mVideoSource->GetCurrentDeviceName().c_str());
}

void VideoWorkerThread::SetCurrentDevice(QString pName)
{
    if ((pName != "auto") && (pName != "") && (pName != "auto") && (pName != "automatic"))
    {
        mDeviceName = pName;
        mSetCurrentDeviceAsap = true;
    }
}

QStringList VideoWorkerThread::GetPossibleDevices()
{
    QStringList tResult;
    VideoDevicesList::iterator tIt;
    VideoDevicesList tVList;

    mVideoSource->getVideoDevices(tVList);
    for (tIt = tVList.begin(); tIt != tVList.end(); tIt++)
        tResult.push_back(QString(tIt->Name.c_str()));

    return tResult;
}

QString VideoWorkerThread::GetDeviceDescription(QString pName)
{
    VideoDevicesList::iterator tIt;
    VideoDevicesList tVList;

    mVideoSource->getVideoDevices(tVList);
    for (tIt = tVList.begin(); tIt != tVList.end(); tIt++)
        if (pName.toStdString() == tIt->Name)
            return QString(tIt->Desc.c_str());

    return "";
}

void VideoWorkerThread::PlayFile(QString pName)
{
    // remove "file:///" and "file://" from the beginning if existing
    #ifdef WIN32
        if (pName.startsWith("file:///"))
            pName = pName.right(pName.size() - 8);

        if (pName.startsWith("file://"))
            pName = pName.right(pName.size() - 7);
    #else
        if (pName.startsWith("file:///"))
            pName = pName.right(pName.size() - 7);

        if (pName.startsWith("file://"))
            pName = pName.right(pName.size() - 6);
    #endif

    pName = QString(pName.toLocal8Bit());

	if ((mPaused) && (pName == mDesiredFile))
	{
		LOG(LOG_VERBOSE, "Continue playback of file: %s", pName.toStdString().c_str());
		mVideoSource->Seek(mPausedPos, false);
		mPaused = false;
        mFrameTimestamps.clear();
	}else
	{
		LOG(LOG_VERBOSE, "Trigger playback of file: %s", pName.toStdString().c_str());
		mDesiredFile = pName;
		mPlayNewFileAsap = true;
	}
}

void VideoWorkerThread::PauseFile()
{
	LOG(LOG_VERBOSE, "Trigger pause state");
	mPausedPos = mVideoSource->GetSeekPos();
	mPaused = true;
}

void VideoWorkerThread::StopFile()
{
	LOG(LOG_VERBOSE, "Trigger stop state");
	mPausedPos = 0;
	mPaused = true;
}

bool VideoWorkerThread::EofReached()
{
	return (((mEofReached) && (!mResetVideoSourceAsap) && (!mPlayNewFileAsap)) || (mPlayNewFileAsap) || (mSetCurrentDeviceAsap));
}

QString VideoWorkerThread::CurrentFile()
{
	return mCurrentFile;
}

bool VideoWorkerThread::SupportsSeeking()
{
    return mVideoSource->SupportsSeeking();
}

void VideoWorkerThread::Seek(int pPos)
{
    mVideoSource->Seek(mVideoSource->GetSeekEnd() * pPos / 1000, false);
}

int64_t VideoWorkerThread::GetSeekPos()
{
    return mVideoSource->GetSeekPos();
}

int64_t VideoWorkerThread::GetSeekEnd()
{
    return mVideoSource->GetSeekEnd();
}

bool VideoWorkerThread::SupportsMultipleChannels()
{
    return mVideoSource->SupportsMultipleInputChannels();
}

QString VideoWorkerThread::GetCurrentChannel()
{
    return QString(mVideoSource->CurrentInputChannel().c_str());
}

void VideoWorkerThread::SetChannel(int pIndex)
{
    mVideoSource->SelectInputChannel(pIndex);
    mFrameTimestamps.clear();
}

QStringList VideoWorkerThread::GetPossibleChannels()
{
    QStringList tResult;

    list<string> tList = mVideoSource->GetInputChannels();
    list<string>::iterator tIt;
    for (tIt = tList.begin(); tIt != tList.end(); tIt++)
        tResult.push_back(QString((*tIt).c_str()));

    return tResult;
}

void VideoWorkerThread::StartRecorder(std::string pSaveFileName, int pQuality)
{
    mSaveFileName = pSaveFileName;
    mSaveFileQuality = pQuality;
    mStartRecorderAsap = true;
}

void VideoWorkerThread::StopRecorder()
{
    mStopRecorderAsap = true;
}

void VideoWorkerThread::DoStartRecorder()
{
    mVideoSource->StartRecording(mSaveFileName, mSaveFileQuality);
    mStartRecorderAsap = false;
}

void VideoWorkerThread::DoStopRecorder()
{
    mVideoSource->StopRecording();
    mStopRecorderAsap = false;
}

void VideoWorkerThread::DoPlayNewFile()
{
    QStringList tList = GetPossibleDevices();
    int tPos = -1, i = 0;

    LOG(LOG_VERBOSE, "DoPlayNewFile now...");
    for (i = 0; i < tList.size(); i++)
    {
        if (tList[i].contains("FILE: " + mDesiredFile))
        {
            tPos = i;
            break;
        }
    }

    // found something?
    if (tPos == -1)
    {
        LOG(LOG_VERBOSE, "File is new, going to add..");
    	MediaSourceFile *tVSource = new MediaSourceFile(mDesiredFile.toStdString());
        if (tVSource != NULL)
        {
            VideoDevicesList tVList;
            tVSource->getVideoDevices(tVList);
            mVideoSource->RegisterMediaSource(tVSource);
            SetCurrentDevice(QString(tVList.front().Name.c_str()));
        }
    }else{
        LOG(LOG_VERBOSE, "File is already known, we select it as video media source");
        SetCurrentDevice(tList[tPos]);
    }

    mPlayNewFileAsap = false;
	mPaused = false;
	mFrameTimestamps.clear();
}

void VideoWorkerThread::DoSetGrabResolution()
{
    LOG(LOG_VERBOSE, "DoSetGrabResolution now...");

    // lock
    mDeliverMutex.lock();

    // delete old frame buffers
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        mVideoSource->FreeChunkBuffer(mFrame[i]);
    }

    // set new resolution for frame grabbing
    mVideoSource->SetVideoGrabResolution(mResX, mResY);

    // create new frame buffers
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        mFrame[i] = mVideoSource->AllocChunkBuffer(mFrameSize[i], MEDIA_VIDEO);
        InitFrameBuffer(i);
    }

    mVideoWidget->InformAboutNewSourceResolution();
    mSetGrabResolutionAsap = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void VideoWorkerThread::DoResetVideoSource()
{
    LOG(LOG_VERBOSE, "DoResetVideoSource now...");
    // lock
    mDeliverMutex.lock();

    // restart frame grabbing device
    mSourceAvailable = mVideoSource->Reset(MEDIA_VIDEO);

    mResetVideoSourceAsap = false;
    mPaused = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void VideoWorkerThread::DoSetInputStreamPreferences()
{
    LOG(LOG_VERBOSE, "DoSetInputStreamPreferences now...");
    // lock
    mDeliverMutex.lock();

    if (mVideoSource->SetInputStreamPreferences(mCodec.toStdString()))
    {
    	mSourceAvailable = mVideoSource->Reset(MEDIA_VIDEO);
        mResetVideoSourceAsap = false;
    }

    mSetInputStreamPreferencesAsap = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void VideoWorkerThread::DoSetCurrentDevice()
{
    LOG(LOG_VERBOSE, "DoSetCurrentDevice now...");
    // lock
    mDeliverMutex.lock();

    bool tNewSourceSelected = false;

    if ((mSourceAvailable = mVideoSource->SelectDevice(mDeviceName.toStdString(), MEDIA_VIDEO, tNewSourceSelected)))
    {
        bool tHadAlreadyInputData = false;
        for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
        {
            if (mFrameNumber[i] > 0)
            {
                tHadAlreadyInputData = true;
                break;
            }
        }
        if (!tHadAlreadyInputData)
        {
            LOG(LOG_VERBOSE, "Haven't found any input data, will force a reset of video source");
            mSourceAvailable = mVideoSource->Reset(MEDIA_VIDEO);
        }else
        {
            // seek to the beginning if we have reselected the source file
            if (!tNewSourceSelected)
            {
                LOG(LOG_VERBOSE, "Seeking to the beginning of the source file");
                mVideoSource->Seek(0);
            }

            if ((!tNewSourceSelected) && (mResetVideoSourceAsap))
            {
                LOG(LOG_VERBOSE, "Haven't selected new media source, reset of current media source forced");
                mSourceAvailable = mVideoSource->Reset(MEDIA_VIDEO);
            }
        }
        // we had an source reset in every case because "SelectDevice" does this if old source was already opened
        mResetVideoSourceAsap = false;
        mPaused = false;
        mVideoWidget->InformAboutNewSource();
    }else
        mVideoWidget->InformAboutOpenError(mDeviceName);

    mSetCurrentDeviceAsap = false;
    mCurrentFile = mDesiredFile;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

int VideoWorkerThread::GetCurrentFrame(void **pFrame, float *pFps)
{
    int tResult = -1;

    // lock
    if (!mDeliverMutex.tryLock(100))
        return -1;

    if ((mWorkerWithNewData) && (!mSetGrabResolutionAsap) && (!mResetVideoSourceAsap))
    {
        mFrameCurrentIndex = FRAME_BUFFER_SIZE - mFrameCurrentIndex - mFrameGrabIndex;
        mWorkerWithNewData = false;

        // calculate FPS
        if ((pFps != NULL) && (mFrameTimestamps.size() > 1))
        {
            int64_t tCurrentTime = Time::GetTimeStamp();
            int64_t tMeasurementStartTime = mFrameTimestamps.first();
            int tMeasuredValues = mFrameTimestamps.size() - 1;
            double tMeasuredTimeDifference = ((double)tCurrentTime - tMeasurementStartTime) / 1000000;

            // now finally calculate the FPS as follows: "count of measured values / measured time difference"
            *pFps = ((float)tMeasuredValues) / tMeasuredTimeDifference;
            //LOG(LOG_WARN, "FPS: %f, frames %d, interval %f, oldest %ld", *pFps, tMeasuredValues, tMeasuredSeconds, tMeasurementStartTimestamp);
        }

        *pFrame = mFrame[mFrameCurrentIndex];
        tResult = mFrameNumber[mFrameCurrentIndex];
        //printf("[%3lu %3lu %3lu] cur: %d grab: %d\n", mFrameNumber[0], mFrameNumber[1], mFrameNumber[2], mFrameCurrentIndex, mFrameGrabIndex);
    }

    // unlock
    mDeliverMutex.unlock();

    //printf("GUI-GetFrame -> %d\n", mFrameCurrentIndex);

    return tResult;
}

void VideoWorkerThread::run()
{
    int tFrameSize;
    const size_t tFpsMeasurementSteps = 10;
    bool  tGrabSuccess;
    int tFrameNumber = -1, tLastFrameNumber = -1;

    // if grabber was stopped before source has been opened this BOOL is reset
    mWorkerNeeded = true;

    // reset timestamp list
    mFrameTimestamps.clear();

    // assign default thread name
    SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber()");

    // start the video source
    mCodec = CONF.GetVideoCodec();
    mVideoSource->SetInputStreamPreferences(CONF.GetVideoCodec().toStdString());
    if (!(mSourceAvailable = mVideoSource->OpenVideoGrabDevice(mResX, mResY)))
    {
    	LOG(LOG_WARN, "Couldn't open video grabbing device \"%s\"", mVideoSource->GetCurrentDeviceName().c_str());
    	mVideoWidget->InformAboutOpenError(QString(mVideoSource->GetCurrentDeviceName().c_str()));
    }else
    {
        mVideoWidget->InformAboutNewSource();
    }

    while(mWorkerNeeded)
    {
    	// store last frame number
        tLastFrameNumber = tFrameNumber;

        // lock
        mGrabMutex.lock();

        // play new file from disc
        if (mPlayNewFileAsap)
        	DoPlayNewFile();

        // input device
        if (mSetCurrentDeviceAsap)
            DoSetCurrentDevice();

        // input stream preferences
        if (mSetInputStreamPreferencesAsap)
            DoSetInputStreamPreferences();

        // reset video source
        if (mResetVideoSourceAsap)
            DoResetVideoSource();

        // change the resolution
        if (mSetGrabResolutionAsap)
            DoSetGrabResolution();

        // start video recording
        if (mStartRecorderAsap)
            DoStartRecorder();

        // stop video recording
        if (mStopRecorderAsap)
            DoStopRecorder();

        if ((!mPaused) && (mSourceAvailable))
        {
			// set input frame size
			tFrameSize = mFrameSize[mFrameGrabIndex];

			// get new frame from video grabber
			tFrameNumber = mVideoSource->GrabChunk(mFrame[mFrameGrabIndex], tFrameSize, mDropFrames);
			mEofReached = (tFrameNumber == GRAB_RES_EOF);
			if (mEofReached)
			    mSourceAvailable = false;

			//LOG(LOG_ERROR, "DO THE BEST %d %d", tFrameNumber, tFrameSize);

			// unlock
			mGrabMutex.unlock();

			if ((tFrameNumber >= 0) && (tFrameSize > 0))
			{
				//LOG(LOG_ERROR, "A NEW ONE");
				// lock
				mDeliverMutex.lock();

				mFrameNumber[mFrameGrabIndex] = tFrameNumber;

				mWorkerWithNewData = true;

				mFrameGrabIndex = FRAME_BUFFER_SIZE - mFrameCurrentIndex - mFrameGrabIndex;

                // store timestamp starting from frame number 3 to avoid peaks
				if(tFrameNumber > 3)
				{
                    //HINT: locking is done via mDeliverMutex!
                    mFrameTimestamps.push_back(Time::GetTimeStamp());
                    //LOG(LOG_WARN, "Time %ld", Time::GetTimeStamp());
                    while (mFrameTimestamps.size() > FPS_MEASUREMENT_STEPS)
                        mFrameTimestamps.removeFirst();
				}

                // unlock
				mDeliverMutex.unlock();

				mVideoWidget->InformAboutNewFrame();
				//printf("VideoWorker--> %d\n", mFrameGrabIndex);
				//printf("VideoWorker-grabbing FPS: %2d grabbed frame number: %d\n", mResultingFps, tFrameNumber);

				if ((tLastFrameNumber > tFrameNumber) && (tFrameNumber > 9 /* -1 means error, 1 is received after every reset, use "9" because of possible latencies */))
					LOG(LOG_ERROR, "Frame ordering problem detected (%d -> %d)", tLastFrameNumber, tFrameNumber);
			}else
			{
				LOG(LOG_VERBOSE, "Invalid grabbing result");
				usleep(500 * 1000); // check for new frames every 1/10 seconds
			}
        }else
        {
			// unlock
			mGrabMutex.unlock();

        	if (mSourceAvailable)
        		LOG(LOG_VERBOSE, "VideoWorkerThread is in pause state");
        	else
        		LOG(LOG_VERBOSE, "VideoWorkerThread waits for available grabbing device");
        	usleep(500 * 1000); // check for new pause state every 3 seconds
        }
    }
    mVideoSource->CloseGrabDevice();
    mVideoSource->DeleteAllRegisteredMediaSinks();
}

void VideoWorkerThread::StopGrabber()
{
    LOG(LOG_VERBOSE, "StobGrabber now...");
    mWorkerNeeded = false;
    mVideoSource->StopGrabbing();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
