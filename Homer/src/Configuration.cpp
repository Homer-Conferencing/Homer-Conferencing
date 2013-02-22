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
 * Purpose: Implementation of the global program settings management
 * Author:  Thomas Volkert
 * Since:   2009-07-30
 */

#include <Configuration.h>
#include <Logger.h>
#include <Meeting.h>

#include <Berkeley/SocketSetup.h>
#include <QString>
#include <QHostInfo>
#include <QDir>
#include <QDesktopWidget>
#include <QApplication>
#include <QImageReader>
#ifdef APPLE
	#include <CoreFoundation/CoreFoundation.h>
#endif

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Conference;

Configuration sConfiguration;

///////////////////////////////////////////////////////////////////////////////
// define some additional default translations

QString tDefaultDialogTranslations[] = {
QT_TRANSLATE_NOOP("QTextControl", "&Undo"),
QT_TRANSLATE_NOOP("QTextControl", "&Redo"),
QT_TRANSLATE_NOOP("QTextControl", "Cu&t"),
QT_TRANSLATE_NOOP("QTextControl", "&Copy"),
QT_TRANSLATE_NOOP("QTextControl", "Copy &Link Location"),
QT_TRANSLATE_NOOP("QTextControl", "&Paste"),
QT_TRANSLATE_NOOP("QTextControl", "Delete"),
QT_TRANSLATE_NOOP("QTextControl", "Select All"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Name"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Size"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Kind"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Type"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Date Modified"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Show Size"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Show Type"),
QT_TRANSLATE_NOOP("QFileSystemModel", "Show Date Modified"),
QT_TRANSLATE_NOOP("QFileDialog", "Computer"),
QT_TRANSLATE_NOOP("QFileDialog", "Recent Places"),
QT_TRANSLATE_NOOP("QFileDialog", "Show "),
QT_TRANSLATE_NOOP("QFileDialog", "Look in:"),
QT_TRANSLATE_NOOP("QFileDialog", "File &name:"),
QT_TRANSLATE_NOOP("QFileDialog", "Files of type:"),
QT_TRANSLATE_NOOP("QFileDialog", "Back"),
QT_TRANSLATE_NOOP("QFileDialog", "Forward"),
QT_TRANSLATE_NOOP("QFileDialog", "Parent Directory"),
QT_TRANSLATE_NOOP("QFileDialog", "Create New Folder"),
QT_TRANSLATE_NOOP("QFileDialog", "List View"),
QT_TRANSLATE_NOOP("QFileDialog", "Detail View"),
QT_TRANSLATE_NOOP("QFileDialog", "&Open"),
QT_TRANSLATE_NOOP("QFileDialog", "&Save"),
QT_TRANSLATE_NOOP("QFileDialog", "%1 already exists.\nDo you want to replace it?"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "Okay"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "Cancel"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "Reset"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "&Yes"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "&No"),
QT_TRANSLATE_NOOP("QDialogButtonBox", "Close"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Services"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide %1"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide Others"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Show All"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Preferences..."),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Quit %1"),
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "About %1")
};

///////////////////////////////////////////////////////////////////////////////

Configuration::Configuration()
{
    mAudioOutputEnabled = true;
    mAudioCaptureEnabled = true;
    mConferencingEnabled = true;
    mDebuggingEnabled = false;
    mQSettings = new QSettings("Homer Software", "Homer");
    LOG(LOG_VERBOSE, "Created");
    LOG(LOG_VERBOSE, "Program settings are stored in: %s", mQSettings->fileName().toStdString().c_str());
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

void Configuration::Init(QString &pAbsBinPath)
{
    mAbsBinPath = pAbsBinPath;
	#ifdef APPLE
		CFURLRef tAppUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFStringRef tMacPath = CFURLCopyFileSystemPath(tAppUrlRef, kCFURLPOSIXPathStyle);
		const char *tPath = CFStringGetCStringPtr(tMacPath, CFStringGetSystemEncoding());
		LOG(LOG_VERBOSE, "Path to OSX bundle: %s", tPath);
		CFRelease(tAppUrlRef);
		CFRelease(tMacPath);
		if (tPath != NULL)
		{
			mAbsBinPath = QString(tPath);
			pAbsBinPath = mAbsBinPath;
		}
	#endif
}

void Configuration::SetDefaults()
{
	LOG(LOG_VERBOSE, "Setting program defaults");
	printf("Setting program defaults\n");
	mQSettings->clear();
	Sync();
}

static int sGifIsSupported = -1;
bool Configuration::IsGifReadingSupported()
{
	if (sGifIsSupported == -1)
	{
		sGifIsSupported = 0;
		QList<QByteArray> tFormats = QImageReader::supportedImageFormats();
		QByteArray tEntry;
		foreach(tEntry, tFormats)
		{
		    QString tFormat = QString(tEntry.data()).toUpper();
			LOG(LOG_VERBOSE, "Supported image reader format: %s", tFormat.toStdString().c_str());
			if (tFormat == "GIF")
			{
				LOG(LOG_WARN, "GIF reader available");
				sGifIsSupported = 1;
			}
		}
	}
	return (sGifIsSupported == 1);
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

void Configuration::SetPreventScreensaverInFullscreenMode(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("PreventScreensaverInFullscreenMode", pActive);
    mQSettings->endGroup();
}

void Configuration::SetBroadcastAudioPlaybackMuted(bool pActive)
{
    mQSettings->beginGroup("Broadcast");
    mQSettings->setValue("AudioPlaybackMuted", pActive);
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

void Configuration::SetVisibilityNetworkSimulationWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityNetworkSimulationWidget", pActive);
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

void Configuration::SetVisibilityBroadcastMessageWidget(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityBroadcastMessageWidget", pActive);
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

void Configuration::SetVisibilityMenuBar(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityMenuBar", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityStatusBar(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityStatusBar", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityBroadcastAudio(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityBroadcastAudio", pActive);
    mQSettings->endGroup();
}

void Configuration::SetVisibilityBroadcastVideo(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("VisibilityBroadcastVideo", pActive);
    mQSettings->endGroup();
}

void Configuration::SetPreviewSelection(int pSelection)
{
    mQSettings->beginGroup("PreviewDialog");
    mQSettings->setValue("Selection", pSelection);
    mQSettings->endGroup();
}

void Configuration::SetPreviewSelectionVideo(bool pActive)
{
    mQSettings->beginGroup("PreviewDialog");
    mQSettings->setValue("SelectionVideo", pActive);
    mQSettings->endGroup();
}

void Configuration::SetPreviewSelectionAudio(bool pActive)
{
    mQSettings->beginGroup("PreviewDialog");
    mQSettings->setValue("SelectionAudio", pActive);
    mQSettings->endGroup();
}

void Configuration::SetPreviewPreBufferingActivation(bool pActivation)
{
    mQSettings->beginGroup("PreviewDialog");
    mQSettings->setValue("PreBufferingActivation", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetConfigurationSelection(int pSelection)
{
    mQSettings->beginGroup("ConfigurationDialog");
    mQSettings->setValue("Selection", pSelection);
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

void Configuration::SetFeatureConferencing(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("FeatureConferencing", pActive);
    mQSettings->endGroup();
}

void Configuration::SetFeatureAutoLogging(bool pActive)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("FeatureAutoLogging", pActive);
    mQSettings->endGroup();
}

void Configuration::SetLanguage(QString pLanguage)
{
    mQSettings->beginGroup("Global");
    mQSettings->setValue("Language", pLanguage);
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

void Configuration::SetVideoBitRate(int pBitRate)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamBitRate", pBitRate);
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

void Configuration::SetVideoStreamingNAPIImpl(QString pImpl)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("VideoStreamNAPIImpl", pImpl);
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

void Configuration::SetAudioActivationPushToTalk(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamActivationPushToTalk", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetAudioSkipSilence(bool pActivation)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamSkipSilence", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetAudioSkipSilenceThreshold(int pThreshold)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamSkipSilenceThreshold", pThreshold);
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

void Configuration::SetAudioBitRate(int pBitRate)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamBitRate", pBitRate);
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

void Configuration::SetAudioStreamingNAPIImpl(QString pImpl)
{
    mQSettings->beginGroup("Streaming");
    mQSettings->setValue("AudioStreamNAPIImpl", pImpl);
    mQSettings->endGroup();
}

void Configuration::SetAppDataTransport(enum TransportType pType)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("AppDataTransportType", QString(Socket::TransportType2String(pType).c_str()));
    mQSettings->endGroup();
}

void Configuration::SetAppDataNAPIImpl(QString pImpl)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("AppDataNAPIImpl", pImpl);
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

void Configuration::SetSipServerPort(int pPort)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipServerPort", pPort);
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

void Configuration::SetSipListenerTransport(enum TransportType pType)
{
    mQSettings->beginGroup("Network");
    mQSettings->setValue("SipListenerTransportType", QString(Socket::TransportType2String(pType).c_str()));
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

void Configuration::SetStartSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StartSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetStartSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StartSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetStartSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StartSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetStopSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StopSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetStopSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StopSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetStopSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("StopSystray", pActivation);
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

void Configuration::SetCallAcknowledgeSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallAcknowledgeSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetCallAcknowledgeSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallAcknowledgeSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallAcknowledgeSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallAcknowledgeSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallDenySoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallDenySoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetCallDenySound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallDenySound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallDenySystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallDenySystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallHangupSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallHangupSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetCallHangupSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallHangupSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetCallHangupSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("CallHangupSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetErrorSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ErrorSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetErrorSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ErrorSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetErrorSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("ErrorSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationFailedSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationFailedSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationFailedSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationFailedSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationFailedSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationFailedSystray", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationSuccessfulSoundFile(QString pSoundFile)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationSuccessfulSoundFile", pSoundFile);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationSuccessfulSound(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationSuccessfulSound", pActivation);
    mQSettings->endGroup();
}

void Configuration::SetRegistrationSuccessfulSystray(bool pActivation)
{
    mQSettings->beginGroup("Notification");
    mQSettings->setValue("RegistrationSuccessfulSystray", pActivation);
    mQSettings->endGroup();
}

QString Configuration::GetSipListenerAddress()
{
    return mQSettings->value("Network/SipListenerAddress", QString("")).toString();
}

enum TransportType Configuration::GetSipListenerTransport()
{
    return Socket::String2TransportType(mQSettings->value("Network/SipListenerTransportType", QString("auto")).toString().toStdString());
}

QString Configuration::GetBinaryPath()
{
	return mAbsBinPath;
}

QString Configuration::GetConferenceAvailability()
{
    return mQSettings->value("Global/Availability", QString(MEETING.GetAvailabilityStateStr().c_str())).toString();
}

QString Configuration::GetContactFile()
{
    return mQSettings->value("Global/ContactFile", mAbsBinPath + "Homer-Contacts.xml").toString();
}

QString Configuration::GetDataDirectory()
{
    return mQSettings->value("Global/DataDirectory", QDir::homePath()).toString();
}

QString Configuration::GetLanguagePath()
{
    QString tResult = mAbsBinPath;
    if (QString(HOMER_INSTALL_DATADIR) != "")
    {
        LOG(LOG_WARN, "HOMER_INSTALL_DATADIR is defined as %s", string(HOMER_INSTALL_DATADIR).c_str());
        tResult = QString(HOMER_INSTALL_DATADIR);
    }
    if ((!tResult.endsWith("\\")) && (!tResult.endsWith("/")))
        tResult += "/";
    tResult += "lang/";
    return tResult;
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
    return mQSettings->value("Global/ParticipantWidgetsSeparation", false).toBool();
}

bool Configuration::GetParticipantWidgetsCloseImmediately()
{
    return mQSettings->value("Global/ParticipantWidgetsCloseImmediately", true).toBool();
}

bool Configuration::GetPreventScreensaverInFullscreenMode()
{
    return mQSettings->value("Global/PreventScreensaverInFullscreenMode", true).toBool();
}

bool Configuration::GetBroadcastAudioPlaybackMuted()
{
    return mQSettings->value("Broadcast/AudioPlaybackMuted", true).toBool();
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

bool Configuration::GetVisibilityNetworkSimulationWidget()
{
    return mQSettings->value("Global/VisibilityNetworkSimulationWidget", false).toBool();
}

bool Configuration::GetVisibilityNetworkStreamsWidget()
{
    return mQSettings->value("Global/VisibilityNetworkStreamsWidget", false).toBool();
}

bool Configuration::GetVisibilityDataStreamsWidget()
{
    return mQSettings->value("Global/VisibilityDataStreamsWidget", false).toBool();
}

bool Configuration::GetVisibilityBroadcastMessageWidget()
{
    return mQSettings->value("Global/VisibilityBroadcastMessageWidget", false).toBool();
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

bool Configuration::GetVisibilityMenuBar()
{
    return mQSettings->value("Global/VisibilityMenuBar", true).toBool();
}

bool Configuration::GetVisibilityStatusBar()
{
    return mQSettings->value("Global/VisibilityStatusBar", false).toBool();
}

int Configuration::GetPreviewSelection()
{
    return mQSettings->value("PreviewDialog/Selection", 2).toInt();
}

bool Configuration::GetPreviewSelectionVideo()
{
    return mQSettings->value("PreviewDialog/SelectionVideo", true).toBool();
}

bool Configuration::GetPreviewSelectionAudio()
{
    return mQSettings->value("PreviewDialog/SelectionAudio", true).toBool();
}

bool Configuration::GetPreviewPreBufferingActivation()
{
    return mQSettings->value("PreviewDialog/PreBufferingActivation", true).toBool();
}

int Configuration::GetConfigurationSelection()
{
    return mQSettings->value("ConfigurationDialog/Selection", 0).toInt();
}

bool Configuration::GetSmoothVideoPresentation()
{
	bool tDefault = false;
	#ifdef APPLE
		// for APPLE environments, Qt uses accelerated functions for smooth picture scaling
		tDefault = true;
	#endif
    return mQSettings->value("Global/SmoothVideoPresentation", tDefault).toBool();
}

bool Configuration::GetVisibilityBroadcastAudio()
{
    return mQSettings->value("Global/VisibilityBroadcastAudio", true).toBool();
}

bool Configuration::GetVisibilityBroadcastVideo()
{
    return mQSettings->value("Global/VisibilityBroadcastVideo", true).toBool();
}

bool Configuration::GetAutoUpdateCheck()
{
    return mQSettings->value("Global/AutomaticUpdateCheck", false).toBool();
}

bool Configuration::GetFeatureConferencing()
{
    return mQSettings->value("Global/FeatureConferencing", true).toBool();
}

bool Configuration::GetFeatureAutoLogging()
{
    return mQSettings->value("Global/FeatureAutoLogging", false).toBool();
}

QString Configuration::GetLanguage()
{
	return mQSettings->value("Global/Language", GetSystemLanguage()).toString();
}

QString Configuration::GetSystemLanguage()
{
    QString tSystemDefaultLang = QLocale::system().name();
    tSystemDefaultLang.truncate(tSystemDefaultLang.lastIndexOf('_'));
    LOG(LOG_VERBOSE, "Current system language is: %s", tSystemDefaultLang.toStdString().c_str());

    return tSystemDefaultLang;
}

QString Configuration::GetUserName()
{
    return mQSettings->value("User/UserName", QString::fromAscii(MEETING.GetUserName().c_str())).toString();
}

QString Configuration::GetUserMail()
{
    return mQSettings->value("User/UserMail", QString(MEETING.SipCreateId(MEETING.GetUserName(), QHostInfo::localHostName().toStdString()).c_str())).toString();
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
    return mQSettings->value("Streaming/VideoStreamCodec", QString("H.263+")).toString();
}

int Configuration::GetVideoQuality()
{
    return mQSettings->value("Streaming/VideoStreamQuality", 10).toInt();
}

int Configuration::GetVideoBitRate()
{
    return mQSettings->value("Streaming/VideoStreamBitRate", 90 * 1024).toInt();
}

int Configuration::GetVideoMaxPacketSize()
{
    return mQSettings->value("Streaming/VideoStreamMaxPacketSize", 1280).toInt();
}

enum TransportType Configuration::GetVideoTransportType()
{
    return Socket::String2TransportType(mQSettings->value("Streaming/VideoStreamTransportType", QString("UDP")).toString().toStdString());
}

QString Configuration::GetVideoStreamingNAPIImpl()
{
    return mQSettings->value("Streaming/VideoStreamNAPIImpl", QString(BERKEYLEY_SOCKETS)).toString();
}

QString Configuration::GetVideoResolution()
{
    return mQSettings->value("Streaming/VideoStreamResolution", QString("auto")).toString();
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

bool Configuration::GetAudioActivationPushToTalk()
{
    return mQSettings->value("Streaming/AudioStreamActivationPushToTalk", false).toBool();
}

bool Configuration::GetAudioSkipSilence()
{
    return mQSettings->value("Streaming/AudioStreamSkipSilence", false).toBool();
}

int Configuration::GetAudioSkipSilenceThreshold()
{
    return mQSettings->value("Streaming/AudioStreamSkipSilenceThreshold", 128).toInt();
}

bool Configuration::GetAudioRtp()
{
    return mQSettings->value("Streaming/AudioStreamRtp", true).toBool();
}

QString Configuration::GetAudioCodec()
{
    return mQSettings->value("Streaming/AudioStreamCodec", QString("G722 adpcm")).toString();
}

int Configuration::GetAudioBitRate()
{
    return mQSettings->value("Streaming/AudioStreamBitRate", 256 * 1024).toInt();
}

int Configuration::GetAudioMaxPacketSize()
{
    return mQSettings->value("Streaming/AudioStreamMaxPacketSize", 1280).toInt();
}

enum TransportType Configuration::GetAudioTransportType()
{
    return Socket::String2TransportType(mQSettings->value("Streaming/AudioStreamTransportType", QString("UDP")).toString().toStdString());
}

QString Configuration::GetAudioStreamingNAPIImpl()
{
    return mQSettings->value("Streaming/AudioStreamNAPIImpl", QString(BERKEYLEY_SOCKETS)).toString();
}

enum TransportType Configuration::GetAppDataTransportType()
{
    return Socket::String2TransportType(mQSettings->value("Network/AppDataTransportType", QString("UDP")).toString().toStdString());
}

QString Configuration::GetAppDataNAPIImpl()
{
    return mQSettings->value("Network/AppDataNAPIImpl", QString(BERKEYLEY_SOCKETS)).toString();
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
    return mQSettings->value("Network/SipUserName", QString::fromAscii(MEETING.GetUserName().c_str())).toString();
}

QString Configuration::GetSipPassword()
{
    return mQSettings->value("Network/SipPassword", QString("")).toString();
}

QString Configuration::GetSipServer()
{
    return mQSettings->value("Network/SipServer", QString("sip2sip.info")).toString();
}

int Configuration::GetSipServerPort()
{
    return mQSettings->value("Network/SipServerPort", 5060).toInt();
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
    return mQSettings->value("Network/NatSupportActivation", true).toBool();
}

QString Configuration::GetStunServer()
{
    return mQSettings->value("Network/StunServer", QString("stun.voipbuster.com")).toString();
}

QString Configuration::GetStartSoundFile()
{
    return mQSettings->value("Notification/StartSoundFile", mAbsBinPath + "sounds/Start.wav").toString();
}

bool Configuration::GetStartSound()
{
    return mQSettings->value("Notification/StartSound", false).toBool();
}

bool Configuration::GetStartSystray()
{
    return mQSettings->value("Notification/StartSystray", true).toBool();
}

QString Configuration::GetStopSoundFile()
{
    return mQSettings->value("Notification/StopSoundFile", mAbsBinPath + "sounds/Stop.wav").toString();
}

bool Configuration::GetStopSound()
{
    return mQSettings->value("Notification/StopSound", false).toBool();
}

bool Configuration::GetStopSystray()
{
    return mQSettings->value("Notification/StopSystray", true).toBool();
}

QString Configuration::GetImSoundFile()
{
    return mQSettings->value("Notification/ImSoundFile", mAbsBinPath + "sounds/Message.wav").toString();
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
    return mQSettings->value("Notification/CallSoundFile", mAbsBinPath + "sounds/Call.wav").toString();
}

bool Configuration::GetCallSound()
{
    return mQSettings->value("Notification/CallSound", false).toBool();
}

bool Configuration::GetCallSystray()
{
    return mQSettings->value("Notification/CallSystray", true).toBool();
}

QString Configuration::GetCallAcknowledgeSoundFile()
{
    return mQSettings->value("Notification/CallAcknowledgeSoundFile", mAbsBinPath + "sounds/Call_Acknowledge.wav").toString();
}

bool Configuration::GetCallAcknowledgeSound()
{
    return mQSettings->value("Notification/CallAcknowledgeSound", false).toBool();
}

bool Configuration::GetCallAcknowledgeSystray()
{
    return mQSettings->value("Notification/CallAcknowledgeSystray", true).toBool();
}

QString Configuration::GetCallDenySoundFile()
{
    return mQSettings->value("Notification/CallDenySoundFile", mAbsBinPath + "sounds/Call_Deny.wav").toString();
}

bool Configuration::GetCallDenySound()
{
    return mQSettings->value("Notification/CallDenySound", false).toBool();
}

bool Configuration::GetCallDenySystray()
{
    return mQSettings->value("Notification/CallDenySystray", true).toBool();
}

QString Configuration::GetCallHangupSoundFile()
{
    return mQSettings->value("Notification/CallHangupSoundFile", mAbsBinPath + "sounds/Call_Hangup.wav").toString();
}

bool Configuration::GetCallHangupSound()
{
    return mQSettings->value("Notification/CallHangupSound", false).toBool();
}

bool Configuration::GetCallHangupSystray()
{
    return mQSettings->value("Notification/CallHangupSystray", true).toBool();
}

QString Configuration::GetErrorSoundFile()
{
    return mQSettings->value("Notification/ErrorSoundFile", mAbsBinPath + "sounds/Error.wav").toString();
}

bool Configuration::GetErrorSound()
{
    return mQSettings->value("Notification/ErrorSound", false).toBool();
}

bool Configuration::GetErrorSystray()
{
    return mQSettings->value("Notification/ErrorSystray", true).toBool();
}

QString Configuration::GetRegistrationFailedSoundFile()
{
    return mQSettings->value("Notification/RegistrationFailedSoundFile", mAbsBinPath + "sounds/Registration_Failed.wav").toString();
}

bool Configuration::GetRegistrationFailedSound()
{
    return mQSettings->value("Notification/RegistrationFailedSound", false).toBool();
}

bool Configuration::GetRegistrationFailedSystray()
{
    return mQSettings->value("Notification/RegistrationFailedSystray", true).toBool();
}

QString Configuration::GetRegistrationSuccessfulSoundFile()
{
    return mQSettings->value("Notification/RegistrationSuccessfulSoundFile", mAbsBinPath + "sounds/Registration_Successful.wav").toString();
}

bool Configuration::GetRegistrationSuccessfulSound()
{
    return mQSettings->value("Notification/RegistrationSuccessfulSound", false).toBool();
}

bool Configuration::GetRegistrationSuccessfulSystray()
{
    return mQSettings->value("Notification/RegistrationSuccessfulSystray", true).toBool();
}

void Configuration::Sync()
{
    LOG(LOG_VERBOSE, "Synch. program settings with: %s", mQSettings->fileName().toStdString().c_str());
    printf("Synch. program settings with: %s\n", mQSettings->fileName().toStdString().c_str());
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

bool Configuration::AudioOutputEnabled()
{
    return mAudioOutputEnabled;
}

void Configuration::DisableAudioOutput()
{
    printf("Audio output disabled\n");
    mAudioOutputEnabled = false;
}

bool Configuration::AudioCaptureEnabled()
{
    return mAudioCaptureEnabled;
}

void Configuration::DisableAudioCapture()
{
    printf("Audio capture disabled\n");
    mAudioCaptureEnabled = false;
}

bool Configuration::ConferencingEnabled()
{
    return mConferencingEnabled;
}

void Configuration::DisableConferencing()
{
    printf("Conference functions disabled\n");
    mConferencingEnabled = false;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
