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
#include <QContextMenuEvent>
#include <QBuffer>
#include <QQueue>
#include <QList>

#include <MediaSource.h>
#include <AudioOutSdl.h>
#include <ui_AudioWidget.h>

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

#define SAMPLE_BUFFER_SIZE               3

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
    void Init(MediaSource *pAudioSource, QMenu *pAudioMenu, QString pActionTitle = "Audio", QString pWidgetTitle = "Audio", bool pVisible = false, bool pMuted = false);

    virtual ~AudioWidget();
    void SetVisible(bool pVisible);
    void InformAboutOpenError(QString pSourceName);
    void InformAboutNewSamples();
    void InformAboutNewMuteState();
    AudioWorkerThread* GetWorker();

public slots:
    void ToggleVisibility();

private slots:
    void ToggleMuteState(bool pState = true);

private:
    void initializeGUI();
    void SetVolume(int pValue); // 0-100 %
    int GetVolume();
    void ShowSample(void* pBuffer, int pSampleSize, int pSampleNumber = 0);
    void showAudioLevel(int pLevel);
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void customEvent (QEvent* pEvent);
    void contextMenuEvent(QContextMenuEvent *event);
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


class AudioBuffer:
    public QBuffer
{
public:
    AudioBuffer(QObject * pParent = NULL);
    ~AudioBuffer();

protected:
    virtual qint64 readData(char * pData, qint64 pLen);
    virtual qint64 writeData(const char * pData, qint64 pLen);

private:
    QMutex              mMutex;
};

class AudioWorkerThread:
    public QThread
{
    Q_OBJECT;
public:
    AudioWorkerThread(MediaSource *pAudioSource, AudioWidget *pAudioWidget);

    virtual ~AudioWorkerThread();
    virtual void run();
    void StopGrabber();

    /* forwarded interface to media source */
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
    void SelectInputChannel(int pIndex);

    /* frame grabbing */
    void SetSampleDropping(bool pDrop);
    int GetCurrentSample(void **pSample, int& pSampleSize, int *pFps = NULL);

    /* audio out control */
    void SetMuteState(bool pMuted);
    bool GetMuteState();

public slots:
    void ToggleMuteState(bool pState = true);
    void SetVolume(int pValue);

private:
    void OpenPlaybackDevice();
    void ClosePlaybackDevice();
    void PlaySamples(void *pSampleBuffer, int pSampleBufferSize);
    void DoResetAudioSource();
    void DoSetInputStreamPreferences();
    void DoSetCurrentDevice();
    void DoSelectInputChannel();
    void DoStartRecorder();
    void DoStopRecorder();
    void DoPlayNewFile();

    MediaSource         *mAudioSource;
    AudioWidget         *mAudioWidget;
    void                *mSamples[SAMPLE_BUFFER_SIZE];
    unsigned long       mSampleNumber[SAMPLE_BUFFER_SIZE];
    int                 mSamplesSize[SAMPLE_BUFFER_SIZE];
    int                 mSamplesBufferSize[SAMPLE_BUFFER_SIZE];
    int                 mSampleCurrentIndex, mSampleGrabIndex;
    QMutex              mDeliverMutex, mGrabMutex;
    int                 mResX;
    int                 mResY;
    bool                mWorkerNeeded;
    bool                mWorkerWithNewData;
    bool                mDropSamples;
    int                 mResultingSps;
    bool                mAudioOutMuted;
    int                 mAudioChannel;
    std::string         mSaveFileName;
    int                 mSaveFileQuality;
    QString             mCodec;
    QString             mDeviceName;
    QString 			mCurrentFile;
    bool				mEofReached;
    bool				mPaused;
    bool				mSourceAvailable;
    int64_t				mPausedPos;

    /* for forwarded interface to media source */
    int                 mDesiredInputChannel;
    QString             mDesiredFile;
    bool                mSetInputStreamPreferencesAsap;
    bool                mSetCurrentDeviceAsap;
    bool                mResetAudioSourceAsap;
    bool                mStartRecorderAsap;
    bool                mStopRecorderAsap;
    bool				mPlayNewFileAsap;
    bool                mSelectInputChannelAsap;
    /* playback */
    AudioBuffer         *mAudioBuffer;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
