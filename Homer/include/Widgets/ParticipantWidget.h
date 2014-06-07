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
 * Purpose: participant widget
 * Since:   2008-12-06
 */

#ifndef _PARTICIPANT_WIDGET_
#define _PARTICIPANT_WIDGET_

#include <Widgets/MessageWidget.h>
#include <Widgets/MovieControlWidget.h>
#include <Widgets/PlaybackSlider.h>
#include <Widgets/SessionInfoWidget.h>
#include <Widgets/VideoWidget.h>
#include <AudioPlayback.h>

#include <MediaSourceMuxer.h>
#include <Widgets/AudioWidget.h>
#include <MediaSourceMuxer.h>
#include <MeetingEvents.h>
#include <MediaSource.h>
#include <MediaSinkNet.h>
#include <WaveOut.h>

#include <HBSocket.h>

#include <list>

#include <QMenu>
#include <QEvent>
#include <QAction>
#include <QString>
#include <QWidget>
#include <QFrame>
#include <QDockWidget>
#include <QSplitter>
#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHostInfo>

#include <ui_ParticipantWidget.h>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

class ParticipantWidget;
typedef std::list<ParticipantWidget*>  ParticipantWidgetList;

///////////////////////////////////////////////////////////////////////////////

// de/activate A/V synch. for file playback: audio grabbing is always synchronized with video grabbing and not the other way around
#define PARTICIPANT_WIDGET_AV_SYNC

// avoid over buffering of input A/V stream
#define PARTICIPANT_WIDGET_AV_SYNC_AVOID_OVER_BUFFERING
//#define PARTICIPANT_WIDGET_AV_SYNC_AVOID_UNDER_BUFFERING

///////////////////////////////////////////////////////////////////////////////
// use the SIP-Events-structure for signaling the deletion of a participant ///
///////////////////////////////////////////////////////////////////////////////
#define DELETE_SESSION                                              100000

enum SessionType{
    INVALID = -1,
    BROADCAST = 0,
    PARTICIPANT,
    PREVIEW
};

class DeleteSessionEvent:
    public TEvent<DeleteSessionEvent, DELETE_SESSION>
{
public:
        DeleteSessionEvent(ParticipantWidget *pParticipantWidget):PWidget(pParticipantWidget)
        { }

    ParticipantWidget   *PWidget;
};

///////////////////////////////////////////////////////////////////////////////
class MainWindow;
class ParticipantWidget:
    public QDockWidget,
    AudioPlayback,
    public Ui_ParticipantWidget
{
    Q_OBJECT;

public:
    /* factory functions */
    static ParticipantWidget* CreateBroadcast(MainWindow *pMainWindow, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pAVControlsMenu, QMenu *pMessageMenu, MediaSourceMuxer *pVideoSourceMuxer, MediaSourceMuxer *pAudioSourceMuxer);
    static ParticipantWidget* CreateParticipant(MainWindow *pMainWindow, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pAVControlsMenu, QMenu *pMessageMenu, MediaSourceMuxer *pVideoSourceMuxer, MediaSourceMuxer *pAudioSourceMuxer, QString pParticipant, enum TransportType pTransport);
    static ParticipantWidget* CreatePreview(MainWindow *pMainWindow, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pAVControlsMenu);
    static ParticipantWidget* CreatePreviewNetworkStreams(MainWindow *pMainWindow, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pAVControlsMenu, unsigned int pVideoListenerPort, enum TransportType pVideoTransportType, unsigned int pAudioListenerPort, enum TransportType pAudioTransportType);

    virtual ~ParticipantWidget();

    void HandleGeneralError(bool pIncoming, int pCode, QString pDescription);
    void HandleMessage(bool pIncoming, QString pSender, QString pMessage);
    void HandleMessageAccept(bool pIncoming);
    void HandleMessageAcceptDelayed(bool pIncoming);
    void HandleMessageUnavailable(bool pIncoming, int pStatusCode, QString pDescription);
    void HandleCall(bool pIncoming, QString pRemoteApplication);
    void HandleCallAccept(bool pIncoming);
    void HandleCallCancel(bool pIncoming);
    void HandleCallHangup(bool pIncoming);
    void HandleCallTermination(bool pIncoming);
    void HandleCallUnavailable(bool pIncoming, int pStatusCode, QString pDescription);
    void HandleCallRinging(bool pIncoming);
    void HandleCallDenied(bool pIncoming);
    void HandleMediaUpdate(bool pIncoming, QString pRemoteAudioAdr, unsigned int pRemoteAudioPort, QString pRemoteAudioCodec, unsigned int pNegotiatedRTPAudioPayloadID, QString pRemoteVideoAdr, unsigned int pRemoteVideoPort, QString pRemoteVideoCodec, unsigned int pNegotiatedRTPVideoPayloadID);

    void SetVideoStreamPreferences(QString pCodec, bool pJustReset = false);
    void SetAudioStreamPreferences(QString pCodec, bool pJustReset = false);
    void UpdateParticipantName(QString pParticipantName);
    void UpdateParticipantState(int pState);
    bool IsThisParticipant(QString pParticipant, enum Homer::Base::TransportType pParticipantTransport);
    QString GetParticipantName();
    enum TransportType GetParticipantTransport();
    enum SessionType GetSessionType();
    QString GetSipInterface();
    VideoWorkerThread* GetVideoWorker();
    AudioWorkerThread* GetAudioWorker();
    bool IsAVDriftOkay();

    void SeekMovieFileRelative(float pSeconds);

    /* mosaic mode */
    void ToggleMosaicMode(bool pActive);

    /* fullscreen mode */
    void ToggleFullScreenMode(bool pActive);
    void SetVisible(bool pVisible);

    void Call();

private slots:
    void ActionPlayPauseMovieFile(QString pFileName = "");
    void ActionRecordMovieFile();
	void LookedUpParticipantHost(const QHostInfo &pHost);
	void ActionSeekMovieFile(int pPos);
	void ActionSeekMovieFileToPos(int pPos);
	void ActionUserAVDriftChanged(double pDrift);
	void ActionToggleUserAVDriftWidget();
	friend class PlaybackSlider;

	void ActionToggleAudioSenderActivation(bool pActivation);
    void ActionToggleVideoSenderActivation(bool pActivation);
    void ActionPlaylistPrevious();
    void ActionPlaylistNext();
    void ActionPlaylistSetVisible(bool pVisible);

    void UpdateMenuSettings();

    /* AV controls */
    void SelectedMenuAVControls(QAction *pAction);
    void ToggleAVControlsVisibility();

private:
    // private constructor, factory functions should be used to create a new participant widget
    ParticipantWidget(enum SessionType pSessionType, MainWindow *pMainWindow);

    QMenu *GetMenuSettings();

    friend class VideoWidget;

	/* media sink helper */
	void ResetMediaSinks();

	/* visibility of A/V widget */
    void StartFullscreenMode(int tFullscreenPosX, int tFullscreenPosY);
    void StopFullscreenMode();

	void ResizeAVView(int pSize);
	void Init(QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pAVControlsMenu, QMenu *pMessageMenu = NULL, MediaSourceMuxer *pVideoSourceMuxer = NULL, MediaSourceMuxer *pAudioSourceMuxer = NULL, QString pParticipant = "", enum TransportType pTransport = SOCKET_UDP);
    void FindSipInterface(QString pSessionName);
    void UpdateMovieControls();
    void UpdateAVInfo();
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void closeEvent(QCloseEvent* pEvent = NULL);
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    virtual void dropEvent(QDropEvent *pEvent);
    virtual void keyPressEvent(QKeyEvent *pEvent);
    virtual void wheelEvent(QWheelEvent *pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);

    /* AV sync */
    void ResetAVSync();
    void AVSync();
    double GetAVDrift(int64_t *pVideoSyncTime = NULL, int64_t *pAudioSyncTime = NULL); // positive values mean "video before audio, audio is too late"
    float GetUserAVDrift();
    float GetVideoDelayAVDrift();
    void SetUserAVDrift(float pDrift);
    void ReportVideoDelay(float pDelay); // adds an additional delay to audio if video presentation is delayed
    void InformAboutVideoSeekingComplete();
    bool IsPlayingMovieFile();
    bool isVideoFilePaused();
    bool isAudioFilePaused();
    void AVSeek(int pPos); // position given in x/1000

    /* fullscreen movie controls */
    void CreateFullscreenControls(int tFullscreenPosX, int tFullscreenPosY);
    void DestroyFullscreenControls();

    /* user activity handling */
    void ShowFullscreenMovieControls();
    void HideFullscreenMovieControls();

    /* AV controls */
    void InitializeMenuAVControls(QMenu *pMenu);

    void ShowNewState();
    void ShowStreamPosition(float pCurPos, float pEndPos);
    void SessionEstablished(bool pIncoming);
    void SessionStopped(bool pIncoming);

    MainWindow          	*mMainWindow;
    QMessageBox         	*mCallBox;
    QString            	 	mSessionName;
    enum TransportType  	mSessionTransport;
    QString					mSipInterface; // service access point name in form "IP:PORT" or "[IPv6]:PORT"
    QString            		mWidgetTitle;
    bool                	mQuitForced;
    bool                	mIncomingCall;
    enum SessionType    	mSessionType;
    MediaSourceMuxer    	*mAudioSourceMuxer;
    MediaSourceMuxer    	*mVideoSourceMuxer;
    MediaSinkNet            *mParticipantVideoSink;
    bool                    mParticipantVideoSinkActivation;
    MediaSinkNet            *mParticipantAudioSink;
    bool                    mParticipantAudioSinkActivation;
    bool                    mSessionIsRunning; // a call is running?
    QString             	mRemoteAudioAdr;
    unsigned int        	mRemoteAudioPort;
    QString            		mRemoteAudioCodec;
    QString             	mRemoteVideoAdr;
    unsigned int        	mRemoteVideoPort;
    QString             	mRemoteVideoCodec;
    MediaSource         	*mVideoSource, *mAudioSource;
    Socket					*mVideoSendSocket, *mAudioSendSocket, *mVideoReceiveSocket, *mAudioReceiveSocket;
    int                 	mTimerId;
    int						mMovieSliderPosition;
    /* settings menus */
    QMenu                   *mMenuSettings;
    QMenu                   *mMenuSettingsVideo;
    QMenu                   *mMenuSettingsAudio;
    QMenu					*mMenuSettingsAVControls;
    QMenu                   *mMenuSettingsMessages;
    QMenu                   *mMenuSettingsSessionInfo;
    /* A/V controls */
    QAction					*mAssignedActionAVControls;
    int                     mPlayPauseButtonIsPaused; // used to avoid unnecessary periodic updates
    /* A/V synch. */
    int64_t                 mAVSyncCounter;
    int64_t					mLastAudioSynchronizationTimestamp;
    int64_t					mLastVideoSynchronizationTimestamp;
    float                   mVideoDelayAVDrift;
    int64_t					mTimeOfLastAVSynch;
    int 					mAVAsyncCounterSinceLastSynchronization;
    int						mAVASyncCounter;
    MovieControlWidget      *mFullscreeMovieControlWidget;
    bool                    mAVSynchActive; //controls A/V synch.
    bool                    mAVPreBuffering; // only for GUI output
    bool                    mAVPreBufferingAutoRestart; // only for GUI output
    /* Mosaic mode */
    bool					mMosaicMode;
    bool					mMosaicModeAVControlsWereVisible;
    QWidget					*mMosaicModeGenericTitleWidget;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
