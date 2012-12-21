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
 * Purpose: Implementation of a participant widget
 * Author:  Thomas Volkert
 * Since:   2008-12-06
 */

#include <Dialogs/OpenVideoAudioPreviewDialog.h>
#include <Widgets/StreamingControlWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <Widgets/ParticipantWidget.h>
#include <Widgets/VideoWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <MediaSourceNet.h>
#include <WaveOutPortAudio.h>
#include <WaveOutSdl.h>
#include <Widgets/AudioWidget.h>
#include <MediaSourceFile.h>
#include <MediaSourceNet.h>
#include <Widgets/MessageWidget.h>
#include <Widgets/SessionInfoWidget.h>
#include <MainWindow.h>
#include <Configuration.h>
#include <Meeting.h>
#include <MeetingEvents.h>
#include <Logger.h>
#include <Snippets.h>

#include <QInputDialog>
#include <QMenu>
#include <QEvent>
#include <QAction>
#include <QString>
#include <QWidget>
#include <QFrame>
#include <QDockWidget>
#include <QLabel>
#include <QMessageBox>
#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
#include <QCoreApplication>
#include <QHostInfo>

#include <stdlib.h>

using namespace Homer::Conference;
using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

// how often should we play the "call" event sound?
#define SOUND_LOOPS_FOR_CALL                                        10
// at what time period is timerEvent() called?
#define STREAM_POS_UPDATE_DELAY                                    250 // ms
// max. allowed drift between audio and video playback
#define AV_SYNC_MAX_DRIFT                                          0.1 // seconds
// min. time difference between two synch. processes
#define AV_SYNC_MIN_PERIOD                                         500 // ms
// how many times do we have to detect continuous asynch. A/V playback before we synch. audio and video? (to avoid false-positives)
#define AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD                          4 // checked every STREAM_POS_UPDATE_DELAY ms
// how many times should we try to adapt the A/V synch bei waiting cycles? afterwards we try a reset of both the video and audio source
#define AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD_TRY_RESET                4
// size of the pre-buffer during a live conference
#define AV_CONFERENCE_BUFFER									   0.2 // seconds

///////////////////////////////////////////////////////////////////////////////

ParticipantWidget::ParticipantWidget(enum SessionType pSessionType, MainWindow *pMainWindow, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pMessageMenu, MediaSourceMuxer *pVideoSourceMuxer, MediaSourceMuxer *pAudioSourceMuxer, QString pParticipant, enum TransportType pTransport):
    QDockWidget(pMainWindow), AudioPlayback()
{
    LOG(LOG_VERBOSE, "Creating new participant widget for %s..", pParticipant.toStdString().c_str());

    hide();
    mLastAudioSynchronizationTimestamp = 0;
    mLastVideoSynchronizationTimestamp = 0;
    mAVSynchActive = false;
    mAVPreBuffering = false;
    mAvPreBufferingAutoRestart = false;
    mCurrentMovieFile = "";
    mMainWindow = pMainWindow;
    mMovieSliderPosition = 0;
    mRemoteVideoAdr = "";
    mRemoteAudioAdr = "";
    mAVAsyncCounterSinceLastSynchronization = 0;
    mAVASyncCounter = 0;
    mVideoDelayAVDrift = 0;
    mRemoteVideoPort = 0;
    mRemoteAudioPort = 0;
    mParticipantVideoSink = NULL;
    mParticipantAudioSink = NULL;
    mFullscreeMovieControlWidget = NULL;
    mSessionTransport = CONF.GetSipListenerTransport();
    mTimeOfLastAVSynch = Time::GetTimeStamp();
    mCallBox = NULL;
    mVideoSourceMuxer = pVideoSourceMuxer;
    mAudioSourceMuxer = pAudioSourceMuxer;
    mSessionType = pSessionType;
    mSessionName = "";
    mSessionTransport = SOCKET_TRANSPORT_AUTO;
    mIncomingCall = false;
    mQuitForced = false;

    //####################################################################
    //### create the remaining necessary widgets, menu and layouts
    //####################################################################
    LOG(LOG_VERBOSE, "..init participant widget");
    Init(pVideoMenu, pAudioMenu, pMessageMenu, pParticipant, pTransport);
}

ParticipantWidget::~ParticipantWidget()
{
    LOG(LOG_VERBOSE, "Going to destroy %s participant widget..", mSessionName.toStdString().c_str());

    if (mTimerId != -1)
        killTimer(mTimerId);

    switch(mSessionType)
    {
        case BROADCAST:
            CONF.SetVisibilityBroadcastWidget(isVisible());
            CONF.SetVisibilityBroadcastAudio(mAudioWidget->isVisible());
            CONF.SetVisibilityBroadcastVideo(mVideoWidget->isVisible());
            break;
        case PARTICIPANT:
            {
                // inform the call partner
                switch (MEETING.GetCallState(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport))
                {
                    case CALLSTATE_RUNNING:
                        MEETING.SendHangUp(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport);
                        break;
                    case CALLSTATE_RINGING:
                        MEETING.SendCallCancel(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport);
                        break;
                    default:
                        break;
                }
            }
            break;
        case PREVIEW:
        default:
            break;
    }

    LOG(LOG_VERBOSE, "..destroying all widgets");
    delete mVideoWidget;
    delete mAudioWidget;
    delete mMessageWidget;
    delete mSessionInfoWidget;

    ResetMediaSinks();

    if (mSessionType != BROADCAST)
    {
		LOG(LOG_VERBOSE, "..destroying video source");
		delete mVideoSource;
		LOG(LOG_VERBOSE, "..destroying audio source");
		delete mAudioSource;
    }

	LOG(LOG_VERBOSE, "..closing playback device");
    ClosePlaybackDevice();
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void ParticipantWidget::Init(QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pMessageMenu, QString pParticipant, enum TransportType pTransport)
{
    setupUi(this);

    QFont font;
    font.setPointSize(8);
    font.setBold(true);
    font.setWeight(75);
    setFont(font);

    mSlMovie->Init(this);
    mAVDriftFrame->hide();
    mLbAVStatistics->hide();

    //TODO: remove the following if the feature is complete
    #ifdef RELEASE_VERSION
        mTbRecord->hide();
    #endif

    connect(mTbSendAudio, SIGNAL(clicked(bool)), this, SLOT(ActionToggleAudioSenderActivation(bool)));
    connect(mTbSendVideo, SIGNAL(clicked(bool)), this, SLOT(ActionToggleVideoSenderActivation(bool)));
    connect(mTbPlayPause, SIGNAL(clicked()), this, SLOT(ActionPlayPauseMovieFile()));
    connect(mTbRecord, SIGNAL(clicked()), this, SLOT(ActionRecordMovieFile()));
    connect(mSlMovie, SIGNAL(sliderMoved(int)), this, SLOT(ActionSeekMovieFile(int)));
    connect(mSlMovie, SIGNAL(valueChanged(int)), this, SLOT(ActionSeekMovieFileToPos(int)));
    connect(mSbAVDrift, SIGNAL(valueChanged(double)), this, SLOT(ActionUserAVDriftChanged(double)));
    connect(mTbNext, SIGNAL(clicked()), this, SLOT(ActionPlaylistNext()));
    connect(mTbPrevious, SIGNAL(clicked()), this, SLOT(ActionPlaylistPrevious()));
    connect(mTbPlaylist, SIGNAL(clicked(bool)), this, SLOT(ActionPlaylistSetVisible(bool)));
    mTbPlaylist->setChecked(CONF.GetVisibilityPlaylistWidgetMovie());

    //####################################################################
    //### create additional widget and allocate resources
    //####################################################################
    if ((CONF.GetParticipantWidgetsSeparation()) && (mSessionType == PARTICIPANT))
    {
        setParent(NULL);
        setWindowIcon(QPixmap(":/images/32_32/UserUnavailable.png"));
        setFeatures(QDockWidget::DockWidgetClosable);
    }else
    {
        setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        setAllowedAreas(Qt::AllDockWidgetAreas);
        mMainWindow->addDockWidget(Qt::RightDockWidgetArea, this, Qt::Horizontal);
    }

    mVideoSource = NULL;
    mAudioSource = NULL;
    OpenVideoAudioPreviewDialog *tOpenVideoAudioPreviewDialog = NULL;
    switch(mSessionType)
    {
        case BROADCAST:
                    LOG(LOG_VERBOSE, "Creating participant widget for "BROACAST_IDENTIFIER);
                    mSessionName = BROACAST_IDENTIFIER;
                    LOG(LOG_VERBOSE, "..init broadcast message widget");
                    mMessageWidget->Init(pMessageMenu, mSessionName, CONF.GetSipListenerTransport(), CONF.GetVisibilityBroadcastMessageWidget());
                    LOG(LOG_VERBOSE, "..init broadcast video widget");
                    mVideoSource = mVideoSourceMuxer;
                    mAudioSource = mAudioSourceMuxer;
                    if (mVideoSourceMuxer != NULL)
                    {
                        mVideoWidgetFrame->show();
                        mVideoWidget->Init(mMainWindow, this, mVideoSourceMuxer, pVideoMenu, mSessionName, CONF.GetVisibilityBroadcastVideo());
                    }
                    LOG(LOG_VERBOSE, "..init broadcast audio widget");
                    if (mAudioSourceMuxer != NULL)
                        mAudioWidget->Init(mAudioSourceMuxer, pAudioMenu, mSessionName, CONF.GetVisibilityBroadcastAudio(), true);
                    setFeatures(QDockWidget::NoDockWidgetFeatures);

                    // push-to-talk mode
                    if (CONF.GetAudioActivationPushToTalk())
                    {//PTT is active in config.
                        mAudioWidget->GetWorker()->SetRelayActivation(false);
                    }

                    // hide Homer logo
                    mLogoFrame->hide();
                    mTbSendAudio->hide();
                    mTbSendVideo->hide();

                    mAVSynchActive = true;
                    mAVPreBuffering = true;
                    mAvPreBufferingAutoRestart = true;

                    break;
        case PARTICIPANT:
					LOG(LOG_VERBOSE, "Creating participant widget for PARTICIPANT");
					mMovieControlsFrame->hide();
					mSessionName = pParticipant;
					mSessionTransport = pTransport;
					FindSipInterface(pParticipant);
					mMessageWidget->Init(pMessageMenu, mSessionName, mSessionTransport);
					mVideoSendSocket = MEETING.GetVideoSendSocket(mSessionName.toStdString(), mSessionTransport);
					mAudioSendSocket = MEETING.GetAudioSendSocket(mSessionName.toStdString(), mSessionTransport);
					mVideoReceiveSocket = MEETING.GetVideoReceiveSocket(mSessionName.toStdString(), mSessionTransport);
					mAudioReceiveSocket = MEETING.GetAudioReceiveSocket(mSessionName.toStdString(), mSessionTransport);
					if (mVideoReceiveSocket != NULL)
					{
						mVideoSource = new MediaSourceNet(mVideoReceiveSocket, true);
						mVideoSource->SetPreBufferingActivation(true);
						mVideoSource->SetPreBufferingAutoRestartActivation(true);
						mVideoSource->SetFrameBufferPreBufferingTime(AV_CONFERENCE_BUFFER);
						mVideoSource->SetInputStreamPreferences(CONF.GetVideoCodec().toStdString());
						mVideoWidgetFrame->hide();
						mVideoWidget->Init(mMainWindow, this, mVideoSource, pVideoMenu, mSessionName);
					}else
						LOG(LOG_ERROR, "Determined video socket is NULL");
					if (mAudioReceiveSocket != NULL)
					{
						mAudioSource = new MediaSourceNet(mAudioReceiveSocket, true);
						mAudioSource->SetPreBufferingActivation(true);
						mAudioSource->SetPreBufferingAutoRestartActivation(true);
						mAudioSource->SetFrameBufferPreBufferingTime(AV_CONFERENCE_BUFFER);
						mAudioSource->SetInputStreamPreferences(CONF.GetAudioCodec().toStdString());
						mAudioWidget->Init(mAudioSource, pAudioMenu, mSessionName);
					}else
						LOG(LOG_ERROR, "Determined audio socket is NULL");

					// hide Homer logo
					mLogoFrame->hide();

					mTbPrevious->hide();
					mTbNext->hide();
					mTbPlaylist->hide();

                    mAVSynchActive = true;
                    mAVPreBuffering = true;
                    mAvPreBufferingAutoRestart = true;

					break;
        case PREVIEW:
                    LOG(LOG_VERBOSE, "Creating participant widget for PREVIEW");
                    mSessionName = "PREVIEW";
                    if ((pVideoMenu != NULL) || (pAudioMenu != NULL))
                    {
                    	bool tFoundPreviewSource = false;
                        tOpenVideoAudioPreviewDialog = new OpenVideoAudioPreviewDialog(this);
                        if (tOpenVideoAudioPreviewDialog->exec() == QDialog::Accepted)
                        {
                        	bool tFilePreview = tOpenVideoAudioPreviewDialog->FileSourceSelected();

                            QString tVDesc, tADesc;

                            // create A/V source
                            mVideoSource = tOpenVideoAudioPreviewDialog->GetMediaSourceVideo();
                            mAudioSource = tOpenVideoAudioPreviewDialog->GetMediaSourceAudio();

                            // derive A/V media source description
                            if (mVideoSource != NULL)
                            	tVDesc = QString(mVideoSource->GetCurrentDeviceName().c_str());
                            if (mAudioSource != NULL)
                            	tADesc = QString(mAudioSource->GetCurrentDeviceName().c_str());

                            // update session name
                            if(tVDesc != tADesc)
                                mSessionName = "PREVIEW " + tVDesc + " / " + tADesc;
                            else
                                mSessionName = "PREVIEW " + tVDesc;

                            // create VIDEO widget
                            if (mVideoSource != NULL)
                            {
                                mVideoWidgetFrame->show();
                                mVideoWidget->Init(mMainWindow, this, mVideoSource, pVideoMenu, mSessionName, true);
                                tFoundPreviewSource = true;
                            }

                            // create AUDIO widget
                            if (mAudioSource != NULL)
                            {
                                mAudioWidget->Init(mAudioSource, pAudioMenu, mSessionName, true, false);
                                tFoundPreviewSource = true;
                            }

                            if (!tFilePreview)
                                mMovieControlsFrame->hide();
                        }
                        if(!tFoundPreviewSource)
                        {
                            // delete this participant widget again if no preview could be opened
                            QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new DeleteSessionEvent(this)));

                            delete tOpenVideoAudioPreviewDialog;

                            return;
                        }
                    }

                    // hide Homer logo
                    mLogoFrame->hide();

                    mTbSendAudio->hide();
                    mTbSendVideo->hide();
					mTbPrevious->hide();
					mTbNext->hide();
					mTbPlaylist->hide();

                    mAVSynchActive = tOpenVideoAudioPreviewDialog->AVSynchronization();
                    mAVPreBuffering = tOpenVideoAudioPreviewDialog->AVPreBuffering();
                    mAvPreBufferingAutoRestart = tOpenVideoAudioPreviewDialog->AVPreBufferingAutoRestart();

                    delete tOpenVideoAudioPreviewDialog;

					break;
        default:
                    break;
    }

    mSessionInfoWidget->Init(mSessionName, mSessionTransport, false);
    mSplitter->setStretchFactor(0, 1);
    mSplitter->setStretchFactor(1, 0);

    //####################################################################
    //### create additional user menus
    //####################################################################
    mMenuSettings = new QMenu(this);
    connect(mMenuSettings, SIGNAL(aboutToShow()), this, SLOT(UpdateMenuSettings()));
    if (mVideoSource != NULL)
    {
        mMenuSettingsVideo = mMenuSettings->addMenu(QPixmap(":/images/46_46/VideoReel.png"), "Video");
        connect(mMenuSettingsVideo, SIGNAL(triggered(QAction *)), mVideoWidget, SLOT(SelectedMenuVideoSettings(QAction*)));
    }
    if (mAudioSource)
    {
        mMenuSettingsAudio = mMenuSettings->addMenu(QPixmap(":/images/46_46/Speaker.png"), "Audio");
        connect(mMenuSettingsAudio, SIGNAL(triggered(QAction *)), mAudioWidget, SLOT(SelectedMenuAudioSettings(QAction*)));
    }
    if (mSessionType != PREVIEW)
    {
        mMenuSettingsMessages = mMenuSettings->addMenu(QPixmap(":/images/22_22/Message.png"), "Messages");
        connect(mMenuSettingsMessages, SIGNAL(triggered(QAction *)), mMessageWidget, SLOT(SelectedMenuMessagesSettings(QAction*)));
    }
    if (CONF.DebuggingEnabled())
    {
        if (mSessionType == PARTICIPANT)
        {
            mMenuSettingsSessionInfo = mMenuSettings->addMenu(QPixmap(":/images/22_22/Info.png"), "Session info");
            connect(mMenuSettingsSessionInfo, SIGNAL(triggered(QAction *)), mSessionInfoWidget, SLOT(SelectedMenuSessionInfoSettings(QAction*)));
        }
    }
    mTbSettings->setMenu(mMenuSettings);
    mTbSettings->setText("  ");

    //####################################################################
    //### update GUI
    //####################################################################

    UpdateParticipantName(mSessionName);
    // hide automatic QAction within QDockWidget context menu
    toggleViewAction()->setVisible(false);
    show();

    if (mSessionType == BROADCAST)
        setVisible(CONF.GetVisibilityBroadcastWidget());
    if (mSessionType == PARTICIPANT)
    	ResizeAVView(0);
    UpdateMovieControls();

    mTimerId = startTimer(STREAM_POS_UPDATE_DELAY);

    LOG(LOG_VERBOSE, "Initiating sound object for acoustic notifications..");
    OpenPlaybackDevice(mSessionName + "-Events");
}

void ParticipantWidget::UpdateMenuSettings()
{
    if (mVideoSource != NULL)
        mVideoWidget->InitializeMenuVideoSettings(mMenuSettingsVideo);
    if (mAudioSource != NULL)
        mAudioWidget->InitializeMenuAudioSettings(mMenuSettingsAudio);
    if (mSessionType != PREVIEW)
        mMessageWidget->InitializeMenuMessagesSettings(mMenuSettingsMessages);
    if (CONF.DebuggingEnabled())
    {
        if (mSessionType == PARTICIPANT)
            mSessionInfoWidget->InitializeMenuSessionInfoSettings(mMenuSettingsSessionInfo);
    }
}

void ParticipantWidget::HideAudioVideoWidget()
{
    switch(mSessionType)
    {
        case BROADCAST:
            mVideoAudioWidget->hide();

            // show Homer logo
            mLogoFrame->show();

            break;
        case PARTICIPANT:
            mVideoAudioWidget->hide();
            break;
        case PREVIEW:
            hide();
            break;
    }
    CreateFullscreenControls();
}

void ParticipantWidget::ShowAudioVideoWidget()
{
    DestroyFullscreenControls();
    switch(mSessionType)
    {
        case BROADCAST:
            mVideoAudioWidget->show();

            // hide Homer logo
            mLogoFrame->hide();

            break;
        case PARTICIPANT:
            mVideoAudioWidget->show();
            break;
        case PREVIEW:
            show();
            break;
    }
}

void ParticipantWidget::ResizeAVView(int pSize)
{
	QList<int>tSizes  = mSplitter->sizes();
	LOG(LOG_VERBOSE, "Splitter with %d widgets", tSizes.size());
	for (int i = 0; i < tSizes.size(); i++)
	{
		LOG(LOG_VERBOSE, "Entry: %d", tSizes[i]);
	}

	if (pSize == 0)
	{
		tSizes[1] += tSizes[0];
		tSizes[0] = 0;
	}else
	{
		if (pSize > tSizes[0] + tSizes[1])
			pSize = tSizes[0] + tSizes[1];
		int tAVOverSize = tSizes[0] - pSize;
		tSizes[1] += tAVOverSize;
		tSizes[0] = pSize;
	}
	mSplitter->setSizes(tSizes);
}

void ParticipantWidget::closeEvent(QCloseEvent* pEvent)
{
    QMessageBox *tMB = NULL;

    switch(mSessionType)
    {
        case PARTICIPANT:
                    if (CONF.GetParticipantWidgetsCloseImmediately())
                    {
                        if (pEvent != NULL)
                            pEvent->accept();
                        QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new DeleteSessionEvent(this)));
                    }else
                    {
                        tMB = new QMessageBox(QMessageBox::Question, "Really remove this participant?", "Do you really want to remove participant " + mSessionName + "?", QMessageBox::Yes | QMessageBox::Cancel, this);

                        if ((mQuitForced) || (tMB->exec() == QMessageBox::Yes))
                        {
                            if (pEvent != NULL)
                                pEvent->accept();
                            QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new DeleteSessionEvent(this)));
                        }else
                        {
                            if (pEvent != NULL)
                                pEvent->ignore();
                        }
                    }
                    break;
        case PREVIEW:
                    if (pEvent != NULL)
                        pEvent->accept();
                    QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new DeleteSessionEvent(this)));
                    break;
        case BROADCAST:
                    break;
        default:
                    break;
    }
}

void ParticipantWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
    QAction *tAction;
    QMenu tMenu(this);

    switch(mSessionType)
    {
        case PARTICIPANT:
            if (CONF.DebuggingEnabled())
            {
                QIcon tIcon2;
                tIcon2.addPixmap(QPixmap(":/images/22_22/Info.png"), QIcon::Normal, QIcon::Off);
                if (mSessionInfoWidget->isVisible())
                {
                    tAction = tMenu.addAction("Hide session info");
                    tAction->setCheckable(true);
                    tAction->setChecked(true);
                }else
                {
                    tAction = tMenu.addAction("Show session info");
                    tAction->setCheckable(true);
                    tAction->setChecked(false);
                }
                tAction->setIcon(tIcon2);
                if (mLbAVStatistics->isVisible())
                {
                    tAction = tMenu.addAction("Hide A/V statistics");
                    tAction->setCheckable(true);
                    tAction->setChecked(true);

                }else
                {
                    tAction = tMenu.addAction("Show A/V statistics");
                    tAction->setCheckable(true);
                    tAction->setChecked(false);
                }
                tAction->setIcon(tIcon2);

                QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
                if (tPopupRes != NULL)
                {
                    if (tPopupRes->text().compare("Show session info") == 0)
                    {
                        mSessionInfoWidget->SetVisible(true);
                        return;
                    }
                    if (tPopupRes->text().compare("Hide session info") == 0)
                    {
                        mSessionInfoWidget->SetVisible(false);
                        return;
                    }
                    if (tPopupRes->text().compare("Show A/V statistics") == 0)
                    {
                    	mLbAVStatistics->setVisible(true);
                        return;
                    }
                    if (tPopupRes->text().compare("Hide A/V statistics") == 0)
                    {
                    	mLbAVStatistics->setVisible(false);
                        return;
                    }
                }
            }
            break;
        case BROADCAST:
        case PREVIEW:
            if (CONF.DebuggingEnabled())
            {
                QIcon tIcon2;
                tIcon2.addPixmap(QPixmap(":/images/22_22/Info.png"), QIcon::Normal, QIcon::Off);
                if (mLbAVStatistics->isVisible())
                {
                    tAction = tMenu.addAction("Hide A/V statistics");
                    tAction->setCheckable(true);
                    tAction->setChecked(true);
                }else
                {
                    tAction = tMenu.addAction("Show A/V statistics");
                    tAction->setCheckable(true);
                    tAction->setChecked(false);
                }
                tAction->setIcon(tIcon2);

                QAction* tPopupRes = tMenu.exec(pEvent->globalPos());
                if (tPopupRes != NULL)
                {
                    if (tPopupRes->text().compare("Show A/V statistics") == 0)
                    {
                    	mLbAVStatistics->setVisible(true);
                        return;
                    }
                    if (tPopupRes->text().compare("Hide A/V statistics") == 0)
                    {
                    	mLbAVStatistics->setVisible(false);
                        return;
                    }
                }
            }
            break;
        default:
            break;
    }
}

void ParticipantWidget::dragEnterEvent(QDragEnterEvent *pEvent)
{
    if (pEvent->mimeData()->hasUrls())
    {
        QList<QUrl> tList = pEvent->mimeData()->urls();
        QUrl tUrl;
        int i = 0;

        foreach(tUrl, tList)
            LOG(LOG_VERBOSE, "New drag+drop url (%d) \"%s\"", ++i, QString(tUrl.toString().toLocal8Bit()).toStdString().c_str());

        switch(mSessionType)
        {
            case BROADCAST:
                        if (tList.size() > 0)
                        {
                            QString tFileName = tList.begin()->toString();
                            if ((OverviewPlaylistWidget::IsVideoFile(tFileName)) || (OverviewPlaylistWidget::IsAudioFile(tFileName)))
                            {
                                pEvent->acceptProposedAction();
                            }
                        }
                        break;
            case PARTICIPANT:
                        pEvent->acceptProposedAction();
                        break;
            case PREVIEW:
                        LOG(LOG_VERBOSE, "..ignored");
                        break;
            default:
                        LOG(LOG_ERROR, "Unknown session type");
                        break;
        }
    }
}

void ParticipantWidget::dropEvent(QDropEvent *pEvent)
{
    if (mMessageWidget != NULL)
    {
        if (pEvent->mimeData()->hasUrls())
        {
            LOG(LOG_VERBOSE, "Got some dropped urls");
            QList<QUrl> tList = pEvent->mimeData()->urls();
            switch(mSessionType)
            {
                case BROADCAST:
                            if (tList.size() > 0)
                            {
                            	bool tFirstEntry = true;
                            	QUrl tEntry;
                            	foreach(tEntry, tList)
                            	{
                                    QString tFileName = tEntry.toString();
                                    PLAYLISTWIDGET.AddEntry(tFileName, tFirstEntry);
									if (tFirstEntry)
										tFirstEntry = false;
                            	}
                                pEvent->acceptProposedAction();
                            }
                            break;
                case PARTICIPANT:
                            mMessageWidget->SendFile(&tList);
                            pEvent->acceptProposedAction();
                            break;
                case PREVIEW:
                            LOG(LOG_VERBOSE, "..ignored");
                            break;
                default:
                            LOG(LOG_ERROR, "Unknown session type");
                            break;
            }
        }
    }
}

void ParticipantWidget::keyPressEvent(QKeyEvent *pEvent)
{
	//LOG(LOG_VERBOSE, "Got participant window key press event with key %s(%d, mod: %d)", pEvent->text().toStdString().c_str(), pEvent->key(), (int)pEvent->modifiers());

    if ((pEvent->key() == Qt::Key_T) && (!pEvent->isAutoRepeat()))
    {
        // forward the event to the main widget
        QCoreApplication::postEvent(mMainWindow, new QKeyEvent(QEvent::KeyPress, pEvent->key(), pEvent->modifiers(), pEvent->text()));
    }else
    {
        // forward the event to the video widget
        QCoreApplication::postEvent(mVideoWidget, new QKeyEvent(QEvent::KeyPress, pEvent->key(), pEvent->modifiers(), pEvent->text()));
    }

	pEvent->accept();
}

void ParticipantWidget::wheelEvent(QWheelEvent *pEvent)
{
    int tOffset = pEvent->delta() * 25 / 120;
    //LOG(LOG_VERBOSE, "Got new wheel event with orientation %d and delta %d, derived volume offset: %d", (int)pEvent->orientation(), pEvent->delta(), tOffset);
	if (pEvent->orientation() == Qt::Vertical)
	{
		mAudioWidget->SetVolume(mAudioWidget->GetVolume() + tOffset);
	}
}

void ParticipantWidget::LookedUpParticipantHost(const QHostInfo &pHost)
{
    if (pHost.error() != QHostInfo::NoError)
    {
        ShowError(tr("DNS lookup error"), tr("Unable to lookup DNS entry for") + " \"" + pHost.hostName() + "\" " + tr("because") + " \"" + pHost.errorString() + "\"");
        return;
    }

    foreach (QHostAddress tAddress, pHost.addresses())
        LOG(LOG_VERBOSE, "Found DNS entry for %s with value %s", pHost.hostName().toStdString().c_str(), tAddress.toString().toStdString().c_str());

    QString tSipInterface = pHost.addresses().first().toString();
    if (mSipInterface != "")
        tSipInterface += ":" + mSipInterface;
    mSipInterface = tSipInterface;
    mSessionInfoWidget->SetSipInterface(mSipInterface);
    LOG(LOG_VERBOSE, "Set SIP interface to \"%s\"", mSipInterface.toStdString().c_str());
}

void ParticipantWidget::FindSipInterface(QString pSessionName)
{
	QString tServiceAP = pSessionName.section("@", 1);
	if (tServiceAP.contains(':'))
        mSipInterface = tServiceAP.section(":", -1, -1);
	else
	    mSipInterface = "";

	// first sign is a letter? -> we have a DNS name here
	if (((tServiceAP[0] >= 'a') && (tServiceAP[0] <= 'z')) || ((tServiceAP[0] >= 'A') && (tServiceAP[0] <= 'Z')))
	{
		QString tDnsName = tServiceAP.section(":", 0, -2);
		if (tDnsName == "")
		    tDnsName = tServiceAP;

		LOG(LOG_VERBOSE, "Try to lookup %s via DNS system", tDnsName.toStdString().c_str());
		QHostInfo::lookupHost(tDnsName, this, SLOT(LookedUpParticipantHost(QHostInfo)));
	}else
	    mSipInterface = tServiceAP;

	LOG(LOG_VERBOSE, "FindSipInterface resulted in %s", mSipInterface.toStdString().c_str());
}

bool ParticipantWidget::IsThisParticipant(QString pParticipant, enum TransportType pParticipantTransport)
{
    bool tResult = false;

    if (mSessionType != PARTICIPANT)
        return false;

    QString tUser = pParticipant.section('@', 0, 0);
    QString tHost = pParticipant.section('@', 1, 1);

    // first sign is a letter? -> we have a DNS name here
    if (((tHost[0] >= 'a') && (tHost[0] <= 'z')) || ((tHost[0] >= 'A') && (tHost[0] <= 'Z')))
    {
        QString tPort = tHost.section(':', 1, 1);
        tHost = tHost.section(':', 0, 0);
        QHostInfo tHostInfo = QHostInfo::fromName(tHost);
        QString tIp = tHostInfo.addresses().first().toString();
        pParticipant = tUser + '@' + tIp;
        if (tPort != "")
            pParticipant += ':' + tPort;
    }

    bool tSearchedParticipantIsServercontact = false;
    if ((mSessionName.section("@", 1).contains(CONF.GetSipServer())) || (mSipInterface.contains(CONF.GetSipServer())))
    {// this participant belongs to SIP server (pbx box) -> we also have to check the user name
        tResult = ((pParticipant.contains(mSessionName.section("@", 1)) || pParticipant.contains(mSipInterface))) && (mSessionName.section("@", 0, 0) == pParticipant.section("@", 0, 0)) && (mSessionTransport == pParticipantTransport);
        tSearchedParticipantIsServercontact = true;
    }else
    {// this participant is located on some foreign host and uses peer-to-peer communication
        tResult = (pParticipant.contains(mSessionName.section("@", 1)) || pParticipant.contains(mSipInterface)) && (mSessionTransport == pParticipantTransport);
    }
    LOG(LOG_VERBOSE, "@\"%s\"[%s] - IsThisParticipant \"%s\"[%s](server contact: %d) ? ==> %s", mSessionName.toStdString().c_str(), Socket::TransportType2String(mSessionTransport).c_str(), pParticipant.toStdString().c_str(), Socket::TransportType2String(pParticipantTransport).c_str(), tSearchedParticipantIsServercontact, tResult ? "MATCH" : "no match");

    return tResult;
}

void ParticipantWidget::HandleMessage(bool pIncoming, QString pSender, QString pMessage)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (pIncoming)
    {
        LOG(LOG_VERBOSE, "Message \"%s\" received from \"%s\"", pMessage.toStdString().c_str(), pSender.toStdString().c_str());
    }else
    {
        LOG(LOG_ERROR, "Loopback message \"%s\" received from \"%s\"", pMessage.toStdString().c_str(), pSender.toStdString().c_str());
        return;
    }

    if (mMessageWidget != NULL)
    {
        mMessageWidget->AddMessage(pSender, pMessage);
        if (pIncoming)
        {
            if (CONF.GetImSound())
            {
                StartAudioPlayback(CONF.GetImSoundFile());
            }
        }
    }
}

void ParticipantWidget::HandleGeneralError(bool pIncoming, int pCode, QString pDescription)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    UpdateParticipantState(CONTACT_UNDEFINED_STATE);
    CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_UNDEFINED_STATE);

    ShowError(tr("General error occurred"), tr("General error of code") + " " + QString("%1").arg(pCode) + " " + tr("occurred. The error is described with") + " \"" + pDescription + "\"");
}

void ParticipantWidget::HandleMessageAccept(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    UpdateParticipantState(CONTACT_AVAILABLE);
    CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_AVAILABLE);
}

void ParticipantWidget::HandleMessageAcceptDelayed(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "server delays message(s)", true);

    UpdateParticipantState(CONTACT_UNDEFINED_STATE);
    CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_UNDEFINED_STATE);
}

void ParticipantWidget::HandleMessageUnavailable(bool pIncoming, int pStatusCode, QString pDescription)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support vor preview widgets");
        return;
    }

    UpdateParticipantState(CONTACT_UNAVAILABLE);
    CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_UNAVAILABLE);
    ShowError(tr("Participant unavailable"), tr("The participant") + " " + mSessionName + " " + tr("is currently unavailable for an instant message! The reason is") + " \"" + pDescription + "\"(" + QString("%1").arg(pStatusCode) + ").");
}

void ParticipantWidget::HandleCallRinging(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    ShowNewState();

    if (pIncoming)
    {
        if (CONF.GetCallAcknowledgeSound())
        {
            StartAudioPlayback(CONF.GetCallAcknowledgeSoundFile());
        }
    }
}

void ParticipantWidget::HandleCall(bool pIncoming, QString pRemoteApplication)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "call request", true);

    if (!mIncomingCall)
    {
        mIncomingCall = true;

        MEETING.SendCallAcknowledge(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport);

        mCallBox = new QMessageBox(QMessageBox::Question, "Incoming call from application " + pRemoteApplication, "Do you want to accept the incoming call from " + mSessionName + "?", QMessageBox::Yes | QMessageBox::Cancel, this);

        // start sound output
        if (pIncoming)
        {
            if (CONF.GetCallSound())
            {
                StartAudioPlayback(CONF.GetCallSoundFile(), SOUND_LOOPS_FOR_CALL);
            }
        }

        if (mCallBox->exec() == QMessageBox::Yes)
        {
            if (mIncomingCall)
            {
                MEETING.SendCallAccept(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport);
            }
        }else
        {
            MEETING.SendCallDeny(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport);
        }
    }
}

void ParticipantWidget::HandleCallCancel(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    CallStopped(pIncoming);

    if (pIncoming)
    {
        if (CONF.GetCallHangupSound())
        {
            StartAudioPlayback(CONF.GetCallHangupSoundFile());
        }
    }
}

void ParticipantWidget::HandleCallHangup(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    CallStopped(pIncoming);

    if (pIncoming)
    {
        if (CONF.GetCallHangupSound())
        {
            StartAudioPlayback(CONF.GetCallHangupSoundFile());
        }
    }
}

void ParticipantWidget::HandleCallTermination(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    CallStopped(pIncoming);

    if (pIncoming)
    {
        if (CONF.GetErrorSound())
        {
            StartAudioPlayback(CONF.GetErrorSoundFile());
        }
    }
}

void ParticipantWidget::CallStopped(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "call stopped", true);

    ResetMediaSinks();

    if ((mIncomingCall) && (mCallBox))
    {
        mCallBox->done(QMessageBox::Cancel);
        mCallBox = NULL;
    }
    ShowNewState();
    if (mVideoWidget != NULL)
    {
        mVideoWidgetFrame->hide();
        mVideoWidget->SetVisible(false);
    }
    if (mAudioWidget != NULL)
    {
        mAudioWidget->SetVisible(false);
        mAudioWidget->ToggleMuteState(false);
    }
	if (!mSessionInfoWidget->isVisible())
		ResizeAVView(0);
    mIncomingCall = false;

    if (mWaveOut != NULL)
    {
        // stop sound output
        LOG(LOG_VERBOSE, "Playing acoustic notification from file: %s", mWaveOut->CurrentFile().c_str());
        if ((mWaveOut->CurrentFile() == CONF.GetCallSoundFile().toStdString()) && (mWaveOut->IsPlaying()))
        {
            LOG(LOG_VERBOSE, "Stopping playback of sound file");
            mWaveOut->Stop();
        }
    }
}

void ParticipantWidget::HandleCallUnavailable(bool pIncoming, int pStatusCode, QString pDescription)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (!mIncomingCall)
    {
    	CallStopped(pIncoming);
        UpdateParticipantState(CONTACT_UNAVAILABLE);
        CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_UNAVAILABLE);

        if (pStatusCode == 488)
        	ShowError(tr("Participant unavailable"), tr("The participant") + " " + mSessionName + " " + tr("does not accept your video/audio codecs. Please, check the configuration and use different settings."));
		else
        	ShowError(tr("Participant unavailable"), tr("The participant") + " " + mSessionName + " " + tr("is currently unavailable for a call! The reason is") + " \"" + pDescription + "\"(" + QString("%1").arg(pStatusCode) + ").");
    }else
    	CallStopped(pIncoming);

    if (pIncoming)
    {
        if (CONF.GetErrorSound())
        {
            StartAudioPlayback(CONF.GetErrorSoundFile());
        }
    }
}

void ParticipantWidget::HandleCallDenied(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "call denied", true);

	CallStopped(pIncoming);

    if (pIncoming)
    {
        if (CONF.GetCallDenySound())
        {
            StartAudioPlayback(CONF.GetCallDenySoundFile());
        }
    }
}

void ParticipantWidget::HandleCallAccept(bool pIncoming)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    UpdateParticipantState(CONTACT_AVAILABLE);
    CONTACTS.UpdateContactState(mSessionName, mSessionTransport, CONTACT_AVAILABLE);

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "session established", true);

    ShowNewState();
    if (mVideoWidget != NULL)
    {
        mVideoWidgetFrame->show();
        mVideoWidget->SetVisible(true);
    }
    if (mAudioWidget != NULL)
    {
        mAudioWidget->SetVisible(true);
        mAudioWidget->ToggleMuteState(true);
    }
    ResizeAVView(288);
    mIncomingCall = false;

    if (mWaveOut != NULL)
    {
        // stop sound output
        if ((mWaveOut->CurrentFile() == CONF.GetCallSoundFile().toStdString()) && (mWaveOut->IsPlaying()))
        {
            LOG(LOG_VERBOSE, "Stopping playback of sound file");
            mWaveOut->Stop();
        }

        if (pIncoming)
        {
            if (CONF.GetCallAcknowledgeSound())
            {
                StartAudioPlayback(CONF.GetCallAcknowledgeSoundFile());
            }
        }
    }
}

void ParticipantWidget::HandleMediaUpdate(bool pIncoming, QString pRemoteAudioAdr, unsigned int pRemoteAudioPort, QString pRemoteAudioCodec, QString pRemoteVideoAdr, unsigned int pRemoteVideoPort, QString pRemoteVideoCodec)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support for preview widgets");
        return;
    }

    mRemoteAudioAdr = pRemoteAudioAdr;
    mRemoteAudioPort = pRemoteAudioPort;
    mRemoteAudioCodec = pRemoteAudioCodec;
    mRemoteVideoAdr = pRemoteVideoAdr;
    mRemoteVideoPort = pRemoteVideoPort;
    mRemoteVideoCodec = pRemoteVideoCodec;

    if ((pRemoteVideoPort != 0) || (pRemoteAudioPort != 0))
    {
        ResetMediaSinks();

        LOG(LOG_VERBOSE, "Video sink set to %s:%u", pRemoteVideoAdr.toStdString().c_str(), pRemoteVideoPort);
        LOG(LOG_VERBOSE, "Video sink uses codec: \"%s\"", pRemoteVideoCodec.toStdString().c_str());
        LOG(LOG_VERBOSE, "Audio sink set to %s:%u", pRemoteAudioAdr.toStdString().c_str(), pRemoteAudioPort);
        LOG(LOG_VERBOSE, "Audio sink uses codec: \"%s\"", pRemoteAudioCodec.toStdString().c_str());

        if (pRemoteVideoPort != 0)
            mParticipantVideoSink = mVideoSourceMuxer->RegisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort, mVideoSendSocket, true); // always use RTP/AVP profile (RTP/UDP)
        if (pRemoteAudioPort != 0)
            mParticipantAudioSink = mAudioSourceMuxer->RegisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort, mAudioSendSocket, true); // always use RTP/AVP profile (RTP/UDP)

        if (mParticipantVideoSink != NULL)
            mParticipantVideoSink->AssignStreamName("CONF-OUT: " + mSessionName.toStdString());
        if (mParticipantAudioSink != NULL)
            mParticipantAudioSink->AssignStreamName("CONF-OUT: " + mSessionName.toStdString());

        if (mVideoWidget != NULL)
            mVideoWidget->GetWorker()->SetStreamName("CONF-IN: " + mSessionName);
        if (mAudioWidget != NULL)
            mAudioWidget->GetWorker()->SetStreamName("CONF-IN: " + mSessionName);
    }

    if (pRemoteVideoPort == 0)
    {
        if (pRemoteVideoCodec != "")
            ShowWarning("Video streaming problem", "Remote side has selected video codec " + pRemoteVideoCodec + ". Adapt the local video streaming preferences to correct this!");
        else
            ShowWarning("Video streaming problem", "Remote wasn't able to found fitting video codec for your settings. Select another codec in the video streaming preferences!");
    }
    if (pRemoteAudioPort == 0)
    {
        if (pRemoteAudioCodec != "")
            ShowWarning("Audio streaming problem", "Remote side has selected audio codec " + pRemoteAudioCodec + ". Adapt the local audio streaming preferences to correct this!");
        else
            ShowWarning("Audio streaming problem", "Remote wasn't able to found fitting video codec for your settings. Select another codec in the audio streaming preferences!");
    }
}

void ParticipantWidget::SetVideoStreamPreferences(QString pCodec, bool pJustReset)
{
    if (mVideoWidget == NULL)
        return;

    if (pJustReset)
    {
        LOG(LOG_VERBOSE, "Going to reset source");
        mVideoWidget->GetWorker()->ResetSource();
        return;
    }

    mVideoWidget->GetWorker()->SetInputStreamPreferences(pCodec);
}

void ParticipantWidget::SetAudioStreamPreferences(QString pCodec, bool pJustReset)
{
    if ((mAudioWidget == NULL) || (mSessionType == PREVIEW))
        return;

    if (pJustReset)
    {
        LOG(LOG_VERBOSE, "Going to reset source");
        mAudioWidget->GetWorker()->ResetSource();
        return;
    }

    mAudioWidget->GetWorker()->SetInputStreamPreferences(pCodec);
}

void ParticipantWidget::ResetAVSync()
{
    LOG(LOG_VERBOSE, "Reseting A/V sync.");

    // avoid A/V sync. in the near future
    mTimeOfLastAVSynch = Time::GetTimeStamp();
    mAVAsyncCounterSinceLastSynchronization = 0;
}

void ParticipantWidget::AVSync()
{
    #ifdef PARTICIPANT_WIDGET_AV_SYNC

        // only try to synch. if it is desired
        if (!mAVSynchActive)
            return;

        int64_t tCurTime = Time::GetTimeStamp();
        if ((tCurTime - mTimeOfLastAVSynch  >= AV_SYNC_MIN_PERIOD * 1000))
        {
            if (mAVSynchActive)
            {
                if ((mSessionType == BROADCAST) && (mVideoWidget->GetWorker()->CurrentFile() != mCurrentMovieFile))
                {// file has changed and we have to reset A/V sync. to wait for some time until we synch. for the first time
                    mCurrentMovieFile = mVideoWidget->GetWorker()->CurrentFile();
                    LOG(LOG_VERBOSE, "Setting current movie file to %s and reseting A/V sync.", mCurrentMovieFile.toStdString().c_str());
                    ResetAVSync();
                    return;
                }

                // synch. video and audio by seeking in video stream based to the position of the audio stream, the other way around it would be more obvious to the user because he would hear audio gaps
                int64_t tAudioSyncTime;
                int64_t tVideoSyncTime;
                float tTimeDiff = GetAVDrift(&tVideoSyncTime, &tAudioSyncTime);

                // is video and audio stream still active and their synch. times are still changing?
                if ((mLastAudioSynchronizationTimestamp != tAudioSyncTime) && (mLastVideoSynchronizationTimestamp != tVideoSyncTime))
                {// video and audio stream are still active
					// are audio and video playback out of synch.?
					if (((tTimeDiff < -AV_SYNC_MAX_DRIFT) || (tTimeDiff > AV_SYNC_MAX_DRIFT)))
					{
						if ((mSessionType != BROADCAST) || ((!mVideoWidget->GetWorker()->EofReached()) && (!mAudioWidget->GetWorker()->IsPaused()) && (mVideoWidget->GetWorker()->GetCurrentDevice() == mAudioWidget->GetWorker()->GetCurrentDevice())))
						{
							if (mAVAsyncCounterSinceLastSynchronization >= AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD)
							{
								if (mAVASyncCounter < AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD * (AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD_TRY_RESET + 1))
								{// try to adapt waiting times
									if (tTimeDiff > 0)
										LOG(LOG_WARN, "Detected asynchronous A/V playback, drift is %3.2f seconds (video before audio), max. allowed drift is %3.2f seconds, last synch. was at %lld, synchronizing now..", tTimeDiff, AV_SYNC_MAX_DRIFT, mTimeOfLastAVSynch);
									else
										LOG(LOG_WARN, "Detected asynchronous A/V playback, drift is %3.2f seconds (audio before video), max. allowed drift is %3.2f seconds, last synch. was at %lld, synchronizing now..", tTimeDiff, AV_SYNC_MAX_DRIFT, mTimeOfLastAVSynch);

									mAudioWidget->GetWorker()->SyncClock(mVideoSource);
									ResetAVSync();
								}else
								{// we tried to adapt waiting times for AV_SYNC_CONTINUOUS_ASYNC_THRESHOLD_TRY_RESET times, now we try to reset the A/V media sources
									LOG(LOG_WARN, "Detected asynchronous A/V playback, drift is %3.2f seconds, max. allowed drift is %3.2f seconds, tried synchronization %d times, reset audio and video source now..", tTimeDiff, AV_SYNC_MAX_DRIFT, AV_SYNC_CONSECUTIVE_ASYNC_THRESHOLD_TRY_RESET);
									mVideoWidget->GetWorker()->ResetSource();
									mAudioWidget->GetWorker()->ResetSource();
									ResetAVSync();
									mAVASyncCounter = 0;
								}
							}else
							{
								mAVAsyncCounterSinceLastSynchronization++;
								mAVASyncCounter++;
	                            #ifdef PARTICIPANT_WIDGET_DEBUG_AV_SYNC
									if (tTimeDiff > 0)
										LOG(LOG_WARN, "Detected asynchronous A/V playback %d times, drift is %3.2f seconds (video before audio), max. allowed drift is %3.2f seconds, last synch. was at %lld, synchronizing now..", mAVAsyncCounterSinceLastSynchronization, tTimeDiff, AV_SYNC_MAX_DRIFT, mTimeOfLastAVSynch);
									else
										LOG(LOG_WARN, "Detected asynchronous A/V playback %d times, drift is %3.2f seconds (audio before video), max. allowed drift is %3.2f seconds, last synch. was at %lld, synchronizing now..", mAVAsyncCounterSinceLastSynchronization, tTimeDiff, AV_SYNC_MAX_DRIFT, mTimeOfLastAVSynch);
	                            #endif
							}
						}
					}else
					{
						mAVASyncCounter = 0;
						mAVAsyncCounterSinceLastSynchronization = 0;
					}
				    mLastAudioSynchronizationTimestamp = tAudioSyncTime;
				    mLastVideoSynchronizationTimestamp = tVideoSyncTime;
                }else
                {// either the video or the audio stream is broken
                	// do not try to synchronize!
                }
            }
        }

    #endif
}

float ParticipantWidget::GetAVDrift(int64_t *pVideoSyncTime, int64_t *pAudioSyncTime)
{
    if (!mAVSynchActive)
        return 0;

    if (mVideoWidget->GetWorker() == NULL)
        return 0;

    if (mAudioWidget->GetWorker() == NULL)
        return 0;

    int64_t tVideoSyncTime = 0;
    if (mVideoSource != NULL)
        tVideoSyncTime = mVideoSource->GetSynchronizationTimestamp();
    if (pVideoSyncTime != NULL)
    	*pVideoSyncTime = tVideoSyncTime;

    int64_t tAudioSyncTime = 0;
    if (mAudioSource != NULL)
        tAudioSyncTime = mAudioSource->GetSynchronizationTimestamp();
    if (pAudioSyncTime != NULL)
    	*pAudioSyncTime = tAudioSyncTime;

    float tResult = 0;

	// do we have valid synchronization timestamps from video and audio source?
	if ((tAudioSyncTime != 0) && (tVideoSyncTime != 0))
	{// we are able to synch. audio and video
		int64_t tAVPlaybackDrift = tVideoSyncTime - tAudioSyncTime;
		//LOG(LOG_VERBOSE, "Detected A/V drift of %ld ms", tAVPlaybackDrift / 1000);

		tResult = ((float)tAVPlaybackDrift) / 1000000 - mAudioWidget->GetWorker()->GetUserAVDrift() - mAudioWidget->GetWorker()->GetVideoDelayAVDrift();
	}

    return tResult;
}

float ParticipantWidget::GetUserAVDrift()
{
    float tResult = 0;

    if (mVideoWidget->GetWorker() == NULL)
        return 0;

    if (mAudioWidget->GetWorker() == NULL)
        return 0;

    // do we play video and audio from the same file?
    if (PlayingMovieFile())
    {
        tResult = mAudioWidget->GetWorker()->GetUserAVDrift();
    }

    return tResult;
}

float ParticipantWidget::GetVideoDelayAVDrift()
{
    float tResult = 0;

    if (mVideoWidget->GetWorker() == NULL)
        return 0;

    if (mAudioWidget->GetWorker() == NULL)
        return 0;

    // do we play video and audio from the same file?
    if (PlayingMovieFile())
    {
        tResult = mAudioWidget->GetWorker()->GetVideoDelayAVDrift();
    }

    return tResult;
}

void ParticipantWidget::SetUserAVDrift(float pDrift)
{
    if (mVideoWidget->GetWorker() == NULL)
        return;

    if (mAudioWidget->GetWorker() == NULL)
        return;

    // do we play video and audio from the same file?
    if (PlayingMovieFile())
    {
        mAudioWidget->GetWorker()->SetUserAVDrift(pDrift);
    }
}

void ParticipantWidget::ReportVideoDelay(float pDelay)
{
    if (pDelay != mVideoDelayAVDrift)
    {
        #ifdef PARTICIPANT_WIDGET_DEBUG_AV_SYNC
            LOG(LOG_VERBOSE, "Reporting for %s a video delay of: %.2f", mSessionName.toStdString().c_str(), pDelay);
        #endif
        mVideoDelayAVDrift = pDelay;

        if (mVideoWidget->GetWorker() == NULL)
            return;

        if (mAudioWidget->GetWorker() == NULL)
            return;

        // do we play video and audio from the same file?
        if (PlayingMovieFile())
        {
            mAudioWidget->GetWorker()->SetVideoDelayAVDrift(pDelay);
        }
    }
}

void ParticipantWidget::InformAboutVideoSeekingComplete()
{
    // sync. audio to video clock
    AVSync();
}

void ParticipantWidget::ShowNewState()
{
    switch (MEETING.GetCallState(QString(mSessionName.toLocal8Bit()).toStdString(), mSessionTransport))
    {
        case CALLSTATE_STANDBY:
                    if ((mMessageWidget != NULL) && (mSessionType != PREVIEW))
                    {
                        mMessageWidget->UpdateParticipantName(mWidgetTitle.section('@', 0, 0) + " (in chat)");
                        mMessageWidget->ShowNewState();
                    }
                    setWindowTitle(mWidgetTitle + " (chat)");
                    break;
        case CALLSTATE_RUNNING:
                    if ((mMessageWidget != NULL) && (mSessionType != PREVIEW))
                    {
                        mMessageWidget->UpdateParticipantName(mWidgetTitle.section('@', 0, 0) + " (in conference)");
                        mMessageWidget->ShowNewState();
                    }
                    setWindowTitle(mWidgetTitle + " (conference)");
                    break;
        case CALLSTATE_RINGING:
                    if ((mMessageWidget != NULL) && (mSessionType != PREVIEW))
                    {
                        mMessageWidget->UpdateParticipantName(mWidgetTitle.section('@', 0, 0) + " (ringing)");
                        mMessageWidget->ShowNewState();
                    }
                    setWindowTitle(mWidgetTitle + " (ringing)");
                    break;
        case CALLSTATE_INVALID:
                    if ((mMessageWidget != NULL) && (mSessionType != PREVIEW))
                    {
                        mMessageWidget->UpdateParticipantName(mWidgetTitle.section('@', 0, 0));
                        mMessageWidget->ShowNewState();
                    }
                    setWindowTitle(mWidgetTitle);
                    break;
    }
}

bool ParticipantWidget::PlayingMovieFile()
{
    if ((mVideoWidget != NULL) && (mAudioWidget != NULL))
    {
        VideoWorkerThread *tVWorker = mVideoWidget->GetWorker();
        AudioWorkerThread *tAWorker = mAudioWidget->GetWorker();

        bool tPlayingSameFile = (tVWorker->CurrentFile() == tAWorker->CurrentFile());
        return ((tVWorker->PlayingFile()) && (tAWorker->PlayingFile()) && tPlayingSameFile);
    }else
        return false;
}

void ParticipantWidget::UpdateParticipantName(QString pParticipantName)
{
	mWidgetTitle = pParticipantName;
    ShowNewState();
}

void ParticipantWidget::UpdateParticipantState(int pState)
{
    if (mMessageWidget != NULL)
        mMessageWidget->UpdateParticipantState(pState);

    switch(pState)
    {
        case CONTACT_UNAVAILABLE:
            setWindowIcon(QPixmap(":/images/32_32/UserUnavailable.png"));
            break;
        case CONTACT_AVAILABLE:
            setWindowIcon(QPixmap(":/images/32_32/UserAvailable.png"));
            break;
        default:
        case CONTACT_UNDEFINED_STATE:
            setWindowIcon(QPixmap(":/images/22_22/Error.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::FastTransformation));
            break;
    }
}

QString ParticipantWidget::GetParticipantName()
{
    if (mSessionType == PARTICIPANT)
        return mSessionName;
    else
        return "";
}

enum TransportType ParticipantWidget::GetParticipantTransport()
{
    if (mSessionType == PARTICIPANT)
        return mSessionTransport;
    else
        return SOCKET_TRANSPORT_TYPE_INVALID;
}

enum SessionType ParticipantWidget::GetSessionType()
{
    return mSessionType;
}

QString ParticipantWidget::GetSipInterface()
{
	return mSipInterface;
}

void ParticipantWidget::ActionPlayPauseMovieFile(QString pFileName)
{
    LOG(LOG_VERBOSE, "User triggered play/pause");
    if (mVideoWidget->GetWorker()->IsPaused() || mAudioWidget->GetWorker()->IsPaused())
    {
        LOG(LOG_VERBOSE, "User triggered play");
        mVideoWidget->GetWorker()->PlayFile(pFileName);
        mAudioWidget->GetWorker()->PlayFile(pFileName);
        mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Pause.png"));
        if (mFullscreeMovieControlWidget != NULL)
        	mFullscreeMovieControlWidget->mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Pause.png"));
    }else
    {
        LOG(LOG_VERBOSE, "User triggered pause");
        mVideoWidget->GetWorker()->PauseFile();
        mAudioWidget->GetWorker()->PauseFile();
        mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Play.png"));
        if (mFullscreeMovieControlWidget != NULL)
        	mFullscreeMovieControlWidget->mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Play.png"));
    }
}

void ParticipantWidget::ActionRecordMovieFile()
{
    QString tFileName = OverviewPlaylistWidget::LetUserSelectVideoSaveFile(this, "Set file name for video/audio recording");

    if (tFileName.isEmpty())
        return;

    // get the quality value from the user
    bool tAck = false;
    QStringList tPosQuals;
    for (int i = 1; i < 11; i++)
        tPosQuals.append(QString("%1").arg(i * 10));

    // ################
    // video quality
    // ################
    QString tVideoQualityStr = QInputDialog::getItem(this, "Select video recording quality", "Record video with quality:                                 ", tPosQuals, 0, false, &tAck);

    if(tVideoQualityStr.isEmpty())
        return;

    if (!tAck)
        return;

    // convert QString to int
    bool tConvOkay = false;
    int tVideoQuality = tVideoQualityStr.toInt(&tConvOkay, 10);
    if (!tConvOkay)
    {
        LOG(LOG_ERROR, "Error while converting QString to int for video quality");
        return;
    }

    // ################
    // audio quality
    // ################
    QString tAudioQualityStr = QInputDialog::getItem(this, "Select audio recording quality", "Record audio with quality:                                 ", tPosQuals, 0, false, &tAck);

    if(tAudioQualityStr.isEmpty())
        return;

    if (!tAck)
        return;

    // convert QString to int
    tConvOkay = false;
    int tAudioQuality = tAudioQualityStr.toInt(&tConvOkay, 10);
    if (!tConvOkay)
    {
        LOG(LOG_ERROR, "Error while converting QString to int for audio quality");
        return;
    }

    // finally start the recording
//TODO    mVideoWorker->StartRecorder(tFileName.toStdString(), tQuality);
    LOG(LOG_ERROR, "Implement ActionRecordMovieFile()");
}

void ParticipantWidget::SeekMovieFileRelative(float pSeconds)
{
    float tCurPos = 0;
    if ((mVideoWidget != NULL) && (mVideoWidget->GetWorker()->SupportsSeeking()))
        tCurPos = mVideoWidget->GetWorker()->GetSeekPos();
    if ((mAudioWidget != NULL) && (mAudioWidget->GetWorker()->SupportsSeeking()))
        tCurPos = mAudioWidget->GetWorker()->GetSeekPos();
    float tTargetPos = tCurPos + pSeconds;
    LOG(LOG_VERBOSE, "Seeking relative %.2f seconds to position %.2f", pSeconds, tTargetPos);

    mVideoWidget->GetWorker()->Seek(tTargetPos);
    #ifdef PARTICIPANT_WIDGET_AV_SYNC
        if (PlayingMovieFile())
        {// synch. audio to video
            // force an AV sync
            mTimeOfLastAVSynch = 0;
        }else
        {// explicit seeking in audio file
            mAudioWidget->GetWorker()->Seek(tTargetPos);
        }
    #else
        mAudioWidget->GetWorker()->Seek(tTargetPos);
    #endif
}

void ParticipantWidget::ActionSeekMovieFile(int pPos)
{
	LOG(LOG_VERBOSE, "User moved playback slider to position %d", pPos);
    AVSeek(pPos);
}

void ParticipantWidget::ActionSeekMovieFileToPos(int pPos)
{
    //LOG(LOG_VERBOSE, "Value of playback slider changed to %d", pPos);
	if (mMovieSliderPosition != pPos)
	{
		LOG(LOG_VERBOSE, "User clicked playback slider at position %d", pPos);
		AVSeek(pPos);
	}
}

void ParticipantWidget::AVSeek(int pPos)
{
    double tPos = 0;

    //mVideoWidget->GetWorker()->PlayingFile()) && (!mVideoWidget->GetWorker()->IsPaused()
    if ((mVideoWidget != NULL) && (mVideoWidget->GetWorker()->PlayingFile()))
        tPos = (double)mVideoWidget->GetWorker()->GetSeekEnd() * pPos / 1000;
    if ((mAudioWidget != NULL) && (mAudioWidget->GetWorker()->PlayingFile()))
        tPos = (double)mAudioWidget->GetWorker()->GetSeekEnd() * pPos / 1000;

    mVideoWidget->GetWorker()->Seek(tPos);
    #ifdef PARTICIPANT_WIDGET_AV_SYNC
        if (PlayingMovieFile())
        {// synch. audio to video
            // force an AV sync
            mTimeOfLastAVSynch = 0;
        }else
        {// explicit seeking in audio file
            mAudioWidget->GetWorker()->Seek(tPos);
        }
    #else
        mAudioWidget->GetWorker()->Seek(tPos);
    #endif
}

void ParticipantWidget::CreateFullscreenControls()
{
    if ((mVideoSource) && (mVideoWidget->GetWorker()->SupportsSeeking()))
    {
        if (mFullscreeMovieControlWidget == NULL)
        {
            mFullscreeMovieControlWidget = new MovieControlWidget(mVideoWidget);
            mFullscreeMovieControlWidget->resize(mVideoWidget->width(), 58);
            mFullscreeMovieControlWidget->move(mVideoWidget->pos().x(), mVideoWidget->height() - 58);
            if (mVideoWidget->GetWorker()->IsPaused() || mAudioWidget->GetWorker()->IsPaused())
				mFullscreeMovieControlWidget->mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Play.png"));
            else
				mFullscreeMovieControlWidget->mTbPlayPause->setIcon(QPixmap(":/images/22_22/Audio_Pause.png"));
            connect(mFullscreeMovieControlWidget->mTbPlayPause, SIGNAL(clicked()), this, SLOT(ActionPlayPauseMovieFile()));
            connect(mFullscreeMovieControlWidget->mSlMovie, SIGNAL(sliderMoved(int)), this, SLOT(ActionSeekMovieFile(int)));
            connect(mFullscreeMovieControlWidget->mSlMovie, SIGNAL(valueChanged(int)), this, SLOT(ActionSeekMovieFileToPos(int)));
            connect(mFullscreeMovieControlWidget->mTbNext, SIGNAL(clicked()), this, SLOT(ActionPlaylistNext()));
            connect(mFullscreeMovieControlWidget->mTbPrevious, SIGNAL(clicked()), this, SLOT(ActionPlaylistPrevious()));

        }else
            LOG(LOG_WARN, "Error in state machine");
    }
}

void ParticipantWidget::DestroyFullscreenControls()
{
    if ((mVideoSource) && (mVideoWidget->GetWorker()->SupportsSeeking()))
    {
        if (mFullscreeMovieControlWidget != NULL)
        {
            delete mFullscreeMovieControlWidget;
            mFullscreeMovieControlWidget = NULL;
        }else
            LOG(LOG_WARN, "Error in state machine");
    }
}

void ParticipantWidget::ShowFullscreenMovieControls()
{
    if (mFullscreeMovieControlWidget != NULL)
        mFullscreeMovieControlWidget->show();
}

void ParticipantWidget::HideFullscreenMovieControls()
{
    if (mFullscreeMovieControlWidget != NULL)
        mFullscreeMovieControlWidget->hide();
}

void ParticipantWidget::ActionToggleUserAVDriftWidget()
{
    if (mAVDriftFrame != NULL)
    {
        if (mAVDriftFrame->isVisible())
            mAVDriftFrame->hide();
        else
            mAVDriftFrame->show();
    }
}

void ParticipantWidget::ActionUserAVDriftChanged(double pDrift)
{
    LOG(LOG_VERBOSE, "User changed A/V drift to %.2f", (float)pDrift);
    SetUserAVDrift(pDrift);
}

VideoWorkerThread* ParticipantWidget::GetVideoWorker()
{
    if (mVideoWidget != NULL)
        return mVideoWidget->GetWorker();
    else
        return NULL;
}

AudioWorkerThread* ParticipantWidget::GetAudioWorker()
{
    if (mAudioWidget != NULL)
        return mAudioWidget->GetWorker();
    else
        return NULL;
}

void ParticipantWidget::ShowStreamPosition(int64_t pCurPos, int64_t pEndPos)
{
    int tHour, tMin, tSec, tEndHour, tEndMin, tEndSec;
    int tSliderPos;

    if(pEndPos)
        tSliderPos = 1000 * pCurPos / pEndPos;
    else
        tSliderPos = 0;

    tHour = pCurPos / 3600;
    pCurPos %= 3600;
    tMin = pCurPos / 60;
    pCurPos %= 60;
    tSec = pCurPos;

    tEndHour = pEndPos / 3600;
    pEndPos %= 3600;
    tEndMin = pEndPos / 60;
    pEndPos %= 60;
    tEndSec = pEndPos;

    QString tPosText =  QString("%1:%2:%3").arg(tHour, 2, 10, (QLatin1Char)'0').arg(tMin, 2, 10, (QLatin1Char)'0').arg(tSec, 2, 10, (QLatin1Char)'0') + "\n" + \
                        QString("%1:%2:%3").arg(tEndHour, 2, 10, (QLatin1Char)'0').arg(tEndMin, 2, 10, (QLatin1Char)'0').arg(tEndSec, 2, 10, (QLatin1Char)'0');

    mLbCurPos->setText(tPosText);

    //LOG(LOG_VERBOSE, "Updating slider position, slider is down: %d", mSlMovie->isSliderDown());
    // update movie slider only if user doesn't currently adjust the playback position
    if (!mSlMovie->isSliderDown())
    {
        mMovieSliderPosition = tSliderPos;
        mSlMovie->setValue(tSliderPos);
    }

    if (mFullscreeMovieControlWidget != NULL)
    {
        mFullscreeMovieControlWidget->mLbCurPos->setText(tPosText);

        //LOG(LOG_VERBOSE, "Updating slider position, slider is down: %d", mSlMovie->isSliderDown());
        // update movie slider only if user doesn't currently adjust the playback position
        if (!mFullscreeMovieControlWidget->mSlMovie->isSliderDown())
        {
            mFullscreeMovieControlWidget->mSlMovie->setValue(tSliderPos);
        }
    }
}

void ParticipantWidget::UpdateMovieControls()
{
    // do we play a file?
    int tShowMovieControls = 0;

    if ((mVideoSource) && (mVideoWidget->GetWorker()->SupportsSeeking()))
        tShowMovieControls++;
    if ((mAudioSource) && (mAudioWidget->GetWorker()->SupportsSeeking()))
        tShowMovieControls++;

    //LOG(LOG_VERBOSE, "Updating movie controls");

    // do we play a file?
    if (tShowMovieControls > 0)
    {
    	//#################
        // update movie slider and position display
        //#################
		int64_t tCurPos = 0;
		int64_t tEndPos = 0;
		if (mVideoWidget->GetWorker()->PlayingFile())
		{
		    //LOG(LOG_VERBOSE, "Valid video position");
			// get current stream position from video source and use it as movie position
			tCurPos = mVideoWidget->GetWorker()->GetSeekPos();
			tEndPos = mVideoWidget->GetWorker()->GetSeekEnd();
		}
        if ((mAudioWidget->GetWorker()->PlayingFile()) && ((tCurPos == 0) || (tCurPos == tEndPos)))
		{
            //LOG(LOG_VERBOSE, "Valid audio position");
			// get current stream position from audio source and use it as movie position
			tCurPos = mAudioWidget->GetWorker()->GetSeekPos();
			tEndPos = mAudioWidget->GetWorker()->GetSeekEnd();
		}

        ShowStreamPosition(tCurPos, tEndPos);

    	//#################
    	// make sure the movie slider is displayed
    	//#################
    	mMovieControlsFrame->show();
    }else
    {
        mMovieControlsFrame->hide();
    }
}

void ParticipantWidget::UpdateAVStatistics()
{
    // should we update the A/V statistic widget?
    if (mLbAVStatistics->isVisible())
    {// time for an update
        QString tAVStats = "";
        QStringList tStats;
        int tStatLines;

        // VIDEO
        tAVStats += "Video statistics:";
        tStats = mVideoWidget->GetVideoStatistic();
        tStatLines = tStats.size();
        for (int i = 0; i < tStatLines; i++)
            tAVStats += "\n     " + tStats[i];

        tAVStats += "\n\n";

        tAVStats += "Video/audio:";

        tAVStats += "\n     A/V pre-buffering: ";
        if (mAVPreBuffering)
            tAVStats += "active";
        else
            tAVStats += "inactive";
        tAVStats += " ";
        if (mAvPreBufferingAutoRestart)
            tAVStats += "(auto restart)";

        tAVStats += "\n     A/V synchronization: ";
        if (mAVSynchActive)
            tAVStats += "active";
        else
            tAVStats += "inactive";

        tAVStats += "\n\n";

        // AUDIO
        tAVStats += "Audio statistics:";
        tStats = mAudioWidget->GetAudioStatistic();
        tStatLines = tStats.size();
        for (int i = 0; i < tStatLines; i++)
            tAVStats += "\n     " + tStats[i];

        mLbAVStatistics->setText(tAVStats);
    }
}

void ParticipantWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif

	if (pEvent->timerId() != mTimerId)
    {
    	LOG(LOG_WARN, "Qt event timer ID %d doesn't match the expected one %d", pEvent->timerId(), mTimerId);
        pEvent->ignore();
    	return;
    }

	// make sure that for BROADCAST widget we only synchronize in case of movie input data
	if (mSessionType == BROADCAST)
	{
	    bool tActive = PlayingMovieFile();

        // update the A/V synch. state
        mAVSynchActive = tActive;
        mAVPreBuffering = tActive;
        mAvPreBufferingAutoRestart = tActive;
	}

    UpdateMovieControls();

    AVSync();

    UpdateAVStatistics();

    pEvent->accept();
}

void ParticipantWidget::ResetMediaSinks()
{
    if (mVideoSourceMuxer != NULL)
        mVideoSourceMuxer->UnregisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort);
    if (mAudioSourceMuxer != NULL)
        mAudioSourceMuxer->UnregisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort);
    mParticipantVideoSink = NULL;
    mParticipantAudioSink = NULL;
}

void ParticipantWidget::ActionToggleAudioSenderActivation(bool pActivation)
{
    if (mParticipantAudioSink != NULL)
    {
        if (pActivation)
            mParticipantAudioSink->Start();
        else
            mParticipantAudioSink->Stop();
    }
}

void ParticipantWidget::ActionToggleVideoSenderActivation(bool pActivation)
{
    if (mParticipantVideoSink != NULL)
    {
        if (pActivation)
            mParticipantVideoSink->Start();
        else
            mParticipantVideoSink->Stop();
    }
}

void ParticipantWidget::ActionPlaylistPrevious()
{
	LOG(LOG_VERBOSE, "Triggered playback of previous entry in playlist");
	PLAYLISTWIDGET.PlayPrevious();
}

void ParticipantWidget::ActionPlaylistNext()
{
	LOG(LOG_VERBOSE, "Triggered playback of next entry in playlist");
	PLAYLISTWIDGET.PlayNext();
}

void ParticipantWidget::ActionPlaylistSetVisible(bool pVisible)
{
	LOG(LOG_VERBOSE, "Triggered toggling of visibility of playlist");
	PLAYLISTWIDGET.SetVisible(pVisible);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
