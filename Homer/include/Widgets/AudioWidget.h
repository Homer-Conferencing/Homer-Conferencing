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
 * Purpose: widget for audio display
 * Author:  Thomas Volkert
 * Since:   2009-02-12
 */

#ifndef _AUDIO_WIDGET_
#define _AUDIO_WIDGET_

#include <QImage>
#include <QWidget>
#include <QAction>
#include <QMenu>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QContextMenuEvent>
#include <QBuffer>
#include <QQueue>
#include <QList>

#include <AudioPlayback.h>
#include <MediaSourceGrabberThread.h>
#include <MediaSource.h>
#include <WaveOut.h>
#include <ui_AudioWidget.h>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

// debug performance of audio widget
//#define DEBUG_AUDIOWIDGET_PERFORMANCE

#define SAMPLE_BUFFER_SIZE               16

// de/activate frame handling
//#define AUDIO_WIDGET_DEBUG_FRAMES

// de/activate automatic sample dropping in case the audo widget is invisible (default is off)
//#define AUDIO_WIDGET_DROP_WHEN_INVISIBLE

///////////////////////////////////////////////////////////////////////////////

class AudioWorkerThread;

class AudioWidget:
    public QWidget,
    public Ui_AudioWidget
{
    Q_OBJECT;

public:
    AudioWidget(QWidget* pParent = NULL);

    virtual ~AudioWidget();

    void Init(MediaSource *pAudioSource, QMenu *pAudioMenu, QString pName = "Audio", bool pVisible = false, bool pMuted = false);

    void SetVisible(bool pVisible);

    void InformAboutNewSamples();
    void InformAboutOpenError(QString pSourceName);
    void InformAboutNewSource();
    void InformAboutNewMuteState();

    AudioWorkerThread* GetWorker();

    QStringList GetAudioStatistic();

    /* volume control */
    void SetVolume(int pValue); // 0-300 %
    int GetVolume();

public slots:
    void ToggleVisibility();
    void ToggleMuteState(bool pState = true);

private:
    void initializeGUI();
    void ShowSample(void* pBuffer, int pSampleSize);
    void showAudioLevel(int pLevel);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void customEvent (QEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pEvent);
    void DialogAddNetworkSink();
    void StartRecorder();
    void StopRecorder();

    QPoint              mWinPos;
    QAction             *mAssignedAction;
    QString             mAudioTitle;
    int                 mUpdateTimerId;
    int                 mCustomEventReason;
    bool                mShowLiveStats;
    int                 mResX;
    int                 mResY;
    bool                mRecorderStarted;
    bool                mAudioOutAvailable;
    bool                mAudioPaused;
    int					mAudioVolume;
    int                 mCurrentSampleNumber;
    int                 mLastSampleNumber;
    AudioWorkerThread   *mAudioWorker;
    MediaSource         *mAudioSource;
};

class AudioWorkerThread:
    public MediaSourceGrabberThread, AudioPlayback
{
    Q_OBJECT;
public:
    AudioWorkerThread(QString pName, MediaSource *pAudioSource, AudioWidget *pAudioWidget);

    virtual ~AudioWorkerThread();

    virtual void run();

    /* device control */
    AudioDevices GetPossibleDevices();

    /* A/V sync. */
    float GetUserAVDrift();
    void SetUserAVDrift(float pDrift);
    float GetVideoDelayAVDrift();
    void SetVideoDelayAVDrift(float pDrift);

    /* frame grabbing */
    void SetSampleDropping(bool pDrop);
    int GetCurrentSample(void **pSample, int& pSampleSize, int *pFps = NULL);

    /* audio playback control */
    void SetMuteState(bool pMuted);
    bool GetMuteState();
    int GetVolume();
    bool IsPlaybackAvailable();
    int64_t GetPlaybackGapsCounter();

public slots:
    void SetVolume(int pValue);

private:
    /* audio playback */
    void ResetPlayback();
    virtual void OpenPlaybackDevice();
    virtual void ClosePlaybackDevice();

    virtual void DoPlayNewFile();
    virtual void DoSetCurrentDevice();
    virtual void DoResetMediaSource();
    virtual void DoSeek();
    virtual void DoSyncClock();
    virtual void HandlePlayFileError();
    virtual void HandlePlayFileSuccess();

    /* audio playback */
    void DoStartPlayback();
    void DoStopPlayback();

    AudioWidget         *mAudioWidget;
    void                *mSamples[SAMPLE_BUFFER_SIZE];
    unsigned long       mSampleNumber[SAMPLE_BUFFER_SIZE];
    int                 mSamplesSize[SAMPLE_BUFFER_SIZE];
    int                 mSamplesBufferSize[SAMPLE_BUFFER_SIZE];
    int                 mSampleCurrentIndex, mSampleGrabIndex;
    bool                mWorkerWithNewData;
    bool                mDropSamples;
    int                 mResultingSps;
    bool                mAudioOutMuted;
    bool				mPlaybackAvailable;

    /* for forwarded interface to media source */
    int                 mDesiredInputChannel;
    bool				mStartPlaybackAsap;
    bool				mStopPlaybackAsap;
    /* A/V synch. */
    float               mUserAVDrift;
    float               mVideoDelayAVDrift;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
