/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Main window header
 * Author:  Thomas Volkert
 * Since:   2008-11-25
 */

#ifndef _MAIN_WINDOW_
#define _MAIN_WINDOW_

#include <MediaSourceMuxer.h>
#include <MediaSourceDesktop.h>
#include <MediaSourceLogo.h>
#include <Header_NetworkSimulator.h>
#include <Widgets/AudioWidget.h>
#include <Widgets/AvailabilityWidget.h>
#include <Widgets/StreamingControlWidget.h>
#include <Widgets/MessageWidget.h>
#include <Widgets/OverviewContactsWidget.h>
#include <Widgets/OverviewDataStreamsWidget.h>
#include <Widgets/OverviewErrorsWidget.h>
#include <Widgets/OverviewNetworkStreamsWidget.h>
#include <Widgets/OverviewThreadsWidget.h>
#include <Widgets/OverviewFileTransfersWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <Widgets/ParticipantWidget.h>
#include <Widgets/VideoWidget.h>
#include <AudioPlayback.h>
#include <Meeting.h>
#include <MeetingEvents.h>

#include <QMeetingEvents.h>

#include <QMainWindow>
#include <QMenu>
#include <QMutex>
#include <QSettings>
#include <QDockWidget>
#include <QTimer>
#include <QHttp>
#include <QSystemTrayIcon>
#include <QShortcut>
#include <QTranslator>

#include <list>

#include <ui_MainWindow.h>

namespace Homer {
namespace Gui {

using namespace Homer::Multimedia;
using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

#define SCREEN_CAPTURE_FPS					(29.97)

///////////////////////////////////////////////////////////////////////////////
class StreamingControlWidget;
class MainWindow:
        public QMainWindow,
        AudioPlayback,
        public Ui_MainWindow,
        public MeetingObserver
{
Q_OBJECT
    ;
public:
    /// The default constructor
    MainWindow(QStringList pArguments, QString pAbsBinPath);

    /// The destructor
    virtual ~MainWindow();

    MediaSourceMuxer* GetVideoMuxer();
    MediaSourceMuxer* GetAudioMuxer();

    static void removeArguments(QStringList &pArguments, QString pFilter);

public slots:
    void actionOpenVideoAudioPreview();

private slots:
    void actionExit();

    void actionIdentity();
    void actionConfiguration();

    void actionHelp();
    void actionUpdateCheck();
    void actionVersion();

    void actionToggleWindowState();
    void actionMuteMe();
    void actionMuteOthers();

    void actionActivateToolBarOnlineStatus(bool pActive);
    void actionActivateToolBarMediaSources(bool pActive);
    void actionActivateStatusBar(bool pActive);
    void actionActivateMenuBar(bool pActive);
    void actionActivateDebuggingWidgets();
    void actionActivateDebuggingGlobally();
    void actionActivateNetworkSimulationWidgets();
    void actionActivateMosaicMode(bool pActive);

    void activatedSysTray(QSystemTrayIcon::ActivationReason pReason);

    void GotAnswerForVersionRequest(bool pError);
    void CreateScreenShot();

    void RegisterAtStunSipServer();
    void UpdateSysTrayContextMenu();

private:
    void initializeConfiguration(QStringList &pArguments);
    void initializeGUI();
    void initializeLanguage();
    void initializeFeatureDisablers(QStringList &pArguments);
    void initializeDebugging(QStringList &pArguments);
    void ShowFfmpegCaps(QStringList &pArguments);
    void initializeConferenceManagement();
    void initializeVideoAudioIO();
    void initializeColoring();
    void initializeWidgetsAndMenus();
    void initializeScreenCapturing();
    void initializeNetworkSimulator(QStringList &pArguments, bool pForce = false);
    void ProcessRemainingArguments(QStringList &pArguments);
    void connectSignalsSlots();

    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual void keyReleaseEvent(QKeyEvent *pEvent);
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    virtual void dropEvent(QDropEvent *pEvent);
    virtual void changeEvent (QEvent *pEvent);
    virtual void customEvent(QEvent* pEvent);

    void triggerUpdateCheck();

    void SetLanguage(QString pLanguage);
    void CreateSysTray();
    void loadSettings();
    bool GetNetworkInfo(LocalAddressesList &pLocalAddressesList, QString &pLocalSourceIp, QString &pLocalLoopIp);
    QString CompleteIpAddress(QString pAddr);
    ParticipantWidget* AddParticipantSession(QString pUser, QString pHost, QString pPort, enum TransportType pTransport, QString pIp, int pInitState);
    void DeleteParticipantSession(ParticipantWidget *pParticipantWidget);

    /* handle incoming Meeting events */
    void GetEventSource(GeneralEvent *pEvent, QString &pSender, QString &pSenderApp);
    virtual void handleMeetingEvent(GeneralEvent *pEvent);

    QHttp           		    *mHttpGetVersionServer;
    QString		 			    mAbsBinPath;
    AvailabilityWidget 		    *mOnlineStatusWidget;
    StreamingControlWidget 	    *mMediaSourcesControlWidget;
    LocalAddressesList 		    mLocalAddresses;
    OverviewContactsWidget 	    *mOverviewContactsWidget;
    OverviewDataStreamsWidget   *mOverviewDataStreamsWidget;
    OverviewErrorsWidget        *mOverviewErrorsWidget;
    OverviewFileTransfersWidget *mOverviewFileTransfersWidget;
    OverviewNetworkStreamsWidget *mOverviewNetworkStreamsWidget;
    OverviewPlaylistWidget	    *mOverviewPlaylistWidget;
    OverviewThreadsWidget 	    *mOverviewThreadsWidget;
    ParticipantWidgetList 	    mParticipantWidgets;
    ParticipantWidget 		    *mLocalUserParticipantWidget;
    MediaSourceMuxer 		    *mOwnVideoMuxer;
    MediaSourceMuxer 		    *mOwnAudioMuxer;
    QTimer 					    *mScreenShotTimer;
    QSystemTrayIcon			    *mSysTrayIcon;
    QMenu					    *mSysTrayMenu, *mDockMenu /* OSX dock menu */;
    MediaSourceDesktop 		    *mMediaSourceDesktop;
    MediaSourceLogo				*mMediaSourceLogo;
    QShortcut                   *mShortcutActivateDebugWidgets, *mShortcutActivateDebuggingGlobally, *mShortcutActivateNetworkSimulationWidgets;
    /* SIP server registration */
    QString                     mSipServerRegistrationHost;
    QString                     mSipServerRegistrationPort;
    QString                     mSipServerRegistrationUser;
    /* event handling */
    static bool                 mShuttingDown;
    static bool                 mStarting;
    /* program timing */
    QTime                       mStartTime;
    /* network simulator */
    #if HOMER_NETWORK_SIMULATOR
        NetworkSimulator            *mNetworkSimulator;
    #endif
	/* multi language support */
	QString						mCurrentLanguage;
	QTranslator					*mTranslator;
	/* Mosaic mode */
	QFlags<Qt::WindowType>		mMosaicModeFormerWindowFlags;
	bool						mMosaicModeToolBarOnlineStatusWasVisible;
	bool						mMosaicModeToolBarMediaSourcesWasVisible;
    QPalette					mMosaicOriginalPalette;
};

///////////////////////////////////////////////////////////////////////////////

}
}

#endif
