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
 * Purpose: Implementation of StreamingControlWidget.h
 * Author:  Thomas Volkert
 * Since:   2010-11-17
 */

#include <Widgets/StreamingControlWidget.h>
#include <MediaSourceMuxer.h>
#include <MediaSourceFile.h>
#include <MediaSourceDesktop.h>
#include <Configuration.h>
#include <Logger.h>
#include <Snippets.h>

#include <QWidget>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>

#include <list>
#include <string>

using namespace Homer::Multimedia;
using namespace std;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

StreamingControlWidget::StreamingControlWidget(ParticipantWidget* pBroadcastParticipantWidget, MediaSourceDesktop *pMediaSourceDesktop, OverviewPlaylistWidget *pOverviewPlaylistWidgetVideo, OverviewPlaylistWidget *pOverviewPlaylistWidgetAudio, OverviewPlaylistWidget *pOverviewPlaylistWidgetMovie):
    QWidget()
{
    mVideoWorker = pBroadcastParticipantWidget->GetVideoWorker();
    mAudioWorker = pBroadcastParticipantWidget->GetAudioWorker();
    mBroadcastParticipantWidget = pBroadcastParticipantWidget;
    mMediaSourceDesktop = pMediaSourceDesktop;
    mOverviewPlaylistWidgetVideo = pOverviewPlaylistWidgetVideo;
    mOverviewPlaylistWidgetAudio = pOverviewPlaylistWidgetAudio;
    mOverviewPlaylistWidgetMovie = pOverviewPlaylistWidgetMovie;

    initializeGUI();
    connect(mPbBroadcastScreenSegment, SIGNAL(clicked()), this, SLOT(StartScreenSegmentStreaming()));
    connect(mPbBroadcastCamera, SIGNAL(clicked()), this, SLOT(StartCameraStreaming()));
    connect(mPbBroadcastVoice, SIGNAL(clicked()), this, SLOT(StartVoiceStreaming()));
    connect(mPbBroadcastVideoFile, SIGNAL(clicked()), this, SLOT(StartVideoFileStreaming()));
    connect(mPbBroadcastAudioFile, SIGNAL(clicked()), this, SLOT(StartAudioFileStreaming()));
    connect(mPbBroadcastMovieFile, SIGNAL(clicked()), this, SLOT(StartMovieFileStreaming()));
    //connect(mSlAudio, SIGNAL(valueChanged(int)), this, SLOT(SeekAudioFile(int)));
    //connect(mSlVideo, SIGNAL(valueChanged(int)), this, SLOT(SeekVideoFile(int)));
    connect(mSlAudio, SIGNAL(sliderMoved(int)), this, SLOT(SeekAudioFile(int)));
    connect(mSlVideo, SIGNAL(sliderMoved(int)), this, SLOT(SeekVideoFile(int)));
    connect(mCbVideoInput, SIGNAL(currentIndexChanged(int)), this, SLOT(SelectedNewVideoInputChannel(int)));

    mTimerId = startTimer(STREAM_POS_UPDATE_DELAY);
}

StreamingControlWidget::~StreamingControlWidget()
{
    if (mTimerId != -1)
        killTimer(mTimerId);
}

///////////////////////////////////////////////////////////////////////////////

void StreamingControlWidget::initializeGUI()
{
    setupUi(this);
    if (mAudioWorker->SupportsSeeking())
        SetAudioSliderVisible();
    else
        SetAudioSliderVisible(false);

    if (mVideoWorker->SupportsSeeking())
        SetVideoSliderVisible();
    else
        SetVideoSliderVisible(false);
    if (mVideoWorker->SupportsMultipleChannels())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);
}

void StreamingControlWidget::StartScreenSegmentStreaming()
{
    mOverviewPlaylistWidgetVideo->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StopPlaylist();

    QStringList tList = mVideoWorker->GetPossibleDevices();
    if (tList.contains("DESKTOP: screen segment"))
    mVideoWorker->SetCurrentDevice("DESKTOP: screen segment");

    SetVideoSliderVisible(false);
    if (mVideoWorker->SupportsMultipleChannels())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);

    mMediaSourceDesktop->SelectSegment(this);
}

void StreamingControlWidget::StartCameraStreaming()
{
    mOverviewPlaylistWidgetVideo->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StopPlaylist();

    QStringList tList = mVideoWorker->GetPossibleDevices();
    int tPos = -1, i = 0;

    for (i = 0; i < tList.size(); i++)
    {
        if ((tList[i].contains("V4L2")) || (tList[i].contains("VFW")))
        {
            tPos = i;
            break;
        }
    }

    // found something?
    if (tPos == -1)
    {
        ShowWarning("Missing camera", "No camera available. Please, select another source!");
        return;
    }

    mVideoWorker->SetCurrentDevice(tList[tPos]);
    mSlVideo->setValue(0);
    SetVideoSliderVisible(false);
    if (mVideoWorker->SupportsMultipleChannels())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);
}

void StreamingControlWidget::StartVoiceStreaming()
{
    mOverviewPlaylistWidgetAudio->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StopPlaylist();

    QStringList tList = mAudioWorker->GetPossibleDevices();
    int tPos = -1, i = 0;

    for (i = 0; i < tList.size(); i++)
    {
        if ((tList[i].contains("ALSA")) || (tList[i].contains("MSYS")) || (tList[i].contains("OSS")))
        {
            tPos = i;
            break;
        }
    }

    // found something?
    if (tPos == -1)
    {
        ShowWarning("Missing microphone", "No microphone available. Please, select another source!");
        return;
    }

    mAudioWorker->SetCurrentDevice(tList[tPos]);
    mSlAudio->setValue(0);
    SetAudioSliderVisible(false);
}

void StreamingControlWidget::StartVideoFileStreaming()
{
    mOverviewPlaylistWidgetMovie->StopPlaylist();
    mOverviewPlaylistWidgetVideo->StartPlaylist();
}

void StreamingControlWidget::StartAudioFileStreaming()
{
    mOverviewPlaylistWidgetMovie->StopPlaylist();
    mOverviewPlaylistWidgetAudio->StartPlaylist();
}

void StreamingControlWidget::StartMovieFileStreaming()
{
    mOverviewPlaylistWidgetVideo->StopPlaylist();
    mOverviewPlaylistWidgetAudio->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StartPlaylist();
}

void StreamingControlWidget::SeekMovieFile(int pPos)
{
    mVideoWorker->Seek(pPos);
    mAudioWorker->Seek(pPos);
}

void StreamingControlWidget::SeekVideoFile(int pPos)
{
    mVideoWorker->Seek(pPos);
}

void StreamingControlWidget::SeekAudioFile(int pPos)
{
    mAudioWorker->Seek(pPos);
}

void StreamingControlWidget::SetVideoSliderVisible(bool pVisible)
{
    mSlVideo->setVisible(pVisible);
    if (pVisible)
    {
        if (mSlAudio->isVisible())
        {
            //check if video and audio source are the same
            if (mVideoWorker->GetStreamName() == mAudioWorker->GetStreamName())
                SetMovieSliderVisible();
            else
                SetMovieSliderVisible(false);
        }
//        if (mTimerId == -1)
//        {
//            mTimerId = startTimer(STREAM_POS_UPDATE_DELAY);
//            LOG(LOG_VERBOSE, "Starting timer with an update delay of %d ms", STREAM_POS_UPDATE_DELAY);
//        }
    }else
    {
        SetMovieSliderVisible(false);
//        if ((!mSlAudio->isVisible())  && (mTimerId != -1))
//        {
//            killTimer(mTimerId);
//            mTimerId = -1;
//        }
    }
}

void StreamingControlWidget::SetAudioSliderVisible(bool pVisible)
{
    mSlAudio->setVisible(pVisible);
    if (pVisible)
    {
        if (mSlVideo->isVisible())
        {
            //check if video and audio source are the same
            if (mVideoWorker->GetStreamName() == mAudioWorker->GetStreamName())
                SetMovieSliderVisible();
            else
                SetMovieSliderVisible(false);
        }
//        if (mTimerId == -1)
//        {
//            mTimerId = startTimer(STREAM_POS_UPDATE_DELAY);
//            LOG(LOG_VERBOSE, "Starting timer with an update delay of %d ms", STREAM_POS_UPDATE_DELAY);
//        }
    }else
    {
        SetMovieSliderVisible(false);
//        if ((!mSlVideo->isVisible()) && (mTimerId != -1))
//        {
//            killTimer(mTimerId);
//            mTimerId = -1;
//        }
    }
}

void StreamingControlWidget::SetMovieSliderVisible(bool pVisible)
{
    mBroadcastParticipantWidget->SetMovieControlsVisible(pVisible);
}

void StreamingControlWidget::SetVideoInputSelectionVisible(bool pVisible)
{
    mCbVideoInput->clear();
    if ((pVisible) && (mVideoWorker->SupportsMultipleChannels()))
    {
        QStringList tList = mVideoWorker->GetPossibleChannels();
        int i = 0;
        for (i = 0; i < tList.size(); i++)
        {
            mCbVideoInput->addItem(tList[i]);
            if (tList[i] == mVideoWorker->GetCurrentChannel())
                mCbVideoInput->setCurrentIndex(mCbVideoInput->count() - 1);
        }
    }
    mCbVideoInput->setVisible(pVisible);
}

void StreamingControlWidget::SelectedNewVideoInputChannel(int pIndex)
{
    mVideoWorker->SetChannel(pIndex);
}

void StreamingControlWidget::timerEvent(QTimerEvent *pEvent)
{
    int tTmp = 0;
    int tHour, tMin, tSec;

    if (pEvent->timerId() == mTimerId)
    {
        if ((mVideoWorker->SupportsSeeking()) && (mVideoWorker->GetSeekEnd() > 0))
        {
        	SetVideoSliderVisible();
        	if (mVideoWorker->SupportsMultipleChannels())
        		SetVideoInputSelectionVisible();
        	else
        		SetVideoInputSelectionVisible(false);

            if (mSlVideo->isVisible())
            {
                int64_t tCurVideoPos = mVideoWorker->GetSeekPos();
                int64_t tEndVideoPos = mVideoWorker->GetSeekEnd();
                tTmp = 1000 * tCurVideoPos / tEndVideoPos;

                mSlVideo->setValue(tTmp);
            }
        }else
        {
            SetVideoSliderVisible(false);
        }
        if ((mAudioWorker->SupportsSeeking()) && (mAudioWorker->GetSeekEnd() > 0))
        {
        	SetAudioSliderVisible();

        	if (mSlAudio->isVisible())
        	{
                int64_t tCurAudioPos = mAudioWorker->GetSeekPos();
                int64_t tEndAudioPos = mAudioWorker->GetSeekEnd();
                tTmp = 1000 * tCurAudioPos / mAudioWorker->GetSeekEnd();

                mSlAudio->setValue(tTmp);
        	}
        }else
        {
            SetAudioSliderVisible(false);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
