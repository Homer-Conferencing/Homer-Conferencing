/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
#include <QList>
#include <QMutex>
#include <QContextMenuEvent>
#include <QKeyEvent>

#include <MediaSource.h>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

#define FRAME_BUFFER_SIZE               3

#define FPS_MEASUREMENT_STEPS          30

///////////////////////////////////////////////////////////////////////////////

class VideoWorkerThread;

class VideoWidget:
    public QWidget
{
    Q_OBJECT;

public:
    VideoWidget(QWidget* pParent = NULL);
    void Init(MediaSource *pVideoSource, QMenu *pVideoMenu, QString pActionTitle = "Video", QString pWidgetTitle = "Video", bool pVisible = false, bool pInsideDockWidget = false);

    virtual ~VideoWidget();
    void SetVisible(bool pVisible);
    virtual void contextMenuEvent(QContextMenuEvent *event);
    void InformAboutOpenError(QString pSourceName);
    void InformAboutNewFrame();
    void InformAboutNewSource();
    void SetOriginalResolution();
    VideoWorkerThread* GetWorker();

public slots:
    void ToggleVisibility();
    void ShowHourGlass();

private:
    void DialogAddNetworkSink();
    void ShowFrame(void* pBuffer, float pFps = 15, int pFrameNumber = 0);
    void SetResolution(int mX, int mY);
    void SetResolutionFormat(VideoFormat pFormat);
    void ToggleFullScreenMode();
    void SavePicture();
    void StartRecorder();
    void StopRecorder();

    virtual void paintEvent(QPaintEvent *pEvent);
    virtual void resizeEvent(QResizeEvent *pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual void mouseDoubleClickEvent(QMouseEvent *pEvent);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void customEvent (QEvent* pEvent);

    QWidget             *mCurrentApplicationFocusedWidget;
    QImage              mCurrentFrame;
    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QString             mVideoTitle;
    int                 mUpdateTimerId;
    int                 mResX;
    int                 mResY;
    bool                mShowLiveStats;
    int                 mCustomEventReason;
    QTimer              *mHourGlassTimer;
    qreal               mHourGlassAngle;
    int                 mHourGlassOffset;
    bool                mRecorderStarted;
    bool                mInsideDockWidget;
    bool                mVideoPaused;
    bool                mKeepAspectRatio;
    bool                mNeedBackgroundUpdate;
    bool				mVideoMirroredHorizontal;
    bool				mVideoMirroredVertical;
    int                 mCurrentFrameNumber;
    int                 mLastFrameNumber;
    VideoWorkerThread   *mVideoWorker;
    MediaSource         *mVideoSource;
};


class VideoWorkerThread:
    public QThread
{
    Q_OBJECT;
public:
    VideoWorkerThread(MediaSource *pVideoSource, VideoWidget *pVideoWidget);

    virtual ~VideoWorkerThread();
    virtual void run();
    void StopGrabber();

    /* forwarded interface to media source */
    void SetGrabResolution(int mX, int mY);
    void ResetSource();
    void SetInputStreamPreferences(QString pCodec);
    /* recording */
    void StartRecorder(std::string pSaveFileName, int pQuality);
    void StopRecorder();
    //--
    void SetStreamName(QString pName);
    QString GetStreamName();
    //--
    QString GetCurrentDevice();
    void SetCurrentDevice(QString pName);
    QStringList GetPossibleDevices();
    QString GetDeviceDescription(QString pName);
    //--
    void PlayFile(QString pName);
    void PauseFile();
    void StopFile();
    bool EofReached();
    QString CurrentFile();
    //--
    bool SupportsSeeking();
    void Seek(int pPos); // max. value is 1000
    int64_t GetSeekPos();
    int64_t GetSeekEnd();
    //--
    bool SupportsMultipleChannels();
    QString GetCurrentChannel();
    void SetChannel(int pIndex);
    QStringList GetPossibleChannels();

    /* frame grabbing */
    void SetFrameDropping(bool pDrop);
    int GetCurrentFrame(void **pFrame, float *pFps = NULL);

private:
    void InitFrameBuffer(int pBufferId);
    void DoSetGrabResolution();
    void DoResetVideoSource();
    void DoSetInputStreamPreferences();
    void DoSetCurrentDevice();
    void DoStartRecorder();
    void DoStopRecorder();
    void DoPlayNewFile();

    MediaSource         *mVideoSource;
    VideoWidget         *mVideoWidget;
    void                *mFrame[FRAME_BUFFER_SIZE];
    unsigned long       mFrameNumber[FRAME_BUFFER_SIZE];
    int                 mFrameSize[FRAME_BUFFER_SIZE];
    int                 mFrameCurrentIndex, mFrameGrabIndex;
    QMutex              mDeliverMutex, mGrabMutex;
    int                 mResX;
    int                 mResY;
    bool                mWorkerNeeded;
    bool                mWorkerWithNewData;
    bool                mDropFrames;
    std::string         mSaveFileName;
    int                 mSaveFileQuality;
    QString             mCodec;
    QString             mDeviceName;
    QString				mDesiredFile;
    QString 			mCurrentFile;
    bool				mEofReached;
    bool				mPaused;
    bool				mSourceAvailable;
    int64_t				mPausedPos;
    QList<int64_t>      mFrameTimestamps;

    /* for forwarded interface to media source */
    bool                mSetInputStreamPreferencesAsap;
    bool                mSetCurrentDeviceAsap;
    bool                mSetGrabResolutionAsap;
    bool                mResetVideoSourceAsap;
    bool                mStartRecorderAsap;
    bool                mStopRecorderAsap;
    bool				mPlayNewFileAsap;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
