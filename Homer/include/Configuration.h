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
 * Purpose: global program settings
 * Author:  Thomas Volkert
 * Since:   2009-07-30
 */

#ifndef _CONFIGURATION_
#define _CONFIGURATION_

#include <MainWindow.h>
#include <Dialogs/IdentityDialog.h>
#include <Dialogs/ConfigurationDialog.h>
#include <Dialogs/UpdateCheckDialog.h>
#include <Dialogs/OpenVideoAudioPreviewDialog.h>
#include <Widgets/VideoWidget.h>
#include <Widgets/StreamingControlWidget.h>
#include <Widgets/OverviewContactsWidget.h>
#include <Widgets/OverviewDataStreamsWidget.h>
#include <Widgets/OverviewErrorsWidget.h>
#include <Widgets/OverviewFileTransfersWidget.h>
#include <Widgets/OverviewNetworkStreamsWidget.h>
#include <Widgets/OverviewThreadsWidget.h>
#include <Widgets/ParticipantWidget.h>
#include <Widgets/StreamingControlWidget.h>

#include <string>

#include <QSettings>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

// select if it is a release or development version
//#define RELEASE_VERSION

// version string which is checked/used in the whole application
#define RELEASE_VERSION_STRING          "Beta 0.19"

// for home calls
#define RELEASE_SERVER                  "www.homer-conferencing.com"
#define PATH_VERSION_TXT                "/live/version.txt"
#define PATH_CHANGELOG_TXT              "/live/changelog.txt"
#define PATH_HELP_TXT                   "/live/help.txt"
#define PATH_INSTALL_EXE				CONF.GetBinaryPath() + "install.exe"
#define PATH_HOMER_RELEASES				"http://"RELEASE_SERVER"/releases/"

#define CONF Configuration::GetInstance()

///////////////////////////////////////////////////////////////////////////////
class Configuration
{
public:
    Configuration();

    virtual ~Configuration();

    static Configuration& GetInstance();

    void Init(string pAbsBinPath);

    /* general settings */
    QString GetBinaryPath();
    QString GetConferenceAvailability();
    QString GetContactFile();
    QString GetDataDirectory();

    QPoint GetMainWindowPosition();
    QSize GetMainWindowSize();

    bool GetVisibilityContactsWidget();
    bool GetVisibilityFileTransfersWidget();
    bool GetVisibilityErrorsWidget();

    bool GetVisibilityPlaylistWidgetAudio();
    bool GetVisibilityPlaylistWidgetVideo();
    bool GetVisibilityPlaylistWidgetMovie();

    bool GetVisibilityThreadsWidget();
    bool GetVisibilityNetworkStreamsWidget();
    bool GetVisibilityDataStreamsWidget();
    bool GetVisibilityBroadcastWidget();

    bool GetVisibilityToolBarMediaSources();
    bool GetVisibilityToolBarOnlineStatus();

    int GetColoringScheme();
    bool GetParticipantWidgetsSeparation();
    bool GetParticipantWidgetsCloseImmediately();

    bool GetSmoothVideoPresentation();
    bool GetAutoUpdateCheck();

    /* user settings */
    QString GetUserName();
    QString GetUserMail();

    /* video settings */
    bool GetVideoActivation();
    bool GetVideoRtp();
    QString GetVideoCodec();
    int GetVideoQuality();
    int GetVideoMaxPacketSize();
    enum Homer::Base::TransportType GetVideoTransportType();
    QString GetLocalVideoSource();
    int GetVideoFps();
    QString GetVideoResolution();
    bool GetLocalVideoSourceHFlip();
    bool GetLocalVideoSourceVFlip();

    /* audio settings */
    bool GetAudioActivation();
    bool GetAudioRtp();
    QString GetAudioCodec();
    int GetAudioQuality();
    int GetAudioMaxPacketSize();
    enum Homer::Base::TransportType GetAudioTransportType();
    QString GetLocalAudioSource();

    /* playback settings */
    QString GetLocalAudioSink();

    /* network settings */
    int GetVideoAudioStartPort();
    int GetSipStartPort();
    QString GetSipUserName();
    QString GetSipPassword();
    QString GetSipServer();
    bool GetSipContactsProbing();
    QString GetSipListenerAddress();
    int GetSipInfrastructureMode();
    bool GetNatSupportActivation();
    QString GetStunServer();

    /* notification settings */
    QString GetImSoundFile();
    bool GetImSound();
    bool GetImSystray();
    QString GetCallSoundFile();
    bool GetCallSound();
    bool GetCallSystray();

    /* additional static values */
    int GetSystrayTimeout(){ return 6000; /* ms */ }
    int GetContactPresenceCheckPeriod(){ return 60*1000; /* ms */ }

    /* debugging state machine */
    bool DebuggingEnabled();

private:
    friend class MainWindow;
    friend class ConfigurationDialog;
    friend class ContactsPool;
    friend class IdentityDialog;
    friend class UpdateCheckDialog;
    friend class AddNetworkSinkDialog;
    friend class MessageWidget;
    friend class OpenVideoAudioPreviewDialog;
    friend class OverviewContactsWidget;
    friend class OverviewErrorsWidget;
    friend class OverviewFileTransfersWidget;
    friend class OverviewNetworkStreamsWidget;
    friend class OverviewPlaylistWidget;
    friend class OverviewThreadsWidget;
    friend class OverviewDataStreamsWidget;
    friend class ParticipantWidget;
    friend class StreamingControlWidget;
    friend class VideoWidget;

    /* global settings */
    void SetConferenceAvailability(QString pState);
    void SetContactFile(QString pContactFile);
    void SetDataDirectory(QString pDataDirectory);
    void SetMainWindowPosition(QPoint pPos);
    void SetMainWindowSize(QSize pSize);
    void SetParticipantWidgetsSeparation(bool pSepWins);
    void SetParticipantWidgetsCloseImmediately(bool pActive);
    void SetVisibilityContactsWidget(bool pActive);
    void SetVisibilityErrorsWidget(bool pActive);
    void SetVisibilityFileTransfersWidget(bool pActive);

    void SetVisibilityPlaylistWidgetAudio(bool pActive);
    void SetVisibilityPlaylistWidgetVideo(bool pActive);
    void SetVisibilityPlaylistWidgetMovie(bool pActive);

    void SetVisibilityThreadsWidget(bool pActive);
    void SetVisibilityNetworkStreamsWidget(bool pActive);
    void SetVisibilityDataStreamsWidget(bool pActive);
    void SetVisibilityBroadcastWidget(bool pActive);

    void SetVisibilityToolBarMediaSources(bool pActive);
    void SetVisibilityToolBarOnlineStatus(bool pActive);

    void SetSmoothVideoPresentation(bool pActive);
    void SetAutoUpdateCheck(bool pActive);
    void SetColoringScheme(int pColoringScheme);

    /* user settings */
    void SetUserName(QString pUserName);
    void SetUserMail(QString pUserMail);

    /* video settings */
    void SetVideoActivation(bool pActivation);
    void SetVideoRtp(bool pActivation);
    void SetVideoCodec(QString pCodec);
    void SetVideoQuality(int pQuality);
    void SetVideoMaxPacketSize(int pSize);
    void SetVideoTransport(enum Homer::Base::TransportType pType);
    void SetVideoResolution(QString pResolution);
    void SetLocalVideoSource(QString pVSource);
    void SetVideoFps(int pFps);
    void SetLocalVideoSourceHFlip(bool pHFlip);
    void SetLocalVideoSourceVFlip(bool pVFlip);

    /* audio settings */
    void SetAudioActivation(bool pActivation);
    void SetAudioRtp(bool pActivation);
    void SetAudioCodec(QString pCodec);
    void SetAudioQuality(int pQuality);
    void SetAudioMaxPacketSize(int pSize);
    void SetAudioTransport(enum Homer::Base::TransportType pType);
    void SetLocalAudioSource(QString pASource);

    /* playback settings */
    void SetLocalAudioSink(QString pASink);

    /* network settings */
    void SetVideoAudioStartPort(int pPort);
    void SetSipStartPort(int pPort);
    void SetSipUserName(QString pUserName);
    void SetSipPassword(QString pPassword);
    void SetSipServer(QString pServer);
    void SetSipInfrastructureMode(int pMode);
    void SetSipContactsProbing(bool pActivation);
    void SetSipListenerAddress(QString pAddress);
    void SetStunServer(QString pServer);
    void SetNatSupportActivation(bool pActivation);

    /* notification settings */
    void SetImSoundFile(QString pSoundFile);
    void SetImSound(bool pActivation);
    void SetImSystray(bool pActivation);
    void SetCallSoundFile(QString pSoundFile);
    void SetCallSound(bool pActivation);
    void SetCallSystray(bool pActivation);


    /* debugging state machine */
    void SetDebugging(bool pState = true);

    // important because some write operations could be delayed
    void Sync();

    std::string             mAbsBinPath;
    QSettings               *mQSettings;
    bool                    mDebuggingEnabled;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
