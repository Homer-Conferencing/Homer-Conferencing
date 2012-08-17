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
 * Purpose: Implementation of the widget for audio display
 * Author:  Thomas Volkert
 * Since:   2009-02-12
 */

#include <MediaSourceFile.h>
#include <WaveOutPortAudio.h>
#include <WaveOutSdl.h>
#include <ProcessStatisticService.h>
#include <Widgets/AudioWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <Dialogs/AddNetworkSinkDialog.h>
#include <Configuration.h>
#include <Logger.h>
#include <Meeting.h>
#include <Snippets.h>
#include <HBThread.h>

#include <QInputDialog>
#include <QPalette>
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDir>
#include <QTime>
#include <QPainter>
#include <QEvent>
#include <QApplication>

#include <stdlib.h>
#include <string>
#include <vector>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;
using namespace Homer::Multimedia;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

// how many audio buffers do we allow for audio playback?
#define AUDIO_MAX_PLAYBACK_QUEUE                            3

///////////////////////////////////////////////////////////////////////////////

#define AUDIO_EVENT_NEW_SAMPLES               (QEvent::User + 1001)
#define AUDIO_EVENT_OPEN_ERROR                (QEvent::User + 1002)
#define AUDIO_EVENT_NEW_MUTE_STATE            (QEvent::User + 1003)

class AudioEvent:
    public QEvent
{
public:
    AudioEvent(int pReason, QString pDescription):QEvent(QEvent::User), mReason(pReason), mDescription(pDescription)
    {

    }
    ~AudioEvent()
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

AudioWidget::AudioWidget(QWidget* pParent):
    QWidget(pParent)
{
    mResX = 640;
    mResY = 480;
    mAudioVolume = 100;
    mCurrentSampleNumber = 0;
    mLastSampleNumber = 0;
    mCustomEventReason = 0;
    mAudioWorker = NULL;
    mAssignedAction = NULL;
    mShowLiveStats = false;
    mRecorderStarted = false;
    mAudioPaused = false;

    hide();
}

void AudioWidget::Init(MediaSource *pAudioSource, QMenu *pMenu, QString pActionTitle, QString pWidgetTitle, bool pVisible, bool pMuted)
{
    mAudioSource = pAudioSource;
    mAudioTitle = pActionTitle;
    LOG(LOG_VERBOSE, "Creating audio widget");

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
        tIcon.addPixmap(QPixmap(":/images/22_22/Unchecked.png"), QIcon::Normal, QIcon::Off);
        mAssignedAction->setIcon(tIcon);
    }

    //####################################################################
    //### update GUI
    //####################################################################
    LOG(LOG_VERBOSE, "..init GUI elements");
    initializeGUI();
    setWindowTitle(pWidgetTitle);
    //setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (mAssignedAction != NULL)
        connect(mAssignedAction, SIGNAL(triggered()), this, SLOT(ToggleVisibility()));
    if (mAudioSource != NULL)
    {
        LOG(LOG_VERBOSE, "..create audio worker");
        mAudioWorker = new AudioWorkerThread(mAudioSource, this);
        LOG(LOG_VERBOSE, "..start audio worker");
        mAudioWorker->start(QThread::TimeCriticalPriority);
        int tLoop = 0;
        while(!mAudioWorker->IsPlaybackAvailable())
        {
            tLoop++;
            LOG(LOG_VERBOSE, "Waiting for available audio playback, loop %d", tLoop);
            Thread::Suspend(100 * 1000);
        }
        LOG(LOG_VERBOSE, "..set mute state to %d", pMuted);
        mAudioWorker->SetMuteState(pMuted);
    }

    connect(mPbMute, SIGNAL(clicked(bool)), this, SLOT(ToggleMuteState(bool)));

    LOG(LOG_VERBOSE, "..set visibility to %d", pVisible);
    SetVisible(pVisible);

    LOG(LOG_VERBOSE, "..hide stream info");
    mLbStreamInfo->hide();
    mPbMute->setChecked(!pMuted);
    mLbRecording->setVisible(false);
}

AudioWidget::~AudioWidget()
{
    LOG(LOG_VERBOSE, "Going to destroy audio widget..");

    if (mAudioWorker != NULL)
    {
        mAudioWorker->StopGrabber();
        LOG(LOG_VERBOSE, "..waiting for end of audio worker thread");
        if (!mAudioWorker->wait(3000))
        {
            LOG(LOG_WARN, "..going to force termination of worker thread");
            mAudioWorker->terminate();
        }

        LOG(LOG_VERBOSE, "..waiting for termination of audio worker thread");
        if (!mAudioWorker->wait(5000))
        {
            LOG(LOG_ERROR, "Termination of AudioWorker-Thread timed out");
        }
    	LOG(LOG_VERBOSE, "Going to delete audio worker..");
        delete mAudioWorker;
    }else
    	LOG(LOG_VERBOSE, "Audio worker soesn't exist");

    if (mAssignedAction != NULL)
        delete mAssignedAction;

    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void AudioWidget::initializeGUI()
{
    setupUi(this);
}

void AudioWidget::closeEvent(QCloseEvent* pEvent)
{
    ToggleVisibility();
}

void AudioWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
    vector<string>::iterator tIt;
    QAction *tAction;
    vector<string> tRegisteredAudioSinks = mAudioSource->ListRegisteredMediaSinks();
    vector<string>::iterator tRegisteredAudioSinksIt;

    QMenu tMenu(this);

    //###############################################################################
    //### RECORD
    //###############################################################################
    if (mAudioSource->SupportsRecording())
    {
		QIcon tIcon5;
		if (mRecorderStarted)
		{
			tAction = tMenu.addAction("Stop recording");
			tIcon5.addPixmap(QPixmap(":/images/22_22/Audio_Stop.png"), QIcon::Normal, QIcon::Off);
		}else
		{
			tAction = tMenu.addAction("Record audio");
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

    //###############################################################################
    //### TRACKS
    //###############################################################################
    vector<string> tInputChannels = mAudioSource->GetInputChannels();
    if (mAudioSource->SupportsMultipleInputChannels())
    {
        QMenu *tStreamMenu = tMenu.addMenu("Audio streams");

        QIcon tIcon14;
        tIcon14.addPixmap(QPixmap(":/images/22_22/Audio_Play.png"), QIcon::Normal, QIcon::Off);

        string tCurrentStream = mAudioSource->CurrentInputChannel();
        QAction *tStreamAction;
        for (tIt = tInputChannels.begin(); tIt != tInputChannels.end(); tIt++)
        {
            tStreamAction = tStreamMenu->addAction(QString(tIt->c_str()));
            tStreamAction->setIcon(tIcon14);
            tStreamAction->setCheckable(true);
            if ((*tIt) == tCurrentStream)
                tStreamAction->setChecked(true);
            else
                tStreamAction->setChecked(false);
        }

        QIcon tIcon13;
        tIcon13.addPixmap(QPixmap(":/images/22_22/SpeakerLoud.png"), QIcon::Normal, QIcon::Off);
        tStreamMenu->setIcon(tIcon13);
    }

    //###############################################################################
    //### AUDIO SETTINGS
    //###############################################################################
    QIcon tIcon3;
    tIcon3.addPixmap(QPixmap(":/images/22_22/SpeakerLoud.png"), QIcon::Normal, QIcon::Off);
    QMenu *tAudioMenu = tMenu.addMenu("Audio settings");
            //###############################################################################
            //### VOLUMES
            //###############################################################################
            QMenu *tVolMenu = tAudioMenu->addMenu("Volume");

            for (int i = 0; i <13; i++)
            {
                QAction *tVolAction = tVolMenu->addAction(QString("%1 %").arg(i * 25));
                tVolAction->setCheckable(true);
                if (i * 25 == mAudioVolume)
                    tVolAction->setChecked(true);
                else
                    tVolAction->setChecked(false);
            }

    tAudioMenu->setIcon(tIcon3);

    //###############################################################################
    //### STREAM RELAY
    //###############################################################################
    if(mAudioSource->SupportsRelaying())
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

        if (tRegisteredAudioSinks.size())
        {
            for (tRegisteredAudioSinksIt = tRegisteredAudioSinks.begin(); tRegisteredAudioSinksIt != tRegisteredAudioSinks.end(); tRegisteredAudioSinksIt++)
            {
                QAction *tSinkAction = tRegisteredVideoSinksMenu->addAction(QString(tRegisteredAudioSinksIt->c_str()));
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
        if (mAudioPaused)
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
    tAction = tMenu.addAction("Close audio");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/22_22/Close.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon1);

    //###############################################################################
    //### RESULTING REACTION
    //###############################################################################
    QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Close audio") == 0)
        {
            ToggleVisibility();
            return;
        }
        if (tPopupRes->text().compare("Reset source") == 0)
        {
            mAudioWorker->ResetSource();
            return;
        }
        if (tPopupRes->text().compare("Stop recording") == 0)
        {
            StopRecorder();
            return;
        }
        if (tPopupRes->text().compare("Record audio") == 0)
        {
            StartRecorder();
            return;
        }
        if (tPopupRes->text().compare("Add network sink") == 0)
        {
            DialogAddNetworkSink();
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
            mAudioPaused = true;
            mAudioWorker->SetSampleDropping(true);
            return;
        }
        if (tPopupRes->text().compare("Continue stream") == 0)
        {
            mAudioPaused = false;
            mAudioWorker->SetSampleDropping(false);
            return;
        }
        for (tRegisteredAudioSinksIt = tRegisteredAudioSinks.begin(); tRegisteredAudioSinksIt != tRegisteredAudioSinks.end(); tRegisteredAudioSinksIt++)
        {
            if (tPopupRes->text().compare(QString(tRegisteredAudioSinksIt->c_str())) == 0)
            {
                mAudioSource->UnregisterGeneralMediaSink((*tRegisteredAudioSinksIt));
                return;
            }
        }
        for (int i = 0; i < 13; i++)
        {
            if(tPopupRes->text() == QString("%1 %").arg(i * 25))
            {
                SetVolume(i * 25);
                return;
            }
        }
        int tSelectedAudioTrack = 0;
        for (tIt = tInputChannels.begin(); tIt != tInputChannels.end(); tIt++)
        {
            if ((*tIt) == tPopupRes->text().toStdString())
            {
                mAudioWorker->SelectInputChannel(tSelectedAudioTrack);
            }
            tSelectedAudioTrack++;
        }
    }
}

void AudioWidget::StartRecorder()
{
    QString tFileName = OverviewPlaylistWidget::LetUserSelectAudioSaveFile(this, "Set file name for audio recording");

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

    mAudioWorker->StartRecorder(tFileName.toStdString(), tQuality);

    //record source data
    mRecorderStarted = true;
    mLbRecording->setVisible(true);
}

void AudioWidget::StopRecorder()
{
    mAudioWorker->StopRecorder();
    mRecorderStarted = false;
    mLbRecording->setVisible(false);
}

void AudioWidget::DialogAddNetworkSink()
{
    AddNetworkSinkDialog tANSDialog(this, "Configure audio streaming", DATA_TYPE_AUDIO, mAudioSource);

    tANSDialog.exec();
}

void AudioWidget::ShowSample(void* pBuffer, int pSampleSize, int pSampleNumber)
{
    int tMSecs = QTime::currentTime().msec();
    short int tData = 0;
    int tSum = 0, tMax = 1, tMin = -1;
    int tLevel = 0;

    // sample size is given in bytes but we use 16 bit values, furthermore we are using stereo -> division by 4
    int tSampleAmount = pSampleSize / 4;
    //if (pSampleSize != 4096)
        //printf("AudioWidget-SampleSize: %d Sps: %d SampleNumber: %d\n", pSampleSize, pSps, pSampleNumber);

    //#############################################################
    //### find minimum and maximum values
    //#############################################################
    for (int i = 0; i < tSampleAmount; i++)
    {
        tData = *((int16_t*)pBuffer + i * 2);
        tSum += tData;
        if (tData < tMin)
            tMin = tData;
        if (tData > tMax)
            tMax = tData;
        //if (i < 20)
            //printf("%4hd ", tData);
    }

    //#############################################################
    //### scale the values to 100 %
    //#############################################################
    int tScaledMin = 100 * tMin / (-32768);
    int tScaledMax = 100 * tMax / ( 32767);

    // check the range
    if (tScaledMin < 0)
        tScaledMin = 0;
    if (tScaledMin > 100)
        tScaledMin = 100;
    if (tScaledMax < 0)
        tScaledMax = 0;
    if (tScaledMax > 100)
        tScaledMax = 100;

    //#############################################################
    //### set the level of the level bar widget
    //#############################################################
    if (tScaledMin > tScaledMax)
        showAudioLevel(tScaledMin);
    else
        showAudioLevel(tScaledMax);

    //#############################################################
    //### draw statistics
    //#############################################################
    if (mShowLiveStats)
    {
        int tHour = 0, tMin = 0, tSec = 0, tTime = mAudioSource->GetSeekPos();
        tSec = tTime % 60;
        tTime /= 60;
        tMin = tTime % 60;
        tHour = tTime / 60;

        int tMaxHour = 0, tMaxMin = 0, tMaxSec = 0, tMaxTime = mAudioSource->GetSeekEnd();
        tMaxSec = tMaxTime % 60;
        tMaxTime /= 60;
        tMaxMin = tMaxTime % 60;
        tMaxHour = tMaxTime / 60;

        QFont tFont = QFont("Tahoma", 10, QFont::Bold);
        tFont.setFixedPitch(true);
        mLbStreamInfo->setFont(tFont);
        QString tMuxCodecName = QString(mAudioSource->GetMuxingCodec().c_str());
        QString tText = "<font color=red><b>"                                                                                                \
                "Source: " + mAudioWorker->GetCurrentDevice() + "<br>" +                                                            \
/*                "Buffer: " + QString("%1").arg(pSampleNumber) + (mAudioSource->GetChunkDropCounter() ? (" (" + QString("%1").arg(mAudioSource->GetChunkDropCounter()) + " dropped)") : "") + ", "\*/
                "Codec: " + QString((mAudioSource->GetCodecName() != "") ? mAudioSource->GetCodecName().c_str() : "unknown") + " (" + QString("%1").arg(mAudioSource->GetSampleRate()) + "Hz)" + \
/*                                   "Output: " + QString("%1").arg(AUDIO_OUTPUT_SAMPLE_RATE) + " Hz" + "<br>" + \*/
                "";
        if (mAudioSource->SupportsSeeking())
            tText +=    ", Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0');

//        if (mAudioSource->SupportsMuxing())
//            tText +=     "<br>Mux codec: " + ((tMuxCodecName != "") ? tMuxCodecName : "unknown") + (mAudioSource->GetMuxingBufferCounter() ? (" (" + QString("%1").arg(mAudioSource->GetMuxingBufferCounter()) + "/" + QString("%1").arg(mAudioSource->GetMuxingBufferSize()) + " buffered frames)") : "");

        tText +=        "</b></font>";
        mLbStreamInfo->setText(tText);
    }
    if ((mShowLiveStats) && (!mLbStreamInfo->isVisible()))
    {
        mLbStreamInfo->setVisible(true);
    }
    if ((!mShowLiveStats) && (mLbStreamInfo->isVisible()))
        mLbStreamInfo->setVisible(false);

    //#############################################################
    //### draw record icon
    //#############################################################
    if (mAudioSource->IsRecording())
    {
        if (tMSecs % 500 < 250)
        {
            QPixmap tPixmap = QPixmap(":/images/22_22/Audio_Record_active.png");
            mLbRecording->setPixmap(tPixmap);
        }else
        {
            QPixmap tPixmap = QPixmap(":/images/22_22/Audio_Record.png");
            mLbRecording->setPixmap(tPixmap);
        }
    }

    //printf("Sum: %d SampleSize: %d SampleAmount: %d Average: %d Min: %d Max: %d scaledMin: %d scaledMax: %d\n", tSum, pSampleSize, tSampleAmount, (int)tSum/pSampleSize, tMin, tMax, tScaledMin, tScaledMax);
}

void AudioWidget::showAudioLevel(int pLevel)
{
    if (mAudioPaused)
        return;

    mPbLevel1->setValue(100 - pLevel);
    mPbLevel2->setValue(100 - pLevel);
}

int AudioWidget::GetVolume()
{
    return mAudioVolume;
}

void AudioWidget::SetVolume(int pValue)
{
    if (pValue < 0)
        pValue = 0;
    if (pValue > 300)
        pValue = 300;
    LOG(LOG_VERBOSE, "Setting audio volume to %d \%", pValue);
    mLbVolume->setText(QString("%1").arg(pValue, 3) + " %");
	mAudioVolume = pValue;
	mAudioWorker->SetVolume(pValue);
}

void AudioWidget::ToggleVisibility()
{
    if (isVisible())
        SetVisible(false);
    else
        SetVisible(true);
}

void AudioWidget::ToggleMuteState(bool pState)
{
    mAudioWorker->ToggleMuteState(pState);
}

void AudioWidget::InformAboutNewSamples()
{
    QApplication::postEvent(this, new AudioEvent(AUDIO_EVENT_NEW_SAMPLES, ""));
}

void AudioWidget::InformAboutOpenError(QString pSourceName)
{
    QApplication::postEvent(this, new AudioEvent(AUDIO_EVENT_OPEN_ERROR, pSourceName));
}

void AudioWidget::InformAboutNewMuteState()
{
    QApplication::postEvent(this, new AudioEvent(AUDIO_EVENT_NEW_MUTE_STATE, ""));
}

AudioWorkerThread* AudioWidget::GetWorker()
{
    return mAudioWorker;
}

void AudioWidget::SetVisible(bool pVisible)
{
    if (pVisible)
    {
        #ifdef AUDIO_WIDGET_DROP_WHEN_INVISIBLE
            mAudioWorker->SetSampleDropping(false);
        #endif
        move(mWinPos);
        show();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(true);
        mAudioWorker->SetVolume(mAudioVolume);
        if (parentWidget()->isHidden())
            parentWidget()->show();
    }else
    {
        #ifdef AUDIO_WIDGET_DROP_WHEN_INVISIBLE
            mAudioWorker->SetSampleDropping(true);
        #endif
        mWinPos = pos();
        hide();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(false);
        mAudioWorker->SetVolume(0);
    }
}

void AudioWidget::customEvent(QEvent* pEvent)
{
    void* tSample;
    int tSampleSize;

    // make sure we have a user event here
    if (pEvent->type() != QEvent::User)
    {
        pEvent->ignore();
        return;
    }

    AudioEvent *tAudioEvent = (AudioEvent*)pEvent;

    switch(tAudioEvent->GetReason())
    {
        case AUDIO_EVENT_NEW_SAMPLES:
            pEvent->accept();
            if (isVisible())
            {

                mLastSampleNumber = mCurrentSampleNumber;
                mCurrentSampleNumber = mAudioWorker->GetCurrentSample(&tSample, tSampleSize);
                if (mCurrentSampleNumber != -1)
                {
                    ShowSample(tSample, tSampleSize, mCurrentSampleNumber);
                    //printf("VideoWidget-Sample number: %d\n", mCurrentSampleNumber);
                    if ((mLastSampleNumber > mCurrentSampleNumber) && (mCurrentSampleNumber > 32 /* -1 means error, 1 is received after every reset, use "32" because of possible latencies */))
                        LOG(LOG_WARN, "Samples received in wrong order, [%d->%d]", mLastSampleNumber, mCurrentSampleNumber);
                    //if (tlSampleNumber == tSampleNumber)
                        //LOG(LOG_ERROR, "Unnecessary frame grabbing detected");
                }
            }
            break;
        case AUDIO_EVENT_OPEN_ERROR:
            pEvent->accept();
            if (tAudioEvent->GetDescription() != "")
                ShowWarning("Audio source not available", "The selected audio source \"" + tAudioEvent->GetDescription() + "\" is not available. Please, select another one!");
            else
            	ShowWarning("Audio source not available", "The selected audio source auto detection was not successful. Please, connect an additional audio device to your hardware!");
            break;
        case AUDIO_EVENT_NEW_MUTE_STATE:
            pEvent->accept();
            if (mPbMute->isChecked() == mAudioWorker->GetMuteState())
            	mPbMute->click();
            break;
        default:
            break;
    }
    mCustomEventReason = 0;
}

//####################################################################
//###################### WORKER ######################################
//####################################################################
AudioWorkerThread::AudioWorkerThread(MediaSource *pAudioSource, AudioWidget *pAudioWidget):
    MediaSourceGrabberThread(pAudioSource), AudioPlayback()
{
    LOG(LOG_VERBOSE, "..Creating audio worker");
    mStartPlaybackAsap = false;
    mStopPlaybackAsap = false;
    mAudioOutMuted = false;
    mPlaybackAvailable = false;
    mSourceAvailable = false;
    mUserAVDrift = 0;
    mVideoDelayAVDrift = 0;
    if (pAudioSource == NULL)
        LOG(LOG_ERROR, "Audio source is NULL");
    mAudioWidget = pAudioWidget;
    mSampleCurrentIndex = SAMPLE_BUFFER_SIZE - 1;
    mSampleGrabIndex = 0;
    mDropSamples = false;
    mWorkerWithNewData = false;
}

AudioWorkerThread::~AudioWorkerThread()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

void AudioWorkerThread::OpenPlaybackDevice()
{
    LOG(LOG_VERBOSE, "Allocating audio buffers");
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        mSamples[i] = mMediaSource->AllocChunkBuffer(mSamplesBufferSize[i], MEDIA_AUDIO);
        mSampleNumber[i] = 0;
    }

    AudioPlayback::OpenPlaybackDevice();

    mPlaybackAvailable = true;
}

void AudioWorkerThread::ClosePlaybackDevice()
{
    AudioPlayback::ClosePlaybackDevice();

    LOG(LOG_VERBOSE, "Releasing audio buffers");
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
        mMediaSource->FreeChunkBuffer(mSamples[i]);
}

void AudioWorkerThread::ToggleMuteState(bool pState)
{
	if (!mPlaybackAvailable)
	{
		LOG(LOG_VERBOSE, "Playback device isn't available");
		return;
	}

    LOG(LOG_VERBOSE, "Setting mute state to %d", !pState);
    mAudioOutMuted = !pState;
    mAudioWidget->InformAboutNewMuteState();
    if (pState)
    	mStartPlaybackAsap = true;
    else
    	mStopPlaybackAsap = true;
    mGrabbingCondition.wakeAll();
}

void AudioWorkerThread::SetVolume(int pValue)
{
	if (mAudioWidget->GetVolume() != pValue)
	{// call from outside
		mAudioWidget->SetVolume(pValue);
	}else
	{// call from audio widget
		if (!mPlaybackAvailable)
		{
			LOG(LOG_VERBOSE, "Playback device isn't available");
			return;
		}

		if (mWaveOut != NULL)
		    mWaveOut->SetVolume(pValue);
	}
}

int AudioWorkerThread::GetVolume()
{
	return mAudioWidget->GetVolume();
}

void AudioWorkerThread::SetMuteState(bool pMuted)
{
	if (!mPlaybackAvailable)
	{
		LOG(LOG_VERBOSE, "Playback device isn't available");
		return;
	}

    LOG(LOG_VERBOSE, "Setting mute state to %d", pMuted);
    mAudioOutMuted = pMuted;
    mAudioWidget->InformAboutNewMuteState();
    if(pMuted)
        mStopPlaybackAsap = true;
    else
    	mStartPlaybackAsap = true;
    mGrabbingCondition.wakeAll();
}

bool AudioWorkerThread::GetMuteState()
{
    return mAudioOutMuted;
}

bool AudioWorkerThread::IsPlaybackAvailable()
{
    return mPlaybackAvailable;
}

void AudioWorkerThread::SetSampleDropping(bool pDrop)
{
    mDropSamples = pDrop;
}

AudioDevices AudioWorkerThread::GetPossibleDevices()
{
    AudioDevices tResult;

    LOG(LOG_VERBOSE, "Enumerate all audio devices..");
    mMediaSource->getAudioDevices(tResult);

    return tResult;
}

float AudioWorkerThread::GetUserAVDrift()
{
    return mUserAVDrift;
}

void AudioWorkerThread::SetUserAVDrift(float pDrift)
{
    int tDrift = 1000 * pDrift;
    pDrift = tDrift / 1000;
    if (mUserAVDrift != pDrift)
    {
        LOG(LOG_VERBOSE, "Setting user defined A/V drift from %f to %f", mUserAVDrift, pDrift);
        mUserAVDrift = pDrift;
    }
}

float AudioWorkerThread::GetVideoDelayAVDrift()
{
    return mVideoDelayAVDrift;
}

void AudioWorkerThread::SetVideoDelayAVDrift(float pDrift)
{
    int tDrift = 1000 * pDrift;
    pDrift = tDrift / 1000;
    if (mVideoDelayAVDrift != pDrift)
    {
        LOG(LOG_VERBOSE, "Setting video delay A/V drift from %f to %f", mVideoDelayAVDrift, pDrift);
        mVideoDelayAVDrift = pDrift;
    }
}

void AudioWorkerThread::DoPlayNewFile()
{
    LOG(LOG_VERBOSE, "DoPlayNewFile now...");

    AudioDevices tList = GetPossibleDevices();
    AudioDevices::iterator tIt;
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
    	MediaSourceFile *tASource = new MediaSourceFile(mDesiredFile.toStdString());
        if (tASource != NULL)
        {
            AudioDevices tAList;
            tASource->getAudioDevices(tAList);
            mMediaSource->RegisterMediaSource(tASource);
            SetCurrentDevice(mDesiredFile);
        }
    }else{
        LOG(LOG_VERBOSE, "File is already known, we select it as audio media source");
        SetCurrentDevice(mDesiredFile);
    }

    mUserAVDrift = 0;
    mVideoDelayAVDrift = 0;
    mEofReached = false;
    mPlayNewFileAsap = false;
    mPaused = false;
}

void AudioWorkerThread::DoSeek()
{
    MediaSourceGrabberThread::DoSeek();
    ResetPlayback();
}

void AudioWorkerThread::DoSyncClock()
{
    LOG(LOG_VERBOSE, "DoSyncClock now...");

    if (mSyncClockMasterSource != NULL)
    {
        // lock
        mDeliverMutex.lock();

        float tSyncPos = mSyncClockMasterSource->GetSeekPos() - mUserAVDrift - mVideoDelayAVDrift;
        LOG(LOG_VERBOSE, "Synchronizing with media source %s (pos.: %.2f)", mSyncClockMasterSource->GetStreamName().c_str(), tSyncPos);
        mSourceAvailable = mMediaSource->Seek(tSyncPos, false);
        if(!mSourceAvailable)
        {
            LOG(LOG_WARN, "Source isn't available anymore after synch. with %s", mSyncClockMasterSource->GetStreamName().c_str());
        }
        mEofReached = false;
        mSyncClockAsap = false;
        ResetPlayback();
        mSeekAsap = false;

        // unlock
        mDeliverMutex.unlock();
    }else
        LOG(LOG_WARN, "Source of reference clock is invalid");
}

void AudioWorkerThread::DoResetMediaSource()
{
    MediaSourceGrabberThread::DoResetMediaSource();
    ResetPlayback();
}

void AudioWorkerThread::DoSetCurrentDevice()
{
    LOG(LOG_VERBOSE, "DoSetCurrentDevice now...");
    // lock
    mDeliverMutex.lock();

    bool tNewSourceSelected = false;

    if ((mSourceAvailable = mMediaSource->SelectDevice(mDeviceName.toStdString(), MEDIA_AUDIO, tNewSourceSelected)))
    {
        LOG(LOG_VERBOSE, "We opened a new source: %d", tNewSourceSelected);

        bool tHadAlreadyInputData = false;
        for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
        {
            if (mSampleNumber[i] > 0)
            {
                tHadAlreadyInputData = true;
                break;
            }
        }
        if (!tHadAlreadyInputData)
        {
            LOG(LOG_VERBOSE, "Haven't found any input data, will force a reset of audio source");
            mMediaSource->CloseGrabDevice();
            mSourceAvailable = mMediaSource->OpenAudioGrabDevice();
            if (!mSourceAvailable)
            {
                LOG(LOG_WARN, "Audio source is (temporary) not available after hard reset of media source in DoSetCurrentDevice()");
            }
        }else
        {
            // seek to the beginning if we have reselected the source file
            if (!tNewSourceSelected)
            {
                if (mMediaSource->GetCurrentDeviceName() == mDeviceName.toStdString())
                { // do we have what we required?
                    if (mMediaSource->SupportsSeeking())
                    {
                        // seek to the beginning if we have reselected the source file
                        LOG(LOG_VERBOSE, "Seeking to the beginning of the source file");
                        mMediaSource->Seek(0);
                        mSeekAsap = false;
                    }
                    if (mResetMediaSourceAsap)
                    {
                        LOG(LOG_VERBOSE, "Haven't selected new audio source, reset of current source forced");
                        mSourceAvailable = mMediaSource->Reset(MEDIA_AUDIO);
                        if (!mSourceAvailable)
                        {
                            LOG(LOG_WARN, "Video source is (temporary) not available after Reset() in DoSetCurrentDevice()");
                        }
                    }
                }else
                    mAudioWidget->InformAboutOpenError(mDeviceName);
            }
        }
        // we had an source reset in every case because "SelectDevice" does this if old source was already opened
        mResetMediaSourceAsap = false;
        mPaused = false;
        //mAudioWidget->InformAboutNewSource();
    }else
    {
        if (!mSourceAvailable)
            LOG(LOG_WARN, "Audio source is (temporary) not available after SelectDevice() in DoSetCurrentDevice()");
        if (!mTryingToOpenAFile)
            mAudioWidget->InformAboutOpenError(mDeviceName);
        else
            LOG(LOG_VERBOSE, "Couldn't open audio file source %s", mDeviceName.toStdString().c_str());
    }

    // reset audio output
    ResetPlayback();

    mSetCurrentDeviceAsap = false;
    mCurrentFile = mDesiredFile;

    // unlock
    mDeliverMutex.unlock();
}

void AudioWorkerThread::ResetPlayback()
{
    bool tAudioWasMuted = mAudioOutMuted;

    if (!tAudioWasMuted)
    {
        DoStopPlayback();
        DoStartPlayback();
    }
}

void AudioWorkerThread::DoStartPlayback()
{
    // okay don't have to wait, time to start playback
    LOG(LOG_VERBOSE, "DoStartPlayback now...(playing: %d)", (mWaveOut != NULL) ? mWaveOut->IsPlaying() : false);
    mStartPlaybackAsap = false;
    if (mPlaybackAvailable)
    {
        if (mWaveOut != NULL)
            mWaveOut->Play();
    }
    mAudioOutMuted = false;
}

void AudioWorkerThread::DoStopPlayback()
{
    LOG(LOG_VERBOSE, "DoStopPlayback now...");
	mStopPlaybackAsap = false;
	if (mPlaybackAvailable)
	{
	    if (mWaveOut != NULL)
	    {
	        LOG(LOG_VERBOSE, "..triggering playback stop");
	        mWaveOut->Stop();
	        LOG(LOG_VERBOSE, "..playback stopped");
	    }
	}else
		LOG(LOG_VERBOSE, "Playback can't be stopped because it is not available");

	mAudioOutMuted = true;
	LOG(LOG_VERBOSE, "Playback is stopped");
}

int AudioWorkerThread::GetCurrentSample(void **pSample, int& pSampleSize, int *pSps)
{
    int tResult = -1;

    // lock
    if (!mDeliverMutex.tryLock(100))
        return -1;

    if ((mWorkerWithNewData) && (!mResetMediaSourceAsap))
    {
        mSampleCurrentIndex = SAMPLE_BUFFER_SIZE - mSampleCurrentIndex - mSampleGrabIndex;
        mWorkerWithNewData = false;
        if (pSps != NULL)
            *pSps = mResultingSps;
        *pSample = mSamples[mSampleCurrentIndex];
        pSampleSize = mSamplesSize[mSampleCurrentIndex];
        tResult = mSampleNumber[mSampleCurrentIndex];
    }

    // unlock
    mDeliverMutex.unlock();

    //printf("GUI-GetSample -> %d\n", mFrameCurrentIndex);

    return tResult;
}

void AudioWorkerThread::run()
{
    bool  tGrabSuccess;
    int tSamplesSize;
    int tSampleNumber = -1, tLastSampleNumber = -1;

    // if grabber was stopped before source has been opened this BOOL is reset
    mWorkerNeeded = true;

    // open audio playback
    LOG(LOG_VERBOSE, "..open playback device");
    OpenPlaybackDevice();

    // assign default thread name
    LOG(LOG_VERBOSE, "..assign thread name");
    SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber()");

    // start the audio source
    mCodec = CONF.GetAudioCodec();
    if(mMediaSource == NULL)
    {
        LOG(LOG_ERROR, "Invalid audio source");
    }
    LOG(LOG_VERBOSE, "..open audio grab device");
    if(!(mSourceAvailable = mMediaSource->OpenAudioGrabDevice()))
    {
    	LOG(LOG_WARN, "Couldn't open audio grabbing device \"%s\"", mMediaSource->GetCurrentDeviceName().c_str());
    	mAudioWidget->InformAboutOpenError(QString(mMediaSource->GetCurrentDeviceName().c_str()));
    }

    LOG(LOG_VERBOSE, "..start main loop");
    while(mWorkerNeeded)
    {
        // get the next frame from audio source
        tLastSampleNumber = tSampleNumber;

        if (mSyncClockAsap)
            DoSyncClock();

        if (mSeekAsap)
            DoSeek();

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

        // reset audio source
        if (mResetMediaSourceAsap)
            DoResetMediaSource();

        // start video recording
        if (mStartRecorderAsap)
            DoStartRecorder();

        // stop video recording
        if (mStopRecorderAsap)
            DoStopRecorder();

        // stop playback
        if (mStopPlaybackAsap)
        	DoStopPlayback();

        // start playback
        if (mStartPlaybackAsap)
        	DoStartPlayback();

        mGrabbingStateMutex.lock();

        if ((!mPaused) && (mSourceAvailable))
        {
            mGrabbingStateMutex.unlock();

            // set input samples size
			tSamplesSize = mSamplesBufferSize[mSampleGrabIndex];

			// get new samples from audio grabber
			tSampleNumber = mMediaSource->GrabChunk(mSamples[mSampleGrabIndex], tSamplesSize, mDropSamples);
            mSamplesSize[mSampleGrabIndex] = tSamplesSize;
			mEofReached = (tSampleNumber == GRAB_RES_EOF);
            if (mEofReached)
            {// EOF
                mSourceAvailable = false;
                LOG(LOG_WARN, "Got EOF signal from audio source, marking audio source as unavailable");
            }

            #ifdef AUDIO_WIDGET_DEBUG_FRAMES
                LOG(LOG_WARN, "Got from media source the sample block %d with size of %d bytes and stored it as index %d", tSampleNumber, tSamplesSize, mSampleGrabIndex);
            #endif

			//printf("SampleSize: %d Sample: %d\n", mSamplesSize[mSampleGrabIndex], tSampleNumber);

			// play the sample block if audio out isn't currently muted
			if ((!mAudioOutMuted) && (tSampleNumber >= 0) && (tSamplesSize > 0) && (!mDropSamples) && (mPlaybackAvailable))
			{
			    if (mWaveOut != NULL)
			    {
                    #ifdef DEBUG_AUDIOWIDGET_PLAYBACK
			            LOG(LOG_VERBOSE, "Writing buffer at index %d and size of %d bytes to audio output FIFO", mSampleGrabIndex, tSamplesSize);
			            LOG(LOG_VERBOSE, "Writing buffer at %p with size of %d bytes to audio output FIFO", mSamples[mSampleGrabIndex], tSamplesSize);
			        #endif
			        mWaveOut->WriteChunk(mSamples[mSampleGrabIndex], tSamplesSize);
			        mWaveOut->LimitQueue(AUDIO_MAX_PLAYBACK_QUEUE);
			    }
			}else
			{
				#ifdef DEBUG_AUDIOWIDGET_PERFORMANCE
				    LOG(LOG_VERBOSE, "Ignoring audio buffer, mute state: %d, drop state: %d, device available: %d, sample nr.: %d, sample size: %d", mAudioOutMuted, mDropSamples, mPlaybackAvailable, tSampleNumber, tSamplesSize);
			    #endif
			}

			if(!mDropSamples)
			{
                if ((tSampleNumber >= 0) && (mSamplesSize[mSampleGrabIndex] > 0))
                {
                    // lock
                    mDeliverMutex.lock();

                    mSampleNumber[mSampleGrabIndex] = tSampleNumber;

                    mWorkerWithNewData = true;

                    mSampleGrabIndex = SAMPLE_BUFFER_SIZE - mSampleCurrentIndex - mSampleGrabIndex;

                    // unlock
                    mDeliverMutex.unlock();

                    mAudioWidget->InformAboutNewSamples();

                    if ((tLastSampleNumber > tSampleNumber) && (tSampleNumber > 9 /* -1 means error, 1 is received after every reset, use "9" because of possible latencies */))
                        LOG(LOG_ERROR, "Sample ordering problem detected");
                }else
                {
                    LOG(LOG_VERBOSE, "Invalid grabbing result: %d, current sample size: %d", tSampleNumber, mSamplesSize[mSampleGrabIndex]);
                    usleep(100 * 1000); // check for new frames every 1/10 seconds
                }
            }
        }else
        {
        	if (mSourceAvailable)
        		LOG(LOG_VERBOSE, "AudioWorkerThread is in pause state");
        	else
        		LOG(LOG_VERBOSE, "AudioWorkerThread waits for available grabbing device");

            mGrabbingCondition.wait(&mGrabbingStateMutex);
            mGrabbingStateMutex.unlock();

            LOG(LOG_VERBOSE, "Continuing processing");
        }

    }
    mMediaSource->CloseGrabDevice();
    mMediaSource->DeleteAllRegisteredMediaSinks();

    // close audio playback
    ClosePlaybackDevice();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
