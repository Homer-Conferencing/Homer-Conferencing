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
 * Purpose: participant widget
 * Author:  Thomas Volkert
 * Since:   2008-12-06
 */

#ifndef _PARTICIPANT_WIDGET_
#define _PARTICIPANT_WIDGET_

#include <Widgets/MessageWidget.h>
#include <Widgets/OverviewContactsWidget.h>
#include <Widgets/SessionInfoWidget.h>
#include <Widgets/VideoWidget.h>
#include <MediaSourceMuxer.h>
#include <Widgets/AudioWidget.h>
#include <MediaSourceMuxer.h>
#include <MeetingEvents.h>
#include <MediaSource.h>

#include <list>

#include <QSound>
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

#define	SOUND_FOR_CALL_LOOPS										10

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

class ParticipantWidget:
    public QDockWidget,
    public Ui_ParticipantWidget
{
    Q_OBJECT;

public:
    ParticipantWidget(enum SessionType pSessionType, QMainWindow *pMainWindow, OverviewContactsWidget *pContactsWidget, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pMessageMenu, MediaSourceMuxer *pVideoSourceMuxer = NULL, MediaSourceMuxer *pAudioSourceMuxer = NULL, QString pParticipant = "unknown");

    virtual ~ParticipantWidget();

    void HandleGeneralError(bool pIncoming, int pCode, QString pDescription);
    void HandleMessage(bool pIncoming, QString pSender, QString pMessage);
    void HandleMessageAccept(bool pIncoming);
    void HandleMessageAcceptDelayed(bool pIncoming);
    void HandleMessageUnavailable(bool pIncoming);
    void HandleCall(bool pIncoming, QString pRemoteApplication);
    void HandleCallAccept(bool pIncoming);
    void HandleCallUnavailable(bool pIncoming);
    void HandleCallRinging(bool pIncoming);
    void HandleCallStopped(bool pIncoming);
    void HandleCallDenied(bool pIncoming);
    void HandleMediaUpdate(bool pIncoming, QString pRemoteAudioAdr, unsigned int pRemoteAudioPort, QString pRemoteAudioCodec, QString pRemoteVideoAdr, unsigned int pRemoteVideoPort, QString pRemoteVideoCodec);

    void SetVideoStreamPreferences(QString pCodec, bool pJustReset = false);
    void SetAudioStreamPreferences(QString pCodec, bool pJustReset = false);
    void UpdateParticipantName(QString pParticipantName);
    void UpdateParticipantState(int pState);
    bool IsThisParticipant(QString pParticipant);
    QString getParticipantName();
    enum SessionType GetSessionType();
    QString GetSipInterface();
    VideoWorkerThread* GetVideoWorker();
    AudioWorkerThread* GetAudioWorker();

private slots:
	void LookedUpParticipantHost(const QHostInfo &pHost);

private:
	void initializeGUI();
    void FindSipInterface(QString pSessionName);
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void closeEvent(QCloseEvent* pEvent = NULL);
    virtual void dragEnterEvent(QDragEnterEvent *pEvent);
    virtual void dropEvent(QDropEvent *pEvent);
    void ShowNewState();

    QMainWindow         *mMainWindow;
    QMessageBox         *mCallBox;
    QString             mSessionName;
    QString				mSipInterface; // service access point name in form "IP:PORT" or "[IPv6]:PORT"
    QString             mWidgetTitle;
    bool                mQuitForced;
    bool                mIncomingCall;
    enum SessionType    mSessionType;
    MediaSourceMuxer    *mAudioSourceMuxer;
    MediaSourceMuxer    *mVideoSourceMuxer;
    QString             mRemoteAudioAdr;
    unsigned int        mRemoteAudioPort;
    QString             mRemoteAudioCodec;
    QString             mRemoteVideoAdr;
    unsigned int        mRemoteVideoPort;
    QString             mRemoteVideoCodec;
    MediaSource         *mVideoSource, *mAudioSource;
    QSound				*mSoundForIncomingCall;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
