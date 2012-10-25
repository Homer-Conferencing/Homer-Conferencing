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
#include <Widgets/OverviewPlaylistWidget.h>
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

StreamingControlWidget::StreamingControlWidget(ParticipantWidget* pBroadcastParticipantWidget, MediaSourceDesktop *pMediaSourceDesktop):
    QWidget()
{
    mVideoWorker = pBroadcastParticipantWidget->GetVideoWorker();
    mAudioWorker = pBroadcastParticipantWidget->GetAudioWorker();
    mBroadcastParticipantWidget = pBroadcastParticipantWidget;
    mMediaSourceDesktop = pMediaSourceDesktop;

    initializeGUI();
    connect(mPbBroadcastScreenSegment, SIGNAL(clicked()), this, SLOT(StartScreenSegmentStreaming()));
    connect(mPbBroadcastCamera, SIGNAL(clicked()), this, SLOT(StartCameraStreaming()));
    connect(mPbBroadcastVoice, SIGNAL(clicked()), this, SLOT(StartVoiceStreaming()));
    connect(mPbBroadcastFile, SIGNAL(clicked()), this, SLOT(StartFileStreaming()));
    connect(mCbVideoInput, SIGNAL(currentIndexChanged(int)), this, SLOT(SelectedNewVideoInputStream(int)));

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
    if (mVideoWorker->SupportsMultipleInputStreams())
    {
        mCbVideoInput->clear();
        QStringList tList = mVideoWorker->GetPossibleInputStreams();
        int i = 0;
        for (i = 0; i < tList.size(); i++)
        {
            mCbVideoInput->addItem(tList[i]);
            if (tList[i] == mVideoWorker->GetCurrentInputStream())
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
    mVideoWorker->SetCurrentDevice(MEDIA_SOURCE_DESKTOP); // used fixed name

    if (mVideoWorker->SupportsMultipleInputStreams())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);

    mMediaSourceDesktop->SelectSegment(this);
}

void StreamingControlWidget::StartCameraStreaming()
{
    VideoDevices tList = mVideoWorker->GetPossibleDevices();
    VideoDevices::iterator tIt;
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
    if (mVideoWorker->SupportsMultipleInputStreams())
        SetVideoInputSelectionVisible();
    else
        SetVideoInputSelectionVisible(false);
}

void StreamingControlWidget::StartVoiceStreaming()
{
    AudioDevices tList = mAudioWorker->GetPossibleDevices();
    AudioDevices::iterator tIt;
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

void StreamingControlWidget::StartFileStreaming()
{
	LOG(LOG_VERBOSE, "Trigger start of file based A/V grabbing");
	PLAYLISTWIDGET.StartPlaylist();
}

void StreamingControlWidget::SetVideoInputSelectionVisible(bool pVisible)
{
    if (((mCbVideoInput->isVisible()) && (!pVisible))  || ((!mCbVideoInput->isVisible()) && (pVisible)))
    {
        LOG(LOG_VERBOSE, "Setting new visibility state for video input selection");
        mCbVideoInput->clear();
        if ((pVisible) && (mVideoWorker->SupportsMultipleInputStreams()))
        {
            QStringList tList = mVideoWorker->GetPossibleInputStreams();
            int i = 0;
            for (i = 0; i < tList.size(); i++)
            {
                mCbVideoInput->addItem(tList[i]);
                if (tList[i] == mVideoWorker->GetCurrentInputStream())
                    mCbVideoInput->setCurrentIndex(mCbVideoInput->count() - 1);
            }
        }
        mCbVideoInput->setVisible(pVisible);
    }
}

void StreamingControlWidget::SelectedNewVideoInputStream(int pIndex)
{
    LOG(LOG_VERBOSE, "User selected new video input stream: %d", pIndex);
    if (pIndex >= 0)
        mVideoWorker->SelectInputStream(pIndex);
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
        if (mVideoWorker->SupportsMultipleInputStreams())
            SetVideoInputSelectionVisible();
        else
            SetVideoInputSelectionVisible(false);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
