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

#include <ContactsPool.h>
#include <MainWindow.h>
#include <Configuration.h>
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
#include <LogSinkFile.h>
#include <LogSinkNet.h>
#include <Meeting.h>
#include <AudioOutSdl.h>
#include <MediaSourceV4L2.h>
#include <MediaSourceMMSys.h>
#include <MediaSourceVFW.h>
#include <MediaSourceMuxer.h>
#include <MediaSourceOss.h>
#include <MediaSourceAlsa.h>
#include <MediaSourceFile.h>
#include <MediaSourceDesktop.h>
#include <ProcessStatisticService.h>
#include <WaveOutAlsa.h>
#include <WaveOutMMSys.h>
#include <Snippets.h>

#include <QPlastiqueStyle>
#include <QApplication>
#include <QTime>
#include <QTextEdit>
#include <QMenu>
#include <QScrollBar>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QStatusBar>
#include <QStringList>
#include <QLabel>

using namespace Homer::Monitor;
using namespace Homer::SoundOutput;
using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(const std::string& pAbsBinPath) :
    QMainWindow(),
    Ui_MainWindow(),
    MeetingObserver()
{
    SVC_PROCESS_STATISTIC.AssignThreadName("Qt-MainLoop");
    mAbsBinPath = pAbsBinPath;
    mSourceDesktop = NULL;
    QCoreApplication::setApplicationName("Homer");
    QCoreApplication::setApplicationVersion("1.0");

    // get the program arguments
    QStringList tArguments = QCoreApplication::arguments();
    QString tArg;
    int i = 0;
    foreach(tArg, tArguments)
    {
        LOG(LOG_VERBOSE, "Argument[%d]: \"%s\"", i++, tArg.toStdString().c_str());
    }

    // init program configuration
    CONF.Init(mAbsBinPath);
    // disabling of features
    initializeFeatureDisablers(tArguments);
    // init log sinks
    initializeLogging(tArguments);
    // create basic GUI objects
    initializeGUI();
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
    CONTACTSPOOL.Init(CONF.GetContactFile().toStdString());
    // connect signals and slots, set visibility of some GUI objects
    connectSignalsSlots();
    // auto update check
    triggerUpdateCheck();
    // init screen capturing
    initializeScreenCapturing();
}

MainWindow::~MainWindow()
{
}

void MainWindow::initializeGUI()
{
    LOG(LOG_VERBOSE, "Initialization of GUI..");
    setupUi(this);
    move(CONF.GetMainWindowPosition());
    resize(CONF.GetMainWindowSize());
}

void MainWindow::connectSignalsSlots()
{
    LOG(LOG_VERBOSE, "Connecting signals and slots of GUI..");
    connect(mActionExit, SIGNAL(triggered()), this, SLOT(actionExit()));

    connect(mActionIdentity, SIGNAL(triggered()), this, SLOT(actionIdentity()));
    connect(mActionConfiguration, SIGNAL(triggered()), this, SLOT(actionConfiguration()));

    connect(mActionOpenVideoAudioPreview, SIGNAL(triggered()), this, SLOT(actionOpenVideoAudioPreview()));
    connect(mActionHelp, SIGNAL(triggered()), this, SLOT(actionHelp()));
    connect(mActionUpdateCheck, SIGNAL(triggered()), this, SLOT(actionUpdateCheck()));
    connect(mActionVersion, SIGNAL(triggered()), this, SLOT(actionVersion()));

    connect(mShortcutActivateDebugWidgets, SIGNAL(activated()), this, SLOT(actionActivateDebuggingWidgets()));
    connect(mShortcutActivateDebuggingGlobally, SIGNAL(activated()), this, SLOT(actionActivateDebuggingGlobally()));

    connect(mActionToolBarOnlineStatus, SIGNAL(toggled(bool)), mToolBarOnlineStatus, SLOT(setVisible(bool)));
    connect(mToolBarOnlineStatus->toggleViewAction(), SIGNAL(toggled(bool)), mActionToolBarOnlineStatus, SLOT(setChecked(bool)));
    connect(mActionToolBarMediaSources, SIGNAL(toggled(bool)), mToolBarMediaSources, SLOT(setVisible(bool)));
    connect(mToolBarMediaSources->toggleViewAction(), SIGNAL(toggled(bool)), mActionToolBarMediaSources, SLOT(setChecked(bool)));
    connect(mActionMonitorBroadcastWidget, SIGNAL(toggled(bool)), mLocalUserParticipantWidget, SLOT(setVisible(bool)));

    mActionMonitorBroadcastWidget->setChecked(CONF.GetVisibilityBroadcastWidget());

    mToolBarMediaSources->setVisible(CONF.GetVisibilityToolBarMediaSources());
    mToolBarMediaSources->toggleViewAction()->setChecked(CONF.GetVisibilityToolBarMediaSources());
    mActionToolBarMediaSources->setChecked(CONF.GetVisibilityToolBarMediaSources());

    mToolBarOnlineStatus->setVisible(CONF.GetVisibilityToolBarOnlineStatus());
    mToolBarOnlineStatus->toggleViewAction()->setChecked(CONF.GetVisibilityToolBarOnlineStatus());
    mActionToolBarOnlineStatus->setChecked(CONF.GetVisibilityToolBarOnlineStatus());
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
    mScreenShotTimer->start(3000);
}

void MainWindow::initializeFeatureDisablers(QStringList pArguments)
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
        }
    }
}

void MainWindow::initializeLogging(QStringList pArguments)
{
    LOG(LOG_VERBOSE, "Initialization of logging..");

    // console based log sink
    if (pArguments.contains("-DebugLevel=Error"))
    {
        CONF.SetDebugging(true);
    }else
        if (pArguments.contains("-DebugLevel=Info"))
        {
            CONF.SetDebugging(true);
        }else
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

    // file based log sinks
    QStringList tFiles = pArguments.filter("-DebugOutputFile=");
    if (tFiles.size())
    {
        QString tFileName;
        foreach(tFileName, tFiles)
        {
            tFileName = tFileName.remove("-DebugOutputFile=");
            LOGGER.RegisterLogSink(new LogSinkFile(tFileName.toStdString()));
        }
    }

    // network based log sinks
    QStringList tPorts = pArguments.filter("-DebugOutputNetwork=");
    if (tPorts.size())
    {
        QString tNetwork;
        foreach(tNetwork, tPorts)
        {
        	tNetwork = tNetwork.remove("-DebugOutputNetwork=");
        	int tPos = tNetwork.lastIndexOf(':');
        	if (tPos != -1)
        	{
        		QString tPortStr = tNetwork.right(tNetwork.size() - tPos -1);
        		tNetwork = tNetwork.left(tPos);
				bool tPortParsingWasOkay = false;
        		int tPort = tPortStr.toInt(&tPortParsingWasOkay);
        		if (tPortParsingWasOkay)
        		{
        			LOG(LOG_VERBOSE, "New network based log sink at %s:%d", tNetwork.toStdString().c_str(), tPort);
					LOGGER.RegisterLogSink(new LogSinkNet(tNetwork.toStdString(), tPort));
        		}else
        			LOG(LOG_ERROR, "Couldn't parse %s as network port", tPortStr.toStdString().c_str());
        	}
        }
    }
}

void MainWindow::initializeConferenceManagement()
{
    LOG(LOG_VERBOSE, "Initialization of conference management..");

    QString tLocalSourceIp = "";
    QString tLocalLoopIp = "";
    bool tInterfaceFound = GetNetworkInfo(mLocalAddresses, tLocalSourceIp, tLocalLoopIp);

    if (tInterfaceFound)
    {
        LOG(LOG_INFO, "Using IP address: %s", tLocalSourceIp.toStdString().c_str());
        CONF.SetSipListenerAddress(tLocalSourceIp);
        MEETING.Init(tLocalSourceIp.toStdString(), mLocalAddresses, "BROADCAST", CONF.GetSipStartPort(), CONF.GetSipStartPort() + 10, CONF.GetVideoAudioStartPort());
        MEETING.AddObserver(this);
    } else
    {
        if (tLocalLoopIp != "")
        {
            LOG(LOG_INFO, "No fitting network interface towards outside found");
            LOG(LOG_INFO, "Using loopback interface with IP address: %s", tLocalLoopIp.toStdString().c_str());
            LOG(LOG_INFO, "==>>>>> NETWORK TIMEOUTS ARE POSSIBLE! APPLICATION MAY HANG FOR MOMENTS! <<<<<==");
            MEETING.Init(tLocalLoopIp.toStdString(), mLocalAddresses, "BROADCAST", CONF.GetSipStartPort(), CONF.GetSipStartPort() + 10, CONF.GetVideoAudioStartPort());
            MEETING.AddObserver(this);
        } else
        {
            LOG(LOG_ERROR, "No fitting network interface present");
            exit(-1);
        }
    }
}

void MainWindow::initializeVideoAudioIO()
{
    LOG(LOG_VERBOSE, "Initialization of video/audio I/O..");

    string tVSourceSelection = CONF.GetLocalVideoSource().toStdString();
    string tASourceSelection = CONF.GetLocalAudioSource().toStdString();

    // ############################
    // ### VIDEO
    // ############################
    LOG(LOG_VERBOSE, "Creating video media objects..");
    mOwnVideoMuxer = new MediaSourceMuxer();
	#ifdef LINUX
		mOwnVideoMuxer->RegisterMediaSource(new MediaSourceV4L2(tVSourceSelection));
	#endif
	#ifdef WIN32
		mOwnVideoMuxer->RegisterMediaSource(new MediaSourceVFW(tVSourceSelection));
	#endif
    mOwnVideoMuxer->RegisterMediaSource(mSourceDesktop = new MediaSourceDesktop());
    // ############################
    // ### AUDIO
    // ############################
    LOG(LOG_VERBOSE, "Creating audio media objects..");
    mOwnAudioMuxer = new MediaSourceMuxer(NULL);
	#ifdef LINUX
		mOwnAudioMuxer->RegisterMediaSource(new MediaSourceAlsa(tASourceSelection));
		mOwnAudioMuxer->RegisterMediaSource(new MediaSourceOss());
		mWaveOut = new WaveOutAlsa("");
	#endif
	#ifdef WIN32
		mOwnAudioMuxer->RegisterMediaSource(new MediaSourceMMSys(tASourceSelection));
        mWaveOut = new WaveOutMMSys("");
	#endif
    #ifdef APPLE
        mWaveOut = NULL;
    #endif
    // audio output
	LOG(LOG_VERBOSE, "Opening audio output object..");
    AUDIOOUTSDL.OpenPlaybackDevice(AUDIO_OUTPUT_SAMPLE_RATE, true, "alsa", "auto");
}

void MainWindow::initializeColoring()
{
    LOG(LOG_VERBOSE, "Initialization of coloring..");

    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
            mMenuParticipantVideoWidgets->setStyleSheet(" QMenu { background-color: #ABABAB; border: 1px solid black; } QMenu::item { background-color: transparent; } QMenu::item:selected { background-color: #654321; }");
            mMenuParticipantAudioWidgets->setStyleSheet(" QMenu { background-color: #ABABAB; border: 1px solid black; } QMenu::item { background-color: transparent; } QMenu::item:selected { background-color: #654321; }");
            mMenuParticipantMessageWidgets->setStyleSheet(" QMenu { background-color: #ABABAB; border: 1px solid black; } QMenu::item { background-color: transparent; } QMenu::item:selected { background-color: #654321; }");
            mMenuBar->setStyleSheet(QString::fromUtf8(  " QMenuBar {\n"
                                                        "     background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,\n"
                                                        "                                       stop:0 lightgray, stop:1 darkgray);\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenuBar::item {\n"
                                                        "     spacing: 3px; /* spacing between menu bar items */\n"
                                                        "     padding: 1px 4px;\n"
                                                        "     background: transparent;\n"
                                                        "     border-radius: 4px;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenuBar::item:selected { /* when selected using mouse or keyboard */\n"
                                                        "     background: #008080;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenuBar::item:pressed {\n"
                                                        "     background: #008080;\n"
                                                        " }"));
            mMenuSettings->setStyleSheet(QString::fromUtf8( " QMenu {\n"
                                                        "     background-color: #ABABAB; /* sets background of the menu */\n"
                                                        "     border: 1px solid black;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item {\n"
                                                        "     /* sets background of menu item. set this to something non-transparent\n"
                                                        "         if you want menu color and menu item color to be different */\n"
                                                        "     background-color: transparent;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item:selected { /* when user selects item using mouse or keyboard */\n"
                                                        "     background-color: #654321;\n"
                                                        " }"));
            mMenuHomer->setStyleSheet(QString::fromUtf8( " QMenu {\n"
                                                        "     background-color: #ABABAB; /* sets background of the menu */\n"
                                                        "     border: 1px solid black;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item {\n"
                                                        "     /* sets background of menu item. set this to something non-transparent\n"
                                                        "         if you want menu color and menu item color to be different */\n"
                                                        "     background-color: transparent;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item:selected { /* when user selects item using mouse or keyboard */\n"
                                                        "     background-color: #654321;\n"
                                                        " }"));
            mMenuWindows->setStyleSheet(QString::fromUtf8(  " QMenu {\n"
                                                            "     background-color: #ABABAB; /* sets background of the menu */\n"
                                                            "     border: 1px solid black;\n"
                                                            " }\n"
                                                            "\n"
                                                            " QMenu::item {\n"
                                                            "     /* sets background of menu item. set this to something non-transparent\n"
                                                            "         if you want menu color and menu item color to be different */\n"
                                                            "     background-color: transparent;\n"
                                                            " }\n"
                                                            "\n"
                                                            " QMenu::item:selected { /* when user selects item using mouse or keyboard */\n"
                                                            "     background-color: #654321;\n"
                                                            " }"));
            mMenuMain->setStyleSheet(QString::fromUtf8( " QMenu {\n"
                                                        "     background-color: #ABABAB; /* sets background of the menu */\n"
                                                        "     border: 1px solid black;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item {\n"
                                                        "     /* sets background of menu item. set this to something non-transparent\n"
                                                        "         if you want menu color and menu item color to be different */\n"
                                                        "     background-color: transparent;\n"
                                                        " }\n"
                                                        "\n"
                                                        " QMenu::item:selected { /* when user selects item using mouse or keyboard */\n"
                                                        "     background-color: #654321;\n"
                                                        " }"));

            break;
        default:
            break;
    }
}

void MainWindow::initializeWidgetsAndMenus()
{
    LOG(LOG_VERBOSE, "Initialization of widgets and menus..");

    // set fixed style "plastic"
    QApplication::setStyle(new QPlastiqueStyle());

    mMenuParticipantMessageWidgets = new QMenu("Participant messages");
    mActionParticipantMessageWidgets->setMenu(mMenuParticipantMessageWidgets);

    mLocalUserParticipantWidget = new ParticipantWidget(BROADCAST, this, mOverviewContactsWidget, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer);
    setCentralWidget(mLocalUserParticipantWidget);

    mOnlineStatusWidget = new AvailabilityWidget(this);
    mToolBarOnlineStatus->addWidget(mOnlineStatusWidget);

    CreateSysTray();

    LOG(LOG_VERBOSE, "Creating playlist control widgets..");
    mOverviewPlaylistWidgetVideo = new OverviewPlaylistWidget(mActionOverviewVideoPlaylistWidget, this, PLAYLIST_VIDEO, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker());
    mOverviewPlaylistWidgetAudio = new OverviewPlaylistWidget(mActionOverviewAudioPlaylistWidget, this, PLAYLIST_AUDIO, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker());
    mOverviewPlaylistWidgetMovie = new OverviewPlaylistWidget(mActionOverviewMoviePlaylistWidget, this, PLAYLIST_MOVIE, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker());
    tabifyDockWidget(mOverviewPlaylistWidgetVideo, mOverviewPlaylistWidgetAudio);
    tabifyDockWidget(mOverviewPlaylistWidgetAudio, mOverviewPlaylistWidgetMovie);

    mMediaSourcesControlWidget = new StreamingControlWidget(mLocalUserParticipantWidget, mSourceDesktop, mOverviewPlaylistWidgetVideo, mOverviewPlaylistWidgetAudio, mOverviewPlaylistWidgetMovie);
    mToolBarMediaSources->addWidget(mMediaSourcesControlWidget);
    if (mOwnVideoMuxer->SupportsMultipleInputChannels())
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
    else
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

    if (mOwnVideoMuxer->SupportsMultipleInputChannels())
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
    else
        mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

    mOverviewDataStreamsWidget = new OverviewDataStreamsWidget(mActionOverviewDataStreamsWidget, this);
    mOverviewNetworkStreamsWidget = new OverviewNetworkStreamsWidget(mActionOverviewNetworkStreamsWidget, this);
    mOverviewThreadsWidget = new OverviewThreadsWidget(mActionOverviewThreadsWidget, this);
    tabifyDockWidget(mOverviewThreadsWidget, mOverviewDataStreamsWidget);
    tabifyDockWidget(mOverviewDataStreamsWidget, mOverviewNetworkStreamsWidget);

    mShortcutActivateDebugWidgets = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_A), this);
    mShortcutActivateDebuggingGlobally = new QShortcut(QKeySequence(Qt::ALT + Qt::Key_D), this);

    mOverviewContactsWidget = new OverviewContactsWidget(mActionOverviewContactsWidget, this);
    mOverviewErrorsWidget = new OverviewErrorsWidget(mActionOverviewErrorsWidget, this);
    mOverviewFileTransfersWidget = new OverviewFileTransfersWidget(mActionOverviewFileTransfersWidget, this);
    tabifyDockWidget(mOverviewContactsWidget, mOverviewFileTransfersWidget);
}

///////////////////////////////////////////////////////////////////////////////

bool MainWindow::GetNetworkInfo(LocalAddressesList &pLocalAddressesList, QString &pLocalSourceIp, QString &pLocalLoopIp)
{
    QString tLastSipListenerAddress = CONF.GetSipListenerAddress();
    bool tLocalInterfaceSet = false;
    pLocalSourceIp = "";

    //### determine all local IP addresses
    QList<QHostAddress> tQtLocalAddresses = QNetworkInterface::allAddresses();

    LOG(LOG_INFO, "Local IPv4 addresses are:");
    for (int i = 0; i < tQtLocalAddresses.size(); i++)
    {
        if ((tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv4Protocol) || (tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv6Protocol))
        {
            LOG(LOG_INFO, "...%s", tQtLocalAddresses[i].toString().toStdString().c_str());

            if (tQtLocalAddresses[i].toString() == tLastSipListenerAddress)
            {
                pLocalSourceIp = tLastSipListenerAddress;
                LOG(LOG_INFO, ">>> used as local meeting listener address");
            }

            if (tQtLocalAddresses[i].protocol() == QAbstractSocket::IPv4Protocol)
                pLocalAddressesList.push_front(tQtLocalAddresses[i].toString().toStdString());
            else
                pLocalAddressesList.push_back(tQtLocalAddresses[i].toString().toStdString());
        }
    }

    //### go through network interfaces
    QList<QNetworkInterface> tHostInterfaces = QNetworkInterface::allInterfaces();
    QList<QNetworkAddressEntry> tAddresses;

    for (int i = 0; i < tHostInterfaces.size(); i++)
    {
        tAddresses = tHostInterfaces[i].addressEntries();

        if ((tAddresses.size()) &&
           ((tAddresses[0].ip().protocol() == QAbstractSocket::IPv4Protocol) || (tAddresses[0].ip().protocol() == QAbstractSocket::IPv6Protocol)) &&
           (!tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsLoopBack)) &&
           (tHostInterfaces[i].flags().testFlag(QNetworkInterface::IsRunning)) &&
           (tHostInterfaces[i].flags().testFlag(QNetworkInterface::CanBroadcast)))
        {
            LOG(LOG_INFO, "Found possible listener network interface: \"%s\"", tHostInterfaces[i].humanReadableName().toStdString().c_str());
            if (pLocalSourceIp == "")
            {
                LOG(LOG_INFO, ">>> used as local meeting listener interface");
                pLocalSourceIp = tAddresses[0].ip().toString();
            }
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
            switch (tAddresses[j].ip().protocol())
            {
                case QAbstractSocket::IPv4Protocol:
                                        LOG(LOG_INFO, "  ..associated IPv4: %s NETMASK: %s BROADCAST: %s",
                                                tAddresses[j].ip().toString().toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toStdString().c_str(),
                                                tAddresses[j].broadcast().toString().toStdString().c_str());
                                        break;
                case QAbstractSocket::IPv6Protocol:
                                        LOG(LOG_INFO, "  ..associated IPv6: %s NETMASK: %s",
                                                tAddresses[j].ip().toString().toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toStdString().c_str());
                                        break;
                case QAbstractSocket::UnknownNetworkLayerProtocol:
                                        LOG(LOG_INFO, "  ..associated unknown address type: %s NETMASK: %s BROADCAST: %s",
                                                tAddresses[j].ip().toString().toStdString().c_str(),
                                                tAddresses[j].netmask().toString().toStdString().c_str(),
                                                tAddresses[j].broadcast().toString().toStdString().c_str());
                                        break;
            }
        }
    }

    if ((pLocalSourceIp == "") && (pLocalLoopIp != ""))
        pLocalSourceIp = pLocalLoopIp;

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
            ShowInfo("Update available", "An updated version of Homer-Conferencing is available. Current version on server is <font color='green'><b>" + tServerVersion +"</b></font>. Have a look in the \"update check\" dialogue in the application menu.");
    }
}

void MainWindow::CreateScreenShot()
{
	if(mSourceDesktop != NULL)
	    mSourceDesktop->CreateScreenshot();
    mScreenShotTimer->start(1000 / SCREEN_CAPTURE_FPS);
}

void MainWindow::loadSettings()
{
    int tX = 352;
    int tY = 288;

    LOG(LOG_VERBOSE, "Loading program settings..");
    LOG(LOG_VERBOSE, "..meeting settings");
    if (CONF.GetNatSupportActivation())
        MEETING.SetStunServer(CONF.GetStunServer().toStdString());

    MEETING.SetLocalUserName(QString(CONF.GetUserName().toLocal8Bit()).toStdString());
    MEETING.SetLocalUserMailAdr(QString(CONF.GetUserMail().toLocal8Bit()).toStdString());
    MEETING.setAvailabilityState(CONF.GetConferenceAvailability().toStdString());
    // is centralized mode selected activated?
    if (CONF.GetSipInfrastructureMode() == 1)
        MEETING.RegisterAtServer(CONF.GetSipUserName().toStdString(), CONF.GetSipPassword().toStdString(), CONF.GetSipServer().toStdString());

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
    MEETING.SetVideoTransportType(MEDIA_TRANSPORT_RTP_UDP);

    // init audio codec for network streaming, but only support ONE codec and not multiple
    QString tAudioStreamCodec = CONF.GetAudioCodec();
    // set settings within meeting management
    if (tAudioStreamCodec == "MP3 (MPA)")
        MEETING.SetAudioCodecsSupport(CODEC_MP3);
    if (tAudioStreamCodec == "G711 A-law (PCMA)")
        MEETING.SetAudioCodecsSupport(CODEC_G711A);
    if (tAudioStreamCodec == "G711 �-law (PCMU)")
        MEETING.SetAudioCodecsSupport(CODEC_G711U);
    if (tAudioStreamCodec == "AAC")
        MEETING.SetAudioCodecsSupport(CODEC_AAC);
    if (tAudioStreamCodec == "PCM_S16_LE")
        MEETING.SetAudioCodecsSupport(CODEC_PCMS16LE);
    if (tAudioStreamCodec == "GSM")
        MEETING.SetAudioCodecsSupport(CODEC_GSM);
    if (tAudioStreamCodec == "AMR")
        MEETING.SetAudioCodecsSupport(CODEC_AMR);
    MEETING.SetAudioTransportType(MEDIA_TRANSPORT_RTP_UDP);

    LOG(LOG_VERBOSE, "..video/audio settings");
    QString tVideoStreamResolution = CONF.GetVideoResolution();

    // calculate the resolution from string
    MediaSource::VideoString2Resolution(tVideoStreamResolution.toStdString(), tX, tY);

    // init video muxer
    mOwnVideoMuxer->SetOutputStreamPreferences(tVideoStreamCodec.toStdString(), CONF.GetVideoQuality(), CONF.GetVideoMaxPacketSize(), false, tX, tY, CONF.GetVideoRtp(), CONF.GetVideoFps());
    mOwnVideoMuxer->SetActivation(CONF.GetVideoActivation());
    bool tNewDeviceSelected = false;
    mOwnVideoMuxer->SelectDevice(CONF.GetLocalVideoSource().toStdString(), MEDIA_VIDEO, tNewDeviceSelected);
    // if former selected device isn't available we use one of the available instead
    if (!tNewDeviceSelected)
    {
        ShowWarning("Video device not availabe", "Can't use formerly selected video device: \"" + CONF.GetLocalVideoSource() + "\", will use one of the available devices instead!");
        mOwnVideoMuxer->SelectDevice("auto", MEDIA_VIDEO, tNewDeviceSelected);
    }
    mOwnVideoMuxer->SetVideoFlipping(CONF.GetLocalVideoSourceHFlip(), CONF.GetLocalVideoSourceVFlip());

    // init audio muxer
    mOwnAudioMuxer->SetOutputStreamPreferences(tAudioStreamCodec.toStdString(), CONF.GetAudioQuality(), CONF.GetAudioMaxPacketSize(), false, 0, 0, CONF.GetAudioRtp());
    mOwnAudioMuxer->SetActivation(CONF.GetAudioActivation());
    mOwnAudioMuxer->SelectDevice(CONF.GetLocalAudioSource().toStdString(), MEDIA_AUDIO, tNewDeviceSelected);
    // if former selected device isn't available we use one of the available instead
    if (!tNewDeviceSelected)
    {
        ShowWarning("Audio device not available", "Can't use formerly selected audio device: \"" + CONF.GetLocalAudioSource() + "\", will use one of the available devices instead!");
        mOwnAudioMuxer->SelectDevice("auto", MEDIA_AUDIO, tNewDeviceSelected);
    }
}

void MainWindow::closeEvent(QCloseEvent* pEvent)
{
    static bool tShuttingDown = false;

    if (tShuttingDown)
    {
        LOG(LOG_VERBOSE, "Got repeated call for closing the main window");
        return;
    }

    tShuttingDown = true;

    LOG(LOG_VERBOSE, "Got signal for closing main window");

    // remove ourself as observer for new Meeting events
    MEETING.DeleteObserver(this);

    ParticipantWidgetList::iterator tIt;

    CONF.SetMainWindowPosition(pos());
    CONF.SetMainWindowSize(size());
    CONF.SetVisibilityToolBarMediaSources(mToolBarMediaSources->isVisible());
    CONF.SetVisibilityToolBarOnlineStatus(mToolBarOnlineStatus->isVisible());

    // update IP entries within usage statistic
    //setIpStatistic(MEETING.GetHostAdr(), MEETING.getStunNatIp());

    // save the availability state within settings
    CONF.SetConferenceAvailability(QString(MEETING.getAvailabilityStateStr().c_str()));

    // ###### begin shutdown ################
    // stop the screenshot creating timer function
    mScreenShotTimer->stop();

    // prevent the system from further incoming events
    MEETING.Stop();

    // should be the last because video/audio workers could otherwise be deleted while they are still called
    if (mLocalUserParticipantWidget != NULL)
        delete mLocalUserParticipantWidget;

	// destroy all participant widgets
    if (mParticipantWidgets.size())
    {
        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
        {
            delete (*tIt);
        }
    }

    // deinit
    MEETING.Deinit();

    delete mShortcutActivateDebugWidgets;
    delete mShortcutActivateDebuggingGlobally;

    delete mOverviewDataStreamsWidget;
    delete mOverviewNetworkStreamsWidget;
    delete mOverviewThreadsWidget;

    delete mOverviewContactsWidget;
    delete mOverviewErrorsWidget;
    delete mOverviewFileTransfersWidget;
    delete mOverviewPlaylistWidgetVideo;
    delete mOverviewPlaylistWidgetAudio;
    delete mOverviewPlaylistWidgetMovie;
    delete mMediaSourcesControlWidget;
    delete mOnlineStatusWidget;
    delete mSysTrayIcon;

	//HINT: mSourceDesktop will be deleted by VideoWidget which grabbed from there

    // close audio output device
    AUDIOOUTSDL.ClosePlaybackDevice();

    // make sure this main window will be deleted when control returns to Qt event loop (need especially in case of closeEvent from fullscreen video widget)
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

void MainWindow::customEvent(QEvent* pEvent)
{
    // make sure we have our user defined QEvent
    if (pEvent->type() != QEvent::User)
        return;

    // some necessary variables
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

    QString tEventSender, tEventSenderApp;

    if(tEvent->getType() != ADD_PARTICIPANT)
        LOG(LOG_INFO, "Event of type \"%s\"", GeneralEvent::getNameFromType(tEvent->getType()).c_str());
    else
        LOG(LOG_INFO, "Event of type \"Add participant\"");

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
        return;

    switch(tEvent->getType())
    {
        case ADD_PARTICIPANT:
                    //####################### PARTICIPANT ADD #############################
                    tAPEvent = (AddParticipantEvent*) tEvent;
                    AddParticipantSession(tAPEvent->User, tAPEvent->Host, tAPEvent->Port, tAPEvent->Ip, tAPEvent->InitState);
                    return;
        case DELETE_SESSION:
                    //####################### PARTICIPANT DELETE #############################
                    tDSEvent = (DeleteSessionEvent*) tEvent;
                    RemoveSessionWidget(tDSEvent->PWidget);
                    return;
        case INT_START_NAT_DETECTION:
                    //####################### NAT DETECTION ANSWER ###########################
                    tINDEvent = (InternalNatDetectionEvent*) tEvent;
                    if (tINDEvent->Failed)
                        ShowError("NAT detection failed", "Could not detect NAT address and type via STUN server. The failure reason is \"" + QString(tINDEvent->FailureReason.c_str()) + "\".");
                    return;
        case OPTIONS_ACCEPT:
                    //######################## OPTIONS ACCEPT ##########################
                    tOAEvent = (OptionsAcceptEvent*) tEvent;

                    // inform contacts pool about online state
                    CONTACTSPOOL.UpdateContactState(QString::fromLocal8Bit(tOAEvent->Sender.c_str()), CONTACT_AVAILABLE);

                    // inform participant widget about new state
                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tOAEvent->Sender.c_str())))
                            {
                                (*tIt)->UpdateParticipantState(CONTACT_AVAILABLE);
                                return;
                            }
                        }
                    }

                    return;
        case OPTIONS_UNAVAILABLE:
                    //######################## OPTIONS UNAVAILABLE ##########################
                    tOUAEvent = (OptionsUnavailableEvent*) tEvent;

                    // inform contacts pool about online state
                    CONTACTSPOOL.UpdateContactState(QString::fromLocal8Bit(tOUAEvent->Sender.c_str()), CONTACT_UNAVAILABLE);

                    // inform participant widget about new state
                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tOUAEvent->Sender.c_str())))
                            {
                                (*tIt)->UpdateParticipantState(CONTACT_UNAVAILABLE);
                                return;
                            }
                        }
                    }

                    return;
        case GENERAL_ERROR:
                    //############################ GENERAL_ERROR #############################
                    tEEvent = (ErrorEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tEEvent->Sender.c_str())))
                        {
                            (*tIt)->HandleGeneralError(tEEvent->IsIncomingEvent, tEEvent->StatusCode, QString(tEEvent->Description.c_str()));
                            return;
                        }
                    }
                    return;
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
                                if ((*tIt)->IsThisParticipant(QString(tMEvent->Sender.c_str())))
                                {
                                    if (tMEvent->SenderName.size())
                                        (*tIt)->UpdateParticipantName(QString(tMEvent->SenderName.c_str()));
                                    (*tIt)->HandleMessage(tMEvent->IsIncomingEvent, QString(tMEvent->SenderName.c_str()), QString(tMEvent->Text.c_str()));
                                    return;
                                }
                            }
                        }

                        // add without any OpenSession-check, the session is always added automatically by the meeting-layer
                        tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mOverviewContactsWidget, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, QString(tMEvent->Sender.c_str()));

                        if (tParticipantWidget != NULL)
                        {
                            mParticipantWidgets.push_back(tParticipantWidget);
                            if (tMEvent->SenderName.size())
                                tParticipantWidget->UpdateParticipantName(QString(tMEvent->SenderName.c_str()));
                            tParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString(tMEvent->SenderName.c_str()), QString(tMEvent->Text.c_str()));
                        } else
                            LOG(LOG_ERROR, "ParticipantWidget creation failed");
                        return;
                    } else
                    {//broadcast message
                        if (tMEvent->SenderName.size())
                            mLocalUserParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString(("\"" + tMEvent->SenderName + "\"(" + tMEvent->Sender + ")\"").c_str()), QString(tMEvent->Text.c_str()));
                        else
                            mLocalUserParticipantWidget->HandleMessage(tMEvent->IsIncomingEvent, QString(tMEvent->SenderName.c_str()), QString(tMEvent->Text.c_str()));
                    }
                    return;
        case MESSAGE_ACCEPT:
                    //######################## MESSAGE ACCEPT ##########################
                    tMAEvent = (MessageAcceptEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMAEvent->Sender.c_str())))
                        {
                            (*tIt)->HandleMessageAccept(tMAEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case MESSAGE_ACCEPT_DELAYED:
                    //##################### MESSAGE ACCEPT DELAYED ######################
                    tMADEvent = (MessageAcceptDelayedEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMADEvent->Sender.c_str())))
                        {
                            (*tIt)->HandleMessageAcceptDelayed(tMADEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case MESSAGE_UNAVAILABLE:
                    //######################## MESSAGE UNAVAILABLE ##########################
                    tMUEvent = (MessageUnavailableEvent*) tEvent;

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tMUEvent->Sender.c_str())))
                        {
                            (*tIt)->HandleMessageUnavailable(tMUEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL:
                    //############################### CALL ##################################
                    tCEvent = (CallEvent*) tEvent;

                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Warning, CONF.GetSystrayTimeout());
                        }
                    }

                    if (mParticipantWidgets.size())
                    {
                        // search for corresponding participant widget
                        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                        {
                            if ((*tIt)->IsThisParticipant(QString(tCEvent->Sender.c_str())))
                            {
                                if (tCEvent->SenderName.size())
                                    (*tIt)->UpdateParticipantName(QString(tCEvent->SenderName.c_str()));

                                if (!tCEvent->AutoAnswering)
                                    (*tIt)->HandleCall(tCEvent->IsIncomingEvent, QString(tEvent->SenderApplication.c_str()));
                                return;
                            }
                        }
                    }

                    // add without any OpenSession-check, the session is always added automatically by the meeting-layer
                    tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mOverviewContactsWidget, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, QString(tCEvent->Sender.c_str()));

                    if (tParticipantWidget != NULL)
                    {
                        mParticipantWidgets.push_back(tParticipantWidget);
                        if (tCEvent->SenderName.size())
                            tParticipantWidget->UpdateParticipantName(QString(tCEvent->SenderName.c_str()));
                        if (!tCEvent->AutoAnswering)
                            tParticipantWidget->HandleCall(tCEvent->IsIncomingEvent, QString(tCEvent->SenderApplication.c_str()));
                    } else
                        LOG(LOG_ERROR, "ParticipantWidget creation failed");

                    return;
        case CALL_RINGING:
                    //############################# CALL RINGING ##############################
                    tCREvent = (CallRingingEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCREvent->Sender.c_str())))
                        {
                            if (tCREvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCREvent->SenderName.c_str()));
                            (*tIt)->HandleCallRinging(tCREvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_ACCEPT:
                    //############################# CALL ACCEPT ##############################
                    tCAEvent = (CallAcceptEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCAEvent->Sender.c_str())))
                        {
                            if (tCAEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCAEvent->SenderName.c_str()));
                            (*tIt)->HandleCallAccept(tCAEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_CANCEL:
                    //############################# CALL CANCEL ##############################
                    tCCEvent = (CallCancelEvent*) tEvent;

                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCCEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call canceled from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Warning, CONF.GetSystrayTimeout());
                        }
                    }

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCCEvent->Sender.c_str())))
                        {
                            if (tCCEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCCEvent->SenderName.c_str()));
                            (*tIt)->HandleCallCancel(tCCEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_DENY:
                    //############################# CALL DENY ##############################
                    tCDEvent = (CallDenyEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCDEvent->Sender.c_str())))
                        {
                            if (tCDEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCDEvent->SenderName.c_str()));
                            (*tIt)->HandleCallDenied(tCDEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_UNAVAILABLE:
                    //########################### CALL UNAVAILABLE ###########################
                    tCUEvent = (CallUnavailableEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCUEvent->Sender.c_str())))
                        {
                            (*tIt)->HandleCallUnavailable(tCUEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_HANGUP:
                    //############################# CALL HANGUP ##############################
                    tCHUEvent = (CallHangUpEvent*) tEvent;
                    // systray message
                    if ((tEvent->IsIncomingEvent) && (CONF.GetCallSystray()))
                    {
                        if ((!hasFocus()) || (isMinimized()))
                        {
                            GetEventSource(tCHUEvent, tEventSender, tEventSenderApp);
                            mSysTrayIcon->showMessage("Call hangup from " + tEventSender, (tEventSenderApp != "") ? "(via \"" + tEventSenderApp + "\")" : "", QSystemTrayIcon::Warning, CONF.GetSystrayTimeout());
                        }
                    }

                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCHUEvent->Sender.c_str())))
                        {
                            if (tCHUEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCHUEvent->SenderName.c_str()));
                            (*tIt)->HandleCallHangup(tCHUEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_TERMINATION:
                    //######################### CALL TERMINATION ############################
                    tCTEvent = (CallTerminationEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCTEvent->Sender.c_str())))
                        {
                            if (tCTEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCTEvent->SenderName.c_str()));
                            (*tIt)->HandleCallTermination(tCTEvent->IsIncomingEvent);
                            return;
                        }
                    }
                    return;
        case CALL_MEDIA_UPDATE:
                    //######################## CALL MEDIA UPDATE ############################
                    tCMUEvent = (CallMediaUpdateEvent*) tEvent;
                    // search for corresponding participant widget
                    for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
                    {
                        if ((*tIt)->IsThisParticipant(QString(tCMUEvent->Sender.c_str())))
                        {
                            if (tCMUEvent->SenderName.size())
                                (*tIt)->UpdateParticipantName(QString(tCMUEvent->SenderName.c_str()));
                            (*tIt)->HandleMediaUpdate(tCMUEvent->IsIncomingEvent, QString(tCMUEvent->RemoteAudioAddress.c_str()), tCMUEvent->RemoteAudioPort, QString(tCMUEvent->RemoteAudioCodec.c_str()), QString(tCMUEvent->RemoteVideoAddress.c_str()), tCMUEvent->RemoteVideoPort, QString(tCMUEvent->RemoteVideoCodec.c_str()));
                            return;
                        }
                    }
                    return;
        case REGISTRATION:
                    //####################### REGISTRATION SUCCEEDED #############################
                    tREvent = (RegistrationEvent*) tEvent;
                    mSysTrayIcon->showMessage("Registration successful", "Registered  \"" + CONF.GetSipUserName() + "\" at SIP server \"" + CONF.GetSipServer() + "\"!\n" \
                                              "SIP server runs software \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".", QSystemTrayIcon::Warning, CONF.GetSystrayTimeout());
                    return;
        case REGISTRATION_FAILED:
                    //####################### REGISTRATION FAILED #############################
                    tRFEvent = (RegistrationFailedEvent*) tEvent;
                    ShowError("Registration failed", "Could not register \"" + CONF.GetSipUserName() + "\" at the SIP server \"" + CONF.GetSipServer() + "\"!\n" \
                                                     "SIP server runs software \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".");
                    return;
        case PUBLICATION:
                    //####################### PUBLICATION SUCCEEDED #############################
                    tPEvent = (PublicationEvent*) tEvent;
                    //TODO: inform user
                    return;
        case PUBLICATION_FAILED:
                    //####################### PUBLICATION FAILED #############################
                    tPFEvent = (PublicationFailedEvent*) tEvent;
                    ShowError("Presence publication failed", "Could not publish your new presence state at the SIP server \"" + CONF.GetSipServer() + "\"!\n" \
                                                             "SIP server runs software \"" + QString(MEETING.GetServerSoftwareId().c_str()) + "\".");
                    return;
        default:
                    LOG(LOG_ERROR, "We should never reach this point! Otherwise there is an ERROR IN STATE MACHINE");
                    return;
    }
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

ParticipantWidget* MainWindow::AddParticipantSession(QString pUser, QString pHost, QString pPort, QString pIp, int pInitState)
{
    ParticipantWidget *tParticipantWidget = NULL;

    if (pHost.size())
    {
        // search for a participant widget with the same sip interface
        ParticipantWidgetList::iterator tIt;
        for (tIt = mParticipantWidgets.begin(); tIt != mParticipantWidgets.end(); tIt++)
        {
            if ((*tIt)->GetSipInterface() == pIp + ":" + pPort)
            {
                if (pInitState == CALLSTATE_RINGING)
                {
                    if (MEETING.GetCallState(QString((*tIt)->getParticipantName().toLocal8Bit()).toStdString()) == CALLSTATE_STANDBY)
                    {
                        MEETING.SendCall(MEETING.SipCreateId(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString()));
                        return NULL;
                    }else
                    {
                        ShowInfo("Participant already called", "The participant \"" + QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str()) + "\" is already called!\nThe participant is known as \"" + (*tIt)->getParticipantName() + "\".");
                        return NULL;
                    }
                 }else
                     return NULL;
            }
        }

        pHost = CompleteIpAddress(pHost);
        if (MEETING.OpenParticipantSession(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString(), CALLSTATE_STANDBY))
        {
            QString tParticipant = QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str());

            tParticipantWidget = new ParticipantWidget(PARTICIPANT, this, mOverviewContactsWidget, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, mMenuParticipantMessageWidgets, mOwnVideoMuxer, mOwnAudioMuxer, tParticipant);

            mParticipantWidgets.push_back(tParticipantWidget);

            if (pInitState == CALLSTATE_RINGING)
                MEETING.SendCall(MEETING.SipCreateId(QString(pUser.toLocal8Bit()).toStdString(), QString(pHost.toLocal8Bit()).toStdString(), pPort.toStdString()));
        } else
            ShowInfo("Participant is already contacted", "The contact with the address \"" + QString(MEETING.SipCreateId(pUser.toStdString(), pHost.toStdString(), pPort.toStdString()).c_str()) + "\" is already contacted and a participant widget is currently open!");
    }
    return tParticipantWidget;
}

void MainWindow::RemoveSessionWidget(ParticipantWidget *pParticipantWidget)
{
    ParticipantWidgetList::iterator tIt;
    QString tParticipantName;

    // store the participant name for later processing
    tParticipantName = pParticipantWidget->getParticipantName();

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
        if (!MEETING.CloseParticipantSession(QString(tParticipantName.toLocal8Bit()).toStdString()))
        {
            LOG(LOG_ERROR, "Could not close the session with participant");
        }
    }
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

    ConfigurationDialog tConfigurationDialog(this, mLocalAddresses, mLocalUserParticipantWidget->GetVideoWorker(), mLocalUserParticipantWidget->GetAudioWorker(), mWaveOut);

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
        tNeedUpdate = mOwnVideoMuxer->SetOutputStreamPreferences(tVideoCodec, CONF.GetVideoQuality(), CONF.GetVideoMaxPacketSize(), false, tX, tY, CONF.GetVideoRtp(), CONF.GetVideoFps());
        mOwnVideoMuxer->SetActivation(CONF.GetVideoActivation());
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
        tNeedUpdate = mOwnAudioMuxer->SetOutputStreamPreferences(tAudioCodec, CONF.GetAudioQuality(), CONF.GetAudioMaxPacketSize(), false, 0, 0, CONF.GetAudioRtp());
        mOwnAudioMuxer->SetActivation(CONF.GetAudioActivation());
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
        if (mOwnVideoMuxer->SupportsMultipleInputChannels())
            mMediaSourcesControlWidget->SetVideoInputSelectionVisible();
        else
            mMediaSourcesControlWidget->SetVideoInputSelectionVisible(false);

        // do an explicit auto probing of known contacts in case the user has activated this feature in the configuration dialogue
        if ((!tFormerStateMeetingProbeContacts) && (CONF.GetSipContactsProbing()))
        {
            LOG(LOG_VERBOSE, "Do an explicit auto probing of known contacts because user has activated the auto-probing feature via confiuration dialogue");
            CONTACTSPOOL.ProbeAvailabilityForAll();
        }
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

    tParticipantWidget = new ParticipantWidget(PREVIEW, this, mOverviewContactsWidget, mMenuParticipantVideoWidgets, mMenuParticipantAudioWidgets, NULL);

    mParticipantWidgets.push_back(tParticipantWidget);
}

///////////////////////////////////////////////////////////////////////////////
/// SYSTRAY ICON FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
void MainWindow::CreateSysTray()
{
    LOG(LOG_VERBOSE, "Creating system tray object..");
    mSysTrayMenu = new QMenu(this);
    UpdateSysTrayContextMenu();

    mSysTrayIcon = new QSystemTrayIcon(QIcon(":/images/LogoHomer3.png"), this);
    mSysTrayIcon->setContextMenu(mSysTrayMenu);
    mSysTrayIcon->setToolTip("Homer " RELEASE_VERSION_STRING " - live conferencing and more");
    mSysTrayIcon->show();

    connect(mSysTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(activatedSysTray(QSystemTrayIcon::ActivationReason)));
}

void MainWindow::actionToggleWindowState()
{
    static bool sWasFullScreen = false;
    static bool sWasMaximized = false;
    if (isMinimized())
    {
        activateWindow();
        if (sWasFullScreen)
        {
            showFullScreen();
        }else
        {
            if (sWasMaximized)
                showMaximized();
            else
                showNormal();
        }
        setFocus();
        show();
    }else
    {
        if (!hasFocus())
        {
            activateWindow();
            setFocus();
            show();
        }else
        {
            showMinimized();
        }
        sWasFullScreen = isFullScreen();
        sWasMaximized = isMaximized();
    }
}

void MainWindow::actionMuteMe()
{
	mLocalUserParticipantWidget->GetAudioWorker()->SetMuteState(!mLocalUserParticipantWidget->GetAudioWorker()->GetMuteState());
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

    mSysTrayMenu->clear();

    if (isMinimized())
	{
	    tAction = mSysTrayMenu->addAction("Show window");
	}else
	{
	    tAction = mSysTrayMenu->addAction("Hide window");
	}
    tIcon = new QIcon(":/images/Resize.png");
    tAction->setIcon(*tIcon);
    connect(tAction, SIGNAL(triggered()), this, SLOT(actionToggleWindowState()));

    mSysTrayMenu->addSeparator();

    if (mLocalUserParticipantWidget->GetAudioWorker()->GetMuteState())
    {
    	tAction = mSysTrayMenu->addAction("Unmute me");
    	tIcon = new QIcon(":/images/Speaker.png");
    }else
    {
    	tAction = mSysTrayMenu->addAction("Mute me");
    	tIcon = new QIcon(":/images/SpeakerMuted.png");
    }
	tAction->setIcon(*tIcon);
	connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteMe()));

	tAction = mSysTrayMenu->addAction("Mute others");
	tIcon = new QIcon(":/images/SpeakerMuted.png");
	tAction->setIcon(*tIcon);
	connect(tAction, SIGNAL(triggered()), this, SLOT(actionMuteOthers()));

	mSysTrayMenu->addSeparator();

    tAction = mSysTrayMenu->addAction("Online status");
	tMenu = new QMenu(this);
	mOnlineStatusWidget->InitializeMenuOnlineStatus(tMenu);
	tAction->setMenu(tMenu);
    connect(tMenu, SIGNAL(triggered(QAction *)), mOnlineStatusWidget, SLOT(Selected(QAction *)));

	mSysTrayMenu->addSeparator();

	tAction = mSysTrayMenu->addAction("Exit");
	tIcon = new QIcon(":/images/Exit.png");
	tAction->setIcon(*tIcon);
	connect(tAction, SIGNAL(triggered()), this, SLOT(actionExit()));
}

void MainWindow::activatedSysTray(QSystemTrayIcon::ActivationReason pReason)
{
	switch(pReason)
	{
		case QSystemTrayIcon::Context:	//The context menu for the system tray entry was requested
			LOG(LOG_VERBOSE, "SysTrayIcon activated for context menu");
			UpdateSysTrayContextMenu();
			break;
		case QSystemTrayIcon::DoubleClick: //The system tray entry was double clicked
			LOG(LOG_VERBOSE, "SysTrayIcon activated for double click");
			actionToggleWindowState();
			break;
		case QSystemTrayIcon::Trigger:	//The system tray entry was clicked
			LOG(LOG_VERBOSE, "SysTrayIcon activated for trigger");
			break;
		case QSystemTrayIcon::MiddleClick:
			LOG(LOG_VERBOSE, "SysTrayIcon activated for middle click");
			break;
		default:
		case QSystemTrayIcon::Unknown:	//Unknown reason
			LOG(LOG_VERBOSE, "SysTrayIcon activated for unknown reason");
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

}}
