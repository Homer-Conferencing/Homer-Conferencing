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
#include <Widgets/ParticipantWidget.h>
#include <Widgets/VideoWidget.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <MediaSourceNet.h>
#include <Widgets/AudioWidget.h>
#include <MediaSourceNet.h>
#include <Widgets/MessageWidget.h>
#include <Widgets/SessionInfoWidget.h>
#include <MainWindow.h>
#include <Configuration.h>
#include <Meeting.h>
#include <MeetingEvents.h>
#include <Logger.h>
#include <Snippets.h>

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
#include <QSound>

#include <stdlib.h>

using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

ParticipantWidget::ParticipantWidget(enum SessionType pSessionType, QMainWindow *pMainWindow, OverviewContactsWidget *pContactsWidget, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pMessageMenu, MediaSourceMuxer *pVideoSourceMuxer, MediaSourceMuxer *pAudioSourceMuxer, QString pParticipant):
    QDockWidget(pMainWindow)
{
    hide();
    mMainWindow = pMainWindow;
    mRemoteVideoAdr = "";
    mRemoteAudioAdr = "";
    mRemoteVideoPort = 0;
    mRemoteAudioPort = 0;
    mCallBox = NULL;
    mVideoSourceMuxer = pVideoSourceMuxer;
    mAudioSourceMuxer = pAudioSourceMuxer;
    mSessionType = pSessionType;
    mIncomingCall = false;
    mQuitForced = false;
    mSoundForIncomingCall = new QSound(CONF.GetCallSoundFile());

    //####################################################################
    //### create the remaining necessary widgets, menu and layouts
    //####################################################################
    Init(pContactsWidget, pVideoMenu, pAudioMenu, pMessageMenu, pParticipant);
}

ParticipantWidget::~ParticipantWidget()
{
    if (mSessionType == BROADCAST)
        CONF.SetVisibilityBroadcastWidget(isVisible());

    // inform the call partner
    switch (MEETING.GetCallState(QString(mSessionName.toLocal8Bit()).toStdString()))
    {
        case CALLSTATE_RUNNING:
                    MEETING.SendHangUp(QString(mSessionName.toLocal8Bit()).toStdString());
                    break;
        case CALLSTATE_RINGING:
                    MEETING.SendCallCancel(QString(mSessionName.toLocal8Bit()).toStdString());
                    break;
    }
    delete mVideoWidget;
    delete mAudioWidget;
    delete mMessageWidget;
    delete mSessionInfoWidget;

    delete mVideoSource;
    delete mAudioSource;
    delete mSoundForIncomingCall;

    if (mVideoSourceMuxer != NULL)
        mVideoSourceMuxer->UnregisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort);
    if (mAudioSourceMuxer != NULL)
        mAudioSourceMuxer->UnregisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort);

    if (mTimerId != -1)
        killTimer(mTimerId);
}

///////////////////////////////////////////////////////////////////////////////

void ParticipantWidget::Init(OverviewContactsWidget *pContactsWidget, QMenu *pVideoMenu, QMenu *pAudioMenu, QMenu *pMessageMenu, QString pParticipant)
{
    setupUi(this);

    QPalette tPalette;
    QBrush brush(QColor(0, 255, 255, 255));
    QBrush tBrush1(QColor(0, 128, 128, 255));
    QBrush brush2(QColor(155, 220, 198, 255));
    QBrush brush3(QColor(98, 99, 98, 255));
    QBrush brush4(QColor(100, 102, 100, 255));
    QBrush tBrush5(QColor(250, 250, 255, 255));
    QBrush tBrush6(QColor(145, 191, 155, 255));
    switch(CONF.GetColoringScheme())
    {
        case 0:
            // no coloring
            break;
        case 1:
            // set coloring of the DockWidget
            brush.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Active, QPalette::WindowText, brush);
            tBrush1.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Active, QPalette::Button, tBrush1);
            brush2.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Active, QPalette::ButtonText, brush2);
            tPalette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
            tPalette.setBrush(QPalette::Inactive, QPalette::Button, tBrush1);
            tPalette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush2);
            brush3.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
            tPalette.setBrush(QPalette::Disabled, QPalette::Button, tBrush1);
            brush4.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
            tBrush5.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Active, QPalette::Base, tBrush5);
            tBrush6.setStyle(Qt::SolidPattern);
            tPalette.setBrush(QPalette::Active, QPalette::Window, tBrush6);
            tPalette.setBrush(QPalette::Inactive, QPalette::Base, tBrush5);
            tPalette.setBrush(QPalette::Inactive, QPalette::Window, tBrush6);
            tPalette.setBrush(QPalette::Disabled, QPalette::Base, tBrush5);
            tPalette.setBrush(QPalette::Disabled, QPalette::Window, tBrush6);
            setPalette(tPalette);

            setAutoFillBackground(true);
            // manipulate stylesheet for buttons on the top right of the DockWidget and the title bar
            setStyleSheet("QDockWidget::close-button, QDockWidget::float-button { border: 1px solid; background: #9BDCC6; } QDockWidget::title { padding-left: 20px; text-align: left; background: #008080; }");
            break;
        default:
            break;
    }

    QFont font;
    font.setPointSize(8);
    font.setBold(true);
    font.setWeight(75);
    setFont(font);

    connect(mTbPlay, SIGNAL(clicked()), this, SLOT(PlayMovieFile()));
    connect(mTbPause, SIGNAL(clicked()), this, SLOT(PauseMovieFile()));
    connect(mSlMovie, SIGNAL(sliderMoved(int)), this, SLOT(SeekMovieFile(int)));


    //####################################################################
    //### create additional widget and allocate resources
    //####################################################################
    if ((CONF.GetParticipantWidgetsSeparation()) && (mSessionType == PARTICIPANT))
    {
        setParent(NULL);
        setWindowIcon(QPixmap(":/images/UserUnavailable.png"));
        setFeatures(QDockWidget::DockWidgetClosable);
    }else
    {
        setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
        mMainWindow->addDockWidget(Qt::RightDockWidgetArea, this, Qt::Horizontal);
    }

    mVideoSource = NULL;
    mAudioSource = NULL;
    OpenVideoAudioPreviewDialog *tOpenVideoAudioPreviewDialog = NULL;
    bool tFoundPreviewSource = false;
    switch(mSessionType)
    {
        case BROADCAST:
                    LOG(LOG_VERBOSE, "Creating participant widget for BROADCAST");
                    mSessionName = "BROADCAST";
                    LOG(LOG_VERBOSE, "..init broacast message widget");
                    mMessageWidget->Init(pMessageMenu, mSessionName, NULL, CONF.GetVisibilityBroadcastMessageWidget());
                    LOG(LOG_VERBOSE, "..init broacast video widget");
                    if (mVideoSourceMuxer != NULL)
                    {
                        mVideoWidgetFrame->show();
                        mVideoWidget->Init(mMainWindow, mVideoSourceMuxer, pVideoMenu, mSessionName, mSessionName, true);
                    }
                    LOG(LOG_VERBOSE, "..init broacast audio widget");
                    if (mAudioSourceMuxer != NULL)
                        mAudioWidget->Init(mAudioSourceMuxer, pAudioMenu, mSessionName, mSessionName, true, true);
                    setFeatures(QDockWidget::NoDockWidgetFeatures);
                    break;
        case PARTICIPANT:
                    LOG(LOG_VERBOSE, "Creating participant widget for PARTICIPANT");
                    mMovieControlsFrame->hide();
                    mSessionName = pParticipant;
                    FindSipInterface(pParticipant);
                    mMessageWidget->Init(pMessageMenu, mSessionName, pContactsWidget);
                    mVideoSendSocket = MEETING.GetVideoSendSocket(mSessionName.toStdString());
                    mAudioSendSocket = MEETING.GetAudioSendSocket(mSessionName.toStdString());
                    mVideoReceiveSocket = MEETING.GetVideoReceiveSocket(mSessionName.toStdString());
                    mAudioReceiveSocket = MEETING.GetAudioReceiveSocket(mSessionName.toStdString());
                    if (mVideoReceiveSocket != NULL)
                    {
                        mVideoSource = new MediaSourceNet(mVideoReceiveSocket, CONF.GetVideoRtp());
                        mVideoSource->SetInputStreamPreferences(CONF.GetVideoCodec().toStdString());
                        mVideoWidgetFrame->hide();
                        mVideoWidget->Init(mMainWindow, mVideoSource, pVideoMenu, mSessionName);
                    }else
                        LOG(LOG_ERROR, "Determined video socket is NULL");
                    if (mAudioReceiveSocket != NULL)
                    {
                        mAudioSource = new MediaSourceNet(mAudioReceiveSocket, CONF.GetAudioRtp());
                        mAudioSource->SetInputStreamPreferences(CONF.GetAudioCodec().toStdString());
                        mAudioWidget->Init(mAudioSource, pAudioMenu, mSessionName);
                    }else
                        LOG(LOG_ERROR, "Determined audio socket is NULL");
                    break;
        case PREVIEW:
                    LOG(LOG_VERBOSE, "Creating participant widget for PREVIEW");
                    mSessionName = "PREVIEW";
                    if ((pVideoMenu != NULL) || (pAudioMenu != NULL))
                    {
                        tOpenVideoAudioPreviewDialog = new OpenVideoAudioPreviewDialog(this);
                        if (tOpenVideoAudioPreviewDialog->exec() == QDialog::Accepted)
                        {
                            QString tVDesc, tADesc;
                            mVideoSource = tOpenVideoAudioPreviewDialog->GetMediaSourceVideo();
                            if (mVideoSource != NULL)
                            {
                                tVDesc = QString(mVideoSource->GetCurrentDeviceName().c_str());
                                mVideoWidgetFrame->show();
                                mVideoWidget->Init(mMainWindow, mVideoSource, pVideoMenu, mSessionName, mSessionName, true);
                                tFoundPreviewSource = true;
                            }

                            mAudioSource = tOpenVideoAudioPreviewDialog->GetMediaSourceAudio();
                            if (mAudioSource != NULL)
                            {
                                tADesc += QString(mAudioSource->GetCurrentDeviceName().c_str());
                                mAudioWidget->Init(mAudioSource, pAudioMenu, mSessionName, mSessionName, true, false);
                                tFoundPreviewSource = true;
                            }
                            if(tVDesc != tADesc)
                                mSessionName = "PREVIEW " + tVDesc + " / " + tADesc;
                            else
                                mSessionName = "PREVIEW " + tVDesc;

                            if (!tOpenVideoAudioPreviewDialog->FileSourceSelected())
                                mMovieControlsFrame->hide();
                        }
                        if(tFoundPreviewSource)
                            break;
                    }

                    // delete this participant widget again if no preview could be opened
                    QCoreApplication::postEvent(mMainWindow, (QEvent*) new QMeetingEvent(new DeleteSessionEvent(this)));
                    return;
        default:
                    break;
    }

    mSessionInfoWidget->Init(mSessionName, false);

    //####################################################################
    //### update GUI
    //####################################################################

    UpdateParticipantName(mSessionName);
    // hide automatic QAction within QDockWidget context menu
    toggleViewAction()->setVisible(false);
    show();

    // we support Drag+Drop here
    setAcceptDrops(true);

    if (mSessionType == BROADCAST)
        setVisible(CONF.GetVisibilityBroadcastWidget());

    mTimerId = startTimer(STREAM_POS_UPDATE_DELAY);
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
        default:
                    break;
    }
}

void ParticipantWidget::contextMenuEvent(QContextMenuEvent *pEvent)
{
    if (mSessionType != PARTICIPANT)
        return;

    QAction *tAction;
    QMenu tMenu(this);

    if (CONF.DebuggingEnabled())
    {
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
        QIcon tIcon2;
        tIcon2.addPixmap(QPixmap(":/images/Info.png"), QIcon::Normal, QIcon::Off);
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
        }
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
                        if (tList.size() == 1)
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
                            if (tList.size() == 1)
                            {
                                QString tFileName = tList.begin()->toString();
                                bool tAccept = false;
                                if (OverviewPlaylistWidget::IsVideoFile(tFileName))
                                    mVideoWidget->GetWorker()->PlayFile(tFileName);

                                if (OverviewPlaylistWidget::IsAudioFile(tFileName))
                                    mAudioWidget->GetWorker()->PlayFile(tFileName);

                                if (tAccept)
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

void ParticipantWidget::LookedUpParticipantHost(const QHostInfo &pHost)
{
    if (pHost.error() != QHostInfo::NoError)
    {
        ShowError("DNS lookup error", "Unable to lookup DNS entry for \"" + pHost.hostName() + "\" because \"" + pHost.errorString() + "\"");
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

bool ParticipantWidget::IsThisParticipant(QString pParticipant)
{
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

    // we only check the user name and the host address
    // this is a result of some experiments with ekiga/linphone
    //LOG(LOG_VERBOSE, "IsThisParticipant-Compare: %s with %s", pParticipant.toStdString().c_str(), mSessionName.section(":", 0, 0).section("@", 1, 1).toStdString().c_str());
    //return pParticipant.contains(mSessionName.section(":", 0, 0).section("@", 1, 1));

    LOG(LOG_VERBOSE, "CompareIsThisParticipant: %s with %s and %s", pParticipant.toStdString().c_str(), mSessionName.section("@", 1).toStdString().c_str(), mSipInterface.toStdString().c_str());
    return (pParticipant.contains(mSessionName.section("@", 1)) || pParticipant.contains(mSipInterface));
}

void ParticipantWidget::HandleMessage(bool pIncoming, QString pSender, QString pMessage)
{
    // return immediately if we are a preview only
    if (mSessionType == PREVIEW)
    {
        LOG(LOG_ERROR, "This function is not support vor preview widgets");
        return;
    }

    if (!pIncoming)
    {
    	LOG(LOG_ERROR, "Loopback message %s received from %s", pMessage.toStdString().c_str(), pSender.toStdString().c_str());
    	return;
    }else
        LOG(LOG_VERBOSE, "Message %s received from %s", pMessage.toStdString().c_str(), pSender.toStdString().c_str());

    if (mMessageWidget != NULL)
    {
        mMessageWidget->AddMessage(pSender, pMessage);
        if (pIncoming)
			if (CONF.GetImSound())
				QSound::play(CONF.GetImSoundFile());
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
    CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_UNDEFINED_STATE);

    ShowError("General error occurred", "General error of code " + QString("%1").arg(pCode) + " occurred. The error is described with \"" + pDescription + "\"");
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
    CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_AVAILABLE);
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
    CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_UNDEFINED_STATE);
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
    CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_UNAVAILABLE);
    ShowError("Participant unavailable", "The participant " + mSessionName + " is currently unavailable for an instant message! The reason is \"" + pDescription + "\"(" + QString("%1").arg(pStatusCode) + ").");
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
		if (CONF.GetCallAcknowledgeSound())
			QSound::play(CONF.GetCallAcknowledgeSoundFile());
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

        MEETING.SendCallAcknowledge(QString(mSessionName.toLocal8Bit()).toStdString());

        mCallBox = new QMessageBox(QMessageBox::Question, "Incoming call from application " + pRemoteApplication, "Do you want to accept the incoming call from " + mSessionName + "?", QMessageBox::Yes | QMessageBox::Cancel, this);

        // start sound output
        if (pIncoming)
        {
			if (CONF.GetCallSound())
			{
				mSoundForIncomingCall->setLoops(SOUND_FOR_CALL_LOOPS);
				mSoundForIncomingCall->play();
			}
        }

        if (mCallBox->exec() == QMessageBox::Yes)
        {
            if (mIncomingCall)
            {
                MEETING.SendCallAccept(QString(mSessionName.toLocal8Bit()).toStdString());
            }
        }else
        {
            MEETING.SendCallDeny(QString(mSessionName.toLocal8Bit()).toStdString());
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
		if (CONF.GetCallHangupSound())
			QSound::play(CONF.GetCallHangupSoundFile());
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
		if (CONF.GetCallHangupSound())
			QSound::play(CONF.GetCallHangupSoundFile());
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
		if (CONF.GetErrorSound())
			QSound::play(CONF.GetErrorSoundFile());
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

    mVideoSourceMuxer->UnregisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort);
    mAudioSourceMuxer->UnregisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort);

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
        mAudioWidget->SetVisible(false);
    mIncomingCall = false;

    // stop sound output
    if (mSoundForIncomingCall->loopsRemaining())
    	mSoundForIncomingCall->stop();
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
        CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_UNAVAILABLE);

        ShowError("Participant unavailable", "The participant " + mSessionName + " is currently unavailable for a call! The reason is \"" + pDescription + "\"(" + QString("%1").arg(pStatusCode) + ").");
    }else
    	CallStopped(pIncoming);

    if (pIncoming)
		if (CONF.GetErrorSound())
			QSound::play(CONF.GetErrorSoundFile());
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
		if (CONF.GetCallDenySound())
			QSound::play(CONF.GetCallDenySoundFile());
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
    CONTACTSPOOL.UpdateContactState(mSessionName, CONTACT_AVAILABLE);

    if (mMessageWidget != NULL)
        mMessageWidget->AddMessage("", "session established", true);

    ShowNewState();
    if (mVideoWidget != NULL)
    {
        mVideoWidgetFrame->show();
        mVideoWidget->SetVisible(true);
    }
    if (mAudioWidget != NULL)
        mAudioWidget->SetVisible(true);
    mIncomingCall = false;

    // stop sound output
    if (mSoundForIncomingCall->loopsRemaining())
    	mSoundForIncomingCall->stop();

    if (pIncoming)
		if (CONF.GetCallAcknowledgeSound())
			QSound::play(CONF.GetCallAcknowledgeSoundFile());
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
        mVideoSourceMuxer->UnregisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort);
        mAudioSourceMuxer->UnregisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort);

        LOG(LOG_VERBOSE, "Video sink set to %s:%u", pRemoteVideoAdr.toStdString().c_str(), pRemoteVideoPort);
        LOG(LOG_VERBOSE, "Video sink uses codec: \"%s\"", pRemoteVideoCodec.toStdString().c_str());
        LOG(LOG_VERBOSE, "Audio sink set to %s:%u", pRemoteAudioAdr.toStdString().c_str(), pRemoteAudioPort);
        LOG(LOG_VERBOSE, "Audio sink uses codec: \"%s\"", pRemoteAudioCodec.toStdString().c_str());

        MediaSinkNet* tVideoSink = NULL;
        MediaSinkNet* tAudioSink = NULL;
        if (pRemoteVideoPort != 0)
            tVideoSink = mVideoSourceMuxer->RegisterMediaSink(mRemoteVideoAdr.toStdString(), mRemoteVideoPort, mVideoSendSocket, true); // always use RTP/AVP profile (RTP/UDP)
        if (pRemoteAudioPort != 0)
            tAudioSink = mAudioSourceMuxer->RegisterMediaSink(mRemoteAudioAdr.toStdString(), mRemoteAudioPort, mAudioSendSocket, true); // always use RTP/AVP profile (RTP/UDP)

        if (tVideoSink != NULL)
            tVideoSink->AssignStreamName("CONF-OUT: " + mSessionName.toStdString());
        if (tAudioSink != NULL)
            tAudioSink->AssignStreamName("CONF-OUT: " + mSessionName.toStdString());

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

void ParticipantWidget::ShowNewState()
{
    switch (MEETING.GetCallState(QString(mSessionName.toLocal8Bit()).toStdString()))
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
            setWindowIcon(QPixmap(":/images/UserUnavailable.png"));
            break;
        case CONTACT_AVAILABLE:
            setWindowIcon(QPixmap(":/images/UserAvailable.png"));
            break;
        default:
        case CONTACT_UNDEFINED_STATE:
            setWindowIcon(QPixmap(":/images/Warning.png").scaled(24, 24, Qt::KeepAspectRatio, Qt::FastTransformation));
            break;
    }
}

QString ParticipantWidget::getParticipantName()
{
    if (mSessionType == PARTICIPANT)
        return mSessionName;
    else
        return "";
}

enum SessionType ParticipantWidget::GetSessionType()
{
    return mSessionType;
}

QString ParticipantWidget::GetSipInterface()
{
	return mSipInterface;
}

void ParticipantWidget::PlayMovieFile()
{
    mVideoWidget->GetWorker()->PlayFile(mVideoWidget->GetWorker()->CurrentFile());
    mAudioWidget->GetWorker()->PlayFile(mAudioWidget->GetWorker()->CurrentFile());
}

void ParticipantWidget::PauseMovieFile()
{
    mVideoWidget->GetWorker()->PauseFile();
    mAudioWidget->GetWorker()->PauseFile();
}

void ParticipantWidget::SeekMovieFile(int pPos)
{
    mVideoWidget->GetWorker()->Seek(pPos);
    mAudioWidget->GetWorker()->Seek(pPos);
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
    if ((mAudioWidget != NULL) && (mSessionType != PREVIEW))
        return mAudioWidget->GetWorker();
    else
        return NULL;
}

void ParticipantWidget::timerEvent(QTimerEvent *pEvent)
{
    #ifdef DEBUG_TIMING
        LOG(LOG_VERBOSE, "New timer event");
    #endif
    int tTmp = 0;
    int tHour, tMin, tSec;

    if((mVideoWidget->GetWorker()->SupportsSeeking()) || (mAudioWidget->GetWorker()->SupportsSeeking()))
        mMovieControlsFrame->show();
    else
        mMovieControlsFrame->hide();

    if ((pEvent->timerId() == mTimerId) && (mMovieControlsFrame->isVisible()) && ((mVideoWidget->GetWorker()->SupportsSeeking()) || (mAudioWidget->GetWorker()->SupportsSeeking())))
    {
        int64_t tCurPos = 0;
        int64_t tEndPos = 0;
        if(mVideoWidget->GetWorker()->SupportsSeeking())
        {
            // get current stream position from video source and use it as movie position
            tCurPos = mVideoWidget->GetWorker()->GetSeekPos();
            tEndPos = mVideoWidget->GetWorker()->GetSeekEnd();
        }else
        {
            // get current stream position from audio source and use it as movie position
            tCurPos = mAudioWidget->GetWorker()->GetSeekPos();
            tEndPos = mAudioWidget->GetWorker()->GetSeekEnd();
        }
        if(tEndPos)
            tTmp = 1000 * tCurPos / tEndPos;
        else
            tTmp = 0;

        // update GUI widgets
        mSlMovie->setValue(tTmp);
        mMoviePosWidget->showPosition(tCurPos, tEndPos);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
