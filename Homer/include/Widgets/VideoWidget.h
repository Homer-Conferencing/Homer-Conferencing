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
 * Purpose: widget for video display
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 */

#ifndef _VIDEO_WIDGET_
#define _VIDEO_WIDGET_

#include <HBTime.h>

#include <QImage>
#include <QWidget>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <QTimer>
#include <QThread>
#include <QTime>
#include <QList>
#include <QMutex>
#include <QWaitCondition>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMainWindow>

#include <MediaSource.h>
#include <MeetingEvents.h>

#include <MediaSourceGrabberThread.h>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

class ParticipantWidget;

///////////////////////////////////////////////////////////////////////////////

// debug performance of video widget: background repainting
//#define DEBUG_VIDEOWIDGET_PERFORMANCE

// de/activate automatic frame dropping in case the video widget is invisible (default is off)
//#define VIDEO_WIDGET_DROP_WHEN_INVISIBLE

// de/activate frame handling
//#define VIDEO_WIDGET_DEBUG_FRAMES

// de/activate fullscreen display of mute state
//#define VIDEO_WIDGET_SHOW_MUTE_STATE_IN_FULLSCREEN

///////////////////////////////////////////////////////////////////////////////

#define FRAME_BUFFER_SIZE              4

enum AspectRatio{
	ASPECT_RATIO_ORIGINAL = 0,
	ASPECT_RATIO_WINDOW,
	ASPECT_RATIO_1x1, //1
	ASPECT_RATIO_4x3, //1.33
	ASPECT_RATIO_5x4, //1.25
	ASPECT_RATIO_16x9, //1.77
	ASPECT_RATIO_16x10 //1.6
};

///////////////////////////////////////////////////////////////////////////////

class VideoWorkerThread;

class VideoWidget:
    public QWidget
{
    Q_OBJECT;

public:
    VideoWidget(QWidget* pParent = NULL);

    virtual ~VideoWidget();

    void Init(QMainWindow* pMainWindow, ParticipantWidget* pParticipantWidget,  MediaSource *pVideoSource, QMenu *pVideoMenu, QString pActionTitle = "Video", QString pWidgetTitle = "Video", bool pVisible = false);

    void SetVisible(bool pVisible);

    void InformAboutOpenError(QString pSourceName);
    void InformAboutSeekingComplete();
    void InformAboutNewFrame();
    void InformAboutNewSourceResolution();
    void InformAboutNewSource();

    VideoWorkerThread* GetWorker();

public slots:
    void ToggleVisibility();

private slots:
    void ShowHourGlass();

private:
    void DialogAddNetworkSink();
    void ShowFrame(void* pBuffer, float pFps = 15, int pFrameNumber = 0);
    void SetResolution(int mX, int mY);
    void SetScaling(float pVideoScaleFactor);
    bool IsCurrentScaleFactor(float pScaleFactor);
    void SetResolutionFormat(VideoFormat pFormat);
    void ToggleFullScreenMode();
    void ToggleSmoothPresentationMode();
    void SavePicture();
    void StartRecorder();
    void StopRecorder();
    void ShowFullScreen();
    bool SetOriginalResolution();

    /* status message per OSD text */
    void ShowOsdMessage(QString pText);

    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    virtual void dropEvent(QDropEvent *pEvent);
    virtual void paintEvent(QPaintEvent *pEvent);
    virtual void resizeEvent(QResizeEvent *pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void customEvent (QEvent* pEvent);
    virtual void wheelEvent(QWheelEvent *pEvent);
    virtual void mouseMoveEvent (QMouseEvent *pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);

    QWidget             *mCurrentApplicationFocusedWidget;
    QImage              mCurrentFrame;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QMainWindow			*mMainWindow;
    QString             mVideoTitle;
    int                 mUpdateTimerId;
    int                 mResX;
    int                 mResY;
    float				mVideoScaleFactor; // 0.5, 1.0, 1.5, 2.0
    bool                mShowLiveStats;
    int                 mCustomEventReason;
    QTimer              *mHourGlassTimer;
    qreal               mHourGlassAngle;
    int                 mHourGlassOffset;
    bool                mSmoothPresentation;
    bool                mRecorderStarted;
    bool                mInsideDockWidget;
    bool                mVideoPaused;
    int                 mAspectRatio;
    bool                mNeedBackgroundUpdate, mNeedBackgroundUpdatesUntillNextFrame;
    bool				mVideoMirroredHorizontal;
    bool				mVideoMirroredVertical;
    int                 mCurrentFrameNumber;
    int                 mLastFrameNumber;
    VideoWorkerThread   *mVideoWorker;
    MediaSource         *mVideoSource;
    int                 mPendingNewFrameSignals;
    ParticipantWidget   *mParticipantWidget;
    int64_t 			mPaintEventCounter;
    /* status messages per OSD text */
    QString				mOsdStatusMessage;
    int64_t				mOsdStatusMessageTimeout;
    /* mouse hiding */
    QTime               mTimeOfLastMouseMove;
    /* periodic tasks */
    int                 mTimerId;
};


class VideoWorkerThread:
    public MediaSourceGrabberThread
{
    Q_OBJECT;
public:
    VideoWorkerThread(MediaSource *pVideoSource, VideoWidget *pVideoWidget);

    virtual ~VideoWorkerThread();

    virtual void run();

    /* forwarded interface to media source */
    void SetGrabResolution(int mX, int mY);

    /* device control */
    VideoDevices GetPossibleDevices();

    /* frame grabbing */
    void SetFrameDropping(bool pDrop);
    int GetCurrentFrame(void **pFrame, float *pFps = NULL);
    int GetLastFrameNumber();

private:
    void InitFrameBuffers();
    void DeinitFrameBuffers();
    void InitFrameBuffer(int pBufferId);
    void DoSetGrabResolution();
    virtual void DoSetCurrentDevice();
    virtual void DoPlayNewFile();
    virtual void DoSeek();
    virtual void DoSyncClock();

    VideoWidget         *mVideoWidget;

    /* frame buffering */
    void                *mFrame[FRAME_BUFFER_SIZE];
    unsigned long       mFrameNumber[FRAME_BUFFER_SIZE];
    int                 mFrameSize[FRAME_BUFFER_SIZE];
    int                 mFrameCurrentIndex, mFrameGrabIndex;

    int                 mResX;
    int                 mResY;
    int					mFrameWidthLastGrabbedFrame;
    int					mFrameHeightLastGrabbedFrame;
    int                 mPendingNewFrames;
    bool                mDropFrames;

    /* frame statistics */
    int                 mMissingFrames;
    int                 mLastFrameNumber;

    /* A/V synch. */
    bool                mWaitForFirstFrameAfterSeeking;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
