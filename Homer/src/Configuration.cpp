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
 * Purpose: Implementation of the global program settings management
 * Author:  Thomas Volkert
 * Since:   2009-07-30
 */

#include <Configuration.h>
#include <Logger.h>
#include <Meeting.h>

#include <QString>
#include <QHostInfo>
#include <QDir>
#include <QDesktopWidget>

namespace Homer { namespace Gui {

using namespace std;

Configuration sConfiguration;

///////////////////////////////////////////////////////////////////////////////

Configuration::Configuration()
{
    mQSettings = new QSettings("Homer Software", "Homer");
    LOG(LOG_VERBOSE, "Created");
}

Configuration::~Configuration()
{
	Sync();
	LOG(LOG_VERBOSE, "Destroyed");
}

Configuration& Configuration::GetInstance()
{
    return sConfiguration;
}

///////////////////////////////////////////////////////////////////////////////

void Configuration::Init(string pAbsBinPath)
{
    mAbsBinPath = pAbsBinPath;
}

void Configuration::SetConferenceAvailability(QString pState)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("Availability", pState);
    mQSettings->endGroup();
}

void Configuration::SetContactFile(QString pContactFile)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("ContactFile", pContactFile);
    mQSettings->endGroup();
}

void Configuration::SetDataDirectory(QString pDataDirectory)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("DataDirectory", pDataDirectory);
    mQSettings->endGroup();
}

void Configuration::SetMainWindowPosition(QPoint pPos)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("MainWindowPositionX", pPos.x());
    mQSettings->setValue("MainWindowPositionY", pPos.y());
    mQSettings->endGroup();
}

void Configuration::SetMainWindowSize(QSize pSize)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("MainWindowWidth", pSize.width());
    mQSettings->setValue("MainWindowHeight", pSize.height());
    mQSettings->endGroup();
}

void Configuration::SetParticipantWidgetsSeparation(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("ParticipantWidgetsSeparation", pActive);
    mQSettings->endGroup();
}

void Configuration::SetParticipantWidgetsCloseImmediately(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("ParticipantWidgetsCloseImmediately", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityContactsWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityContactsWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityErrorsWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityErrorsWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityFileTransfersWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityFileTransfersWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityPlaylistWidgetAudio(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityPlaylistWidgetAudio", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityPlaylistWidgetVideo(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityPlaylistWidgetVideo", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityPlaylistWidgetMovie(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityPlaylistWidgetMovie", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityThreadsWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityThreadsWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityNetworkStreamsWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityNetworkStreamsWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityDataStreamsWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityDataStreamsWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityBroadcastWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityBroadcastWidget", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityToolBarMediaSources(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityToolBarMediaSources", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityToolBarOnlineStatus(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityToolBarOnlineStatus", pActive);
    mQSettings->endGroup();
}

void Configuration::SetSmoothVideoPresentation(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("SmoothVideoPresentation", pActive);
    mQSettings->endGroup();
}

void Configuration::SetAutoUpdateCheck(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("AutomaticUpdateCheck", pActive);
    mQSettings->endGroup();
}

void Configuration::SetColoringScheme(int pColoringScheme)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("ColoringScheme", pColoringScheme);
    mQSettings->endGroup();
}

void Configuration::SetUserName(QString pUserName)
{
    mQSettings->beginGroup("User");
    mQSettings->setValue("UserName", pUserName);
    mQSettings->endGroup();
}

void Configuration::SetUserMail(QString pUserMail)
{
    mQSettings->beginGroup("User");
    mQSettings->setValue("UserMail", pUserMail);
    mQSettings->endGroup();
}

void Configuration::SetVideoActivation(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamActivation", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetVideoRtp(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamRtp", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetVideoCodec(QString pCodec)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamCodec", pCodec);
    mQSettings->endGroup();
}

void Configuration::SetVideoQuality(int pQuality)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamQuality", pQuality);
    mQSettings->endGroup();
}

void Configuration::SetVideoMaxPacketSize(int pSize)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamMaxPacketSize", pSize);
    mQSettings->endGroup();
}

void Configuration::SetVideoTransport(enum TransportType pType)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamTransportType", QString(Socket::TransportType2String(pType).c_str()));
    mQSettings->endGroup();
}

void Configuration::SetVideoResolution(QString pResolution)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamResolution", pResolution);
    mQSettings->endGroup();
}

void Configuration::SetLocalVideoSource(QString pVSource)
{
    mQSettings->beginGroup("Capturing");
    mQSettings->setValue("LocalVideoDevice", pVSource);
    mQSettings->endGroup();
}

void Configuration::SetVideoFps(int pFps)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoFps", pFps);
    mQSettings->endGroup();
}

void Configuration::SetLocalVideoSourceHFlip(bool pHFlip)
{
    mQSettings->beginGroup("Capturing");
    mQSettings->setValue("HorizontallyFlipInput", pHFlip);
    mQSettings->endGroup();
}

void Configuration::SetLocalVideoSourceVFlip(bool pVFlip)
{
    mQSettings->beginGroup("Capturing");
    mQSettings->setValue("VerticallyFlipInput", pVFlip);
    mQSettings->endGroup();
}

void Configuration::SetAudioActivation(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamActivation", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetAudioRtp(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamRtp", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetAudioCodec(QString pCodec)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamCodec", pCodec);
    mQSettings->endGroup();
}

void Configuration::SetAudioQuality(int pQuality)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamQuality", pQuality);
    mQSettings->endGroup();
}

void Configuration::SetAudioMaxPacketSize(int pSize)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamMaxPacketSize", pSize);
    mQSettings->endGroup();
}

void Configuration::SetAudioTransport(enum TransportType pType)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamTransportType", QString(Socket::TransportType2String(pType).c_str()));
    mQSettings->endGroup();
}

void Configuration::SetLocalAudioSource(QString pASource)
{
    mQSettings->beginGroup("Capturing");
    mQSettings->setValue("LocalAudioDevice", pASource);
    mQSettings->endGroup();
}

void Configuration::SetLocalAudioSink(QString pASink)
{
    mQSettings->beginGroup("Playback");
    mQSettings->setValue("LocalAudioDevice", pASink);
    mQSettings->endGroup();
}

void Configuration::SetVideoAudioStartPort(int pPort)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("VideoAudioListenerStartPort", pPort);
    mQSettings->endGroup();
}

void Configuration::SetSipStartPort(int pPort)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipListenerStartPort", pPort);
    mQSettings->endGroup();
}

void Configuration::SetSipUserName(QString pUserName)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipUserName", pUserName);
    mQSettings->endGroup();
}

void Configuration::SetSipPassword(QString pPassword)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipPassword", pPassword);
    mQSettings->endGroup();
}

void Configuration::SetSipServer(QString pServer)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipServer", pServer);
    mQSettings->endGroup();
}

void Configuration::SetSipInfrastructureMode(int pMode)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipInfrastructureMode", pMode);
    mQSettings->endGroup();
}

void Configuration::SetSipContactsProbing(bool pActivation)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("ContactsProbing", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetSipListenerAddress(QString pAddress)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipListenerAddress", pAddress);
    mQSettings->endGroup();
}

void Configuration::SetStunServer(QString pServer)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("StunServer", pServer);
    mQSettings->endGroup();
}

void Configuration::SetNatSupportActivation(bool pActivation)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("NatSupportActivation", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetImSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ImSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetImSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ImSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetImSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ImSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetCallSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallSystray", pActivation);
    mQSettings->endGroup();
}

QString Configuration::GetSipListenerAddress()
{
    return mQSettings->value("Network/SipListenerAddress", QString("")).toString();
}

QString Configuration::GetBinaryPath()
{
	return QString(mAbsBinPath.c_str());
}

QString Configuration::GetConferenceAvailability()
{
    return mQSettings->value("Global/Availability", QString(MEETING.getAvailabilityStateStr().c_str())).toString();
}

QString Configuration::GetContactFile()
{
    return mQSettings->value("Global/ContactFile", QString((mAbsBinPath + "Homer-Contacts.xml").c_str())).toString();
}

QString Configuration::GetDataDirectory()
{
    return mQSettings->value("Global/DataDirectory", QDir::homePath()).toString();
}

QPoint Configuration::GetMainWindowPosition()
{
    int tScreenResX = QApplication::desktop()->screenGeometry().width();
    int tScreenResY = QApplication::desktop()->screenGeometry().height();

    int tPosX = mQSettings->value("Global/MainWindowPositionX", 0).toInt();
    int tPosY = mQSettings->value("Global/MainWindowPositionY", 0).toInt();
    if (tPosX < 0)
        tPosX = 0;
    if (tPosY < 0)
        tPosY = 0;
    if (tPosX + 32 > tScreenResX)
        tPosX = tScreenResX - 32;
    if (tPosY + 32 > tScreenResY)
        tPosY = tScreenResY - 32;

    return QPoint(tPosX, tPosY);
}

QSize Configuration::GetMainWindowSize()
{
    int tWidth = mQSettings->value("Global/MainWindowWidth", 0).toInt();
    int tHeight = mQSettings->value("Global/MainWindowHeight", 0).toInt();

    return QSize(tWidth, tHeight);
}

bool Configuration::GetParticipantWidgetsSeparation()
{
    return mQSettings->value("Global/ParticipantWidgetsSeparation", true).toBool();
}

bool Configuration::GetParticipantWidgetsCloseImmediately()
{
    return mQSettings->value("Global/ParticipantWidgetsCloseImmediately", true).toBool();
}

bool Configuration::GetVisibilityContactsWidget()
{
    return mQSettings->value("Global/VisibilityContactsWidget", true).toBool();
}

bool Configuration::GetVisibilityErrorsWidget()
{
    return mQSettings->value("Global/VisibilityErrorsWidget", false).toBool();
}

bool Configuration::GetVisibilityFileTransfersWidget()
{
    return mQSettings->value("Global/VisibilityFileTransfersWidget", false).toBool();
}

bool Configuration::GetVisibilityPlaylistWidgetAudio()
{
    return mQSettings->value("Global/VisibilityPlaylistWidgetAudio", false).toBool();
}

bool Configuration::GetVisibilityPlaylistWidgetVideo()
{
    return mQSettings->value("Global/VisibilityPlaylistWidgetVideo", false).toBool();
}

bool Configuration::GetVisibilityPlaylistWidgetMovie()
{
    return mQSettings->value("Global/VisibilityPlaylistWidgetMovie", false).toBool();
}

bool Configuration::GetVisibilityThreadsWidget()
{
    return mQSettings->value("Global/VisibilityThreadsWidget", false).toBool();
}

bool Configuration::GetVisibilityNetworkStreamsWidget()
{
    return mQSettings->value("Global/VisibilityNetworkStreamsWidget", false).toBool();
}

bool Configuration::GetVisibilityDataStreamsWidget()
{
    return mQSettings->value("Global/VisibilityDataStreamsWidget", false).toBool();
}

bool Configuration::GetVisibilityBroadcastWidget()
{
    return mQSettings->value("Global/VisibilityBroadcastWidget", true).toBool();
}

bool Configuration::GetVisibilityToolBarMediaSources()
{
    return mQSettings->value("Global/VisibilityToolBarMediaSources", true).toBool();
}

bool Configuration::GetVisibilityToolBarOnlineStatus()
{
    return mQSettings->value("Global/VisibilityToolBarOnlineStatus", true).toBool();
}

bool Configuration::GetSmoothVideoPresentation()
{
    return mQSettings->value("Global/SmoothVideoPresentation", false).toBool();
}

bool Configuration::GetAutoUpdateCheck()
{
    return mQSettings->value("Global/AutomaticUpdateCheck", false).toBool();
}

int Configuration::GetColoringScheme()
{
    return mQSettings->value("Global/ColoringScheme", 0).toInt();
}

QString Configuration::GetUserName()
{
    return mQSettings->value("User/UserName", QString::fromAscii(MEETING.getUser().c_str())).toString();
}

QString Configuration::GetUserMail()
{
    return mQSettings->value("User/UserMail", QString(MEETING.SipCreateId(MEETING.getUser(), QHostInfo::localHostName().toStdString()).c_str())).toString();
}

bool Configuration::GetVideoActivation()
{
    return mQSettings->value("Streaming/VideoStreamActivation", true).toBool();
}

bool Configuration::GetVideoRtp()
{
    return mQSettings->value("Streaming/VideoStreamRtp", true).toBool();
}

QString Configuration::GetVideoCodec()
{
    return mQSettings->value("Streaming/VideoStreamCodec", QString("H.261")).toString();
}

int Configuration::GetVideoQuality()
{
    return mQSettings->value("Streaming/VideoStreamQuality", 10).toInt();
}

int Configuration::GetVideoMaxPacketSize()
{
    return mQSettings->value("Streaming/VideoStreamMaxPacketSize", 1492).toInt();
}

enum TransportType Configuration::GetVideoTransportType()
{
    return Socket::String2TransportType(mQSettings->value("Streaming/VideoStreamTransportType", QString("UDP")).toString().toStdString());
}

QString Configuration::GetVideoResolution()
{
    return mQSettings->value("Streaming/VideoStreamResolution", QString("352*288")).toString();
}

bool Configuration::GetLocalVideoSourceHFlip()
{
    return mQSettings->value("Capturing/HorizontallyFlipInput", false).toBool();
}

bool Configuration::GetLocalVideoSourceVFlip()
{
    return mQSettings->value("Capturing/VerticallyFlipInput", false).toBool();
}

QString Configuration::GetLocalVideoSource()
{
    return mQSettings->value("Capturing/LocalVideoDevice", QString("auto")).toString();
}

int Configuration::GetVideoFps()
{
    return mQSettings->value("Streaming/VideoFps", 0).toInt();
}

bool Configuration::GetAudioActivation()
{
    return mQSettings->value("Streaming/AudioStreamActivation", true).toBool();
}

bool Configuration::GetAudioRtp()
{
    return mQSettings->value("Streaming/AudioStreamRtp", true).toBool();
}

QString Configuration::GetAudioCodec()
{
    return mQSettings->value("Streaming/AudioStreamCodec", QString("MP3 (MPA)")).toString();
}

int Configuration::GetAudioQuality()
{
    return mQSettings->value("Streaming/AudioStreamQuality", 70).toInt();
}

int Configuration::GetAudioMaxPacketSize()
{
    return mQSettings->value("Streaming/AudioStreamMaxPacketSize", 1492).toInt();
}

enum TransportType Configuration::GetAudioTransportType()
{
    return Socket::String2TransportType(mQSettings->value("Streaming/AudioStreamTransportType", QString("UDP")).toString().toStdString());
}

int Configuration::GetVideoAudioStartPort()
{
    return mQSettings->value("Network/VideoAudioListenerStartPort", 5000).toInt();
}

QString Configuration::GetLocalAudioSource()
{
    return mQSettings->value("Capturing/LocalAudioDevice", QString("auto")).toString();
}

QString Configuration::GetLocalAudioSink()
{
    return mQSettings->value("Playback/LocalAudioDevice", QString("auto")).toString();
}

int Configuration::GetSipStartPort()
{
    return mQSettings->value("Network/SipListenerStartPort", 5060).toInt();
}

QString Configuration::GetSipUserName()
{
    return mQSettings->value("Network/SipUserName", QString::fromAscii(MEETING.getUser().c_str())).toString();
}

QString Configuration::GetSipPassword()
{
    return mQSettings->value("Network/SipPassword", QString("")).toString();
}

QString Configuration::GetSipServer()
{
    return mQSettings->value("Network/SipServer", QString("sip2sip.info")).toString();
}

bool Configuration::GetSipContactsProbing()
{
    return mQSettings->value("Network/ContactsProbing", true).toBool();
}

int Configuration::GetSipInfrastructureMode()
{
    return mQSettings->value("Network/SipInfrastructureMode", 0).toInt();
}

bool Configuration::GetNatSupportActivation()
{
    return mQSettings->value("Network/NatSupportActivation", false).toBool();
}

QString Configuration::GetStunServer()
{
    return mQSettings->value("Network/StunServer", QString("stun.voipbuster.com")).toString();
}

QString Configuration::GetImSoundFile()
{
    return mQSettings->value("Notification/ImSoundFile", QString((mAbsBinPath + "Message.wav").c_str())).toString();
}

bool Configuration::GetImSound()
{
    return mQSettings->value("Notification/ImSound", false).toBool();
}

bool Configuration::GetImSystray()
{
    return mQSettings->value("Notification/ImSystray", true).toBool();
}

QString Configuration::GetCallSoundFile()
{
    return mQSettings->value("Notification/CallSoundFile", QString((mAbsBinPath + "Call.wav").c_str())).toString();
}

bool Configuration::GetCallSound()
{
    return mQSettings->value("Notification/CallSound", false).toBool();
}

bool Configuration::GetCallSystray()
{
    return mQSettings->value("Notification/CallSystray", true).toBool();
}

void Configuration::Sync()
{
    LOG(LOG_VERBOSE, "Sync");
    mQSettings->sync();
}

bool Configuration::DebuggingEnabled()
{
    return mDebuggingEnabled;
}

void Configuration::SetDebugging(bool pState)
{
    mDebuggingEnabled = pState;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
