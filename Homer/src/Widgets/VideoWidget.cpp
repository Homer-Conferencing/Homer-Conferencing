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

#include <Widgets/OverviewPlaylistWidget.h>
#include <Widgets/ParticipantWidget.h>
#include <Widgets/VideoWidget.h>
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
#include <vector>

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
    mVideoScaleFactor = 1.0;
    mCurrentFrameNumber = 0;
    mLastFrameNumber = 0;
    mHourGlassAngle = 0;
    mHourGlassOffset = 0;
    mCustomEventReason = 0;
    mPendingNewFrameSignals = 0;
    mShowLiveStats = false;
    mRecorderStarted = false;
    mVideoPaused = false;
    mAspectRatio = ASPECT_RATIO_ORIGINAL;
    mVideoMirroredHorizontal = false;
    mVideoMirroredVertical = false;
    mCurrentApplicationFocusedWidget = NULL;
    mVideoSource = NULL;
    mVideoWorker = NULL;
    mMainWindow = NULL;
    mAssignedAction = NULL;
    mSmoothPresentation = CONF.GetSmoothVideoPresentation();
    parentWidget()->hide();
    hide();
}

void VideoWidget::Init(QMainWindow* pMainWindow, ParticipantWidget *pParticipantWidget, MediaSource *pVideoSource, QMenu *pMenu, QString pActionTitle, QString pWidgetTitle, bool pVisible)
{
    mVideoSource = pVideoSource;
    mVideoTitle = pActionTitle;
    mMainWindow = pMainWindow;
    mParticipantWidget = pParticipantWidget;

    //####################################################################
    //### create the remaining necessary menu item
    //####################################################################

    if (pMenu != NULL)
    {
        mAssignedAction = pMenu->addAction(pActionTitle);
        mAssignedAction->setCheckable(true);
        mAssignedAction->setChecked(pVisible);
        QIcon tIcon;
        tIcon.addPixmap(QPixmap(":/images/22_22/Checked.png"), QIcon::Normal, QIcon::On);
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
    LOG(LOG_VERBOSE, "Created hour glass timer with ID 0x%X", mHourGlassTimer->timerId());

    //####################################################################
    //### speedup video presentation by setting the following
    //####################################################################
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

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
        pEvent->ignore();
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
    vector<string> tRegisteredVideoSinks = mVideoSource->ListRegisteredMediaSinks();
    vector<string>::iterator tRegisteredVideoSinksIt;

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
    if (mVideoSource->SupportsRecording())
    {
		QIcon tIcon5;
		if (mVideoSource->IsRecording())
		{
			tAction = tMenu.addAction("Stop recording");
			tIcon5.addPixmap(QPixmap(":/images/22_22/Audio_Stop.png"), QIcon::Normal, QIcon::Off);
		}else
		{
			tAction = tMenu.addAction("Record video");
			tIcon5.addPixmap(QPixmap(":/images/22_22/Audio_Record.png"), QIcon::Normal, QIcon::Off);
		}
		tAction->setIcon(tIcon5);
    }

    tMenu.addSeparator();

    //###############################################################################
    //### STREAM INFO
    //###############################################################################
    if (mShowLiveStats)
        tAction = tMenu.addAction("Hide stream info");
    else
        tAction = tMenu.addAction("Show stream info");
    QIcon tIcon4;
    tIcon4.addPixmap(QPixmap(":/images/22_22/Info.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon4);
    tAction->setCheckable(true);
    tAction->setChecked(mShowLiveStats);

    QList<QKeySequence> tIKeys;
    tIKeys.push_back(Qt::Key_I);
    tAction->setShortcuts(tIKeys);

    //###############################################################################
    //### Video settings
    //###############################################################################
    QIcon tIcon3;
    tIcon3.addPixmap(QPixmap(":/images/Computer.png"), QIcon::Normal, QIcon::Off);
    QMenu *tVideoMenu = tMenu.addMenu("Video settings");

            //###############################################################################
            //### "Full screen"
            //###############################################################################
            if (windowState() & Qt::WindowFullScreen)
            {
                tAction = tVideoMenu->addAction("Window mode");
            }else
            {
                tAction = tVideoMenu->addAction("Full screen");
            }
            QList<QKeySequence> tFSKeys;
            tFSKeys.push_back(Qt::Key_F);
            tFSKeys.push_back(Qt::Key_Escape);
            tAction->setShortcuts(tFSKeys);

            //###############################################################################
            //### "Smooth presentation"
            //###############################################################################
            if (mSmoothPresentation)
            {
                tAction = tVideoMenu->addAction("Show smooth video");
            }else
            {
                tAction = tVideoMenu->addAction("Show fast video");
            }
            QList<QKeySequence> tSPKeys;
            tSPKeys.push_back(Qt::Key_S);
            tAction->setShortcuts(tSPKeys);

            //###############################################################################
            //### ASPECT RATION
            //###############################################################################
            QMenu *tAspectRatioMenu = tVideoMenu->addMenu("Aspect ratio");
            //###############################################################################
            //### "Keep aspect ratio"
            //###############################################################################

            tAction = tAspectRatioMenu->addAction("Original");
            tAction->setCheckable(true);
            if (mAspectRatio == ASPECT_RATIO_ORIGINAL)
                tAction->setChecked(true);
            else
                tAction->setChecked(false);

            tAction = tAspectRatioMenu->addAction(" 1 : 1 ");
            tAction->setCheckable(true);
            if (mAspectRatio == ASPECT_RATIO_1x1)
                tAction->setChecked(true);
            else
                tAction->setChecked(false);

            tAction = tAspectRatioMenu->addAction(" 4 : 3 ");
            tAction->setCheckable(true);
            if (mAspectRatio == ASPECT_RATIO_4x3)
                tAction->setChecked(true);
            else
                tAction->setChecked(false);

            tAction = tAspectRatioMenu->addAction(" 5 : 4 ");
            tAction->setCheckable(true);
            if (mAspectRatio == ASPECT_RATIO_5x4)
                tAction->setChecked(true);
            else
                tAction->setChecked(false);

            tAction = tAspectRatioMenu->addAction("16 : 9 ");
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


            //###############################################################################
            //### RESOLUTIONS
            //###############################################################################
            QMenu *tResMenu = tVideoMenu->addMenu("Resolution");
            //###############################################################################
            //### add all possible resolutions which are reported by the media source
            //###############################################################################
            if (tGrabResolutions.size())
            {
                for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
                {
                    QAction *tResAction = tResMenu->addAction(QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX, 4, 10, (const QChar)' ').arg(tIt->ResY, 4, 10, (const QChar)' '));
                    tResAction->setCheckable(true);
                    if ((tIt->ResX == tCurResX) && (tIt->ResY == tCurResY))
                        tResAction->setChecked(true);
                    else
                        tResAction->setChecked(false);
                }
            }

            //###############################################################################
            //### SCALING
            //###############################################################################
            QMenu *tScaleMenu = tVideoMenu->addMenu("Scale video");
            for (int i = 1; i < 5; i++)
            {
                QAction *tScaleAction = tScaleMenu->addAction(QString(" %1%").arg((int)(i * 50), 3, 10, (const QChar)' '));
                tScaleAction->setCheckable(true);
                if (IsCurrentScaleFactor((float)i / 2))
                {
                    //LOG(LOG_INFO, "Scaling factor %f matches", (float)i / 2));
                    tScaleAction->setChecked(true);
                }else
                    tScaleAction->setChecked(false);
            }

            //###############################################################################
            //### MIRRORING
            //###############################################################################
            if (mVideoMirroredHorizontal)
            {
                tAction = tVideoMenu->addAction("Unmirror horizontally");
                tAction->setCheckable(true);
                tAction->setChecked(true);
            }else
            {
                tAction = tVideoMenu->addAction("Mirror horizontally");
                tAction->setCheckable(true);
                tAction->setChecked(false);
            }

            if (mVideoMirroredVertical)
            {
                tAction = tVideoMenu->addAction("Unmirror vertically");
                tAction->setCheckable(true);
                tAction->setChecked(true);
            }else
            {
                tAction = tVideoMenu->addAction("Mirror vertically");
                tAction->setCheckable(true);
                tAction->setChecked(false);
            }
    tVideoMenu->setIcon(tIcon3);

    //###############################################################################
    //### STREAM RELAY
    //###############################################################################
    if(mVideoSource->SupportsRelaying())
    {
        QMenu *tVideoSinksMenu = tMenu.addMenu("Relay stream");
        QIcon tIcon7;
        tIcon7.addPixmap(QPixmap(":/images/22_22/ArrowRight.png"), QIcon::Normal, QIcon::Off);
        tVideoSinksMenu->setIcon(tIcon7);

        tAction =  tVideoSinksMenu->addAction("Add network sink");
        QIcon tIcon8;
        tIcon8.addPixmap(QPixmap(":/images/22_22/Plus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon8);

        QMenu *tRegisteredVideoSinksMenu = tVideoSinksMenu->addMenu("Registered sinks");
        QIcon tIcon9;
        tIcon9.addPixmap(QPixmap(":/images/22_22/ArrowRight.png"), QIcon::Normal, QIcon::Off);
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
            tIcon10.addPixmap(QPixmap(":/images/22_22/Audio_Play.png"), QIcon::Normal, QIcon::Off);
        }else
        {
            tAction = tMenu.addAction("Drop stream");
            tIcon10.addPixmap(QPixmap(":/images/22_22/Exit.png"), QIcon::Normal, QIcon::Off);
        }
        tAction->setIcon(tIcon10);
    }

    tMenu.addSeparator();

    //###############################################################################
    //### RESET SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Reset source");
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/22_22/Reload.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);

    //###############################################################################
    //### CLOSE SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Close video");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Close.png"), QIcon::Normal, QIcon::Off);
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
        if (tPopupRes->text().compare("Drop stream") == 0)
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
        if ((tPopupRes->text().compare("Show smooth video") == 0) || (tPopupRes->text().compare("Show fast video") == 0))
        {
            ToggleSmoothPresentationMode();
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
        if (tPopupRes->text().compare(" 1 : 1 ") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_1x1;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare(" 4 : 3 ") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_4x3;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare(" 5 : 4 ") == 0)
        {
        	mAspectRatio = ASPECT_RATIO_5x4;
        	mNeedBackgroundUpdatesUntillNextFrame = true;
            return;
        }
        if (tPopupRes->text().compare("16 : 9 ") == 0)
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

        //### RESOLUTION
        if (tGrabResolutions.size())
        {
            //printf("%s\n", tPopupRes->text().toStdString().c_str());
            for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
            {
                //printf("to compare: |%s| |%s|\n", (QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX).arg(tIt->ResY)).toStdString().c_str(), tPopupRes->text().toStdString().c_str());
                if (tPopupRes->text().compare(QString(tIt->Name.c_str()) + QString("  (%1 x %2)").arg(tIt->ResX, 4, 10, (const QChar)' ').arg(tIt->ResY, 4, 10, (const QChar)' ')) == 0)
                {
                    SetResolution(tIt->ResX, tIt->ResY);
                    return;
                }
            }
        }
        //### SCALING
        for (int i = 1; i < 5; i++)
        {
        	if (tPopupRes->text() == QString(" %1%").arg((int)(i * 50), 3, 10, (const QChar)' '))
        	{
        		SetScaling((float)i/2);
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
    int tMSecs = QTime::currentTime().msec();
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

	mCurrentFrame = mCurrentFrame.scaled(tFrameOutputWidth, tFrameOutputHeight, tAspectMode, mSmoothPresentation ? Qt::SmoothTransformation : Qt::FastTransformation);
	tFrameOutputWidth = mCurrentFrame.width();
	tFrameOutputHeight = mCurrentFrame.height();

    int tTimeDiff = QTime::currentTime().msecsTo(tTime);
    // did we spend too much time with transforming the image?
    if ((mSmoothPresentation) && (tTimeDiff > 1000 / 3)) // at least we assume 3 FPS!
    {
        mSmoothPresentation = false;
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

        int tMuxResX = 0, tMuxResY = 0;
        mVideoSource->GetMuxingResolution(tMuxResX, tMuxResY);

        QString tCodecName = QString(mVideoSource->GetCodecName().c_str());
        QString tMuxCodecName = QString(mVideoSource->GetMuxingCodec().c_str());
        QString tPeerName = QString(mVideoSource->GetCurrentDevicePeerName().c_str());

        tPainter->setPen(QColor(Qt::darkRed));
        tPainter->drawText(5, 41, " Source: " + mVideoWorker->GetCurrentDevice());
        tPainter->drawText(5, 61, " Frame: " + QString("%1").arg(pFrameNumber) + (mVideoSource->GetChunkDropCounter() ? (" (" + QString("%1").arg(mVideoSource->GetChunkDropCounter()) + " lost packets)") : "") + (mVideoSource->GetFragmentBufferCounter() ? (" (" + QString("%1").arg(mVideoSource->GetFragmentBufferCounter()) + "/" + QString("%1").arg(mVideoSource->GetFragmentBufferSize()) + " buffered packets)") : ""));
        tPainter->drawText(5, 81, " Fps: " + QString("%1").arg(pFps, 4, 'f', 2, ' '));
        tPainter->drawText(5, 101, " Codec: " + ((tCodecName != "") ? tCodecName : "unknown") + " (" + QString("%1").arg(tSourceResX) + "*" + QString("%1").arg(tSourceResY) + ")");
        tPainter->drawText(5, 121, " Output: " + QString("%1").arg(tFrameOutputWidth) + "*" + QString("%1").arg(tFrameOutputHeight) + " (" + tAspectRatio + ")" + (mSmoothPresentation ? "[smoothed]" : ""));
        int tMuxOutputOffs = 0;
        int tPeerOutputOffs = 0;
        if (mVideoSource->SupportsSeeking())
        {
            tMuxOutputOffs = 20;
            tPainter->drawText(5, 141, " Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0'));
        }
        if (mVideoSource->SupportsMuxing())
        {
        	tPeerOutputOffs = 20;
            tPainter->drawText(5, 141 + tMuxOutputOffs, " Mux codec: " + ((tMuxCodecName != "") ? tMuxCodecName : "unknown") + " (" + QString("%1").arg(tMuxResX) + "*" + QString("%1").arg(tMuxResY) + ")" + (mVideoSource->GetMuxingBufferCounter() ? (" (" + QString("%1").arg(mVideoSource->GetMuxingBufferCounter()) + "/" + QString("%1").arg(mVideoSource->GetMuxingBufferSize()) + " buffered frames)") : ""));
        }
        if (tPeerName != "")
        	tPainter->drawText(5, 141 + tMuxOutputOffs + tPeerOutputOffs, " Peer: " + tPeerName);

        tPainter->setPen(QColor(Qt::red));
        tPainter->drawText(4, 40, " Source: " + mVideoWorker->GetCurrentDevice());
        tPainter->drawText(4, 60, " Frame: " + QString("%1").arg(pFrameNumber) + (mVideoSource->GetChunkDropCounter() ? (" (" + QString("%1").arg(mVideoSource->GetChunkDropCounter()) + " lost packets)") : "") + (mVideoSource->GetFragmentBufferCounter() ? (" (" + QString("%1").arg(mVideoSource->GetFragmentBufferCounter()) + "/" + QString("%1").arg(mVideoSource->GetFragmentBufferSize()) + " buffered packets)") : ""));
        tPainter->drawText(4, 80, " Fps: " + QString("%1").arg(pFps, 4, 'f', 2, ' '));
        tPainter->drawText(4, 100, " Codec: " + ((tCodecName != "") ? tCodecName : "unknown") + " (" + QString("%1").arg(tSourceResX) + "*" + QString("%1").arg(tSourceResY) + ")");
        tPainter->drawText(4, 120, " Output: "  + QString("%1").arg(tFrameOutputWidth) + "*" + QString("%1").arg(tFrameOutputHeight) + " (" + tAspectRatio + ")" + (mSmoothPresentation ? "[smoothed]" : ""));
        if (mVideoSource->SupportsSeeking())
			tPainter->drawText(4, 140, " Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0'));
        if (mVideoSource->SupportsMuxing())
            tPainter->drawText(4, 140 + tMuxOutputOffs, " Mux codec: " + ((tMuxCodecName != "") ? tMuxCodecName : "unknown") + " (" + QString("%1").arg(tMuxResX) + "*" + QString("%1").arg(tMuxResY) + ")" + (mVideoSource->GetMuxingBufferCounter() ? (" (" + QString("%1").arg(mVideoSource->GetMuxingBufferCounter()) + "/" + QString("%1").arg(mVideoSource->GetMuxingBufferSize()) + " buffered frames)") : ""));
        if (tPeerName != "")
        	tPainter->drawText(5, 140 + tMuxOutputOffs + tPeerOutputOffs, " Peer: " + tPeerName);
    }

    //#############################################################
    //### draw record icon
    //#############################################################
    if (mVideoSource->IsRecording())
    {
        if (tMSecs % 500 < 250)
        {
            QPixmap tPixmap = QPixmap(":/images/22_22/Audio_Record_active.png");
            tPainter->drawPixmap(10, 10, tPixmap);
        }else
        {
            QPixmap tPixmap = QPixmap(":/images/22_22/Audio_Record.png");
            tPainter->drawPixmap(10, 10, tPixmap);
        }
    }

    //#############################################################
    //### draw pause icon
    //#############################################################
    if ((mVideoPaused) and (tMSecs % 500 < 250))
    {
        QPixmap tPixmap = QPixmap(":/images/22_22/Audio_Paused.png");
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
    mPendingNewFrameSignals++;
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

bool VideoWidget::SetOriginalResolution()
{
	bool tResult = false;

    GrabResolutions tGrabResolutions = mVideoSource->GetSupportedVideoGrabResolutions();
    GrabResolutions::iterator tIt;
    if (tGrabResolutions.size())
    {
        for (tIt = tGrabResolutions.begin(); tIt != tGrabResolutions.end(); tIt++)
        {
            //LOG(LOG_ERROR, "Res: %s", tIt->Name.c_str());
            if (tIt->Name == "Original")
            {
            	tResult = true;
            	LOG(LOG_VERBOSE, "Setting original resolution %d*%d", tIt->ResX, tIt->ResY);
                SetResolution(tIt->ResX, tIt->ResY);
            }
        }
    }

    return tResult;
}

VideoWorkerThread* VideoWidget::GetWorker()
{
    return mVideoWorker;
}

void VideoWidget::SetResolution(int mX, int mY)
{
    if (!mRecorderStarted)
    {
    	LOG(LOG_VERBOSE, "Setting video resolution to %d * %d", mX, mY);

		setUpdatesEnabled(false);
		if ((mResX != mX) || (mResY != mY))
		{
			mResX = mX;
			mResY = mY;

			if(mVideoWorker != NULL)
				mVideoWorker->SetGrabResolution(mResX, mResY);

			if ((windowState() & Qt::WindowFullScreen) == 0)
			{
				setMinimumSize(0, 0);//mResX, mResY);
				//resize(0, 0);
			}
		}
		setUpdatesEnabled(true);
		mNeedBackgroundUpdatesUntillNextFrame = true;
    }else
    {
		ShowInfo("Recording active", "Video playback settings cannot be changed if recording is active");
    }
}

void VideoWidget::SetScaling(float pVideoScaleFactor)
{
	LOG(LOG_VERBOSE, "Setting video scaling to %f", pVideoScaleFactor);

	if ((windowState() & Qt::WindowFullScreen) == 0)
	{
		int tX = mResX * pVideoScaleFactor;
		int tY = mResY * pVideoScaleFactor;
		LOG(LOG_VERBOSE, "Setting video output resolution to %d * %d", tX, tY);

		setMinimumSize(tX, tY);
		resize(0, 0);
		
		mNeedBackgroundUpdatesUntillNextFrame = true;
	}else
		LOG(LOG_VERBOSE, "SetScaling skipped because fullscreen mode detected");
}

bool VideoWidget::IsCurrentScaleFactor(float pScaleFactor)
{
	//LOG(LOG_VERBOSE, "Checking scale factor %f:  %d <=> %d, %d <=> %d", pScaleFactor, width(), (int)(mResX * pScaleFactor), height(), (int)(mResY * pScaleFactor));
	return ((width() == mResX * pScaleFactor) || (height() == mResY * pScaleFactor));
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

void VideoWidget::ToggleSmoothPresentationMode()
{
    mSmoothPresentation = !mSmoothPresentation;
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
        #ifdef VIDEO_WIDGET_DROP_WHEN_INVISIBLE
            mVideoWorker->SetFrameDropping(false);
        #endif
        move(mWinPos);
        parentWidget()->show();
        show();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(true);

    }else
    {
        #ifdef VIDEO_WIDGET_DROP_WHEN_INVISIBLE
            mVideoWorker->SetFrameDropping(true);
        #endif
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
    QString tQualityStr = QInputDialog::getItem(this, "Select recording quality", "Record with quality:                                      ", tPosQuals, 0, false, &tAck);

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
    QWidget::paintEvent(pEvent);

    QPainter tPainter(this);
    QColor tBackgroundColor;

    // selected background color depends on the window state
    if (windowState() & Qt::WindowFullScreen)
        tBackgroundColor = QColor(Qt::black);
    else
        tBackgroundColor = QApplication::palette().brush(backgroundRole()).color();
    #ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
        tBackgroundColor = QColor((int)((long)256 * qrand() / RAND_MAX), (int)((long)256 * qrand() / RAND_MAX), (int)((long)256 * qrand() / RAND_MAX));
    #endif
    // wait until valid new frame is available to be drawn
    if (mCurrentFrame.isNull())
    {
        tPainter.fillRect(0, 0, width(), height(), tBackgroundColor);
        return;
    }

    // force background update as long as we don't have the focus -> we are maybe covered by other applications GUI
    // force background update if focus has changed
    QWidget *tWidget = QApplication::focusWidget();
    if (((tWidget == NULL) || (tWidget != mCurrentApplicationFocusedWidget)) && ((windowState() & Qt::WindowFullScreen) == 0))
    {
        #ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
            LOG(LOG_VERBOSE, "Focused widget has changed, background-update enforced, focused widget: %p", tWidget);
        #endif
        mNeedBackgroundUpdate = true;
        mCurrentApplicationFocusedWidget = tWidget;
    }

    #ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
        if ((mCurrentFrame.width() > width()) || (mCurrentFrame.height() > height()))
            LOG(LOG_WARN, "Current frame is too big: %dx%d with available widget area: %dx%d", mCurrentFrame.width(), mCurrentFrame.height(), width(), height());
    #endif

//TODO: fix the repaint bugs and deactivate continuous background-painting again
//    if ((mNeedBackgroundUpdate) || (mNeedBackgroundUpdatesUntillNextFrame))
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
        #ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
            LOG(LOG_VERBOSE, "Background-update %d, %d", mNeedBackgroundUpdate, mNeedBackgroundUpdatesUntillNextFrame);
            LOG(LOG_VERBOSE, "Current frame size: %dx%d, widget size: %dx%d, background size: %dx%d", mCurrentFrame.width(), mCurrentFrame.height(), width(), height(), tFrameWidth, tFrameHeight);
        #endif
        mNeedBackgroundUpdate = false;
    }

    // draw only fitting new frames (otherwise we could have a race condition and a too big frame which might be drawn)
    if ((mCurrentFrame.width() <= width()) && (mCurrentFrame.height() <= height()))
        tPainter.drawImage((width() - mCurrentFrame.width()) / 2, (height() - mCurrentFrame.height()) / 2, mCurrentFrame);
    pEvent->accept();
}

void VideoWidget::resizeEvent(QResizeEvent *pEvent)
{
	setUpdatesEnabled(false);
    QWidget::resizeEvent(pEvent);
    mNeedBackgroundUpdatesUntillNextFrame = true;
    pEvent->accept();
    setUpdatesEnabled(true);
}

void VideoWidget::keyPressEvent(QKeyEvent *pEvent)
{
	if ((pEvent->key() == Qt::Key_Escape) && (windowState() & Qt::WindowFullScreen))
	{
        setWindowFlags(windowFlags() ^ Qt::Window);
        showNormal();
        return;
	}
    if (pEvent->key() == Qt::Key_F)
    {
        ToggleFullScreenMode();
        return;
    }
    if (pEvent->key() == Qt::Key_S)
    {
        ToggleSmoothPresentationMode();
        return;
    }
    if (pEvent->key() == Qt::Key_I)
    {
        if (mShowLiveStats)
        	mShowLiveStats = false;
        else
        	mShowLiveStats = true;
    }
    if (pEvent->key() == Qt::Key_Space)
    {
        if ((mVideoWorker->IsPaused()) || ((mParticipantWidget->GetAudioWorker() != NULL) && (mParticipantWidget->GetAudioWorker()->IsPaused())))
        {
            mVideoWorker->PlayFile();
            if (mParticipantWidget->GetAudioWorker() != NULL)
                mParticipantWidget->GetAudioWorker()->PlayFile();
            return;
        }else
        {
            mVideoWorker->PauseFile();
            if (mParticipantWidget->GetAudioWorker() != NULL)
                mParticipantWidget->GetAudioWorker()->PauseFile();
            return;
        }
    }
    if (pEvent->key() == Qt::Key_A)
    {
    	mAspectRatio++;
    	if(mAspectRatio > ASPECT_RATIO_16x10)
    		mAspectRatio = ASPECT_RATIO_ORIGINAL;
    	mNeedBackgroundUpdatesUntillNextFrame = true;
    	return;
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
            mPendingNewFrameSignals--;
			#ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
				if (mPendingNewFrameSignals > 2)
					LOG(LOG_VERBOSE, "System too slow?, %d pending signals about new frames", mPendingNewFrameSignals);
			#endif

            tVideoEvent->accept();
            if (isVisible())
            {
                mLastFrameNumber = mCurrentFrameNumber;
                // hint: we don't have to synchronize with resolution changes because Qt has only one synchronous working event loop!
                mCurrentFrameNumber = mVideoWorker->GetCurrentFrame(&tFrame, &tFps);
                if (mCurrentFrameNumber > -1)
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
                    if ((mLastFrameNumber > mCurrentFrameNumber) && (mCurrentFrameNumber  > 32 /* -1 means error, 1 is received after every reset, use "32" because of possible latencies */))
                    {
                        if (mLastFrameNumber - mCurrentFrameNumber == FRAME_BUFFER_SIZE -1)
                            LOG(LOG_WARN, "Buffer underrun occurred, received frames in wrong order, [%d->%d]", mLastFrameNumber, mCurrentFrameNumber);
                        else
                            LOG(LOG_WARN, "Frames received in wrong order, [%d->%d]", mLastFrameNumber, mCurrentFrameNumber);
                    }
                    //if (tlFrameNumber == tFrameNumber)
                        //printf("VideoWidget-unnecessary frame grabbing detected!\n");
                }else
                    LOG(LOG_WARN, "Current frame number is invalid (%d)", mCurrentFrameNumber);
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
            if (SetOriginalResolution())
            {
				if(!mHourGlassTimer->isActive())
				{
					LOG(LOG_VERBOSE, "Reactivating hour glass timer");
					mHourGlassTimer->start(250);
				}
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
    mDesiredInputChannel = 0;
    mPlayNewFileAsap = false;
    mSeekAsap = false;
    mSeekPos = 0;
    mSelectInputChannelAsap = false;
    mSourceAvailable = false;
    mEofReached = false;
    mPaused = false;
    mPausedPos = 0;
    mMissingFrames = 0;
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
    mDropFrames = false;
    InitFrameBuffers();
}

VideoWorkerThread::~VideoWorkerThread()
{
    DeinitFrameBuffers();
}

void VideoWorkerThread::InitFrameBuffers()
{
    mPendingNewFrames = 0;
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        mFrame[i] = mVideoSource->AllocChunkBuffer(mFrameSize[i], MEDIA_VIDEO);

        mFrameNumber[i] = 0;

        QImage tFrameImage = QImage((unsigned char*)mFrame[i], mResX, mResY, QImage::Format_RGB32);
        QPainter *tPainter = new QPainter(&tFrameImage);
        tPainter->setRenderHint(QPainter::TextAntialiasing, true);
        tPainter->fillRect(0, 0, mResX, mResY, QColor(Qt::darkGray));
        tPainter->setFont(QFont("Tahoma", 16));
        tPainter->setPen(QColor(Qt::black));
        tPainter->drawText(5, 70, "..no data");
        delete tPainter;
    }
}

void VideoWorkerThread::DeinitFrameBuffers()
{
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        mVideoSource->FreeChunkBuffer(mFrame[i]);
    }
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
        mGrabbingCondition.wakeAll();

    }
}

void VideoWorkerThread::ResetSource()
{
    mResetVideoSourceAsap = true;
    mGrabbingCondition.wakeAll();
}

void VideoWorkerThread::SetInputStreamPreferences(QString pCodec)
{
    mCodec = pCodec;
    mSetInputStreamPreferencesAsap = true;
    mGrabbingCondition.wakeAll();
}

void VideoWorkerThread::SetStreamName(QString pName)
{
    mVideoSource->AssignStreamName(pName.toStdString());
}

QString VideoWorkerThread::GetStreamName()
{
    return QString(mVideoSource->GetMediaSource()->GetStreamName().c_str());
}

QString VideoWorkerThread::GetCurrentDevicePeer()
{
    return QString(mVideoSource->GetCurrentDevicePeerName().c_str());
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
        mGrabbingCondition.wakeAll();
    }
}

VideoDevices VideoWorkerThread::GetPossibleDevices()
{
    VideoDevices tResult;

    LOG(LOG_VERBOSE, "Enumerate all video devices..");
    mVideoSource->getVideoDevices(tResult);

    return tResult;
}

QString VideoWorkerThread::GetDeviceDescription(QString pName)
{
    VideoDevices::iterator tIt;
    VideoDevices tVList;

    mVideoSource->getVideoDevices(tVList);
    for (tIt = tVList.begin(); tIt != tVList.end(); tIt++)
        if (pName.toStdString() == tIt->Name)
            return QString(tIt->Desc.c_str());

    return "";
}

void VideoWorkerThread::PlayFile(QString pName)
{
    if (pName == "")
        pName = mCurrentFile;

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
		LOG(LOG_VERBOSE, "Continue playback of file: %s at pos.: %ld", pName.toStdString().c_str(), mPausedPos);
		Seek(mPausedPos);
		mGrabbingStateMutex.lock();
		mPaused = false;
        mGrabbingStateMutex.unlock();
		mGrabbingCondition.wakeAll();
        mFrameTimestamps.clear();
	}else
	{
		LOG(LOG_VERBOSE, "Trigger playback of file: %s", pName.toStdString().c_str());
		mDesiredFile = pName;
		mPlayNewFileAsap = true;
        mGrabbingCondition.wakeAll();
	}
}

void VideoWorkerThread::PauseFile()
{
    if (mVideoSource->SupportsSeeking())
    {
        mPausedPos = mVideoSource->GetSeekPos();
        mGrabbingStateMutex.lock();
        mPaused = true;
        mGrabbingStateMutex.unlock();
        LOG(LOG_VERBOSE, "Triggered pause state at position: %ld", mPausedPos);
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, PauseFile() aborted");
}

bool VideoWorkerThread::IsPaused()
{
    if ((mVideoSource != NULL) && (mVideoSource->SupportsSeeking()))
        return mPaused;
    else
        return false;
}

void VideoWorkerThread::StopFile()
{
    if (mVideoSource->SupportsSeeking())
    {
        LOG(LOG_VERBOSE, "Trigger stop state");
        mPausedPos = 0;
        mGrabbingStateMutex.lock();
        mPaused = true;
        mGrabbingStateMutex.unlock();
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, StopFile() aborted");
}

bool VideoWorkerThread::EofReached()
{
	return (((mEofReached) && (!mResetVideoSourceAsap) && (!mPlayNewFileAsap) && (!mSeekAsap)) || (mPlayNewFileAsap) || (mSetCurrentDeviceAsap));
}

QString VideoWorkerThread::CurrentFile()
{
    if (mVideoSource->SupportsSeeking())
    	return mCurrentFile;
    else
        return "";
}

bool VideoWorkerThread::SupportsSeeking()
{
    if(mVideoSource != NULL)
        return mVideoSource->SupportsSeeking();
    else
        return false;
}

void VideoWorkerThread::Seek(int64_t pPos)
{
    mSeekPos = pPos;
    mSeekAsap = true;
    mGrabbingCondition.wakeAll();
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
    if (mVideoSource != NULL)
        return mVideoSource->SupportsMultipleInputChannels();
    else
        return false;
}

QString VideoWorkerThread::GetCurrentChannel()
{
    return QString(mVideoSource->CurrentInputChannel().c_str());
}

void VideoWorkerThread::SelectInputChannel(int pIndex)
{
    if (pIndex != -1)
    {
        LOG(LOG_VERBOSE, "Will select new input channel %d after some short time", pIndex);
        mDesiredInputChannel = pIndex;
        mSelectInputChannelAsap = true;
        mGrabbingCondition.wakeAll();
    }else
    {
        LOG(LOG_WARN, "Will not select new input channel -1, ignoring this request");
    }
}

QStringList VideoWorkerThread::GetPossibleChannels()
{
    QStringList tResult;

    vector<string> tList = mVideoSource->GetInputChannels();
    vector<string>::iterator tIt;
    for (tIt = tList.begin(); tIt != tList.end(); tIt++)
        tResult.push_back(QString((*tIt).c_str()));

    return tResult;
}

void VideoWorkerThread::StartRecorder(std::string pSaveFileName, int pQuality)
{
    mSaveFileName = pSaveFileName;
    mSaveFileQuality = pQuality;
    mStartRecorderAsap = true;
    mGrabbingCondition.wakeAll();
}

void VideoWorkerThread::StopRecorder()
{
    mStopRecorderAsap = true;
    mGrabbingCondition.wakeAll();
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
    LOG(LOG_VERBOSE, "DoPlayNewFile now...");

    VideoDevices tList = GetPossibleDevices();
    VideoDevices::iterator tIt;
    bool tFound = false;

    for (tIt = tList.begin(); tIt != tList.end(); tIt++)
    {
        if (QString(tIt->Name.c_str()).contains(mDesiredFile))
        {
            tFound = true;
            break;
        }
    }

    // found something?
    if (!tFound)
    {
        LOG(LOG_VERBOSE, "File is new, going to add..");
    	MediaSourceFile *tVSource = new MediaSourceFile(mDesiredFile.toStdString());
        if (tVSource != NULL)
        {
            VideoDevices tVList;
            tVSource->getVideoDevices(tVList);
            mVideoSource->RegisterMediaSource(tVSource);
            SetCurrentDevice(mDesiredFile);
        }
    }else{
        LOG(LOG_VERBOSE, "File is already known, we select it as video media source");
        SetCurrentDevice(mDesiredFile);
    }

    mEofReached = false;
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
    DeinitFrameBuffers();

    // set new resolution for frame grabbing
    mVideoSource->SetVideoGrabResolution(mResX, mResY);

    // create new frame buffers
    InitFrameBuffers();

    mVideoWidget->InformAboutNewSourceResolution();
    mSetGrabResolutionAsap = false;
    mFrameTimestamps.clear();

    // unlock
    mDeliverMutex.unlock();
}

void VideoWorkerThread::DoSeek()
{
    LOG(LOG_VERBOSE, "DoSeek now...");

    // lock
    mDeliverMutex.lock();

    LOG(LOG_VERBOSE, "Seeking now to position %d", mSeekPos);
    mVideoSource->Seek(mSeekPos, false);
    mEofReached = false;
    mSeekAsap = false;

    // unlock
    mDeliverMutex.unlock();
}

void VideoWorkerThread::DoSelectInputChannel()
{
    LOG(LOG_VERBOSE, "DoSelectInputChannel now...");

    if(mDesiredInputChannel == -1)
        return;

    // lock
    mDeliverMutex.lock();

    // restart frame grabbing device
    mSourceAvailable = mVideoSource->SelectInputChannel(mDesiredInputChannel);

    mResetVideoSourceAsap = false;
    mSelectInputChannelAsap = false;
    mPaused = false;
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
            if (!tNewSourceSelected)
            {
                if (mVideoSource->GetCurrentDeviceName() == mDeviceName.toStdString())
                { // do we have what we required?
                    // seek to the beginning if we have reselected the source file
                    LOG(LOG_VERBOSE, "Seeking to the beginning of the source file");
                    mVideoSource->Seek(0);
                    mSeekAsap = false;

                    if (mResetVideoSourceAsap)
                    {
                        LOG(LOG_VERBOSE, "Haven't selected new video source, reset of current source forced");
                        mSourceAvailable = mVideoSource->Reset(MEDIA_VIDEO);
                    }
                }else
                    mVideoWidget->InformAboutOpenError(mDeviceName);
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
    {
        LOG(LOG_WARN, "Timeout during locking of deliver mutex");
        return -1;
    }

    if ((!mSetGrabResolutionAsap) && (!mResetVideoSourceAsap))
    {
        if (mPendingNewFrames)
        {
			#ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
				if (mPendingNewFrames > 1)
					LOG(LOG_VERBOSE, "Found %d pending frames", mPendingNewFrames);
			#endif

            mFrameCurrentIndex++;
            if (mFrameCurrentIndex >= FRAME_BUFFER_SIZE)
                mFrameCurrentIndex = 0;
            mPendingNewFrames--;
        }else
        {
            mMissingFrames++;
            LOG(LOG_WARN, "Missing new frame (%d overall missed frames), delivering old frame instead", mMissingFrames);
        }

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
    }else
        LOG(LOG_WARN, "No current frame available, pending frames: %d, grab resolution invalid: %d, have to reset source: %d", mPendingNewFrames, mSetGrabResolutionAsap, mResetVideoSourceAsap);

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

        // play new file from disc
        if (mPlayNewFileAsap)
        	DoPlayNewFile();

        // input device
        if (mSetCurrentDeviceAsap)
            DoSetCurrentDevice();

        // input stream preferences
        if (mSetInputStreamPreferencesAsap)
            DoSetInputStreamPreferences();

        // input channel
        if(mSelectInputChannelAsap)
            DoSelectInputChannel();

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

        if (mSeekAsap)
            DoSeek();

        mGrabbingStateMutex.lock();

        if ((!mPaused) && (mSourceAvailable))
        {
            mGrabbingStateMutex.unlock();

            // set input frame size
			tFrameSize = mFrameSize[mFrameGrabIndex];

			// get new frame from video grabber
			QTime tTime = QTime::currentTime();
			tFrameNumber = mVideoSource->GrabChunk(mFrame[mFrameGrabIndex], tFrameSize, mDropFrames);
            #ifdef DEBUG_VIDEOWIDGET_PERFORMANCE
			    LOG(LOG_WARN, "Grabbing new video frame took: %d ms", tTime.msecsTo(QTime::currentTime()));
            #endif
			mEofReached = (tFrameNumber == GRAB_RES_EOF);
			if (mEofReached)
			    mSourceAvailable = false;


			//LOG(LOG_ERROR, "DO THE BEST %d %d", tFrameNumber, tFrameSize);

			// do we have a valid new video frame?
			if ((tFrameNumber >= 0) && (tFrameSize > 0))
			{

			    // has the source resolution changed in the meantime? -> thread it as new source
			    int tSourceResX = 0, tSourceResY = 0;
			    mVideoSource->GetVideoSourceResolution(tSourceResX, tSourceResY);
			    if ((mFrameWidthLastGrabbedFrame != tSourceResX) || (mFrameHeightLastGrabbedFrame != tSourceResY))
			    {
					if ((tSourceResX != mResX) || (tSourceResY != mResY))
					{
						LOG(LOG_INFO, "New source detect because source video resolution differs: width %d => %d, height %d => %d", mResX, tSourceResX, mResY, tSourceResY);
						mVideoWidget->InformAboutNewSource();
					}
			    }
			    mFrameWidthLastGrabbedFrame = tSourceResX;
			    mFrameHeightLastGrabbedFrame = tSourceResY;

				// lock
				mDeliverMutex.lock();

				mFrameNumber[mFrameGrabIndex] = tFrameNumber;
                if (mPendingNewFrames == FRAME_BUFFER_SIZE)
                    LOG(LOG_WARN, "System too slow?, frame buffer of %d entries is full, will drop oldest frame", FRAME_BUFFER_SIZE);
                mPendingNewFrames++;

				mFrameGrabIndex++;
				if (mFrameGrabIndex >= FRAME_BUFFER_SIZE)
				    mFrameGrabIndex = 0;

                // store timestamp starting from frame number 3 to avoid peaks
				if(tFrameNumber > 3)
				{
                    //HINT: locking is done via mDeliverMutex!
                    mFrameTimestamps.push_back(Time::GetTimeStamp());
                    //LOG(LOG_WARN, "Time %ld", Time::GetTimeStamp());
                    while (mFrameTimestamps.size() > FPS_MEASUREMENT_STEPS)
                        mFrameTimestamps.removeFirst();
				}

                mVideoWidget->InformAboutNewFrame();

                // unlock
				mDeliverMutex.unlock();

				//printf("VideoWorker--> %d\n", mFrameGrabIndex);
				//printf("VideoWorker-grabbing FPS: %2d grabbed frame number: %d\n", mResultingFps, tFrameNumber);

				if ((tLastFrameNumber > tFrameNumber) && (tFrameNumber > 9 /* -1 means error, 1 is received after every reset, use "9" because of possible latencies */))
					LOG(LOG_ERROR, "Frame ordering problem detected (%d -> %d)", tLastFrameNumber, tFrameNumber);
			}else
			{
				LOG(LOG_VERBOSE, "Invalid grabbing result: %d", tFrameNumber);
				usleep(100 * 1000); // check for new frames every 1/10 seconds
			}
        }else
        {
        	if (mSourceAvailable)
        		LOG(LOG_VERBOSE, "VideoWorkerThread is in pause state");
        	else
        		LOG(LOG_VERBOSE, "VideoWorkerThread waits for available grabbing device");

        	mGrabbingCondition.wait(&mGrabbingStateMutex);
            mGrabbingStateMutex.unlock();

        	LOG(LOG_VERBOSE, "Continuing processing");
        }
    }
    mVideoSource->CloseGrabDevice();
    mVideoSource->DeleteAllRegisteredMediaSinks();
}

void VideoWorkerThread::StopGrabber()
{
    LOG(LOG_VERBOSE, "StobGrabber now...");
    mWorkerNeeded = false;
    mGrabbingCondition.wakeAll();
    mVideoSource->StopGrabbing();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
