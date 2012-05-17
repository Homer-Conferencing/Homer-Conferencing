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

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;
using namespace Homer::Multimedia;
using namespace Homer::Monitor;

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
    parentWidget()->hide();
}

void AudioWidget::Init(MediaSource *pAudioSource, QMenu *pMenu, QString pActionTitle, QString pWidgetTitle, bool pVisible, bool pMuted)
{
    mAudioSource = pAudioSource;
    mAudioTitle = pActionTitle;
    LOG(LOG_VERBOSE, "Creating audio widget");

    //####################################################################
    //### create the remaining necessary menu item
    //####################################################################

    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
            setAutoFillBackground(true);
            break;
        default:
            break;
    }

    if (pMenu != NULL)
    {
        mAssignedAction = pMenu->addAction(pActionTitle);
        mAssignedAction->setCheckable(true);
        mAssignedAction->setChecked(pVisible);
        QIcon tIcon;
        tIcon.addPixmap(QPixmap(":/images/Checked.png"), QIcon::Normal, QIcon::On);
        tIcon.addPixmap(QPixmap(":/images/Unchecked.png"), QIcon::Normal, QIcon::Off);
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
    if (mAudioWorker != NULL)
    {
        mAudioWorker->StopGrabber();
        if (!mAudioWorker->wait(250))
        {
            LOG(LOG_VERBOSE, "Going to force termination of worker thread");
            mAudioWorker->terminate();
        }

        if (!mAudioWorker->wait(5000))
        {
            LOG(LOG_ERROR, "Termination of AudioWorker-Thread timed out");
        }
        delete mAudioWorker;
    }
    if (mAssignedAction != NULL)
        delete mAssignedAction;
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
    list<string>::iterator tIt;
    QAction *tAction;
    list<string> tRegisteredAudioSinks = mAudioSource->ListRegisteredMediaSinks();
    list<string>::iterator tRegisteredAudioSinksIt;

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
			tIcon5.addPixmap(QPixmap(":/images/Audio - Stop.png"), QIcon::Normal, QIcon::Off);
		}else
		{
			tAction = tMenu.addAction("Record audio");
			tIcon5.addPixmap(QPixmap(":/images/Audio - Record.png"), QIcon::Normal, QIcon::Off);
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
    tIcon4.addPixmap(QPixmap(":/images/Info.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon4);
    tAction->setCheckable(true);
    tAction->setChecked(mShowLiveStats);

    //###############################################################################
    //### TRACKS
    //###############################################################################
    list<string> tInputChannels = mAudioSource->GetInputChannels();
    if (mAudioSource->SupportsMultipleInputChannels())
    {
        QMenu *tStreamMenu = tMenu.addMenu("Audio streams");

        QIcon tIcon14;
        tIcon14.addPixmap(QPixmap(":/images/Audio - Play.png"), QIcon::Normal, QIcon::Off);

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
        tIcon13.addPixmap(QPixmap(":/images/Speaker.png"), QIcon::Normal, QIcon::Off);
        tStreamMenu->setIcon(tIcon13);
    }

    //###############################################################################
    //### AUDIO SETTINGS
    //###############################################################################
    QIcon tIcon3;
    tIcon3.addPixmap(QPixmap(":/images/SpeakerLoud.png"), QIcon::Normal, QIcon::Off);
    QMenu *tAudioMenu = tMenu.addMenu("Audio settings");
            //###############################################################################
            //### VOLUMES
            //###############################################################################
            QMenu *tVolMenu = tAudioMenu->addMenu("Volume");

            for (int i = 1; i <13; i++)
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
        tIcon7.addPixmap(QPixmap(":/images/ArrowRightGreen.png"), QIcon::Normal, QIcon::Off);
        tVideoSinksMenu->setIcon(tIcon7);

        tAction =  tVideoSinksMenu->addAction("Add network sink");
        QIcon tIcon8;
        tIcon8.addPixmap(QPixmap(":/images/Plus.png"), QIcon::Normal, QIcon::Off);
        tAction->setIcon(tIcon8);

        QMenu *tRegisteredVideoSinksMenu = tVideoSinksMenu->addMenu("Registered sinks");
        QIcon tIcon9;
        tIcon9.addPixmap(QPixmap(":/images/ArrowRightGreen.png"), QIcon::Normal, QIcon::Off);
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
            tIcon10.addPixmap(QPixmap(":/images/Audio - Play.png"), QIcon::Normal, QIcon::Off);
        }else
        {
            tAction = tMenu.addAction("Drop stream");
            tIcon10.addPixmap(QPixmap(":/images/Audio - Pause.png"), QIcon::Normal, QIcon::Off);
        }
        tAction->setIcon(tIcon10);
    }

    tMenu.addSeparator();

    //###############################################################################
    //### RESET SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Reset source");
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/Reload.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);

    //###############################################################################
    //### CLOSE SOURCE
    //###############################################################################
    tAction = tMenu.addAction("Close audio");
    QIcon tIcon1;
    tIcon1.addPixmap(QPixmap(":/images/Close.png"), QIcon::Normal, QIcon::Off);
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
        for (int i = 1; i < 13; i++)
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
    AddNetworkSinkDialog tANSDialog(this, mAudioSource);

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
    if ((mShowLiveStats) && (!mLbStreamInfo->isVisible()))
        mLbStreamInfo->setVisible(true);
    if ((!mShowLiveStats) && (mLbStreamInfo->isVisible()))
        mLbStreamInfo->setVisible(false);

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
        if (mAudioSource->SupportsSeeking())
            mLbStreamInfo->setText("<font color=red><b>"                                                                                                \
                                   /*"Source: " + mAudioWorker->GetCurrentDevice() + "<br>" +                                                            \*/
                                   "Buffer: " + QString("%1").arg(pSampleNumber) + (mAudioSource->GetChunkDropCounter() ? (" (" + QString("%1").arg(mAudioSource->GetChunkDropCounter()) + " dropped)") : "") + "<br>" + \
                                   "Codec: " + QString((mAudioSource->GetCodecName() != "") ? mAudioSource->GetCodecName().c_str() : "unknown") + " (" + QString("%1").arg(mAudioSource->GetSampleRate()) + "Hz)<br>" + \
                                   "Output: " + QString("%1").arg(AUDIO_OUTPUT_SAMPLE_RATE) + " Hz" + "<br>" + \
                                   "Time: " + QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "/" + QString("%1:%2:%3").arg(tMaxHour, 2, 10, (QLatin1Char)'0').arg(tMaxMin, 2, 10, (QLatin1Char)'0').arg(tMaxSec, 2, 10, (QLatin1Char)'0') + \
                                   "</b></font>");
        else
            mLbStreamInfo->setText("<font color=red><b>"                                                                                                \
                                   "Source: " + mAudioWorker->GetCurrentDevice() + "<br>" +                                                            \
                                   "Buffer: " + QString("%1").arg(pSampleNumber) + (mAudioSource->GetChunkDropCounter() ? (" (" + QString("%1").arg(mAudioSource->GetChunkDropCounter()) + " dropped)") : "") + "<br>" + \
                                   "Codec: " + QString((mAudioSource->GetCodecName() != "") ? mAudioSource->GetCodecName().c_str() : "unknown") + " (" + QString("%1").arg(mAudioSource->GetSampleRate()) + "Hz)<br>" + \
                                   "Output: " + QString("%1").arg(AUDIO_OUTPUT_SAMPLE_RATE) + " Hz" + "<br>" + \
                                   "</b></font>");
    }

    //#############################################################
    //### draw record icon
    //#############################################################
    if (mAudioSource->IsRecording())
    {
        if (tMSecs % 500 < 250)
        {
            QPixmap tPixmap = QPixmap(":/images/Audio - Record.png");
            mLbRecording->setPixmap(tPixmap);
        }else
        {
            mLbRecording->setPixmap(QPixmap());
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
    if (pValue < 25)
        pValue = 25;
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
        parentWidget()->show();
        if (mAssignedAction != NULL)
            mAssignedAction->setChecked(true);
        mAudioWorker->SetVolume(mAudioVolume);
    }else
    {
        #ifdef AUDIO_WIDGET_DROP_WHEN_INVISIBLE
            mAudioWorker->SetSampleDropping(true);
        #endif
        mWinPos = pos();
        hide();
        parentWidget()->hide();
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
//###################### BUFFER ######################################
//####################################################################
AudioBuffer::AudioBuffer(QObject * pParent):
    QBuffer(pParent)
{
    if (!open(QIODevice::ReadWrite))
        LOG(LOG_ERROR, "Unable to open audio buffer");
    seek(0);
    LOG(LOG_VERBOSE, "Audio buffer is sequential: %d", isSequential());
}
AudioBuffer::~AudioBuffer()
{
    close();
}

qint64 AudioBuffer::readData(char * pData, qint64 pLen)
{
    qint64 tResult = -1;
    //printf("Got a call to read %d bytes at %p from audio buffer\n", (int)pLen, pData);

    mMutex.lock();
    qint64 tPos = pos();
    seek(0);
    tResult =  QBuffer::readData(pData, pLen);
    seek(tPos);

    if(tResult > 0)
    {
        close();
        setData(buffer().right(size() - tResult));
        if (!open(QIODevice::ReadWrite))
            LOG(LOG_ERROR, "Unable to open audio buffer");

        printf("READ(%d): ", (int)tResult);
        for (int i = 0; i < 92; i++)
            printf("%hx ", 0xFF & pData[i]);
        printf("\n");
    }
    mMutex.unlock();

    return tResult;
}

qint64 AudioBuffer::writeData(const char * pData, qint64 pLen)
{
    qint64 tResult = 0;
    //printf("Got a call to write data to audio buffer\n");
    mMutex.lock();
    tResult =  QBuffer::writeData(pData, pLen);
    printf("WRITE(%d to %d => %d): ", (int)tResult, (int)pos(), (int)size());
    for (int i = 0; i < 92; i++)
        printf("%hx ", 0xFF & pData[i]);
    printf("\n");
    //LOG(LOG_VERBOSE, "New pos: %d", pos());
    if (tResult == -1)
        LOG(LOG_ERROR, "Error in writeData: %s", errorString().toStdString().c_str());
    mMutex.unlock();
    return tResult;
}


//####################################################################
//###################### WORKER ######################################
//####################################################################
AudioWorkerThread::AudioWorkerThread(MediaSource *pAudioSource, AudioWidget *pAudioWidget):
    QThread()
{
    LOG(LOG_VERBOSE, "..Creating audio worker");
    mResetAudioSourceAsap = false;
    mStartRecorderAsap = false;
    mStartPlaybackAsap = false;
    mStopPlaybackAsap = false;
    mSetInputStreamPreferencesAsap = false;
    mStopRecorderAsap = false;
    mSetCurrentDeviceAsap = false;
    mAudioOutMuted = false;
    mPlayNewFileAsap = false;
    mSelectInputChannelAsap = false;
    mDesiredInputChannel = 0;
    mPlaybackAvailable = false;
    mEofReached = false;
    mSourceSeekAsap = false;
    mPaused = false;
    mSourceAvailable = false;
    mPausedPos = 0;
    mAudioPlaybackDelayCount = 0;
    mDesiredFile = "";
    mResX = 352;
    mResY = 288;
    mWaveOut = NULL;
    if (pAudioSource == NULL)
        LOG(LOG_ERROR, "Audio source is NULL");
    mAudioSource = pAudioSource;
    mAudioWidget = pAudioWidget;
    blockSignals(true);
    mSampleCurrentIndex = SAMPLE_BUFFER_SIZE - 1;
    mSampleGrabIndex = 0;
    mDropSamples = false;
    mWorkerWithNewData = false;
}

AudioWorkerThread::~AudioWorkerThread()
{
}

void AudioWorkerThread::OpenPlaybackDevice()
{
    LOG(LOG_VERBOSE, "Allocating audio buffers");
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
    {
        mSamples[i] = mAudioSource->AllocChunkBuffer(mSamplesBufferSize[i], MEDIA_AUDIO);
        mSampleNumber[i] = 0;
    }

    LOG(LOG_VERBOSE, "Going to open playback device");

	#ifndef APPLE
    	mWaveOut = new WaveOutPortAudio(CONF.GetLocalAudioSink().toStdString());
	#else
    	mWaveOut = new WaveOutSdl(CONF.GetLocalAudioSink().toStdString());
	#endif
	mWaveOut->OpenWaveOutDevice();
    mPlaybackAvailable = true;
    LOG(LOG_VERBOSE, "Finished to open playback device");
}

void AudioWorkerThread::ClosePlaybackDevice()
{
    LOG(LOG_VERBOSE, "Going to close playback device");

    mPlaybackAvailable = false;

    // close the audio out
    mWaveOut->CloseWaveOutDevice();
    delete mWaveOut;

    LOG(LOG_VERBOSE, "Releasing audio buffers");
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++)
        mAudioSource->FreeChunkBuffer(mSamples[i]);

    LOG(LOG_VERBOSE, "Finished to close playback device");
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
}

void AudioWorkerThread::SetVolume(int pValue)
{
	if (!mPlaybackAvailable)
	{
		LOG(LOG_VERBOSE, "Playback device isn't available");
		return;
	}

	mWaveOut->SetVolume(pValue);
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

void AudioWorkerThread::ResetSource()
{
    mResetAudioSourceAsap = true;
}

void AudioWorkerThread::SetInputStreamPreferences(QString pCodec)
{
    mCodec = pCodec;
    mSetInputStreamPreferencesAsap = true;
}

void AudioWorkerThread::SetStreamName(QString pName)
{
    mAudioSource->AssignStreamName(pName.toStdString());
}

QString AudioWorkerThread::GetStreamName()
{
    return QString(mAudioSource->GetMediaSource()->GetStreamName().c_str());
}

QString AudioWorkerThread::GetCurrentDevice()
{
    return QString(mAudioSource->GetCurrentDeviceName().c_str());
}

void AudioWorkerThread::SetCurrentDevice(QString pName)
{
    if ((pName != "auto") && (pName != "") && (pName != "auto") && (pName != "automatic"))
    {
        mDeviceName = pName;
        mSetCurrentDeviceAsap = true;
    }
}

AudioDevicesList AudioWorkerThread::GetPossibleDevices()
{
    AudioDevicesList tResult;

    LOG(LOG_VERBOSE, "Enumerate all audio devices..");
    mAudioSource->getAudioDevices(tResult);

    return tResult;
}

QString AudioWorkerThread::GetDeviceDescription(QString pName)
{
    AudioDevicesList::iterator tIt;
    AudioDevicesList tAList;

    mAudioSource->getAudioDevices(tAList);
    for (tIt = tAList.begin(); tIt != tAList.end(); tIt++)
        if (pName.toStdString() == tIt->Name)
            return QString(tIt->Desc.c_str());

    return "";
}

void AudioWorkerThread::PlayFile(QString pName)
{
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
		LOG(LOG_VERBOSE, "Continue playback of file: %s", pName.toStdString().c_str());
		mAudioSource->Seek(mPausedPos, false);
		mPaused = false;
	}else
	{
		LOG(LOG_VERBOSE, "Trigger playback of file: %s", pName.toStdString().c_str());
		mDesiredFile = pName;
		mPlayNewFileAsap = true;
	}
}

void AudioWorkerThread::PauseFile()
{
    if (mAudioSource->SupportsSeeking())
    {
        LOG(LOG_VERBOSE, "Trigger pause state");
        mPausedPos = mAudioSource->GetSeekPos();
        mPaused = true;
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, PauseFile() aborted");
}

void AudioWorkerThread::StopFile()
{
    if (mAudioSource->SupportsSeeking())
    {
        LOG(LOG_VERBOSE, "Trigger stop state");
        mPausedPos = 0;
        mPaused = true;
    }else
        LOG(LOG_VERBOSE, "Seeking not supported, StopFile() aborted");
}

bool AudioWorkerThread::EofReached()
{
//	LOG(LOG_VERBOSE, "EOF-Calculation..");
//	LOG(LOG_VERBOSE, "EOF: %d", mEofReached);
//	LOG(LOG_VERBOSE, "mResetAudioSourceAsap: %d", mResetAudioSourceAsap);
//	LOG(LOG_VERBOSE, "mPlayNewFileAsap: %d", mPlayNewFileAsap);
//	LOG(LOG_VERBOSE, "mSetCurrentDeviceAsap: %d", mSetCurrentDeviceAsap);
	return (((mEofReached) && (!mResetAudioSourceAsap) && (!mPlayNewFileAsap)) || (mPlayNewFileAsap) || (mSetCurrentDeviceAsap));
}

QString AudioWorkerThread::CurrentFile()
{
    if (mAudioSource->SupportsSeeking())
        return mCurrentFile;
    else
        return "";
}

void AudioWorkerThread::SelectInputChannel(int pIndex)
{
    mDesiredInputChannel = pIndex;
    mSelectInputChannelAsap = true;
}

bool AudioWorkerThread::SupportsSeeking()
{
    return mAudioSource->SupportsSeeking();
}

void AudioWorkerThread::Seek(int pPos)
{
    mSourceSeekAsap = true;
    mSourceSeekPos = pPos;
}

int64_t AudioWorkerThread::GetSeekPos()
{
    return mAudioSource->GetSeekPos();
}

int64_t AudioWorkerThread::GetSeekEnd()
{
    return mAudioSource->GetSeekEnd();
}

void AudioWorkerThread::StartRecorder(std::string pSaveFileName, int pQuality)
{
    mSaveFileName = pSaveFileName;
    mSaveFileQuality = pQuality;
    mStartRecorderAsap = true;
}

void AudioWorkerThread::StopRecorder()
{
    mStopRecorderAsap = true;
}

void AudioWorkerThread::DoStartRecorder()
{
    mAudioSource->StartRecording(mSaveFileName, mSaveFileQuality);
    mStartRecorderAsap = false;
}

void AudioWorkerThread::DoStopRecorder()
{
    mAudioSource->StopRecording();
    mStopRecorderAsap = false;
}

void AudioWorkerThread::DoSourceSeek()
{
    LOG(LOG_VERBOSE, "DoSourceSeek now...");
    mAudioSource->Seek(mAudioSource->GetSeekEnd() * mSourceSeekPos / 1000, false);
    ResetPlayback();
    mSourceSeekAsap = false;
}
void AudioWorkerThread::DoPlayNewFile()
{
    LOG(LOG_VERBOSE, "DoPlayNewFile now...");

    AudioDevicesList tList = GetPossibleDevices();
    AudioDevicesList::iterator tIt;
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
            AudioDevicesList tAList;
            tASource->getAudioDevices(tAList);
            mAudioSource->RegisterMediaSource(tASource);
            SetCurrentDevice(mDesiredFile);
        }
    }else{
        LOG(LOG_VERBOSE, "File is already known, we select it as audio media source");
        SetCurrentDevice(mDesiredFile);
    }

    mPlayNewFileAsap = false;
	mPaused = false;
}

void AudioWorkerThread::DoSelectInputChannel()
{
    LOG(LOG_VERBOSE, "AudioWorkerThread-DoSelectInputChannel now...");
    // lock
    mDeliverMutex.lock();

    // restart frame grabbing device
    mSourceAvailable = mAudioSource->SelectInputChannel(mDesiredInputChannel);
    ResetPlayback();

    mResetAudioSourceAsap = false;
    mSelectInputChannelAsap = false;
    mPaused = false;

    // unlock
    mDeliverMutex.unlock();
}

void AudioWorkerThread::DoResetAudioSource()
{
    LOG(LOG_VERBOSE, "AudioWorkerThread-DoResetAudioSource now...");
    // lock
    mDeliverMutex.lock();

    // restart frame grabbing device
    mSourceAvailable = mAudioSource->Reset(MEDIA_AUDIO);
    ResetPlayback();

    mResetAudioSourceAsap = false;
    mPaused = false;

    // unlock
    mDeliverMutex.unlock();
}

void AudioWorkerThread::DoSetInputStreamPreferences()
{
    LOG(LOG_VERBOSE, "DoSetInputStreamPreferences now...");
    // lock
    mDeliverMutex.lock();

    if (mAudioSource->SetInputStreamPreferences(mCodec.toStdString()))
    {
    	mSourceAvailable = mAudioSource->Reset(MEDIA_AUDIO);
        mResetAudioSourceAsap = false;
    }

    mSetInputStreamPreferencesAsap = false;

    // unlock
    mDeliverMutex.unlock();
}

void AudioWorkerThread::DoSetCurrentDevice()
{
    LOG(LOG_VERBOSE, "DoSetCurrentDevice now...");
    // lock
    mDeliverMutex.lock();

    bool tNewSourceSelected = false;

    if ((mSourceAvailable = mAudioSource->SelectDevice(mDeviceName.toStdString(), MEDIA_AUDIO, tNewSourceSelected)))
    {
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
            mSourceAvailable = mAudioSource->Reset(MEDIA_AUDIO);
        }else
        {
            // seek to the beginning if we have reselected the source file
            if (!tNewSourceSelected)
            {
                LOG(LOG_VERBOSE, "Seeking to the beginning of the source file");
                mAudioSource->Seek(0);
            }

            if ((!tNewSourceSelected) && (mResetAudioSourceAsap))
            {
                LOG(LOG_VERBOSE, "Haven't selected new media source, reset of current media source forced");
                mSourceAvailable = mAudioSource->Reset(MEDIA_AUDIO);
            }
        }
        // we had an source reset in every case because "SelectDevice" does this if old source was already opened
        mResetAudioSourceAsap = false;
        mPaused = false;
    }else
        mAudioWidget->InformAboutOpenError(mDeviceName);

    // reset audio output
    ResetPlayback();

    mSetCurrentDeviceAsap = false;
    mCurrentFile = mDesiredFile;

    // unlock
    mDeliverMutex.unlock();
}

void AudioWorkerThread::ResetPlayback()
{
    DoStopPlayback();
    if (!mAudioOutMuted)
        DoStartPlayback();
}

void AudioWorkerThread::DoStartPlayback()
{
    // are we already waiting for some initial audio buffers?
    if (mAudioPlaybackDelayCount > 1)
    {
        LOG(LOG_VERBOSE, "Waiting for %d initial audio buffers", mAudioPlaybackDelayCount);
        return;
    }

    // if audio was muted we have to wait for an initial time
    if ((!mWaveOut->IsPlaying()) && (mAudioPlaybackDelayCount == 0))
    {
        mAudioPlaybackDelayCount = AUDIO_INITIAL_MINIMUM_PLAYBACK_QUEUE;
        mStartPlaybackAsap = true;
        return;
    }

    // okay don't have to wait, time to start playback
    LOG(LOG_VERBOSE, "DoStartPlayback now...(playing: %d, delay count: %d)", mWaveOut->IsPlaying(), mAudioPlaybackDelayCount);
    mStartPlaybackAsap = false;
    if (mPlaybackAvailable)
        mWaveOut->Play();
    mAudioPlaybackDelayCount = 0;
    mAudioOutMuted = false;
}

void AudioWorkerThread::DoStopPlayback()
{
    LOG(LOG_VERBOSE, "DoStopPlayback now...");
	mStopPlaybackAsap = false;
	if (mPlaybackAvailable)
		mWaveOut->Stop();
	mAudioOutMuted = true;
}

int AudioWorkerThread::GetCurrentSample(void **pSample, int& pSampleSize, int *pSps)
{
    int tResult = -1;

    // lock
    if (!mDeliverMutex.tryLock(100))
        return -1;

    if ((mWorkerWithNewData) && (!mResetAudioSourceAsap))
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
    if(mAudioSource == NULL)
    {
        LOG(LOG_ERROR, "Invalid audio source");
    }
    LOG(LOG_VERBOSE, "..open audio grab device");
    if(!(mSourceAvailable = mAudioSource->OpenAudioGrabDevice()))
    {
    	LOG(LOG_WARN, "Couldn't open audio grabbing device \"%s\"", mAudioSource->GetCurrentDeviceName().c_str());
    	mAudioWidget->InformAboutOpenError(QString(mAudioSource->GetCurrentDeviceName().c_str()));
    }

    LOG(LOG_VERBOSE, "..start main loop");
    while(mWorkerNeeded)
    {
        // get the next frame from audio source
        tLastSampleNumber = tSampleNumber;

        // lock
        //LOG(LOG_ERROR, "GrabMutex");
        mGrabMutex.lock();

        if (mSourceSeekAsap)
            DoSourceSeek();

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
        if (mResetAudioSourceAsap)
            DoResetAudioSource();

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

        if ((!mPaused) && (mSourceAvailable))
        {
			// set input samples size
			tSamplesSize = mSamplesBufferSize[mSampleGrabIndex];

			// get new samples from audio grabber
			tSampleNumber = mAudioSource->GrabChunk(mSamples[mSampleGrabIndex], tSamplesSize, mDropSamples);
            mSamplesSize[mSampleGrabIndex] = tSamplesSize;
			mEofReached = (tSampleNumber == GRAB_RES_EOF);
            if (mEofReached)
                mSourceAvailable = false;

			//printf("SampleSize: %d Sample: %d\n", mSamplesSize[mSampleGrabIndex], tSampleNumber);

			// play the sample block if audio out isn't currently muted
			if ((!mAudioOutMuted) && (tSampleNumber >= 0) && (tSamplesSize > 0) && (!mDropSamples) && (mPlaybackAvailable))
			{
			    mWaveOut->WriteChunk(mSamples[mSampleGrabIndex], mSamplesSize[mSampleGrabIndex]);
			    if(mAudioPlaybackDelayCount)
			        mAudioPlaybackDelayCount--;
			}else
			{
				#ifdef DEBUG_AUDIOWIDGET_PERFORMANCE
				    LOG(LOG_VERBOSE, "Ignoring audio buffer, mute state: %d, drop state: %d, device available: %d, sample nr.: %d, sample size: %d", mAudioOutMuted, mDropSamples, mPlaybackAvailable, tSampleNumber, tSamplesSize);
			    #endif
			}

			// unlock
			mGrabMutex.unlock();

			if(!mDropSamples)
			{
                if ((tSampleNumber >= 0) && (mSamplesSize[mSampleGrabIndex] > 0))
                {
                    // lock
                    //LOG(LOG_ERROR, "DeliverMutex");
                    mDeliverMutex.lock();

                    mSampleNumber[mSampleGrabIndex] = tSampleNumber;

                    mWorkerWithNewData = true;

                    mSampleGrabIndex = SAMPLE_BUFFER_SIZE - mSampleCurrentIndex - mSampleGrabIndex;

                    // unlock
                    mDeliverMutex.unlock();

                    mAudioWidget->InformAboutNewSamples();

                    //printf("AudioWorker--> %d\n", mSampleGrabIndex);
                    //printf("AudioWorker-grabbing FPS: %2d grabbed frame number: %d\n", mResultingFps, tSampleNumber);

                    if ((tLastSampleNumber > tSampleNumber) && (tSampleNumber > 9 /* -1 means error, 1 is received after every reset, use "9" because of possible latencies */))
                        LOG(LOG_ERROR, "Sample ordering problem detected");
                }else
                {
                    LOG(LOG_VERBOSE, "Invalid grabbing result: %d, current sample size: %d", tSampleNumber, mSamplesSize[mSampleGrabIndex]);
                    usleep(500 * 1000); // check for new frames every 1/10 seconds
                }
            }
        }else
        {
			// unlock
			mGrabMutex.unlock();

        	if (mSourceAvailable)
        		LOG(LOG_VERBOSE, "AudioWorkerThread is in pause state");
        	else
        		LOG(LOG_VERBOSE, "AudioWorkerThread waits for available grabbing device");
        	usleep(500 * 1000); // check for new pause state every 3 seconds
        }

    }
    mAudioSource->CloseGrabDevice();
    mAudioSource->DeleteAllRegisteredMediaSinks();

    // close audio playback
    ClosePlaybackDevice();
}

void AudioWorkerThread::StopGrabber()
{
    LOG(LOG_VERBOSE, "StobGrabber now...");
    mWorkerNeeded = false;
    mAudioSource->StopGrabbing();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
