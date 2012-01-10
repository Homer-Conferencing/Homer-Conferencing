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
 * Purpose: Implementation of a dialog for configuration edit
 * Author:  Thomas Volkert
 * Since:   2010-10-03
 */

#include <Dialogs/ConfigurationDialog.h>

#include <Configuration.h>
#include <MediaSourceMuxer.h>
#include <MediaSourceFile.h>
#include <HBSocket.h>
#include <Logger.h>
#include <Snippets.h>

#include <list>
#include <string>

#include <QToolTip>
#include <QCursor>
#include <QString>
#include <QFileDialog>
#include <QSound>
#include <QLineEdit>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Multimedia;
using namespace Homer::SoundOutput;

///////////////////////////////////////////////////////////////////////////////

ConfigurationDialog::ConfigurationDialog(QWidget* pParent, list<string>  pLocalAdresses, VideoWorkerThread* pVideoWorker, AudioWorkerThread* pAudioWorker, WaveOut *pWaveOut):
    QDialog(pParent)
{
    mVideoWorker = pVideoWorker;
    mAudioWorker = pAudioWorker;
    mWaveOut = pWaveOut;
    mLocalAdresses = pLocalAdresses;
    initializeGUI();
    LoadConfiguration();
    connect(mCbVideoSource, SIGNAL(currentIndexChanged(QString)), this, SLOT(ShowVideoSourceInfo(QString)));
    connect(mCbAudioSource, SIGNAL(currentIndexChanged(QString)), this, SLOT(ShowAudioSourceInfo(QString)));
    connect(mCbAudioSink, SIGNAL(currentIndexChanged(QString)), this, SLOT(ShowAudioSinkInfo(QString)));
    connect(mPbNotifySoundNewImFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForIm()));
    connect(mPbNotifySoundNewCallFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForCall()));
    connect(mTbPlaySoundNewImFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForIm()));
    connect(mTbPlaySoundNewCallFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForCall()));
    connect(mTbShowPassword, SIGNAL(clicked()), this, SLOT(ToggleSipServerPasswordVisibility()));
    ShowVideoSourceInfo(mCbVideoSource->currentText());
    ShowAudioSourceInfo(mCbAudioSource->currentText());
    ShowAudioSinkInfo(mCbAudioSink->currentText());
}

ConfigurationDialog::~ConfigurationDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void ConfigurationDialog::initializeGUI()
{
    setupUi(this);
}

int ConfigurationDialog::exec()
{
    int tResult = QDialog::exec();

    if (tResult == QDialog::Accepted)
        SaveConfiguration();

    return tResult;
}

void ConfigurationDialog::LoadConfiguration()
{
    AudioDevicesList::iterator tAudioDevicesIt;
    VideoDevicesList::iterator tVideoDevicesIt;
    QString tCurMaxPackSize;

    mAudioCaptureDevices = mAudioWorker->GetPossibleDevices();
    mVideoCaptureDevices = mVideoWorker->GetPossibleDevices();
    if(mWaveOut != NULL)
        mWaveOut->getAudioDevices(mAudioPlaybackDevices);

    //######################################################################
    //### VIDEO configuration
    //######################################################################
    mGrpVideo->setChecked(CONF.GetVideoActivation());

    //### capture source
    mCbVideoSource->clear();
    for (int i = 0; i < mVideoCaptureDevices.size(); i++)
    {
        mCbVideoSource->addItem(mVideoCaptureDevices[i]);
        // if former selected device is current device then reselect it
        if ((mVideoCaptureDevices[i] == mVideoWorker->GetCurrentDevice()) && (CONF.GetLocalVideoSource() != "auto"))
            mCbVideoSource->setCurrentIndex(mCbVideoSource->count() - 1);
    }
    // reselect "auto" if it was the select the last time Home ran
    if (CONF.GetLocalVideoSource() != "auto")
        mCbVideoSource->setCurrentIndex(0);

    //### fps
    mSbVideoFps->setValue(CONF.GetVideoFps());

    //### capture source picture flip
    mCbHFlip->setChecked(CONF.GetLocalVideoSourceHFlip());
    mCbVFlip->setChecked(CONF.GetLocalVideoSourceVFlip());

    //### stream codec
    QString tVideoStreamCodec = CONF.GetVideoCodec();
    if (tVideoStreamCodec == "H.261")
        mCbVideoCodec->setCurrentIndex(0);
    if (tVideoStreamCodec == "H.263")
        mCbVideoCodec->setCurrentIndex(1);
    if (tVideoStreamCodec == "H.263+")
        mCbVideoCodec->setCurrentIndex(2);
    if (tVideoStreamCodec == "H.264")
        mCbVideoCodec->setCurrentIndex(3);
    if (tVideoStreamCodec == "MPEG4")
        mCbVideoCodec->setCurrentIndex(4);
    if (tVideoStreamCodec == "MJPEG")
        mCbVideoCodec->setCurrentIndex(5);

    //### stream quality
    int tVideoStreamQuality = CONF.GetVideoQuality();
    if (tVideoStreamQuality < 10)
        tVideoStreamQuality = 10;
    if (tVideoStreamQuality > 100)
        tVideoStreamQuality = 100;
    mCbVideoQuality->setCurrentIndex((tVideoStreamQuality / 10) - 1);

    //### stream resolution
    QString tVideoStreamResolution = CONF.GetVideoResolution();
    mCbVideoResolution->setCurrentIndex(MediaSource::VideoString2ResolutionIndex(tVideoStreamResolution.toStdString()));

    //### maximum packet size
    for (int i = 0; i < mCbVideoMaxPacketSize->count(); i++)
    {
        tCurMaxPackSize = mCbVideoMaxPacketSize->itemText(i);
        if (CONF.GetVideoMaxPacketSize() == tCurMaxPackSize.left((tCurMaxPackSize.indexOf("(") -1)).toInt())
        {
            mCbVideoMaxPacketSize->setCurrentIndex(i);
            break;
        }
    }

    //######################################################################
    //### AUDIO configuration
    //######################################################################
    mGrpAudio->setChecked(CONF.GetAudioActivation());

    //### capture source
    mCbAudioSource->clear();
    for (int i = 0; i < mAudioCaptureDevices.size(); i++)
    {
        mCbAudioSource->addItem(mAudioCaptureDevices[i]);
        // if former selected device is current device then reselect it
        if ((mAudioCaptureDevices[i] == mAudioWorker->GetCurrentDevice()) && (CONF.GetLocalAudioSource() != "auto"))
            mCbAudioSource->setCurrentIndex(mCbAudioSource->count() - 1);
    }
    // reselect "auto" if it was the select the last time Home ran
    if (CONF.GetLocalAudioSource() != "auto")
        mCbAudioSource->setCurrentIndex(0);

    //### playback device
    mCbAudioSink->clear();
    QString tLocalAudioPlayback = CONF.GetLocalAudioSink();
    AudioOutDevicesList::iterator tAudioOutDevicesIt;
    for (tAudioOutDevicesIt = mAudioPlaybackDevices.begin(); tAudioOutDevicesIt != mAudioPlaybackDevices.end(); tAudioOutDevicesIt++)
    {
        QString tNewAudioPlaybackDevice = QString(tAudioOutDevicesIt->Name.c_str());
        // check if card is already inserted into QComboBox, if false then add the device to the selectable devices list
        if (mCbAudioSink->findText(tNewAudioPlaybackDevice) == -1)
            mCbAudioSink->addItem(tNewAudioPlaybackDevice);
        // if former selected device is current device then reselect it
        if (tLocalAudioPlayback.compare(tNewAudioPlaybackDevice) == 0)
            mCbAudioSink->setCurrentIndex(mCbAudioSink->count() - 1);
    }

    //### stream codec
    QString tAudioStreamCodec = CONF.GetAudioCodec();
    if (tAudioStreamCodec == "MP3 (MPA)")
        mCbAudioCodec->setCurrentIndex(0);
    if (tAudioStreamCodec == "G711 A-law (PCMA)")
        mCbAudioCodec->setCurrentIndex(1);
    if (tAudioStreamCodec == "G711 µ-law (PCMU)")
        mCbAudioCodec->setCurrentIndex(2);
    if (tAudioStreamCodec == "AAC")
        mCbAudioCodec->setCurrentIndex(3);
    if (tAudioStreamCodec == "PCM_S16_LE")
        mCbAudioCodec->setCurrentIndex(4);
    if (tAudioStreamCodec == "GSM")
        mCbAudioCodec->setCurrentIndex(5);
    if (tAudioStreamCodec == "AMR")
        mCbAudioCodec->setCurrentIndex(6);

    //### stream quality
    int tAudioStreamQuality = CONF.GetAudioQuality();
    if (tAudioStreamQuality < 10)
        tAudioStreamQuality = 10;
    if (tAudioStreamQuality > 100)
        tAudioStreamQuality = 100;
    mCbAudioQuality->setCurrentIndex((tAudioStreamQuality / 10) - 1);

    //### maximum packet size
    for (int i = 0; i < mCbAudioMaxPacketSize->count(); i++)
    {
        tCurMaxPackSize = mCbAudioMaxPacketSize->itemText(i);
        if (CONF.GetAudioMaxPacketSize() == tCurMaxPackSize.left((tCurMaxPackSize.indexOf("(") -1)).toInt())
        {
            mCbAudioMaxPacketSize->setCurrentIndex(i);
            break;
        }
    }

    //######################################################################
    //### NETWORK configuration
    //######################################################################
    list<string>::iterator tItAdr;
    mCbLocalAdr->clear();
    for (tItAdr = mLocalAdresses.begin(); tItAdr != mLocalAdresses.end(); tItAdr++)
    {
        mCbLocalAdr->addItem(QString((*tItAdr).c_str()));
        if ((*tItAdr) == CONF.GetSipListenerAddress().toStdString())
            mCbLocalAdr->setCurrentIndex(mCbLocalAdr->count() - 1);
    }
    mSbSipStartPort->setValue(CONF.GetSipStartPort());
    mSbVideoAudioStartPort->setValue(CONF.GetVideoAudioStartPort());
    mGrpServerRegistration->setChecked(CONF.GetSipInfrastructureMode() > 0);
    mLeSipUserName->setText(CONF.GetSipUserName());
    mLeSipPassword->setText(CONF.GetSipPassword());
    mLeSipServer->setText(CONF.GetSipServer());

    //### Contacting
    mCbContactsProbing->setChecked(CONF.GetSipContactsProbing());

    //### NAT support
    mGrpNatSupport->setChecked(CONF.GetNatSupportActivation());
    mLeStunServer->setText(CONF.GetStunServer());

    //######################################################################
    //### GENERAL configuration
    //######################################################################
    mCbAutoUpdateCheck->setChecked(CONF.GetAutoUpdateCheck());
    if ((CONF.GetColoringScheme() > -1) && (CONF.GetColoringScheme() < mCbColoring->count()))
        mCbColoring->setCurrentIndex(CONF.GetColoringScheme());
    mCbSmoothVideoPresentation->setChecked(CONF.GetSmoothVideoPresentation());
    mCbSeparatedParticipantWidgets->setChecked(CONF.GetParticipantWidgetsSeparation());
    mCbCloseParticipantWidgetsImmediately->setChecked(CONF.GetParticipantWidgetsCloseImmediately());

    //######################################################################
    //### NOTIFICATION configuration
    //######################################################################
    mCbNotifySoundNewIm->setChecked(CONF.GetImSound());
    mCbNotifySystrayNewIm->setChecked(CONF.GetImSystray());
    mLbNotifySoundNewImFile->setText(CONF.GetImSoundFile());

    mCbNotifySoundNewCall->setChecked(CONF.GetCallSound());
    mCbNotifySystrayNewCall->setChecked(CONF.GetCallSystray());
    mLbNotifySoundNewCallFile->setText(CONF.GetCallSoundFile());
}

void ConfigurationDialog::SaveConfiguration()
{
    bool tHaveToRestart = false;
    bool tOnlyFutureChanged = false;

    QString tCurMaxPackSize;

    //######################################################################
    //### VIDEO configuration
    //######################################################################
    CONF.SetVideoActivation(mGrpVideo->isChecked());
    switch(mCbVideoCodec->currentIndex())
    {
        case 0:
                MEETING.SetVideoCodecsSupport(CODEC_H261);
                break;
        case 1:
                MEETING.SetVideoCodecsSupport(CODEC_H263);
                break;
        case 2:
                MEETING.SetVideoCodecsSupport(CODEC_H263P);
                break;
        case 3:
                MEETING.SetVideoCodecsSupport(CODEC_H264);
                break;
        case 4:
                MEETING.SetVideoCodecsSupport(CODEC_MPEG4);
                break;
        case 5:
                MEETING.SetVideoCodecsSupport(CODEC_MJPEG);
                break;
        default:
                break;
    }

    //### capture source
    CONF.SetLocalVideoSource(mCbVideoSource->currentText());

    //### fps
    CONF.SetVideoFps(mSbVideoFps->value());

    //### capture source picture flip
    CONF.SetLocalVideoSourceHFlip(mCbHFlip->isChecked());
    CONF.SetLocalVideoSourceVFlip(mCbVFlip->isChecked());

    //### stream codec
    CONF.SetVideoCodec(mCbVideoCodec->currentText());

    //### stream quality
    int tVideoQuality = (mCbVideoQuality->currentIndex() + 1) * 10;
    CONF.SetVideoQuality(tVideoQuality);

    //### stream resolution
    CONF.SetVideoResolution(mCbVideoResolution->currentText());

    //### maximum packet size
    tCurMaxPackSize = mCbVideoMaxPacketSize->currentText();
    CONF.SetVideoMaxPacketSize(tCurMaxPackSize.left((tCurMaxPackSize.indexOf("(") -1)).toInt());

    //######################################################################
    //### AUDIO configuration
    //######################################################################
    CONF.SetAudioActivation(mGrpAudio->isChecked());
    switch(mCbAudioCodec->currentIndex())
    {
        case 0:
                MEETING.SetAudioCodecsSupport(CODEC_MP3);
                break;
        case 1:
                MEETING.SetAudioCodecsSupport(CODEC_G711A);
                break;
        case 2:
                MEETING.SetAudioCodecsSupport(CODEC_G711U);
                break;
        case 3:
                MEETING.SetAudioCodecsSupport(CODEC_AAC);
                break;
        case 4:
                MEETING.SetAudioCodecsSupport(CODEC_PCMS16LE);
                break;
        case 5:
                MEETING.SetAudioCodecsSupport(CODEC_GSM);
                break;
        case 6:
                MEETING.SetAudioCodecsSupport(CODEC_AMR);
                break;
        default:
                break;
    }

    //### capture source
    CONF.SetLocalAudioSource(mCbAudioSource->currentText());

    //### playback device
    CONF.SetLocalAudioSink(mCbAudioSink->currentText());

    //### stream codec
    CONF.SetAudioCodec(mCbAudioCodec->currentText());

    //### stream quality
    int tAudioQuality = (mCbAudioQuality->currentIndex() + 1) * 10;
    CONF.SetAudioQuality(tAudioQuality);

    //### maximum packet size
    tCurMaxPackSize = mCbAudioMaxPacketSize->currentText();
    CONF.SetAudioMaxPacketSize(tCurMaxPackSize.left((tCurMaxPackSize.indexOf("(") -1)).toInt());

    //######################################################################
    //### NETWORK configuration
    //######################################################################
    CONF.SetSipUserName(mLeSipUserName->text());
    CONF.SetSipPassword(mLeSipPassword->text());
    CONF.SetSipServer(mLeSipServer->text());
    CONF.SetSipInfrastructureMode(mGrpServerRegistration->isChecked() ? 1 : 0);

    if (mCbLocalAdr->currentText() != CONF.GetSipListenerAddress())
        tHaveToRestart = true;
    CONF.SetSipListenerAddress(mCbLocalAdr->currentText());
    if (mSbSipStartPort->value() != CONF.GetSipStartPort())
        tHaveToRestart = true;
    CONF.SetSipStartPort(mSbSipStartPort->value());
    if (mSbVideoAudioStartPort->value() != CONF.GetVideoAudioStartPort())
        tOnlyFutureChanged = true;
    CONF.SetVideoAudioStartPort(mSbVideoAudioStartPort->value());

    // is centralized mode selected activated?
    if (CONF.GetSipInfrastructureMode() == 1)
        MEETING.RegisterAtServer(CONF.GetSipUserName().toStdString(), CONF.GetSipPassword().toStdString(), CONF.GetSipServer().toStdString());


    //### NAT support
    CONF.SetNatSupportActivation(mGrpNatSupport->isChecked());
    CONF.SetStunServer(mLeStunServer->text());

    if (CONF.GetNatSupportActivation())
        MEETING.SetStunServer(CONF.GetStunServer().toStdString());

    //### Contacting
    CONF.SetSipContactsProbing(mCbContactsProbing->isChecked());

    //######################################################################
    //### GENERAL configuration
    //######################################################################
    CONF.SetAutoUpdateCheck(mCbAutoUpdateCheck->isChecked());
    CONF.SetColoringScheme(mCbColoring->currentIndex());
    CONF.SetSmoothVideoPresentation(mCbSmoothVideoPresentation->isChecked());
    CONF.SetParticipantWidgetsSeparation(mCbSeparatedParticipantWidgets->isChecked());
    CONF.SetParticipantWidgetsCloseImmediately(mCbCloseParticipantWidgetsImmediately->isChecked());

    //######################################################################
    //### NOTIFICATION configuration
    //######################################################################
    CONF.SetImSound(mCbNotifySoundNewIm->isChecked());
    CONF.SetImSystray(mCbNotifySystrayNewIm->isChecked());
    CONF.SetImSoundFile(mLbNotifySoundNewImFile->text());

    CONF.SetCallSound(mCbNotifySoundNewCall->isChecked());
    CONF.SetCallSystray(mCbNotifySystrayNewCall->isChecked());
    CONF.SetCallSoundFile(mLbNotifySoundNewCallFile->text());

    //######################################################################
    //### Synchronize write operations
    //######################################################################
    CONF.Sync();

    if (tHaveToRestart)
        ShowInfo("Restart necessary", "You have to <font color='red'><b>restart</b></font> Homer to apply the new settings!");
    else
    {
        if (tOnlyFutureChanged)
            ShowInfo("Settings applied for future sessions", "Your new settings are <font color='red'><b>not applied for already established sessions</b></font>. They will only be used for new sessions! Otherwise you have to <font color='red'><b>restart</b></font> Homer to apply the new settings!");
    }
}

void ConfigurationDialog::ShowVideoSourceInfo(QString pCurrentText)
{
    mLbVideoSourceInfo->setText(mVideoWorker->GetDeviceDescription(pCurrentText));
}

void ConfigurationDialog::ShowAudioSourceInfo(QString pCurrentText)
{
    mLbAudioSourceInfo->setText(mAudioWorker->GetDeviceDescription(pCurrentText));
}

void ConfigurationDialog::ShowAudioSinkInfo(QString pCurrentText)
{
    AudioOutDevicesList::iterator tAudioOutDevicesIt;
    QString tInfoText = "";

    for (tAudioOutDevicesIt = mAudioPlaybackDevices.begin(); tAudioOutDevicesIt != mAudioPlaybackDevices.end(); tAudioOutDevicesIt++)
    {
        if (pCurrentText.compare(QString(tAudioOutDevicesIt->Name.c_str())) == 0)
            tInfoText = QString(tAudioOutDevicesIt->Desc.c_str());
    }

    mLbAudioSinkInfo->setText(tInfoText);
}

void ConfigurationDialog::ToggleSipServerPasswordVisibility()
{
    if(mLeSipPassword->echoMode() == QLineEdit::Normal)
        mLeSipPassword->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    else
        mLeSipPassword->setEchoMode(QLineEdit::Normal);
}

void ConfigurationDialog::SelectNotifySoundFileForIm()
{
    QString tSoundFile = QFileDialog::getOpenFileName(this, "Select sound file for instant message notification", CONF.GetImSoundFile(), "Sound file (*.wav)", NULL, QFileDialog::DontUseNativeDialog);

    if (tSoundFile.isEmpty())
        return;

    if (!tSoundFile.endsWith(".wav"))
        tSoundFile += ".wav";

    LOG(LOG_VERBOSE, "Selected sound file: %s", tSoundFile.toStdString().c_str());
    CONF.SetImSoundFile(tSoundFile);
    mLbNotifySoundNewImFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForCall()
{
    QString tSoundFile = QFileDialog::getOpenFileName(this, "Select sound file for incoming call notification", CONF.GetCallSoundFile(), "Sound file (*.wav)", NULL, QFileDialog::DontUseNativeDialog);

    if (tSoundFile.isEmpty())
        return;

    if (!tSoundFile.endsWith(".wav"))
        tSoundFile += ".wav";

    LOG(LOG_VERBOSE, "Selected sound file: %s", tSoundFile.toStdString().c_str());
    CONF.SetCallSoundFile(tSoundFile);
    mLbNotifySoundNewCallFile->setText(tSoundFile);
}

void ConfigurationDialog::PlayNotifySoundFileForIm()
{
    if (!QSound::isAvailable())
    {
        #ifdef LINUX
            ShowError("Sound output failed", "Sound output is unavailable. Check your NAS installation!");
        #endif
        #ifdef WIN32
            ShowError("Sound output failed", "Sound output is unavailable. Check your Windows drivers!");
        #endif
        return;
    }

    if (CONF.GetImSoundFile() != "")
    {
        LOG(LOG_VERBOSE, "Playing sound file: %s", CONF.GetImSoundFile().toStdString().c_str());
        QSound::play(CONF.GetImSoundFile());
    }
}

void ConfigurationDialog::PlayNotifySoundFileForCall()
{
    if (!QSound::isAvailable())
    {
        #ifdef LINUX
            ShowError("Sound output failed", "Sound output is unavailable. Check your NAS installation!");
        #endif
        #ifdef WIN32
            ShowError("Sound output failed", "Sound output is unavailable. Check your Windows drivers!");
        #endif
        return;
    }

    if (CONF.GetCallSoundFile() != "")
    {
        LOG(LOG_VERBOSE, "Playing sound file: %s", CONF.GetCallSoundFile().toStdString().c_str());
        QSound::play(CONF.GetCallSoundFile());
    }
}

void ConfigurationDialog::ClickedButton(QAbstractButton *pButton)
{
    if (mBbButtons->standardButton(pButton) == QDialogButtonBox::Reset)
    {
        switch(mSwConfigPages->currentIndex())
        {
            //### VIDEO configuration
            case 0:
                mGrpVideo->setChecked(true);
                mCbVideoSource->setCurrentIndex(0);
                mCbHFlip->setChecked(false);
                mCbVFlip->setChecked(false);
                mSbVideoFps->setValue(0);
                mCbVideoCodec->setCurrentIndex(0);//H.261
                mCbVideoQuality->setCurrentIndex(0);//10 %
                mCbVideoResolution->setCurrentIndex(2);//CIF
                mCbVideoMaxPacketSize->setCurrentIndex(3);//1492
                break;
            //### AUDIO configuration
            case 1:
                mGrpAudio->setChecked(true);
                mCbAudioSource->setCurrentIndex(0);
                mCbAudioSink->setCurrentIndex(0);
                mCbAudioCodec->setCurrentIndex(0);//MP3
                mCbAudioQuality->setCurrentIndex(6);//70 %
                mCbAudioMaxPacketSize->setCurrentIndex(3);//1492
                break;
            //### NETWORK configuration
            case 2:
                //mCbLocalAdr->setCurrentIndex(0);
                mSbSipStartPort->setValue(5060);
                mSbVideoAudioStartPort->setValue(5000);
                mGrpServerRegistration->setChecked(false);
                //mLeSipUserName->setText(CONF.GetSipUserName());
                //mLeSipPassword->setText(CONF.GetSipPassword());
                //mLeSipServer->setText(CONF.GetSipServer());
                mCbContactsProbing->setChecked(true);
                mGrpNatSupport->setChecked(false);
                mLeStunServer->setText("stun.voipbuster.com");
                break;
            //### GENERAL configuration
            case 3:
                mCbAutoUpdateCheck->setChecked(false);
                mCbColoring->setCurrentIndex(0);
                mCbSmoothVideoPresentation->setChecked(false);
                mCbSeparatedParticipantWidgets->setChecked(true);
                mCbCloseParticipantWidgetsImmediately->setChecked(true);
                break;
            //### NOTIFICATIONS configuration
            case 4:
                mCbNotifySoundNewIm->setChecked(false);
                mCbNotifySystrayNewIm->setChecked(true);

                mCbNotifySoundNewCall->setChecked(false);
                mCbNotifySystrayNewCall->setChecked(true);
                break;
            default: //something is going wrong in this case
                break;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace

