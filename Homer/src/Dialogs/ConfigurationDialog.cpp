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
    connect(mPbNotifySoundImFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForIm()));
    connect(mPbNotifySoundCallFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForCall()));
    connect(mPbNotifySoundCallAcknowledgeFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForCallAcknowledge()));
    connect(mPbNotifySoundCallDenyFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForCallDeny()));
    connect(mPbNotifySoundCallHangupFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForCallHangup()));
    connect(mPbNotifySoundErrorFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForError()));
    connect(mPbNotifySoundRegistrationFailedFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForRegistrationFailed()));
    connect(mPbNotifySoundRegistrationSuccessfulFile, SIGNAL(clicked()), this, SLOT(SelectNotifySoundFileForRegistrationSuccessful()));
    connect(mTbPlaySoundImFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForIm()));
    connect(mTbPlaySoundCallFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForCall()));
    connect(mTbPlaySoundCallAcknowledgeFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForCallAcknowledge()));
    connect(mTbPlaySoundCallDenyFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForCallDeny()));
    connect(mTbPlaySoundCallHangupFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForCallHangup()));
    connect(mTbPlaySoundErrorFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForError()));
    connect(mTbPlaySoundRegistrationFailedFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForRegistrationFailed()));
    connect(mTbPlaySoundRegistrationSuccessfulFile, SIGNAL(clicked()), this, SLOT(PlayNotifySoundFileForRegistrationSuccessful()));
    connect(mTbSelectAllSound, SIGNAL(clicked()), this, SLOT(SelectAllSound()));
    connect(mTbDeselectAllSound, SIGNAL(clicked()), this, SLOT(DeselectAllSound()));
    connect(mTbSelectAllSystray, SIGNAL(clicked()), this, SLOT(SelectAllSystray()));
    connect(mTbDeselectAllSystray, SIGNAL(clicked()), this, SLOT(DeselectAllSystray()));
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

int ConfigurationDialog::VideoString2ResolutionIndex(string pString)
{
    int tResult = 2;

    if (pString == "auto")
        tResult = 0;
    if (pString == "128 * 96")
        tResult = 1;
    if (pString == "176 * 144")
        tResult = 2;
    if (pString == "352 * 288")
        tResult = 3;
    if (pString == "704 * 576")
        tResult = 4;
    if (pString == "720 * 576")
        tResult = 5;
    if (pString == "1056 * 864")
        tResult = 6;
    if (pString == "1280 * 720")
        tResult = 7;
    if (pString == "1408 * 1152")
        tResult = 8;
    if (pString == "1920 * 1080")
        tResult = 9;

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
    mCbVideoResolution->setCurrentIndex(VideoString2ResolutionIndex(tVideoStreamResolution.toStdString()));

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

    mCbSmoothVideoPresentation->setChecked(CONF.GetSmoothVideoPresentation());

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
    if (tAudioStreamCodec == "G711 �-law (PCMU)")
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
    mCbSeparatedParticipantWidgets->setChecked(CONF.GetParticipantWidgetsSeparation());
    mCbCloseParticipantWidgetsImmediately->setChecked(CONF.GetParticipantWidgetsCloseImmediately());

    //######################################################################
    //### NOTIFICATION configuration
    //######################################################################
    mCbNotifySoundIm->setChecked(CONF.GetImSound());
    mCbNotifySystrayIm->setChecked(CONF.GetImSystray());
    mLbNotifySoundImFile->setText(CONF.GetImSoundFile());

    mCbNotifySoundCall->setChecked(CONF.GetCallSound());
    mCbNotifySystrayCall->setChecked(CONF.GetCallSystray());
    mLbNotifySoundCallFile->setText(CONF.GetCallSoundFile());

    mCbNotifySoundCallAcknowledge->setChecked(CONF.GetCallAcknowledgeSound());
    mCbNotifySystrayCallAcknowledge->setChecked(CONF.GetCallAcknowledgeSystray());
    mLbNotifySoundCallAcknowledgeFile->setText(CONF.GetCallAcknowledgeSoundFile());

    mCbNotifySoundCallDeny->setChecked(CONF.GetCallDenySound());
    mCbNotifySystrayCallDeny->setChecked(CONF.GetCallDenySystray());
    mLbNotifySoundCallDenyFile->setText(CONF.GetCallDenySoundFile());

    mCbNotifySoundCallHangup->setChecked(CONF.GetCallHangupSound());
    mCbNotifySystrayCallHangup->setChecked(CONF.GetCallHangupSystray());
    mLbNotifySoundCallHangupFile->setText(CONF.GetCallHangupSoundFile());

    mCbNotifySoundError->setChecked(CONF.GetErrorSound());
    mCbNotifySystrayError->setChecked(CONF.GetErrorSystray());
    mLbNotifySoundErrorFile->setText(CONF.GetErrorSoundFile());

    mCbNotifySoundRegistrationFailed->setChecked(CONF.GetRegistrationFailedSound());
    mCbNotifySystrayRegistrationFailed->setChecked(CONF.GetRegistrationFailedSystray());
    mLbNotifySoundRegistrationFailedFile->setText(CONF.GetRegistrationFailedSoundFile());

    mCbNotifySoundRegistrationSuccessful->setChecked(CONF.GetRegistrationSuccessfulSound());
    mCbNotifySystrayRegistrationSuccessful->setChecked(CONF.GetRegistrationSuccessfulSystray());
    mLbNotifySoundRegistrationSuccessfulFile->setText(CONF.GetRegistrationSuccessfulSoundFile());
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

    CONF.SetSmoothVideoPresentation(mCbSmoothVideoPresentation->isChecked());

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
    CONF.SetParticipantWidgetsSeparation(mCbSeparatedParticipantWidgets->isChecked());
    CONF.SetParticipantWidgetsCloseImmediately(mCbCloseParticipantWidgetsImmediately->isChecked());

    //######################################################################
    //### NOTIFICATION configuration
    //######################################################################
    CONF.SetImSound(mCbNotifySoundIm->isChecked());
    CONF.SetImSystray(mCbNotifySystrayIm->isChecked());
    CONF.SetImSoundFile(mLbNotifySoundImFile->text());

    CONF.SetCallSound(mCbNotifySoundCall->isChecked());
    CONF.SetCallSystray(mCbNotifySystrayCall->isChecked());
    CONF.SetCallSoundFile(mLbNotifySoundCallFile->text());

    CONF.SetCallAcknowledgeSound(mCbNotifySoundCallAcknowledge->isChecked());
    CONF.SetCallAcknowledgeSystray(mCbNotifySystrayCallAcknowledge->isChecked());
    CONF.SetCallAcknowledgeSoundFile(mLbNotifySoundCallAcknowledgeFile->text());

    CONF.SetCallDenySound(mCbNotifySoundCallDeny->isChecked());
    CONF.SetCallDenySystray(mCbNotifySystrayCallDeny->isChecked());
    CONF.SetCallDenySoundFile(mLbNotifySoundCallDenyFile->text());

    CONF.SetCallHangupSound(mCbNotifySoundCallHangup->isChecked());
    CONF.SetCallHangupSystray(mCbNotifySystrayCallHangup->isChecked());
    CONF.SetCallHangupSoundFile(mLbNotifySoundCallHangupFile->text());

    CONF.SetErrorSound(mCbNotifySoundError->isChecked());
    CONF.SetErrorSystray(mCbNotifySystrayError->isChecked());
    CONF.SetErrorSoundFile(mLbNotifySoundErrorFile->text());

    CONF.SetRegistrationFailedSound(mCbNotifySoundRegistrationFailed->isChecked());
    CONF.SetRegistrationFailedSystray(mCbNotifySystrayRegistrationFailed->isChecked());
    CONF.SetRegistrationFailedSoundFile(mLbNotifySoundRegistrationFailedFile->text());

    CONF.SetRegistrationSuccessfulSound(mCbNotifySoundRegistrationSuccessful->isChecked());
    CONF.SetRegistrationSuccessfulSystray(mCbNotifySystrayRegistrationSuccessful->isChecked());
    CONF.SetRegistrationSuccessfulSoundFile(mLbNotifySoundRegistrationSuccessfulFile->text());

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

QString ConfigurationDialog::SelectSoundFile(QString pEventName, QString pSuggestion)
{
    QString tSoundFile = QFileDialog::getOpenFileName(this, "Select sound file for acoustic notification for event \"" + pEventName + "\".", pSuggestion, "Sound file (*.wav)", NULL, QFileDialog::DontUseNativeDialog);

    if (tSoundFile.isEmpty())
        return "";

    if (!tSoundFile.endsWith(".wav"))
        tSoundFile += ".wav";

    LOG(LOG_VERBOSE, "Selected sound file: %s", tSoundFile.toStdString().c_str());

    return tSoundFile;
}

void ConfigurationDialog::SelectNotifySoundFileForIm()
{
    QString tSoundFile = SelectSoundFile("new message", CONF.GetImSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetImSoundFile(tSoundFile);
    mLbNotifySoundImFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForCall()
{
    QString tSoundFile = SelectSoundFile("new call", CONF.GetCallSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetCallSoundFile(tSoundFile);
    mLbNotifySoundCallFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForCallDeny()
{
    QString tSoundFile = SelectSoundFile("call canceled", CONF.GetCallDenySoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetCallDenySoundFile(tSoundFile);
    mLbNotifySoundCallDenyFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForCallAcknowledge()
{
    QString tSoundFile = SelectSoundFile("call acknowledged", CONF.GetCallAcknowledgeSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetCallAcknowledgeSoundFile(tSoundFile);
    mLbNotifySoundCallAcknowledgeFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForCallHangup()
{
    QString tSoundFile = SelectSoundFile("call hangup", CONF.GetCallHangupSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetCallHangupSoundFile(tSoundFile);
    mLbNotifySoundCallHangupFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForError()
{
    QString tSoundFile = SelectSoundFile("error", CONF.GetErrorSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetErrorSoundFile(tSoundFile);
    mLbNotifySoundErrorFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForRegistrationFailed()
{
    QString tSoundFile = SelectSoundFile("registration failed", CONF.GetRegistrationFailedSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetRegistrationFailedSoundFile(tSoundFile);
    mLbNotifySoundRegistrationFailedFile->setText(tSoundFile);
}

void ConfigurationDialog::SelectNotifySoundFileForRegistrationSuccessful()
{
    QString tSoundFile = SelectSoundFile("registration successful", CONF.GetRegistrationSuccessfulSoundFile());

    if (tSoundFile.isEmpty())
        return;

    CONF.SetRegistrationSuccessfulSoundFile(tSoundFile);
    mLbNotifySoundRegistrationSuccessfulFile->setText(tSoundFile);
}

void ConfigurationDialog::PlayNotifySoundFile(QString pFile)
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

    if (pFile != "")
    {
        LOG(LOG_VERBOSE, "Playing sound file: %s", pFile.toStdString().c_str());
        QSound::play(pFile);
    }
}

void ConfigurationDialog::PlayNotifySoundFileForIm()
{
	PlayNotifySoundFile(CONF.GetImSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForCall()
{
	PlayNotifySoundFile(CONF.GetCallSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForCallAcknowledge()
{
	PlayNotifySoundFile(CONF.GetCallAcknowledgeSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForCallDeny()
{
	PlayNotifySoundFile(CONF.GetCallDenySoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForCallHangup()
{
	PlayNotifySoundFile(CONF.GetCallHangupSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForError()
{
	PlayNotifySoundFile(CONF.GetErrorSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForRegistrationFailed()
{
	PlayNotifySoundFile(CONF.GetRegistrationFailedSoundFile());
}

void ConfigurationDialog::PlayNotifySoundFileForRegistrationSuccessful()
{
	PlayNotifySoundFile(CONF.GetRegistrationSuccessfulSoundFile());
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
                mCbSmoothVideoPresentation->setChecked(false);
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
                mCbSeparatedParticipantWidgets->setChecked(true);
                mCbCloseParticipantWidgetsImmediately->setChecked(true);
                break;
            //### NOTIFICATIONS configuration
            case 4:
            	DeselectAllSound();
            	SelectAllSystray();
                break;
            default: //something is going wrong in this case
                break;
        }
    }
}

void ConfigurationDialog::SelectAllSound()
{
    mCbNotifySoundIm->setChecked(true);

    mCbNotifySoundCall->setChecked(true);
    mCbNotifySoundCallAcknowledge->setChecked(true);
    mCbNotifySoundCallDeny->setChecked(true);
    mCbNotifySoundCallHangup->setChecked(true);
    mCbNotifySoundError->setChecked(true);
    mCbNotifySoundRegistrationFailed->setChecked(true);
    mCbNotifySoundRegistrationSuccessful->setChecked(true);
}

void ConfigurationDialog::DeselectAllSound()
{
    mCbNotifySoundIm->setChecked(false);

    mCbNotifySoundCall->setChecked(false);
    mCbNotifySoundCallAcknowledge->setChecked(false);
    mCbNotifySoundCallDeny->setChecked(false);
    mCbNotifySoundCallHangup->setChecked(false);

    mCbNotifySoundError->setChecked(false);

    mCbNotifySoundRegistrationFailed->setChecked(false);
    mCbNotifySoundRegistrationSuccessful->setChecked(false);
}

void ConfigurationDialog::SelectAllSystray()
{
    mCbNotifySystrayIm->setChecked(true);

    mCbNotifySystrayCall->setChecked(true);
    mCbNotifySystrayCallAcknowledge->setChecked(true);
    mCbNotifySystrayCallDeny->setChecked(true);
    mCbNotifySystrayCallHangup->setChecked(true);

    mCbNotifySystrayError->setChecked(true);

    mCbNotifySystrayRegistrationFailed->setChecked(true);
    mCbNotifySystrayRegistrationSuccessful->setChecked(true);

}

void ConfigurationDialog::DeselectAllSystray()
{
    mCbNotifySystrayIm->setChecked(false);

    mCbNotifySystrayCall->setChecked(false);
    mCbNotifySystrayCallAcknowledge->setChecked(false);
    mCbNotifySystrayCallDeny->setChecked(false);
    mCbNotifySystrayCallHangup->setChecked(false);

    mCbNotifySystrayError->setChecked(false);

    mCbNotifySystrayRegistrationFailed->setChecked(false);
    mCbNotifySystrayRegistrationSuccessful->setChecked(false);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace

