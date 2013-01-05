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
 * Purpose: Implementation of MainWindow.h
 * Author:  Thomas Volkert
 * Since:   2008-11-25
 */
#include <string>

#include <ContactsManager.h>
#include <MainWindow.h>
#include <Configuration.h>
#include <Dialogs/AddNetworkSinkDialog.h>
#include <Dialogs/VersionDialog.h>
#include <Dialogs/IdentityDialog.h>
#include <Dialogs/ConfigurationDialog.h>
#include <Dialogs/UpdateCheckDialog.h>
#include <Dialogs/HelpDialog.h>
#include <Widgets/VideoWidget.h>
#include <Widgets/MessageWidget.h>
#include <Widgets/ParticipantWidget.h>
#include <Widgets/AvailabilityWidget.h>
#include <Widgets/OverviewErrorsWidget.h>
#include <Widgets/OverviewFileTransfersWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <Widgets/OverviewThreadsWidget.h>
#include <Logger.h>
#include <Meeting.h>
#include <MediaSourcePortAudio.h>
#include <MediaSourceV4L2.h>
#include <MediaSourceDShow.h>
#include <MediaSourceCoreVideo.h>
#include <MediaSourceMuxer.h>
#include <MediaSourceFile.h>
#include <MediaSourceMMSys.h>
#include <MediaSourceDesktop.h>
#include <MediaSourceLogo.h>
#include <Header_NetworkSimulator.h>
#include <ProcessStatisticService.h>
#include <Snippets.h>

#include <QPlastiqueStyle>
#include <QApplication>
#include <QTime>
#include <QFile>
#include <QTimer>
#include <QTextEdit>
#include <QMenu>
#include <QScrollBar>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QStatusBar>
#include <QStringList>
#include <QLabel>

using namespace Homer::Monitor;
using namespace Homer::Multimedia;
using namespace Homer::Conference;

#ifdef APPLE
    // manually declare qt_mac_set_dock_menu(QMenu*) to set a custom dock menu
    extern void qt_mac_set_dock_menu(QMenu *); // http://doc.trolltech.com/qq/qq18-macfeatures.html
#endif

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define IPV6_LINK_LOCAL_PREFIX                          "fe80" //TODO: check the range FE80-FFFF (link local, site local, multicast)

///////////////////////////////////////////////////////////////////////////////

bool MainWindow::mShuttingDown = false;
bool MainWindow::mStarting = true;

///////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(QStringList pArguments, QString pAbsBinPath) :
    QMainWindow(),
    Ui_MainWindow(),
    MeetingObserver()
{
    mStartTime = QTime::currentTime();
    QApplication::setWindowIcon(QPixmap(":/images/LogoHomer3.png"));

    SVC_PROCESS_STATISTIC.AssignThreadName("Qt-MainLoop");
    mDockMenu= NULL;
    mSysTrayMenu = NULL;
    mAbsBinPath = pAbsBinPath;
    mMediaSourceDesktop = NULL;
    mMediaSourceLogo = NULL;
    mCurrentLanguage = "";
    mTranslator = NULL;
    #if HOMER_NETWORK_SIMULATOR
        mNetworkSimulator = NULL;
    #endif
    mOverviewContactsWidget = NULL;
    mOverviewFileTransfersWidget = NULL;
    mOnlineStatusWidget = NULL;

    QCoreApplication::setApplicationName("Homer");
    QCoreApplication::setApplicationVersion(HOMER_VERSION);

    // init log sinks
    initializeDebugging(pArguments);

    // verbose output of arguments
    QString tArg;
    int i = 0;
    foreach(tArg, pArguments)
    {
        LOG(LOG_VERBOSE, "Argument[%d]: \"%s\"", i++, tArg.toStdString().c_str());
    }
    //remove the self pointer
    pArguments.erase(pArguments.begin());
    // init program configuration
    initializeConfiguration(pArguments);
    // disabling of features
    initializeFeatureDisablers(pArguments);
    // show ffmpeg data
    ShowFfmpegCaps(pArguments);

    // audio playback - start sound
    #ifndef DEBUG_VERSION
        LOG(LOG_VERBOSE, "Playing start sound..");
        OpenPlaybackDevice("Start/stop");
        if (CONF.GetStartSound())
            StartAudioPlayback(CONF.GetStartSoundFile());
    #endif

    // create basic GUI objects
    initializeGUI();
    // set language
    initializeLanguage();
    // retrieve info about network devices and init Meeting
    initializeConferenceManagement();
    // init audio/video muxers
    initializeVideoAudioIO();
    // configure Meeting and video/audio muxers
    loadSettings();
    // create additional widgets and menus
    initializeWidgetsAndMenus();
    // set coloring for GUI objects
    initializeColoring();
    // init contact database
    CONTACTS.Init(CONF.GetContactFile().toStdString());
    // connect signals and slots, set visibility of some GUI objects
    connectSignalsSlots();
    // auto update check
    triggerUpdateCheck();
    // init screen capturing
    initializeScreenCapturing();
    // init network simulator
    initializeNetworkSimulator(pArguments);
    // delayed call to register at Stun and Sip server
    QTimer::singleShot(2000, this, SLOT(registerAtStunSipServer()));
    ProcessRemainingArguments(pArguments);
    mStarting = false;
}

MainWindow::~MainWindow()
{
    LOG(LOG_VERBOSE, "Destroyed");

    // sometimes the Qt event loops blocks, we force an exit here
    exit(0);
}

void MainWindow::removeArguments(QStringList &pArguments, QString pFilter)
{
    QString tArgument;
    QStringList::iterator tIt;
    for (tIt = pArguments.begin(); tIt != pArguments.end(); tIt++)
    {
        tArgument = *tIt;
        if (tArgument.contains(pFilter))
            tIt = pArguments.erase(tIt);
        if (tIt == pArguments.end())
            break;
    }
}

void MainWindow::initializeConfiguration(QStringList &pArguments)
{
	LOG(LOG_INFO, "Homer Conferencing, version "RELEASE_VERSION_STRING", compiled on "__DATE__" at "__TIME__);
    #ifdef RELEASE_VERSION
        LOG(LOG_VERBOSE, "This is the RELEASE version");
    #endif
    #ifdef DEBUG_VERSION
        LOG(LOG_VERBOSE, "This is the DEBUG version");
    #endif
    CONF.Init(mAbsBinPath);
    if (pArguments.contains("-SetDefaults"))
        CONF.SetDefaults();

    removeArguments(pArguments, "-SetDefaults");
}

void MainWindow::initializeGUI()
{
    LOG(LOG_VERBOSE, "Initialization of GUI..");
    setupUi(this);
    setWindowTitle("Homer Conferencing "HOMER_VERSION);
    move(CONF.GetMainWindowPosition());
    resize(CONF.GetMainWindowSize());
}

void MainWindow::initializeLanguage()
{
	LOG(LOG_VERBOSE, "Initialization of Language..");
	mTranslator = new QTranslator(this);
	SetLanguage(CONF.GetLanguage());
}

void MainWindow::SetLanguage(QString pLanguage)
{
	if (mCurrentLanguage != pLanguage)
	{
		LOG(LOG_VERBOSE, "Setting language: %s", pLanguage.toStdString().c_str());

		// remove old translator
		if ((mCurrentLanguage != "") && (mTranslator != NULL))
		{
			LOG(LOG_VERBOSE, "Destroying old translator..");
			QCoreApplication::removeTranslator(mTranslator);
		}

		if (pLanguage != "en")
		{// we need translation file
			// create new translator
			QString tNewLangFile = DATADIR "/lang/Homer_" + pLanguage + ".qm";
			LOG(LOG_VERBOSE, "Loading new translation from file: %s", tNewLangFile.toStdString().c_str());
			if(mTranslator->load(tNewLangFile))
			{
				QCoreApplication::installTranslator(mTranslator);
			}else
			{
				LOG(LOG_ERROR, "Error when loading new translation from file: %s", tNewLangFile.toStdString().c_str());
				pLanguage = "";
			}
		}else
		{// we use default language (English)

		}
		mCurrentLanguage = pLanguage;
	}
}

void MainWindow::ProcessRemainingArguments(QStringList &pArguments)
{
    bool tFirst = true;
    QString tArgument;
    foreach(tArgument, pArguments)
    {
        if ((tFirst) && (tArgument != ""))
        {
            tFirst = false;
            PLAYLISTWIDGET.AddEntry(tArgument, true);
        }else
        {
            LOG(LOG_VERBOSE, "Found unknown command line argument: %s", tArgument.toStdString().c_str());
            printf("Found unknown command line argument: %s\n", tArgument.toStdString().c_str());
        }
    }
}

void MainWindow::connectSignalsSlots()
{
    LOG(LOG_VERBOSE, "Connecting signals and slots of GUI..");
    addAction(mActionExit); // this action will also be available even if the main menu is hidden
    connect(mActionExit, SIGNAL(triggered()), this, SLOT(actionExit()));

    addAction(mActionIdentity); // this action will also be available even if the main menu is hidden
    connect(mActionIdentity, SIGNAL(triggered()), this, SLOT(actionIdentity()));
    addAction(mActionConfiguration); // this action will also be available even if the main menu is hidden
    connect(mActionConfiguration, SIGNAL(triggered()), this, SLOT(actionConfiguration()));

    connect(mActionHelp, SIGNAL(triggered()), this, SLOT(actionHelp()));
    connect(mActionUpdateCheck, SIGNAL(triggered()), this, SLOT(actionUpdateCheck()));
    connect(mActionVersion, SIGNAL(triggered()), this, SLOT(actionVersion()));

    connect(mShortcutActivateDebugWidgets, SIGNAL(activated()), this, SLOT(actionActivateDebuggingWidgets()));
    connect(mShortcutActivateNetworkSimulationWidgets, SIGNAL(activated()), this, SLOT(actionActivateNetworkSimulationWidgets()));
    connect(mShortcutActivateDebuggingGlobally, SIGNAL(activated()), this, SLOT(actionActivateDebuggingGlobally()));

    connect(mActionToolBarMediaSources, SIGNAL(toggled(bool)), this, SLOT(actionActivateToolBarMediaSources(bool)));
    connect(mToolBarMediaSources->toggleViewAction(), SIGNAL(toggled(bool)), mActionToolBarMediaSources, SLOT(setChecked(bool)));

    connect(mActionStautsBarWidget, SIGNAL(toggled(bool)), this, SLOT(actionActivateStatusBar(bool)));
    addAction(mActionMainMenu); // so, this actio will also be available even if the main menu is hidden
    connect(mActionMainMenu, SIGNAL(toggled(bool)), this, SLOT(actionActivateMenuBar(bool)));
    addAction(mActionMonitorBroadcastWidget); // so, this actio will also be available even if the main menu is hidden
    connect(mActionMonitorBroadcastWidget, SIGNAL(toggled(bool)), mLocalUserParticipantWidget, SLOT(setVisible(bool)));
    connect(mActionMosaicMode, SIGNAL(toggled(bool)), this, SLOT(actionActivateMosaicMode(bool)));

    mActionMonitorBroadcastWidget->setChecked(CONF.GetVisibilityBroadcastWidget());

    mToolBarMediaSources->setVisible(CONF.GetVisibilityToolBarMediaSources());
    mToolBarMediaSources->toggleViewAction()->setChecked(CONF.GetVisibilityToolBarMediaSources());
    mActionToolBarMediaSources->setChecked(CONF.GetVisibilityToolBarMediaSources());

    if ((CONF.ConferencingEnabled()) && (mToolBarOnlineStatus != NULL))
    {
        connect(mActionToolBarOnlineStatus, SIGNAL(toggled(bool)), this, SLOT(actionActivateToolBarOnlineStatus(bool)));
        connect(mToolBarOnlineStatus->toggleViewAction(), SIGNAL(toggled(bool)), mActionToolBarOnlineStatus, SLOT(setChecked(bool)));
        mToolBarOnlineStatus->setVisible(CONF.GetVisibilityToolBarOnlineStatus());
        mToolBarOnlineStatus->toggleViewAction()->setChecked(CONF.GetVisibilityToolBarOnlineStatus());
        mActionToolBarOnlineStatus->setChecked(CONF.GetVisibilityToolBarOnlineStatus());
    }
}

void MainWindow::triggerUpdateCheck()
{
    #ifdef RELEASE_VERSION
        if (CONF.GetAutoUpdateCheck())
        {
                mHttpGetVersionServer = new QHttp(this);
                TriggerVersionCheck(mHttpGetVersionServer, GotAnswerForVersionRequest);
        }
    #endif
}

void MainWindow::initializeScreenCapturing()
{
    LOG(LOG_VERBOSE, "Initialization of screen capturing..");

    mScreenShotTimer = new QTimer(this);
    connect(mScreenShotTimer, SIGNAL(timeout()), this, SLOT(CreateScreenShot()));
    mScreenShotTimer->start(50);
}

void MainWindow::initializeNetworkSimulator(QStringList &pArguments, bool pForce)
{
    // use defines here until plugin-interface is integrated completely
    #if HOMER_NETWORK_SIMULATOR
        if (mNetworkSimulator != NULL)
            return;

        if (pArguments.contains("-Enable=NetSim"))
            pForce = true;

        if (pForce)
        {
            mNetworkSimulator = new NetworkSimulator();
            if (!mNetworkSimulator->Init(mMenuWindows, this))
                LOG(LOG_ERROR, "Failed to initialize network simulator");
        }
    #else
        LOG(LOG_WARN, "Network simulator not included in this version");
    #endif
}

void MainWindow::initializeFeatureDisablers(QStringList &pArguments)
{
    // file based log sinks
    QStringList tFiles = pArguments.filter("-Disable=");
    if (tFiles.size())
    {
        QString tFeatureName;
        foreach(tFeatureName, tFiles)
        {
            tFeatureName = tFeatureName.remove("-Disable=");
            if(tFeatureName == "IPv6")
                Socket::DisableIPv6Support();
            if(tFeatureName == "QoS")
                Socket::DisableQoSSupport();
            if(tFeatureName == "AudioOutput")
                CONF.DisableAudioOutput();
            if(tFeatureName == "AudioCapture")
                CONF.DisableAudioCapture();
            if (tFeatureName == "Conferencing")
                CONF.DisableConferencing();
        }
    }

    if (!CONF.GetFeatureConferencing())
        CONF.DisableConferencing();

    if (!CONF.GetFeatureConferencing())
        CONF.DisableConferencing();

    removeArguments(pArguments, "-Disable");
}

void MainWindow::ShowFfmpegCaps(QStringList &pArguments)
{
    if (pArguments.contains("-ListVideoCodecs"))
        MediaSource::LogSupportedVideoCodecs(CONF.DebuggingEnabled());
    if (pArguments.contains("-ListAudioCodecs"))
        MediaSource::LogSupportedAudioCodecs(CONF.DebuggingEnabled());
    if (pArguments.contains("-ListInputFormats"))
        MediaSource::LogSupportedInputFormats(CONF.DebuggingEnabled());
    if (pArguments.contains("-ListOutputFormats"))
        MediaSource::LogSupportedOutputFormats(CONF.DebuggingEnabled());

    removeArguments(pArguments, "-List");
}

void MainWindow::initializeDebugging(QStringList &pArguments)
{
    LOG(LOG_VERBOSE, "Initialization of debugging..");

    // console based log sink
    if (pArguments.contains("-DebugLevel=Error"))
    {
        CONF.SetDebugging(false);
    }else
    {
        if (pArguments.contains("-DebugLevel=Info"))
        {
            CONF.SetDebugging(true);
        }else
        {
            if (pArguments.contains("-DebugLevel=Verbose"))
            {
                CONF.SetDebugging(true);
            }else
            {
                #ifdef RELEASE_VERSION
                    CONF.SetDebugging(false);
                #else
                    CONF.SetDebugging(true);
                #endif
            }
        }
    }
    LOG(LOG_VERBOSE, "################ SYSTEM INFO ################");
    LOG(LOG_VERBOSE, "Found system info:\n%s", HelpDialog::GetSystemInfo().toStdString().c_str());
    LOG(LOG_VERBOSE, "#############################################");
}

void MainWindow::initializeConferenceManagement()
{
    QString tLocalSourceIp = "";
    QString tLocalLoopIp = "";
    bool tInterfaceFound = GetNetworkInfo(mLocalAddresses, tLocalSourceIp, tLocalLoopIp);

    LOG(LOG_VERBOSE, "Initialization of conference management..");
    if (!tInterfaceFound)
    {
        if (tLocalLoopIp != "")
        {
            LOG(LOG_INFO, "No fitting network interface towards outside found");
            LOG(LOG_INFO, "Using loopback interface with IP address: %s", tLocalLoopIp.toStdString().c_str());
            LOG(LOG_INFO, "==>>>>> NETWORK TIMEOUTS ARE POSSIBLE! APPLICATION MAY HANG FOR MOMENTS! <<<<<==");
            tLocalSourceIp = tLocalLoopIp;
        }else
        {
            LOG(LOG_ERROR, "No fitting network interface present");
            exit(-1);
        }
    }

    CONF.SetSipListenerAddress(tLocalSourceIp);

    if (CONF.ConferencingEnabled())
    {
        LOG(LOG_INFO, "Using conference management IP address: %s", tLocalSourceIp.toStdString().c_str());
        MEETING.Init(tLocalSourceIp.toStdString(), mLocalAddresses, CONF.GetNatSupportActivation(), "BROADCAST", CONF.GetSipStartPort(), CONF.GetSipListenerTransport(), CONF.GetSipStartPort() + 10, CONF.GetVideoAudioStartPort());
    }
    MEETING.AddObserver(this);
}

void MainWindow::registerAtStunSipServer()
{
    if (CONF.GetNatSupportActivation())
        MEETING.SetStunServer(CONF.GetStunServer().toStdString());
    // is centralized mode selected activated?
    if (CONF.GetSipInfrastructureMode() == 1)
        MEETING.RegisterAtServer(CONF.GetSipUserName().toStdString(), CONF.GetSipPassword().toStdString(), CONF.GetSipServer().toStdString(), CONF.GetSipServerPort());
}

void MainWindow::initializeVideoAudioIO()
{
    LOG(LOG_VERBOSE, "Initialization of video/audio I/O..");

    // ############################
    // ### VIDEO
    // ############################
    LOG(LOG_VERBOSE, "Creating video media objects..");
    mOwnVideoMuxer = new MediaSourceMuxer();
    #ifdef LINUX
        mOwnVideoMuxer->RegisterMediaSource(new MediaSourceV4L2());
    #endif
    #ifdef WIN32
        mOwnVideoMuxer->RegisterMediaSource(new MediaSourceDShow());
    #endif
    #ifdef APPLE
//        mOwnVideoMuxer->RegisterMediaSource(new MediaSourceCoreVideo());
    #endif
	mOwnVideoMuxer->RegisterMediaSource(mMediaSourceDesktop = new MediaSourceDesktop());
    mOwnVideoMuxer->RegisterMediaSource(mMediaSourceLogo = new MediaSourceLogo());
    // ############################
    // ### AUDIO
    // ############################
    LOG(LOG_VERBOSE, "Creating audio media objects..");
    mOwnAudioMuxer = new MediaSourceMuxer();
    if (CONF.AudioCaptureEnabled())
    {
        mOwnAudioMuxer->RegisterMediaSource(new MediaSourcePortAudio());
    }
}

void MainWindow::initializeColoring()
{
    LOG(LOG_VERBOSE, "Initialization of coloring..");

    // tool bars
    if (mToolBarOnlineStatus != NULL)
        mToolBarOnlineStatus->setStyleSheet("QToolBar#mToolBarOnlineStatus{ background-color: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 rgba(176, 176, 176, 255), stop:1 rgba(255, 255, 255, 255)); border: 0px solid black }");
    mToolBarMediaSources->setStyleSheet("QToolBar#mToolBarMediaSources{ background-color: qlineargradient(x1:0, y1:1, x2:0, y2:0, stop:0 rgba(176, 176, 176, 255), stop:1 rgba(255, 255, 255, 255)); border: 0px solid black }");
}

void MainWindow::initializeWidgetsAndMenus()
{
    LOG(LOG_VERBOSE, "Initialization of widgets and menus..");

    mMenuBar->setVisible(CONF.GetVisibilityMenuBar());
    mStatusBar->setVisible(CONF.GetVisibilityStatusBar());

    #ifndef APPLE
        // set fixed style "plastic"
        QApplication::setStyle(new QPlastiqueStyle());
    #endif

    if (CONF.ConferencingEnabled())
    {
        LOG(LOG_VERBOSE, "..contacts widget");
        mOverviewContactsWidget = new OverviewContactsWidget(mActionOverviewContactsWidget, this);

        LOG(LOG_VERBOSE, "..file transfers widget");
        mOverviewFileTransfersWidget = new OverviewFileTransfersWidget(mActionOverviewFileTransfersWidget, this);
        tabifyDockWidget(mOverviewContactsWidget, mOverviewFileTransfersWidget);

        LOG(LOG_VERBOSE, "..availability widget");
        mOnlineStatusWidget = new AvailabilityWidget(this);
        mToolBarOnlineStatus->addWidget(mOnlineStatusWidget);
    }else
    {
        mActionOverviewContactsWidget->setVisible(false);
        mActionOverviewFileTransfersWidget->setVisible(false);
        mActionIdentity->setVisible(false);
        mActionToolBarOnlineStatus->setVisible(false);
        mMenuParticipantMessageWidgets->setVisible(false);
        mToolBarOnlineStatus->setVisible(false);
        removeToolBar(mToolBarOnlineStatus);
    }

    LOG(LOG_VERBOSE, "..errors widget");
    mOverviewErrorsWidget = new OverviewErrorsWidget(mActionOverviewErrorsWidget, this);

    //TODO: remove the following if the feature is complete
    #ifdef RELEASE_VERSION
        mActionOverviewFileTransfersWidget->setEnabled(false);
    #endif

    LOG(LOG_VERBOSE, "..local broadcast widget");
    mLocalUserParticipantWidget = new ParticipantWidget(BROADCAST, this, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantAVControls, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer);
    setCentralWidget(mLocalUserParticipantWidget);

    CreateSysTray();

    LOG(LOG_VERBOSE, "..playlist control widgets");
    mOverviewPlaylistWidget = new OverviewPlaylistWidget(mActionOverviewPlaylistWidget, this, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker());

    LOG(LOG_VERBOSE, "..streaming control widget");
    mMediaSourcesControlWidget = new StreamingControlWidget(this, mMenuStreaming, mLocalUserParticipantWidget, mMediaSourceDesktop);
    mToolBarMediaSources->addWidget(mMediaSourcesControlWidget);
    if (mOwnVideoMuxer->SupportsMultipleInputStreams())
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
    else
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

    if (mOwnVideoMuxer->SupportsMultipleInputStreams())
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
    else
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

    LOG(LOG_VERBOSE, "..online status menu in the main application menu");
    mOnlineStatusWidget->InitializeMenuOnlineStatus(mMenuOnlineStatus);
    connect(mMenuOnlineStatus, SIGNAL(triggered(QAction *)), mOnlineStatusWidget, SLOT(Selected(QAction *)));

    LOG(LOG_VERBOSE, "..data streams widget");
    mOverviewDataStreamsWidget = new OverviewDataStreamsWidget(mActionOverviewDataStreamsWidget, this);

    LOG(LOG_VERBOSE, "..network streams widget");
    mOverviewNetworkStreamsWidget = new OverviewNetworkStreamsWidget(mActionOverviewNetworkStreamsWidget, this);

    LOG(LOG_VERBOSE, "Creating threads overview widget..");
    mOverviewThreadsWidget = new OverviewThreadsWidget(mActionOverviewThreadsWidget, this);
    tabifyDockWidget(mOverviewThreadsWidget, mOverviewDataStreamsWidget);
    tabifyDockWidget(mOverviewDataStreamsWidget, mOverviewNetworkStreamsWidget);

    mShortcutActivateDebugWidgets = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_A), this);
    mShortcutActivateDebuggingGlobally = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_D), this);
    mShortcutActivateNetworkSimulationWidgets = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_S), this);
}

///////////////////////////////////////////////////////////////////////////////

bool MainWindow::GetNetworkInfo(LocalAddressesList &pLocalAddressesList, QString &pLocalSourceIp, QString &pLocalLoopIp)
{
    QString tLastSipListenerAddress = CONF.GetSipListenerAddress();
    bool tLocalInterfaceSet = false;
    pLocalSourceIp = "";

    //### determine all local IP addresses
    QList<QHostAddress> tQtLocalAddresses = QNetworkInterface::allAddresses();

    LOG(LOG_INFO, "Last listener address: %s", tLastSipListenerAddress.toStdString().c_str());
    LOG(LOG_INFO, "Locally usable IPv4/6 addresses are:");
    for (int i = 0; i < tQtLocalAddresses.size(); i++)
    {
    	QString tAddress = tQtLocalAddresses[i].toString().toLower();
        if ((tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv4Protocol) || ((tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv6Protocol) && (!tAddress.startsWith(IPV6_LINK_LOCAL_PREFIX))))
        {
            LOG(LOG_INFO, "...%s", tAddress.toStdString().c_str());

            if (tAddress == tLastSipListenerAddress)
            {
                pLocalSourceIp = tLastSipListenerAddress;
                LOG(LOG_INFO, ">>> last time used as conference address");
            }

            if (tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv4Protocol)
                pLocalAddressesList.push_front(tAddress.toStdString());
            else
                pLocalAddressesList.push_back(tAddress.toStdString());
        }
    }

    //### go through network interfaces
    QList<QNetworkInterface> tHostInterfaces = QNetworkInterface::allInterfaces();
    QList<QNetworkAddressEntry> tAddresses;

    for (int i = 0; i < tHostInterfaces.size(); i++)
    {
        tAddresses = tHostInterfaces[i].addressEntries();

        bool tInterfaceUsable = false;

        if ((tAddresses.size()) &&
           (!tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsLoopBack)) &&
           (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsRunning)) &&
           (tHostInterfaces[i].flags().testFlag(QNetworkInterface::CanBroadcast)))
        {
            tInterfaceUsable = true;
            LOG(LOG_INFO, "Found possible listener network interface: \"%s\"", tHostInterfaces[i].humanReadableName().toStdString().c_str());
        } else
        {
            if ((tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsLoopBack)) &&
                (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsRunning)))
            {
                LOG(LOG_INFO, "Found working loopback network interface: \"%s\"", tHostInterfaces[i].humanReadableName().toStdString().c_str());
                if (tAddresses.size())
                    pLocalLoopIp = tAddresses[0].ip().toString();
            }else
                LOG(LOG_INFO, "Found additional network interface: \"%s\"", tHostInterfaces[i].humanReadableName().toStdString().c_str());
        }

        LOG(LOG_INFO, "  ..hardware address: %s", tHostInterfaces[i].hardwareAddress().toStdString().c_str());

        if (tHostInterfaces[i].isValid())
            LOG(LOG_INFO, "  ..is valid");
        else
            LOG(LOG_INFO, "  ..is invalid");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsPointToPoint))
            LOG(LOG_INFO, "  ..is P2P enabled");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsUp))
            LOG(LOG_INFO, "  ..is activated");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsRunning))
            LOG(LOG_INFO, "  ..is running");
        else
            LOG(LOG_INFO, "  ..temporarily stopped");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsLoopBack))
            LOG(LOG_INFO, "  ..is loopback");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::CanBroadcast))
            LOG(LOG_INFO, "  ..supports broadcast");

        if (tHostInterfaces[i].flags().testFlag(QNetworkInterface::CanMulticast))
            LOG(LOG_INFO, "  ..supports multicast");

        for (int j = 0; j < tAddresses.size(); j++)
        {
        	QString tAddress = tAddresses[j].ip().toString().toLower();
            switch (tAddresses[j].ip().protocol())
            {
                case QAbstractSocket::IPv4Protocol:
                                        LOG(LOG_INFO, "  ..associated IPv4: %s NETMASK: %s BROADCAST: %s",
                                                tAddress.toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toStdString().c_str(),
                                                tAddresses[j].broadcast().toString().toStdString().c_str());
                                        break;
                case QAbstractSocket::IPv6Protocol:
                                        LOG(LOG_INFO, "  ..associated IPv6: %s NETMASK: %s",
                                                tAddress.toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toLower().toStdString().c_str());
                                        break;
                case QAbstractSocket::UnknownNetworkLayerProtocol:
                                        LOG(LOG_INFO, "  ..associated unknown address type: %s NETMASK: %s BROADCAST: %s",
                                                tAddress.toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toStdString().c_str(),
                                                tAddresses[j].broadcast().toString().toStdString().c_str());
                                        break;
            }
            if ((pLocalSourceIp == "") && (tInterfaceUsable))
            {
                if ((tAddresses[j].ip().protocol() == QAbstractSocket::IPv4Protocol) || ((tAddresses[j].ip().protocol() == QAbstractSocket::IPv6Protocol) && (!tAddress.startsWith(IPV6_LINK_LOCAL_PREFIX))))
                {
                    LOG(LOG_INFO, ">>> selected as conference address");
                    pLocalSourceIp = tAddress;
                }
            }
        }
    }

    if ((pLocalSourceIp == "") && (pLocalLoopIp != ""))
    {
        LOG(LOG_INFO, ">>> using loopback address %s as conference address", pLocalLoopIp.toStdString().c_str());
        pLocalSourceIp = pLocalLoopIp;
    }

    return (pLocalSourceIp != "");
}

void MainWindow::GotAnswerForVersionRequest(bool pError)
{
    if (pError)
        ShowWarning("Server unavailable", "Could not determine software version which is provided by project server");
    else
    {
        QString tServerVersion = QString(mHttpGetVersionServer->readAll().constData());
        if (tServerVersion != RELEASE_VERSION_STRING)
            ShowInfo("Update available", "An updated version of Homer-Conferencing is available. Current version on server is <font color='green'><b>" + tServerVersion +"</b></font>. Have a look in the \"update check\" dialog in the application menu.");
    }
}

void MainWindow::CreateScreenShot()
{
    static int sShiftIndex = 0;
    static QString sShiftingString = "                ";
    #ifdef DEBUG_VERSION
        sShiftIndex++;
        if (sShiftIndex == 32)
            sShiftIndex = 0;
        if(sShiftIndex < 16)
            sShiftingString[sShiftIndex] = '#';
        else
            sShiftingString[31 - sShiftIndex] = '#';
    #endif
    mStatusBar->showMessage("Program Start  " + mStartTime.toString("hh:mm") + "       Time  " + QTime::currentTime().toString("hh:mm") + "    " + sShiftingString);
    sShiftingString[sShiftIndex] = ' ';
    if(mMediaSourceDesktop != NULL)
        mMediaSourceDesktop->CreateScreenshot();
    mScreenShotTimer->start(1000 / SCREEN_CAPTURE_FPS);
}

void MainWindow::loadSettings()
{
    int tX = 352;
    int tY = 288;

    LOG(LOG_VERBOSE, "Loading program settings..");
    LOG(LOG_VERBOSE, "..meeting settings");

    MEETING.SetLocalUserName(QString(CONF.GetUserName().toLocal8Bit()).toStdString());
    MEETING.SetLocalUserMailAdr(QString(CONF.GetUserMail().toLocal8Bit()).toStdString());
    MEETING.SetAvailabilityState(CONF.GetConferenceAvailability().toStdString());

    // init video codec for network streaming, but only support ONE codec and not multiple
    QString tVideoStreamCodec = CONF.GetVideoCodec();
    // set settings within meeting management
    if (tVideoStreamCodec == "H.261")
        MEETING.SetVideoCodecsSupport(CODEC_H261);
    if (tVideoStreamCodec == "H.263")
        MEETING.SetVideoCodecsSupport(CODEC_H263);
    if (tVideoStreamCodec == "H.263+")
        MEETING.SetVideoCodecsSupport(CODEC_H263P);
    if (tVideoStreamCodec == "H.264")
        MEETING.SetVideoCodecsSupport(CODEC_H264);
    if (tVideoStreamCodec == "MPEG4")
        MEETING.SetVideoCodecsSupport(CODEC_MPEG4);
    if (tVideoStreamCodec == "THEORA")
        MEETING.SetVideoCodecsSupport(CODEC_THEORA);
    if (tVideoStreamCodec == "VP8")
        MEETING.SetVideoCodecsSupport(CODEC_VP8);
    MEETING.SetVideoTransportType(MEDIA_TRANSPORT_RTP_UDP); // always use RTP/AVP as profile (RTP/UDP)

    // init audio codec for network streaming, but only support ONE codec and not multiple
    QString tAudioStreamCodec = CONF.GetAudioCodec();
    // set settings within meeting management
    if (tAudioStreamCodec == "MP3")
        MEETING.SetAudioCodecsSupport(CODEC_MP3);
    if (tAudioStreamCodec == "G711 A-law")
        MEETING.SetAudioCodecsSupport(CODEC_G711ALAW);
    if (tAudioStreamCodec == "G711 �-law")
        MEETING.SetAudioCodecsSupport(CODEC_G711ULAW);
    if (tAudioStreamCodec == "AAC")
        MEETING.SetAudioCodecsSupport(CODEC_AAC);
    if (tAudioStreamCodec == "PCM16")
        MEETING.SetAudioCodecsSupport(CODEC_PCMS16);
    if (tAudioStreamCodec == "GSM")
        MEETING.SetAudioCodecsSupport(CODEC_GSM);
    if (tAudioStreamCodec == "G722 adpcm")
        MEETING.SetAudioCodecsSupport(CODEC_G722ADPCM);
    MEETING.SetAudioTransportType(MEDIA_TRANSPORT_RTP_UDP); // always use RTP/AVP as profile (RTP/UDP)

    LOG(LOG_VERBOSE, "..video/audio settings");
    QString tVideoStreamResolution = CONF.GetVideoResolution();

    // calculate the resolution from string
    MediaSource::VideoString2Resolution(tVideoStreamResolution.toStdString(), tX, tY);

    // init video muxer
    mOwnVideoMuxer->SetOutputStreamPreferences(tVideoStreamCodec.toStdString(), CONF.GetVideoQuality(), CONF.GetVideoBitRate(), CONF.GetVideoMaxPacketSize(), false, tX, tY, CONF.GetVideoRtp(), CONF.GetVideoFps());
    mOwnVideoMuxer->SetRelayActivation(CONF.GetVideoActivation());
    bool tNewDeviceSelected = false;
    QString tLastVideoSource = CONF.GetLocalVideoSource();
    if (tLastVideoSource == "auto")
        tLastVideoSource = MEDIA_SOURCE_HOMER_LOGO;
    mOwnVideoMuxer->SelectDevice(tLastVideoSource.toStdString(), MEDIA_VIDEO, tNewDeviceSelected);
    // if former selected device isn't available we use one of the available instead
    if (!tNewDeviceSelected)
    {
        ShowWarning("Video device not available", "Can't use formerly selected video device: \"" + CONF.GetLocalVideoSource() + "\", will use one of the available devices instead!");
        CONF.SetLocalVideoSource("auto");
        mOwnVideoMuxer->SelectDevice("auto", MEDIA_VIDEO, tNewDeviceSelected);
    }
    mOwnVideoMuxer->SetVideoFlipping(CONF.GetLocalVideoSourceHFlip(), CONF.GetLocalVideoSourceVFlip());

    // init audio muxer
    mOwnAudioMuxer->SetOutputStreamPreferences(tAudioStreamCodec.toStdString(), 100, CONF.GetAudioBitRate(), CONF.GetAudioMaxPacketSize(), false, 0, 0, CONF.GetAudioRtp());
    mOwnAudioMuxer->SetRelayActivation(CONF.GetAudioActivation() && !CONF.GetAudioActivationPushToTalk());
    mOwnAudioMuxer->SetRelaySkipSilence(CONF.GetAudioSkipSilence());
    mOwnAudioMuxer->SelectDevice(CONF.GetLocalAudioSource().toStdString(), MEDIA_AUDIO, tNewDeviceSelected);
    // if former selected device isn't available we use one of the available instead
    if (!tNewDeviceSelected)
    {
        AudioDevices tAList;
        mOwnAudioMuxer->getAudioDevices(tAList);
        if (tAList.size() > 1)
            ShowWarning("Audio device not available", "Can't use formerly selected audio device: \"" + CONF.GetLocalAudioSource() + "\", will use one of the available devices instead!");
        CONF.SetLocalAudioSource("auto");
        mOwnAudioMuxer->SelectDevice("auto", MEDIA_AUDIO, tNewDeviceSelected);
    }
}

void MainWindow::closeEvent(QCloseEvent* pEvent)
{
    if (mShuttingDown)
    {
        LOG(LOG_VERBOSE, "Got repeated call for closing the main window");
        return;
    }

    CONF.Sync();

    mShuttingDown = true;

    if (mStarting)
    {
        LOG(LOG_WARN, "Fast shutdown triggered");
        printf("Fast shutdown\n");
        exit(0);
    }

    LOG(LOG_VERBOSE, "Got signal for closing main window");

    // audio playback - stop sound
    #ifndef DEBUG_VERSION
        if (CONF.GetStopSound())
        {
            if (!mWaveOut->IsPlaying())
            {
                LOG(LOG_VERBOSE, "Playing stop sound..");
                StartAudioPlayback(CONF.GetStopSoundFile());
            }else
                LOG(LOG_WARN, "We are still playing the start sound, skipping stop sound");
        }
    #endif

    // remove ourself as observer for new Meeting events
    MEETING.DeleteObserver(this);

    LOG(LOG_VERBOSE, "..saving main window layout");
    CONF.SetVisibilityMenuBar(mMenuBar->isVisible());
    CONF.SetVisibilityStatusBar(mStatusBar->isVisible());
    CONF.SetMainWindowPosition(pos());
    CONF.SetMainWindowSize(size());
    CONF.SetVisibilityToolBarMediaSources(mToolBarMediaSources->isVisible());
    if (mToolBarOnlineStatus != NULL)
        CONF.SetVisibilityToolBarOnlineStatus(mToolBarOnlineStatus->isVisible());

    // update IP entries within usage statistic
    //setIpStatistic(MEETING.GetHostAdr(), MEETING.getStunNatIp());

    // save the availability state within settings
    LOG(LOG_VERBOSE, "..saving conference availability");
    CONF.SetConferenceAvailability(QString(MEETING.GetAvailabilityStateStr().c_str()));

    // ###### begin shutdown ################
    // stop the screenshot creating timer function
    LOG(LOG_VERBOSE, "..stopping GUI capturing");
    mScreenShotTimer->stop();

    // prevent the system from further incoming events
    LOG(LOG_VERBOSE, "..stopping conference manager");
    MEETING.Stop();

    //HINT: delete MediaSourcesControlWidget before local participant widget to avoid crashes caused by race conditions (control widget has a timer which calls local participant widget's video widget!)
    LOG(LOG_VERBOSE, "..destroying media source control widget");
    delete mMediaSourcesControlWidget;

    //HINT: delete before local participant widget is destroyed
    LOG(LOG_VERBOSE, "..destroying playlist widget");
    delete mOverviewPlaylistWidget;

    // should be the last because video/audio workers could otherwise be deleted while they are still called
    LOG(LOG_VERBOSE, "..destroying broadcast widget");
    if (mLocalUserParticipantWidget != NULL)
        delete mLocalUserParticipantWidget;

    // delete video/audio muxer
    LOG(LOG_VERBOSE, "..destroying broadcast video muxer");
    delete mOwnVideoMuxer;
    LOG(LOG_VERBOSE, "..destroying broadcast audio muxer");
    delete mOwnAudioMuxer;

    // destroy all participant widgets
    LOG(LOG_VERBOSE, "..destroying all participant widgets");
    ParticipantWidgetList::iterator tIt;
    if (mParticipantWidgets.size())
    {
        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
        {
            delete (*tIt);
        }
    }

    // deinit
    LOG(LOG_VERBOSE, "..destroying conference manager");
    MEETING.Deinit();

    LOG(LOG_VERBOSE, "..destroying simulator widget");
    #if HOMER_NETWORK_SIMULATOR
        if (mNetworkSimulator != NULL)
            delete mNetworkSimulator;
    #endif

    LOG(LOG_VERBOSE, "..destroying shortcuts");
    delete mShortcutActivateDebugWidgets;
    delete mShortcutActivateDebuggingGlobally;
    delete mShortcutActivateNetworkSimulationWidgets;

    LOG(LOG_VERBOSE, "..destroying remaining widgets");
    delete mOverviewDataStreamsWidget;
    delete mOverviewNetworkStreamsWidget;
    delete mOverviewThreadsWidget;

    delete mOverviewContactsWidget;
    delete mOverviewErrorsWidget;
    delete mOverviewFileTransfersWidget;
    LOG(LOG_VERBOSE, "..destroying online status widget");
    delete mOnlineStatusWidget;
    LOG(LOG_VERBOSE, "..destroying system tray icon");
    delete mSysTrayIcon;

    //HINT: mSourceDesktop will be deleted by VideoWidget which grabbed from there

    // make sure the user doesn't see anything anymore
    hide();

    #ifndef DEBUG_VERSION

        if (mWaveOut != NULL)
        {
            // wait for the end of playback
            while(mWaveOut->IsPlaying())
            {
                LOG(LOG_VERBOSE, "Waiting for the end of acoustic notification");
                Thread::Suspend(250 * 1000);
            }
        }
        LOG(LOG_VERBOSE, "Playback finished");
        ClosePlaybackDevice();
    #endif

    // make sure this main window will be deleted when control returns to Qt event loop (needed especially in case of closeEvent comes from fullscreen video widget)
    deleteLater();
}

void MainWindow::handleMeetingEvent(GeneralEvent *pEvent)
{
    QApplication::postEvent(this, (QEvent*) new QMeetingEvent(pEvent));
}

void MainWindow::GetEventSource(GeneralEvent *pEvent, QString &pSender, QString &pSenderApp)
{
    pSender = (pEvent->SenderName != "") ? QString(pEvent->SenderName.c_str()) : QString(pEvent->Sender.c_str());
    pSenderApp = (pEvent->SenderApplication != "") ? QString(pEvent->SenderApplication.c_str()) : "";
    if (pSenderApp == USER_AGENT_SIGNATURE)
        pSenderApp = "Homer-Conferencing";
}

void MainWindow::keyPressEvent(QKeyEvent *pEvent)
{
    //LOG(LOG_VERBOSE, "Got main window key press event with key %s(%d, mod: %d, auto-repeat: %d)", pEvent->text().toStdString().c_str(), pEvent->key(), (int)pEvent->modifiers(), pEvent->isAutoRepeat());

    if ((pEvent->key() == Qt::Key_T) && (!pEvent->isAutoRepeat()))
    {
        //LOG(LOG_VERBOSE, "Audio activation: %d, PTT mode: %d", CONF.GetAudioActivation(), CONF.GetAudioActivationPushToTalk());
        if ((CONF.GetAudioActivation()) && (CONF.GetAudioActivationPushToTalk()))
        {
                mOwnAudioMuxer->SetRelayActivation(true);
        }
        pEvent->accept();
    }else
    {
        // forward the event to the local participant widget
        QCoreApplication::postEvent(mLocalUserParticipantWidget, new QKeyEvent(QEvent::KeyPress, pEvent->key(), pEvent->modifiers(), pEvent->text()));
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *pEvent)
{
    //LOG(LOG_VERBOSE, "Got main window key release event with key %s(%d, mod: %d, auto-repeat: %d)", pEvent->text().toStdString().c_str(), pEvent->key(), (int)pEvent->modifiers(), pEvent->isAutoRepeat());

    if (pEvent->key() == Qt::Key_T)
    {
        if ((CONF.GetAudioActivation()) && (CONF.GetAudioActivationPushToTalk()))
        {
            if (!pEvent->isAutoRepeat())
                mOwnAudioMuxer->SetRelayActivation(false);
        }
        pEvent->accept();
    }else
    {
        QMainWindow::keyReleaseEvent(pEvent);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if (pEvent->mimeData()->hasUrls())
    {
        pEvent->acceptProposedAction();
        QList<QUrl> tList = pEvent->mimeData()->urls();
        QUrl tUrl;
        int i = 0;

        foreach(tUrl, tList)
            LOG(LOG_VERBOSE, "New entering drag+drop url (%d) \"%s\"", ++i, tUrl.toString().toStdString().c_str());
        return;
    }
}

void MainWindow::dropEvent(QDropEvent *pEvent)
{
    if (pEvent->mimeData()->hasUrls())
    {
        LOG(LOG_VERBOSE, "Got some dropped urls");
        QList<QUrl> tUrlList = pEvent->mimeData()->urls();
        QUrl tUrl;
        foreach(tUrl, tUrlList)
        {
            LOG(LOG_VERBOSE, "Dropped url: %s", tUrl.toLocalFile().toStdString().c_str());
        }
        pEvent->acceptProposedAction();
        return;
    }
}

void MainWindow::changeEvent (QEvent *pEvent)
{
    switch(pEvent->type())
    {
		case QEvent::LanguageChange:
			LOG(LOG_WARN, "Application changed the language setting");
			retranslateUi(this);
			break;
    	case QEvent::WindowStateChange:
    		UpdateSysTrayContextMenu();
    		break;
    	default:
    		break;
    }
    QMainWindow::changeEvent(pEvent);
}

void MainWindow::customEvent(QEvent* pEvent)
{
    // make sure we have our user defined QEvent
    if (pEvent->type() != QEvent::User)
        return;

    // some necessary variables
    bool tKnownParticipant = false;
    ParticipantWidgetList::iterator tIt;
    AddParticipantEvent *tAPEvent;
    DeleteSessionEvent *tDSEvent;
    InternalNatDetectionEvent *tINDEvent;
    ErrorEvent *tEEvent;
    MessageEvent *tMEvent;
    MessageAcceptEvent *tMAEvent;
    MessageAcceptDelayedEvent *tMADEvent;
    MessageUnavailableEvent *tMUEvent;
    CallEvent *tCEvent;
    CallRingingEvent *tCREvent;
    CallAcceptEvent *tCAEvent;
    CallCancelEvent *tCCEvent;
    CallDenyEvent *tCDEvent;
    CallUnavailableEvent *tCUEvent;
    CallHangUpEvent *tCHUEvent;
    CallTerminationEvent *tCTEvent;
    CallMediaUpdateEvent *tCMUEvent;
    RegistrationEvent *tREvent;
    RegistrationFailedEvent *tRFEvent;
    PublicationEvent *tPEvent;
    PublicationFailedEvent *tPFEvent;
    OptionsAcceptEvent *tOAEvent;
    OptionsUnavailableEvent *tOUAEvent;
    GeneralEvent *tEvent = ((QMeetingEvent*) pEvent)->getEvent();
    ParticipantWidget *tParticipantWidget;
    AddNetworkSinkDialog *tANSDialog =  NULL;
    QString tEventSender, tEventSenderApp;

    if(tEvent->getType() != ADD_PARTICIPANT)
        LOG(LOG_INFO, ">>>>>> Event of type \"%s\"", GeneralEvent::getNameFromType(tEvent->getType()).c_str());
    else
        LOG(LOG_INFO, ">>>>>> Event of type \"Add participant\"");

    // stop processing if there is no possible recipient widget and it is no incoming new call/message/contact event
    if ((mParticipantWidgets.size() == 0) &&
                                                (tEvent->getType() != INT_START_NAT_DETECTION) &&
                                                (tEvent->getType() != CALL) &&
                                                (tEvent->getType() != MESSAGE) &&
                                                (tEvent->getType() != ADD_PARTICIPANT) &&
                                                (tEvent->getType() != REGISTRATION) &&
                                                (tEvent->getType() != REGISTRATION_FAILED) &&
                                                (tEvent->getType() != OPTIONS_ACCEPT) &&
                                                (tEvent->getType() != OPTIONS_UNAVAILABLE) &&
                                                (tEvent->getType() != PUBLICATION) &&
                                                (tEvent->getType() != PUBLICATION_FAILED))
    {
        delete tEvent;
        return;
    }

    //HINT: we stop event processing and therefore avoid crashes or unsecure behavior because shutdown is going on in the meanwhile
    if(mShuttingDown)
    {
        LOG(LOG_WARN, "Will ignore this event because the stutdown process was already started");
        delete tEvent;
        return;
    }

    switch(tEvent->getType())
    {
        case ADD_PARTICIPANT:
                    //####################### PARTICIPANT ADD #############################
                    tAPEvent = (AddParticipantEvent*) tEvent;
                    AddParticipantSession(tAPEvent->User, tAPEvent->Host, tAPEvent->Port, tAPEvent->Transport, tAPEvent->Ip, tAPEvent->InitState);
                    break;
        case DELETE_SESSION:
                    //####################### PARTICIPANT DELETE #############################
                    tDSEvent = (DeleteSessionEvent*) tEvent;
                    DeleteParticipantSession(tDSEvent->PWidget);
                    break;
        case ADD_VIDEO_RELAY:
                    //####################### VIDEO ADD RELAY #############################
                    tANSDialog = new AddNetworkSinkDialog(this, "Configure video streaming", DATA_TYPE_VIDEO, GetVideoMuxer());
                    tANSDialog->exec();
                    delete tANSDialog;
                    break;
        case INT_START_NAT_DETECTION:
                    //####################### NAT DETECTION ANSWER ###########################
                    tINDEvent = (InternalNatDetectionEvent*) tEvent;
                    if (tINDEvent->Failed)
                    {
                        ShowError(Homer::Gui::MainWindow::tr("NAT detection failed"), Homer::Gui::MainWindow::tr("Could not detect NAT address and type via STUN server. The failure reason is") + " \"" + QString(tINDEvent->FailureReason.c_str()) + "\".");
                    }else
                    {
                    	LOG(LOG_VERBOSE, "NAT detection was successful");
                    }
                    break;
        case OPTIONS_ACCEPT:
                    //######################## OPTIONS ACCEPT ##########################
                    tOAEvent = (OptionsAcceptEvent*) tEvent;

                    // inform contacts pool about online state
                    CONTACTS.UpdateContactState(QString::fromLocal8Bit(tOAEvent->Sender.c_str()), tOAEvent->Transport, CONTACT_AVAILABLE);

                    // inform participant widget about new state
                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tOAEvent->Sender.c_str()), tOAEvent->Transport))
                            {
                                tKnownParticipant = true;
                                (*tIt)->UpdateParticipantState(CONTACT_AVAILABLE);
                                break;
                            }
                        }
                    }

                    break;
        case OPTIONS_UNAVAILABLE:
                    //######################## OPTIONS UNAVAILABLE ##########################
                    tOUAEvent = (OptionsUnavailableEvent*) tEvent;

                    // inform contacts pool about online state
                    CONTACTS.UpdateContactState(QString::fromLocal8Bit(tOUAEvent->Sender.c_str()), tOUAEvent->Transport, CONTACT_UNAVAILABLE);

                    LOG(LOG_WARN, "Contact unavailable, reason is \"%s\"(%d).", tOUAEvent->Description.c_str(), tOUAEvent->StatusCode);

                    // inform participant widget about new state
                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tOUAEvent->Sender.c_str()), tOUAEvent->Transport))
                            {
                                tKnownParticipant = true;
                                (*tIt)->UpdateParticipantState(CONTACT_UNAVAILABLE);
                                break;
                            }
                        }
                    }

                    break;
        case GENERAL_ERROR:
                    //############################ GENERAL_ERROR #############################
                    tEEvent = (ErrorEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tEEvent->Sender.c_str()), tEEvent->Transport))
                        {
                            tKnownParticipant = true;
                            (*tIt)->HandleGeneralError(tEEvent->IsIncomingEvent, tEEvent->StatusCode, QString(tEEvent->Description.c_str()));
                            break;
                        }
                    }
                    break;
        case MESSAGE:
                    //############################## MESSAGE #################################
                    tMEvent = (MessageEvent*) tEvent;

                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetImSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tMEvent, tEventSender, tEventSenderApp);
                            QString tText = (tMEvent->Text != "") ? QString(tMEvent->Text.c_str()) : "";
                            mSysTrayIcon->showMessage("Message from " + tEventSender, "\"" + tText + "\"" + ((tEventSenderApp != "") ? ("\n(via \"" + tEventSenderApp + "\")") : ""), QSystemTrayIcon::Information, CONF.GetSystrayTimeout());
                        }
                    }

                    if (tMEvent->SenderComment != "Broadcast")
                    {//unicast message

                        if (mParticipantWidgets.size())
                        {
                            // search for corresponding participant widget
                            for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                            {
                                if ((*tIt)->IsThisParticipant(QString(tMEvent->Sender.c_str()), tMEvent->Transport))
                                {
                                    tKnownParticipant = true;
                                    if (tMEvent->SenderName.size())
                                        (*tIt)->UpdateParticipantName(QString(tMEvent->SenderName.c_str()));
                                    (*tIt)->HandleMessage(tMEvent->IsIncomingEvent, QString(tMEvent->SenderName.c_str()), QString(tMEvent->Text.c_str()));
                                    break;
                                }
                            }
                        }

                        if (!tKnownParticipant)
                        {
                            // add without any OpenSession-check, the session is always added automatically by the meeting-layer
                            tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantAVControls, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, QString(tMEvent->Sender.c_str()), tMEvent->Transport);

                            if (tParticipantWidget != NULL)
                            {
                                mParticipantWidgets.push_back(tParticipantWidget);
                                if (tMEvent->SenderName.size())
                                    tParticipantWidget->UpdateParticipantName(QString(tMEvent->SenderName.c_str()));
                                tParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString(tMEvent->SenderName.c_str()), QString(tMEvent->Text.c_str()));
                            } else
                                LOG(LOG_ERROR, "ParticipantWidget creation failed");
                        }
                        break;
                    } else
                    {//broadcast message
                        if (tMEvent->SenderName.size())
                            mLocalUserParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString(("\"" + tMEvent->SenderName + "\"(" + tMEvent->Sender + ")[" + Socket::TransportType2String(tMEvent->Transport) + "]").c_str()), QString(tMEvent->Text.c_str()));
                        else
                            mLocalUserParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString((tMEvent->Sender + "[" + Socket::TransportType2String(tMEvent->Transport) + "]").c_str()), QString(tMEvent->Text.c_str()));
                    }
                    break;
        case MESSAGE_ACCEPT:
                    //######################## MESSAGE ACCEPT ##########################
                    tMAEvent = (MessageAcceptEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMAEvent->Sender.c_str()), tMAEvent->Transport))
                        {
                            tKnownParticipant = true;
                            (*tIt)->HandleMessageAccept(tMAEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case MESSAGE_ACCEPT_DELAYED:
                    //##################### MESSAGE ACCEPT DELAYED ######################
                    tMADEvent = (MessageAcceptDelayedEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMADEvent->Sender.c_str()), tMADEvent->Transport))
                        {
                            tKnownParticipant = true;
                            (*tIt)->HandleMessageAcceptDelayed(tMADEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case MESSAGE_UNAVAILABLE:
                    //######################## MESSAGE UNAVAILABLE ##########################
                    tMUEvent = (MessageUnavailableEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMUEvent->Sender.c_str()), tMUEvent->Transport))
                        {
                            tKnownParticipant = true;
                            (*tIt)->HandleMessageUnavailable(tMUEvent->IsIncomingEvent, tMUEvent->StatusCode, QString(tMUEvent->Description.c_str()));
                            break;
                        }
                    }
                    break;
        case CALL:
                    //############################### CALL ##################################
                    tCEvent = (CallEvent*) tEvent;

                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Information, CONF.GetSystrayTimeout());
                        }
                    }

                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tCEvent->Sender.c_str()), tCEvent->Transport))
                            {
                                tKnownParticipant = true;
                                if (tCEvent->SenderName.size())
                                    (*tIt)->UpdateParticipantName(QString(tCEvent->SenderName.c_str()));

                                if (!tCEvent->AutoAnswering)
                                    (*tIt)->HandleCall(tCEvent->IsIncomingEvent, QString(tEvent->SenderApplication.c_str()));
                                break;
                            }
                        }
                    }

                    if (!tKnownParticipant)
                    {
                        // add without any OpenSession-check, the session is always added automatically by the meeting-layer
                        tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantAVControls, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, QString(tCEvent->Sender.c_str()));

                        if (tParticipantWidget != NULL)
                        {
                            mParticipantWidgets.push_back(tParticipantWidget);
                            if (tCEvent->SenderName.size())
                                tParticipantWidget->UpdateParticipantName(QString(tCEvent->SenderName.c_str()));
                            if (!tCEvent->AutoAnswering)
                                tParticipantWidget->HandleCall(tCEvent->IsIncomingEvent, QString(tCEvent->SenderApplication.c_str()));
                        } else
                            LOG(LOG_ERROR, "ParticipantWidget creation failed");
                    }
                    break;
        case CALL_RINGING:
                    //############################# CALL RINGING ##############################
                    tCREvent = (CallRingingEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCREvent->Sender.c_str()), tCREvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCREvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCREvent->SenderName.c_str()));
                            (*tIt)->HandleCallRinging(tCREvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_ACCEPT:
                    //############################# CALL ACCEPT ##############################
                    tCAEvent = (CallAcceptEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCAEvent->Sender.c_str()), tCAEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCAEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCAEvent->SenderName.c_str()));
                            (*tIt)->HandleCallAccept(tCAEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_CANCEL:
                    //############################# CALL CANCEL ##############################
                    tCCEvent = (CallCancelEvent*) tEvent;

                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCCEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call canceled from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Information, CONF.GetSystrayTimeout());
                        }
                    }

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCCEvent->Sender.c_str()), tCCEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCCEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCCEvent->SenderName.c_str()));
                            (*tIt)->HandleCallCancel(tCCEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_DENY:
                    //############################# CALL DENY ##############################
                    tCDEvent = (CallDenyEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCDEvent->Sender.c_str()), tCDEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCDEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCDEvent->SenderName.c_str()));
                            (*tIt)->HandleCallDenied(tCDEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_UNAVAILABLE:
                    //########################### CALL UNAVAILABLE ###########################
                    tCUEvent = (CallUnavailableEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCUEvent->Sender.c_str()), tCUEvent->Transport))
                        {
                            tKnownParticipant = true;
                            (*tIt)->HandleCallUnavailable(tCUEvent->IsIncomingEvent, tCUEvent->StatusCode, QString(tCUEvent->Description.c_str()));
                            break;
                        }
                    }
                    break;
        case CALL_HANGUP:
                    //############################# CALL HANGUP ##############################
                    tCHUEvent = (CallHangUpEvent*) tEvent;
                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCHUEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call hangup from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Information, CONF.GetSystrayTimeout());
                        }
                    }

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCHUEvent->Sender.c_str()), tCHUEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCHUEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCHUEvent->SenderName.c_str()));
                            (*tIt)->HandleCallHangup(tCHUEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_TERMINATION:
                    //######################### CALL TERMINATION ############################
                    tCTEvent = (CallTerminationEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCTEvent->Sender.c_str()), tCTEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCTEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCTEvent->SenderName.c_str()));
                            (*tIt)->HandleCallTermination(tCTEvent->IsIncomingEvent);
                            break;
                        }
                    }
                    break;
        case CALL_MEDIA_UPDATE:
                    //######################## CALL MEDIA UPDATE ############################
                    tCMUEvent = (CallMediaUpdateEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCMUEvent->Sender.c_str()), tCMUEvent->Transport))
                        {
                            tKnownParticipant = true;
                            if (tCMUEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCMUEvent->SenderName.c_str()));
                            (*tIt)->HandleMediaUpdate(tCMUEvent->IsIncomingEvent, QString(tCMUEvent->RemoteAudioAddress.c_str()), tCMUEvent->RemoteAudioPort, QString(tCMUEvent->RemoteAudioCodec.c_str()), QString(tCMUEvent->RemoteVideoAddress.c_str()), tCMUEvent->RemoteVideoPort, QString(tCMUEvent->RemoteVideoCodec.c_str()));
                            break;
                        }
                    }
                    break;
        case REGISTRATION:
                    //####################### REGISTRATION SUCCEEDED #############################
                    tREvent = (RegistrationEvent*) tEvent;
                    if ((mSipServerRegistrationUser != CONF.GetSipUserName()) || (mSipServerRegistrationHost != CONF.GetSipServer()) || (mSipServerRegistrationPort != QString("%1").arg(CONF.GetSipServerPort())))
                    {
                        mSysTrayIcon->showMessage("Registration successful", "Registered  \"" + CONF.GetSipUserName() + "\" at SIP server \"" + CONF.GetSipServer() + ":<" + QString("%1").arg(CONF.GetSipServerPort()) + ">\"!\n" \
                                                  "SIP server runs software \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".", QSystemTrayIcon::Information, CONF.GetSystrayTimeout());
                    }
                    mSipServerRegistrationUser = CONF.GetSipUserName();
                    mSipServerRegistrationHost = CONF.GetSipServer();
                    mSipServerRegistrationPort = QString("%1").arg(CONF.GetSipServerPort());
                    break;
        case REGISTRATION_FAILED:
                    //####################### REGISTRATION FAILED #############################
                    tRFEvent = (RegistrationFailedEvent*) tEvent;
                    if (tRFEvent->StatusCode != 904)
                    {
                    	ShowError(Homer::Gui::MainWindow::tr("Registration failed"), Homer::Gui::MainWindow::tr("Could not register") + " \"" + CONF.GetSipUserName() + "\" " + Homer::Gui::MainWindow::tr("at the SIP server") + " \"" + CONF.GetSipServer() + ":<" + QString("%1").arg(CONF.GetSipServerPort()) + ">\"! " + Homer::Gui::MainWindow::tr("The reason is") + " \"" + QString(tRFEvent->Description.c_str()) + "\"(" + QString("%1").arg(tRFEvent->StatusCode) + ")\n" +
                    									 Homer::Gui::MainWindow::tr("SIP server runs software") + " \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".");
                    }else
                    {
                    	ShowError(Homer::Gui::MainWindow::tr("Registration failed"), Homer::Gui::MainWindow::tr("Could not register") + " \"" + CONF.GetSipUserName() + "\" " + Homer::Gui::MainWindow::tr("at the SIP server") + " \"" + CONF.GetSipServer() + ":<" + QString("%1").arg(CONF.GetSipServerPort()) + ">\"! " + Homer::Gui::MainWindow::tr("The login name or password is wrong. Check configuration!\n") +
                    	                                 Homer::Gui::MainWindow::tr("SIP server runs software") + " \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".");
                    }
                    // reset stored SIP server data
                    mSipServerRegistrationUser = "";
                    mSipServerRegistrationHost = "";
                    mSipServerRegistrationPort = "";
                    break;
        case PUBLICATION:
                    //####################### PUBLICATION SUCCEEDED #############################
                    tPEvent = (PublicationEvent*) tEvent;
                    //TODO: inform user
                    break;
        case PUBLICATION_FAILED:
                    //####################### PUBLICATION FAILED #############################
                    tPFEvent = (PublicationFailedEvent*) tEvent;
                    ShowError(Homer::Gui::MainWindow::tr("Presence publication failed"), Homer::Gui::MainWindow::tr("Could not publish your new presence state at the SIP server") + " \"" + CONF.GetSipServer() + ":<" + QString("%1").arg(CONF.GetSipServerPort()) + ">\"! " + Homer::Gui::MainWindow::tr("The reason is") + " \"" + QString(tPFEvent->Description.c_str()) + "\"(" + QString("%1").arg(tPFEvent->StatusCode) + ")\n" +
                                                        Homer::Gui::MainWindow::tr("SIP server runs software") + " \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".");
                    break;
        default:
                    LOG(LOG_ERROR, "We should never reach this point! Otherwise there is an ERROR IN STATE MACHINE");
                    break;
    }

    LOG(LOG_VERBOSE, "Event was related to an already known participant");
    // free memory of event
    delete tEvent;
}

///////////////////////////////////////////////////////////////////////////////
QString MainWindow::CompleteIpAddress(QString pAddr)
{
    QString tResult;
    int tPos = -1;

    if ((tPos = pAddr.indexOf("::")) != -1)
    {
        int tCount = pAddr.count(':');

        // is "::" at the QString beginning?
        if (tPos == 0)
        {
            for (int i = 0; i < 9 - tCount; i++)
                tResult += "0:";
            tResult += pAddr.right(pAddr.length() - 2);
        }else
        {
            // is "::" at the QString end?
            if (tPos == pAddr.length() - 2)
            {
                tResult = pAddr.left(pAddr.length() - 2);
                for (int i = 0; i < 9 - tCount; i++)
                    tResult += ":0";

            }else
            {
                tResult = pAddr.left(tPos);
                for (int i = 0; i < 8 - tCount; i++)
                    tResult += ":0";
                tResult += pAddr.right(pAddr.length() - tPos - 1);
            }
        }

    }else
        tResult = pAddr;

    LOG(LOG_VERBOSE, "Completed IP %s to IP %s", pAddr.toStdString().c_str(), tResult.toStdString().c_str());
    return tResult;
}

ParticipantWidget* MainWindow::AddParticipantSession(QString pUser, QString pHost, QString pPort, enum TransportType pTransport, QString pIp, int pInitState)
{
    ParticipantWidget *tParticipantWidget = NULL;

    LOG(LOG_VERBOSE, "Going to add participant session for %s [%s]", MEETING.SipCreateId(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString()).c_str(), Socket::TransportType2String(pTransport).c_str());

    if (pHost.size())
    {
    	if (!MEETING.IsLocalAddress(pIp.toStdString(), pPort.toStdString(), pTransport))
    	{
			// search for a participant widget with the same sip interface
			ParticipantWidgetList::iterator tIt;
			for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
			{
				if (((*tIt)->GetSipInterface() == pIp + ":" + pPort) && ((*tIt)->GetParticipantTransport() == pTransport))
				{
					if (pInitState == CALLSTATE_RINGING)
					{
						if (MEETING.GetCallState(QString((*tIt)->GetParticipantName().toLocal8Bit()).toStdString(), pTransport) == CALLSTATE_STANDBY)
						{
							MEETING.SendCall(MEETING.SipCreateId(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString()), pTransport);
							return NULL;
						}else
						{
							ShowInfo("Participant already called", "The participant \"" + QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str()) + "\" is already called!\nThe participant is known as \"" + (*tIt)->GetParticipantName() + "\".");
							return NULL;
						}
					 }else
					 {
						 LOG(LOG_VERBOSE, "Returning without result");
						 return NULL;
					 }
				}
			}

			pHost = CompleteIpAddress(pHost);
			if (MEETING.OpenParticipantSession(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString(), pTransport))
			{
				QString tParticipant = QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str());

				tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantAVControls, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, tParticipant, pTransport);

				mParticipantWidgets.push_back(tParticipantWidget);

				if (pInitState == CALLSTATE_RINGING)
					MEETING.SendCall(MEETING.SipCreateId(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString()), pTransport);
			} else
				ShowInfo("Participant is already contacted", "The contact with the address \"" + QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str()) + "\" is already contacted and a participant widget is currently open!");
    	}else
    	{
			ShowInfo("Loop detected", "Loop detected: you tried to contact yoursel!");
    	}
	}
    return tParticipantWidget;
}

void MainWindow::DeleteParticipantSession(ParticipantWidget *pParticipantWidget)
{
    ParticipantWidgetList::iterator tIt;

    // store the participant widget ID for later processing
    QString tParticipantName = pParticipantWidget->GetParticipantName();
    enum TransportType tParticipantTransport = pParticipantWidget->GetParticipantTransport();
    enum SessionType tSessionType = pParticipantWidget->GetSessionType();

    // search for corresponding participant widget
    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
    {
        if ((*tIt) == pParticipantWidget)
        {
            delete (*tIt);
            mParticipantWidgets.erase(tIt);
            break;
        }
    }

    if (tSessionType == PARTICIPANT)
    {
        if (!MEETING.CloseParticipantSession(QString(tParticipantName.toLocal8Bit()).toStdString(), tParticipantTransport))
        {
            LOG(LOG_ERROR, "Could not close the session with participant");
        }
    }
}

MediaSourceMuxer* MainWindow::GetVideoMuxer()
{
    return mOwnVideoMuxer;
}

MediaSourceMuxer* MainWindow::GetAudioMuxer()
{
    return mOwnAudioMuxer;
}

void MainWindow::actionExit()
{
    close();
}

void MainWindow::actionIdentity()
{
    IdentityDialog tIdentityDialog(this);
    tIdentityDialog.exec();
}

void MainWindow::actionConfiguration()
{
    bool tFormerStateMeetingProbeContacts = CONF.GetSipContactsProbing();

    ConfigurationDialog tConfigurationDialog(this, mLocalAddresses, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker());

    // inform the whole multimedia system about new settings if user has acknowledged the dialog settings
    if (tConfigurationDialog.exec() == QDialog::Accepted)
    {
        bool tNeedUpdate;
        int tX = 352, tY = 288;
        ParticipantWidgetList::iterator tIt;

        LOG(LOG_VERBOSE, "Propagating new configuration to all participants");

        // calculate the resolution from string
        MediaSource::VideoString2Resolution(CONF.GetVideoResolution().toStdString(), tX, tY);

        // get the codecs
        string tVideoCodec = CONF.GetVideoCodec().toStdString();
        string tAudioCodec = CONF.GetAudioCodec().toStdString();

        /* video */
        tNeedUpdate = mOwnVideoMuxer->SetOutputStreamPreferences(tVideoCodec, CONF.GetVideoQuality(), CONF.GetVideoBitRate(), CONF.GetVideoMaxPacketSize(), false, tX, tY, CONF.GetVideoRtp(), CONF.GetVideoFps());
        mOwnVideoMuxer->SetRelayActivation(CONF.GetVideoActivation());
        if (tNeedUpdate)
            mLocalUserParticipantWidget->GetVideoWorker()->ResetSource();
        mLocalUserParticipantWidget->GetVideoWorker()->SetCurrentDevice(CONF.GetLocalVideoSource());
        //tNeedUpdate =
                //mOwnVideoMuxer->SelectDevice(CONF.GetLocalVideoSource().toStdString(), MEDIA_VIDEO);// || tNeedUpdate;
        mOwnVideoMuxer->SetVideoFlipping(CONF.GetLocalVideoSourceHFlip(), CONF.GetLocalVideoSourceVFlip());
        if (tNeedUpdate)
        {
            LOG(LOG_VERBOSE, "Local user's video source should have a settings update with reset");
            mLocalUserParticipantWidget->SetVideoStreamPreferences(QString(tVideoCodec.c_str()), true);
        }

        /* audio */
        tNeedUpdate = mOwnAudioMuxer->SetOutputStreamPreferences(tAudioCodec, 100, CONF.GetAudioBitRate(), CONF.GetAudioMaxPacketSize(), false, 0, 0, CONF.GetAudioRtp());
        mOwnAudioMuxer->SetRelayActivation(CONF.GetAudioActivation() && !CONF.GetAudioActivationPushToTalk());
        mOwnAudioMuxer->SetRelaySkipSilence(CONF.GetAudioSkipSilence());
        if (tNeedUpdate)
            mLocalUserParticipantWidget->GetAudioWorker()->ResetSource();
        mLocalUserParticipantWidget->GetAudioWorker()->SetCurrentDevice(CONF.GetLocalAudioSource());
        //tNeedUpdate =
                //mOwnAudioMuxer->SelectDevice(CONF.GetLocalAudioSource().toStdString(), MEDIA_AUDIO);// || tNeedUpdate;
        if (tNeedUpdate)
        {
            LOG(LOG_VERBOSE, "Local user's audio source should have a settings update with reset");
            mLocalUserParticipantWidget->SetAudioStreamPreferences(QString(tAudioCodec.c_str()), true);
        }

        if (mParticipantWidgets.size())
        {
            // search for corresponding participant widget
            for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
            {
                (*tIt)->SetVideoStreamPreferences(QString(tVideoCodec.c_str()));
                (*tIt)->SetAudioStreamPreferences(QString(tAudioCodec.c_str()));
            }
        }

        // show QSliders for video and audio if we have seekable media sources selected
        if (mOwnVideoMuxer->SupportsMultipleInputStreams())
            mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
        else
            mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

        // do an explicit auto probing of known contacts in case the user has activated this feature in the configuration dialog
        if ((!tFormerStateMeetingProbeContacts) && (CONF.GetSipContactsProbing()))
        {
            LOG(LOG_VERBOSE, "Do an explicit auto probing of known contacts because user has activated the auto-probing feature via configuration dialog");
            CONTACTS.ProbeAvailabilityForAll();
        }
        MEETING.SetVideoAudioStartPort(CONF.GetVideoAudioStartPort());

        /* language */
    	SetLanguage(CONF.GetLanguage());
    }
}

void MainWindow::actionHelp()
{
    HelpDialog tHelpDia(this);
    tHelpDia.exec();
}

void MainWindow::actionUpdateCheck()
{
    UpdateCheckDialog tUpCheckDia(this);
    tUpCheckDia.exec();
}

void MainWindow::actionVersion()
{
    VersionDialog tVerDia(this);
    tVerDia.exec();
}

void MainWindow::actionOpenVideoAudioPreview()
{
    ParticipantWidget *tParticipantWidget;

    tParticipantWidget = new ParticipantWidget(PREVIEW, this, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantAVControls, NULL);

    mParticipantWidgets.push_back(tParticipantWidget);
}

///////////////////////////////////////////////////////////////////////////////
/// SYSTRAY ICON FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
void MainWindow::CreateSysTray()
{
    LOG(LOG_VERBOSE, "Creating system tray object..");
    mSysTrayMenu = new QMenu(this);

    mSysTrayIcon = new QSystemTrayIcon(QIcon(":/images/LogoHomer3.png"), this);
    mSysTrayIcon->setContextMenu(mSysTrayMenu);
    mSysTrayIcon->setToolTip("Homer " RELEASE_VERSION_STRING " - live conferencing and more");
    mSysTrayIcon->show();

    connect(mSysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(activatedSysTray(QSystemTrayIcon::ActivationReason)));

    connect(mSysTrayMenu, SIGNAL(aboutToShow()), this, SLOT(UpdateSysTrayContextMenu()));

    #ifdef APPLE
        mDockMenu = new QMenu(this);

        qt_mac_set_dock_menu(mDockMenu);
        connect(mDockMenu, SIGNAL(aboutToShow()), this, SLOT(UpdateSysTrayContextMenu()));
    #endif

    UpdateSysTrayContextMenu();
}

void MainWindow::actionToggleWindowState()
{
    static bool sWasFullScreen = false;
    static bool sWasMaximized = false;

    LOG(LOG_VERBOSE, "Toggling window state");
    if (isMinimized())
    {
        LOG(LOG_VERBOSE, "Found window minized");
        activateWindow();
        if (sWasFullScreen)
        {
            LOG(LOG_VERBOSE, "Showing the main window in full screen mode");
            showFullScreen();
        }else
        {
            if (sWasMaximized)
            {
                LOG(LOG_VERBOSE, "Mayiming the main window");
                showMaximized();
            }else
            {
                LOG(LOG_VERBOSE, "Showing the main window in a normal view");
                showNormal();
            }
        }
        LOG(LOG_VERBOSE, "Bring the main window in focus and to the foreground");
        setFocus();
        show();
    }else
    {
        LOG(LOG_VERBOSE, "Found window non-minized");
        #ifndef APPLE
            if (!hasFocus())
            {
                LOG(LOG_VERBOSE, "Bring the main window in focus and to the foreground");
                activateWindow();
                setFocus();
                show();
            }else
            {
        #endif
                LOG(LOG_VERBOSE, "Minimizing the main window");
                showMinimized();
        #ifndef APPLE
            }
        #endif
        sWasFullScreen = isFullScreen();
        sWasMaximized = isMaximized();
    }
}

void MainWindow::actionMuteMe()
{
    mLocalUserParticipantWidget->GetAudioWorker()->SetMuteState(!mLocalUserParticipantWidget->GetAudioWorker()->GetMuteState());
    UpdateSysTrayContextMenu();
}

void MainWindow::actionMuteOthers()
{
    ParticipantWidgetList::iterator tIt;

    // search for corresponding participant widget
    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
    {
        (*tIt)->GetAudioWorker()->SetMuteState(true);
    }
}

void MainWindow::actionActivateNetworkSimulationWidgets()
{
    QStringList tArguments;
    initializeNetworkSimulator(tArguments, true);
}

void MainWindow::actionActivateMosaicMode(bool pActive)
{
    ParticipantWidgetList::iterator tIt;
    LOG(LOG_VERBOSE, "Setting mosaic mode to: %d", pActive);

    if (pActive)
	{
    	mMosaicModeFormerWindowFlags = windowFlags();
		QWidget* tTitleWidget = new QWidget(this);
	    if (mParticipantWidgets.size())
	    {
	        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
	        {
	    		(*tIt)->ToggleMosaicMode(pActive);
	        }
	    }
		mLocalUserParticipantWidget->ToggleMosaicMode(pActive);
		mStatusBar->hide();
		mMenuBar->hide();
		mMosaicModeToolBarOnlineStatusWasVisible = mToolBarOnlineStatus->isVisible();
		mMosaicModeToolBarMediaSourcesWasVisible = mToolBarMediaSources->isVisible();
		mToolBarMediaSources->hide();
		mToolBarOnlineStatus->hide();
		mOverviewContactsWidget->hide();
		mOverviewDataStreamsWidget->hide();
		mOverviewErrorsWidget->hide();
		mOverviewFileTransfersWidget->hide();
		mOverviewNetworkStreamsWidget->hide();
		mOverviewPlaylistWidget->hide();
		mOverviewThreadsWidget->hide();
		showFullScreen();

		mMosaicOriginalPalette = palette();
		QPalette tPalette = palette();
		tPalette.setColor(QPalette::Window, QColor(0, 0, 0));
		setPalette(tPalette);
	}else
	{
		setPalette(mMosaicOriginalPalette);
		showNormal();
		setWindowFlags(mMosaicModeFormerWindowFlags);
	    if (mParticipantWidgets.size())
	    {
	        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
	        {
	    		(*tIt)->ToggleMosaicMode(pActive);
	        }
	    }
		mLocalUserParticipantWidget->ToggleMosaicMode(pActive);
	    mStatusBar->setVisible(CONF.GetVisibilityStatusBar());
	    mMenuBar->setVisible(CONF.GetVisibilityMenuBar());
	    mToolBarMediaSources->setVisible(mMosaicModeToolBarMediaSourcesWasVisible);
	    mToolBarOnlineStatus->setVisible(mMosaicModeToolBarOnlineStatusWasVisible);
	    mOverviewContactsWidget->setVisible(CONF.GetVisibilityContactsWidget());
	    mOverviewDataStreamsWidget->setVisible(CONF.GetVisibilityDataStreamsWidget());
	    mOverviewErrorsWidget->setVisible(CONF.GetVisibilityErrorsWidget());
	    mOverviewFileTransfersWidget->setVisible(CONF.GetVisibilityFileTransfersWidget());
	    mOverviewNetworkStreamsWidget->setVisible(CONF.GetVisibilityNetworkStreamsWidget());
	    mOverviewPlaylistWidget->setVisible(CONF.GetVisibilityPlaylistWidgetMovie());
	    mOverviewThreadsWidget->setVisible(CONF.GetVisibilityThreadsWidget());
	}
}

void MainWindow::actionActivateToolBarOnlineStatus(bool pActive)
{
	LOG(LOG_VERBOSE, "Setting online status tool bar visibility to: %d", pActive);
	CONF.SetVisibilityToolBarOnlineStatus(pActive);
	mToolBarOnlineStatus->setVisible(pActive);
}

void MainWindow::actionActivateToolBarMediaSources(bool pActive)
{
	LOG(LOG_VERBOSE, "Setting media sources tool bar visibility to: %d", pActive);
	CONF.SetVisibilityToolBarMediaSources(pActive);
	mToolBarMediaSources->setVisible(pActive);
}

void MainWindow::actionActivateStatusBar(bool pActive)
{
	LOG(LOG_VERBOSE, "Setting status bar visibility to: %d", pActive);
	CONF.SetVisibilityStatusBar(pActive);
	mStatusBar->setVisible(pActive);
}

void MainWindow::actionActivateMenuBar(bool pActive)
{
	LOG(LOG_VERBOSE, "Setting menu bar visibility to: %d", pActive);
	CONF.SetVisibilityMenuBar(pActive);
	mMenuBar->setVisible(pActive);
}

void MainWindow::actionActivateDebuggingWidgets()
{
    printf("Activating verbose debug widgets\n");
    mActionOverviewDataStreamsWidget->setVisible(true);
    mActionOverviewNetworkStreamsWidget->setVisible(true);
    mActionOverviewThreadsWidget->setVisible(true);
    mOverviewDataStreamsWidget->toggleViewAction()->setVisible(true);
    mOverviewNetworkStreamsWidget->toggleViewAction()->setVisible(true);
    mOverviewThreadsWidget->toggleViewAction()->setVisible(true);
}

void MainWindow::actionActivateDebuggingGlobally()
{
    CONF.SetDebugging(true);
    LOGGER.SetLogLevel(LOG_VERBOSE);
}

void MainWindow::UpdateSysTrayContextMenu()
{
    QAction *tAction;
    QIcon *tIcon;
    QMenu *tMenu;

    LOG(LOG_VERBOSE, "Updating sys tray (and dock) menu");

    if (mSysTrayMenu != NULL)
    {
        mSysTrayMenu->clear();

        if (isMinimized())
        {
            tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Show window"));
        }else
        {
            tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Hide window"));
        }
        tIcon = new QIcon(":/images/22_22/Resize.png");
        tAction->setIcon(*tIcon);
        connect(tAction, SIGNAL(triggered()), this, SLOT(actionToggleWindowState()));

        mSysTrayMenu->addSeparator();

        if (mLocalUserParticipantWidget->GetAudioWorker()->GetMuteState())
        {
            tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Unmute me"));
            tIcon = new QIcon(":/images/22_22/SpeakerLoud.png");
        }else
        {
            tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Mute me"));
            tIcon = new QIcon(":/images/22_22/SpeakerMuted.png");
        }
        tAction->setIcon(*tIcon);
        connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteMe()));

        tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Mute others"));
        tIcon = new QIcon(":/images/22_22/SpeakerMuted.png");
        tAction->setIcon(*tIcon);
        connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteOthers()));

        mSysTrayMenu->addSeparator();

        if (CONF.ConferencingEnabled())
        {
            tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Online status"));
            tMenu = new QMenu(this);
            mOnlineStatusWidget->InitializeMenuOnlineStatus(tMenu);
            tAction->setMenu(tMenu);
            connect(tMenu, SIGNAL(triggered(QAction *)), mOnlineStatusWidget, SLOT(Selected(QAction *)));
        }

        mSysTrayMenu->addSeparator();

        tAction = mSysTrayMenu->addAction(Homer::Gui::MainWindow::tr("Exit"));
        tIcon = new QIcon(":/images/22_22/Exit.png");
        tAction->setIcon(*tIcon);
        connect(tAction, SIGNAL(triggered()), this, SLOT(actionExit()));
    }

    #ifdef APPLE
        if (mDockMenu != NULL)
        {
            mDockMenu->clear();

            if (isMinimized())
            {
                tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Show window"));
            }else
            {
                tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Hide window"));
            }
            tIcon = new QIcon(":/images/22_22/Resize.png");
            tAction->setIcon(*tIcon);
            connect(tAction, SIGNAL(triggered()), this, SLOT(actionToggleWindowState()));

            mDockMenu->addSeparator();

            if (mLocalUserParticipantWidget->GetAudioWorker()->GetMuteState())
            {
                tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Unmute me"));
                tIcon = new QIcon(":/images/22_22/SpeakerLoud.png");
            }else
            {
                tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Mute me"));
                tIcon = new QIcon(":/images/22_22/SpeakerMuted.png");
            }
            tAction->setIcon(*tIcon);
            connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteMe()));

            tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Mute others"));
            tIcon = new QIcon(":/images/22_22/SpeakerMuted.png");
            tAction->setIcon(*tIcon);
            connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteOthers()));

            mDockMenu->addSeparator();

            if (CONF.ConferencingEnabled())
            {
                tAction = mDockMenu->addAction(Homer::Gui::MainWindow::tr("Online status"));
                tMenu = new QMenu(this);
                mOnlineStatusWidget->InitializeMenuOnlineStatus(tMenu);
                tAction->setMenu(tMenu);
                connect(tMenu, SIGNAL(triggered(QAction *)), mOnlineStatusWidget, SLOT(Selected(QAction *)));
            }
        }else
            LOG(LOG_VERBOSE, "Invalid dock menu object");
    #endif
}

void MainWindow::activatedSysTray(QSystemTrayIcon::ActivationReason pReason)
{
    switch(pReason)
    {
        case QSystemTrayIcon::Context:  //The context menu for the system tray entry was requested
            LOG(LOG_VERBOSE, "SysTrayIcon activated for context menu");
            break;
        case QSystemTrayIcon::DoubleClick: //The system tray entry was double clicked
            LOG(LOG_VERBOSE, "SysTrayIcon activated for double click");
            actionToggleWindowState();
            break;
        case QSystemTrayIcon::Trigger:  //The system tray entry was clicked
            LOG(LOG_VERBOSE, "SysTrayIcon activated for trigger");
            break;
        case QSystemTrayIcon::MiddleClick:
            LOG(LOG_VERBOSE, "SysTrayIcon activated for middle click");
            break;
        default:
        case QSystemTrayIcon::Unknown:  //Unknown reason
            LOG(LOG_VERBOSE, "SysTrayIcon activated for unknown reason");
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////

}}
