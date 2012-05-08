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
    connect(mCbVideoInput, SIGNAL(currentIndexChanged(int)), this, SLOT(SelectedNewVideoInputChannel(int)));

    mTimerId = startTimer(1000);
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
    if (mVideoWorker->SupportsMultipleChannels())
    {
        mCbVideoInput->clear();
        QStringList tList = mVideoWorker->GetPossibleChannels();
        int i = 0;
        for (i = 0; i < tList.size(); i++)
        {
            mCbVideoInput->addItem(tList[i]);
            if (tList[i] == mVideoWorker->GetCurrentChannel())
                mCbVideoInput->setCurrentIndex(mCbVideoInput->count() - 1);
        }
        mCbVideoInput->setVisible(true);
    }else
    {
        mCbVideoInput->setVisible(false);
    }
}

void StreamingControlWidget::StartScreenSegmentStreaming()
{
    mOverviewPlaylistWidgetVideo->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StopPlaylist();

    mVideoWorker->SetCurrentDevice(MSD_DESKTOP_SEGMENT); // used fixed name

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

    VideoDevicesList tList = mVideoWorker->GetPossibleDevices();
    VideoDevicesList::iterator tIt;
    QString tSelectedDevice = "";

    for (tIt = tList.begin(); tIt != tList.end(); tIt++)
    {
        if (tIt->Type == Camera)
        {
            tSelectedDevice = QString(tIt->Name.c_str());
            break;
        }
    }

    // found something?
    if (tSelectedDevice == "")
    {
        ShowWarning("Missing camera", "No camera available. Please, select another source!");
        return;
    }

    LOG(LOG_VERBOSE, "Selecting %s", tSelectedDevice.toStdString().c_str());

    mVideoWorker->SetCurrentDevice(tSelectedDevice);
    if (mVideoWorker->SupportsMultipleChannels())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);
}

void StreamingControlWidget::StartVoiceStreaming()
{
    mOverviewPlaylistWidgetAudio->StopPlaylist();
    mOverviewPlaylistWidgetMovie->StopPlaylist();

    AudioDevicesList tList = mAudioWorker->GetPossibleDevices();
    AudioDevicesList::iterator tIt;
    QString tSelectedDevice = "";

    for (tIt = tList.begin(); tIt != tList.end(); tIt++)
    {
        if (tIt->Type == Microphone)
        {
            tSelectedDevice = QString(tIt->Name.c_str());
            break;
        }
    }

    // found something?
    if (tSelectedDevice == "")
    {
        ShowWarning("Missing microphone", "No microphone available. Please, select another source!");
        return;
    }

    LOG(LOG_VERBOSE, "Selecting %s", tSelectedDevice.toStdString().c_str());

    mAudioWorker->SetCurrentDevice(tSelectedDevice);
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

void StreamingControlWidget::SetVideoInputSelectionVisible(bool pVisible)
{
    if (((mCbVideoInput->isVisible()) && (!pVisible))  || ((!mCbVideoInput->isVisible()) && (pVisible)))
    {
        LOG(LOG_VERBOSE, "Setting new visibility state for video input selection");
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
}

void StreamingControlWidget::SelectedNewVideoInputChannel(int pIndex)
{
    LOG(LOG_VERBOSE, "User selected new video input channel: %d", pIndex);
    if (pIndex >= 0)
        mVideoWorker->SelectInputChannel(pIndex);
}

void StreamingControlWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    int tTmp = 0;
    int tHour, tMin, tSec;

    if (pEvent->timerId() == mTimerId)
    {
        if (mVideoWorker->SupportsMultipleChannels())
            SetVideoInputSelectionVisible();
        else
            SetVideoInputSelectionVisible(false);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
