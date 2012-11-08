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
 * Purpose: worker thread for video/audio grabbing
 * Author:  Thomas Volkert
 * Since:   2012-08-08
 */

#ifndef _MEDIA_SOURCE_GRABBER_
#define _MEDIA_SOURCE_GRABBER_

#include <MediaSourceGrabberThread.h>
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

namespace Homer { namespace Gui {

using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

class MediaSourceGrabberThread:
    public QThread
{
    Q_OBJECT;
public:
    MediaSourceGrabberThread(QString pName, MediaSource *pMediaSource);

    virtual ~MediaSourceGrabberThread();

    void StopGrabber();

    /* forwarded interface to media source */
    void ResetSource();
    void SetInputStreamPreferences(QString pCodec);

    /* recording */
    void StartRecorder(std::string pSaveFileName, int pQuality);
    void StopRecorder();

    /* naming */
    void SetStreamName(QString pName);
    QString GetStreamName();

    /* device control */
    QString GetCurrentDevice();
    QString GetCurrentDevicePeer();
    void SetCurrentDevice(QString pName);
    QString GetDeviceDescription(QString pName);

    /* file based video playback */
    bool PlayFile(QString pName = "");
    void PauseFile();
    bool IsPaused();
    bool PlayingFile();
    void StopFile();
    bool EofReached();
    QString CurrentFile();
    bool SupportsSeeking();
    void Seek(float pPos);
    float GetSeekPos();
    float GetSeekEnd();

    /* A/V sync. */
    void SyncClock(MediaSource* pSource);

    /* multiple channels control */
    bool SupportsMultipleInputStreams();
    QString GetCurrentInputStream();
    void SelectInputStream(int pIndex);
    QStringList GetPossibleInputStreams();

    /* relay control */
    void SetRelayActivation(bool pActive);

protected:
    /* recording */
    virtual void DoStartRecorder();
    virtual void DoStopRecorder();
    /* multi-input */
    virtual void DoSelectInputStream();
    /* general purpose */
    virtual void DoPlayNewFile() = 0;
    virtual void DoSetCurrentDevice() = 0;
    virtual void DoResetMediaSource();
    virtual void DoSetInputStreamPreferences();
    virtual void DoSeek();
    virtual void DoSyncClock() = 0;
    /* error report */
    virtual void HandlePlayFileSuccess() = 0;
    virtual void HandlePlayFileError() = 0;

    MediaSource         *mMediaSource;
    QString				mName;
    QMutex              mDeliverMutex;
    QMutex              mGrabbingStateMutex; // secures mPaused, mSourceAvailable in public functions
    QWaitCondition      mGrabbingCondition;
    bool                mWorkerNeeded;
    std::string         mSaveFileName;
    int                 mSaveFileQuality;
    QString             mCodec;
    QString             mDeviceName;
    QString				mDesiredFile;
    QString 			mCurrentFile;
    bool				mEofReached;
    bool				mPaused;
    bool				mSourceAvailable;
    float				mPausedPos;
    QList<int64_t>      mFrameTimestamps;

    /* store if we try to open a file or a device/network/memory based video source */
    bool                mTryingToOpenAFile;

    /* for forwarded interface to media source */
    int                 mDesiredInputStream;

    /* delegated tasks */
    bool                mSetInputStreamPreferencesAsap;
    bool                mSetCurrentDeviceAsap;
    bool                mSetGrabResolutionAsap;
    bool                mResetMediaSourceAsap;
    bool                mStartRecorderAsap;
    bool                mStopRecorderAsap;
    bool				mPlayNewFileAsap;
    bool                mSelectInputStreamAsap;

    /* seeking */
    bool                mSeekAsap;
    float               mSeekPos;

    /* A/V synch. */
    bool                mSyncClockAsap;
    MediaSource*        mSyncClockMasterSource;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
