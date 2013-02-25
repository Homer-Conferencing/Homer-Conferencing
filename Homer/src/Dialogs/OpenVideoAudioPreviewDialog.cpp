/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a dialog to open video/audio preview
 * Author:  Thomas Volkert
 * Since:   2011-11-28
 */

#include <Dialogs/OpenVideoAudioPreviewDialog.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <MediaSourceCoreVideo.h>
#include <MediaSourceV4L2.h>
#include <MediaSourceDShow.h>
#include <MediaSourcePortAudio.h>
#include <MediaSourcePulseAudio.h>
#include <MediaSourceAlsa.h>
#include <MediaSourceNet.h>
#include <MediaSourceFile.h>
#include <WaveOutPulseAudio.h>
#include <Snippets.h>
#include <HBSocket.h>
#include <NAPI.h>
#include <Meeting.h>
#include <Berkeley/SocketSetup.h>
#include <Meeting.h>
#include <Configuration.h>

namespace Homer { namespace Gui {

using namespace Homer::Conference;

///////////////////////////////////////////////////////////////////////////////

OpenVideoAudioPreviewDialog::OpenVideoAudioPreviewDialog(QWidget* pParent) :
    QDialog(pParent)
{
    initializeGUI();
}

OpenVideoAudioPreviewDialog::~OpenVideoAudioPreviewDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void OpenVideoAudioPreviewDialog::initializeGUI()
{
    setupUi(this);
    if (!CONF.DebuggingEnabled())
    {
        mGbInterfaceVideo->hide();
        mLbHostVideo->hide();
        mLeHostVideo->hide();
        mGbInterfaceAudio->hide();
        mLbHostAudio->hide();
        mLeHostAudio->hide();
        // minimize layout
        ShrinkWidgetToMinimumSize();
    }

    connect(mPbFile, SIGNAL(clicked()), this, SLOT(ActionGetFile()));

    connect(mCbNAPIImplVideo, SIGNAL(currentIndexChanged(QString)), this, SLOT(NAPIVideoSelectionChanged(QString)));
    connect(mCbNAPIImplAudio, SIGNAL(currentIndexChanged(QString)), this, SLOT(NAPIAudioSelectionChanged(QString)));

    LoadConfiguration();
}

void OpenVideoAudioPreviewDialog::ShrinkWidgetToMinimumSize()
{
    QTimer::singleShot(0, this, SLOT(DoWidgetShrinking()));
}

void OpenVideoAudioPreviewDialog::DoWidgetShrinking()
{
    resize(minimumSizeHint());
}

int OpenVideoAudioPreviewDialog::exec()
{
    int tResult = -1;

    tResult = QDialog::exec();

    if(tResult == QDialog::Accepted)
    {
        LOG(LOG_VERBOSE, "User wants to open a new video/audio preview");
        SaveConfiguration();
    }

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

MediaSource* OpenVideoAudioPreviewDialog::GetMediaSourceVideo()
{
    MediaSourceNet *tNetSource = NULL;
    string tOldNAPIImpl;
    QString tHost = mLeHostVideo->text();
    QString tPort = QString("%1").arg(mSbPortVideo->value());
    enum TransportType tTransport = (enum TransportType)mCbTransportVideo->currentIndex();

    Requirements *tRequs = new Requirements();
    RequirementTransmitBitErrors *tReqBitErr = new RequirementTransmitBitErrors(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
    RequirementTransmitStream *tReqStream = new RequirementTransmitStream();
    RequirementTargetPort *tReqPort = new RequirementTargetPort(tPort.toInt());

    if(!mCbVideoEnabled->isChecked())
        return NULL;

    switch(mSwPreviewPages->currentIndex())
    {
        case 0: // devices
            #ifdef WIN32
                return new MediaSourceDShow(mCbDeviceVideo->currentText().toStdString());
            #endif
			#ifdef WIN64
				return null; //TODO
			#endif
            #ifdef APPLE
//                return new MediaSourceCoreVideo(mCbDeviceVideo->currentText().toStdString());
            #endif
            #ifdef BSD
                //TODO
            #endif
            #ifdef LINUX
                return new MediaSourceV4L2(mCbDeviceVideo->currentText().toStdString());
            #endif
            break;
        case 1: // file
            if (mLbFile->text() != "")
                return new MediaSourceFile(mLbFile->text().toStdString());
            else
                return NULL;
            break;
        case 2: // network streaming
            #ifdef USE_NAPI
                // add transport details depending on transport protocol selection
                switch(tTransport)
                {
                    case SOCKET_UDP_LITE:
                        tRequs->add(tReqBitErr);
                    case SOCKET_UDP:
                        tRequs->add(tReqChunks);
                        break;
                    case SOCKET_TCP:
                        tRequs->add(tReqStream);
                        break;
                    default:
                        LOG(LOG_WARN, "Unsupported transport protocol selected");
                        break;
                }

                // add local port
                tRequs->add(tReqPort);

                tOldNAPIImpl = NAPI.getCurrentImplName();
                NAPI.selectImpl(mCbNAPIImplVideo->currentText().toStdString());
                tNetSource = new MediaSourceNet(tHost.toStdString(), tRequs);
                NAPI.selectImpl(tOldNAPIImpl);
            #else
                MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortVideo->value(), (enum TransportType)mCbTransportVideo->currentIndex(), mCbRtpVideo->isChecked());
            #endif
            if (tNetSource->GetListenerPort() == 0)
            {
                ShowError(Homer::Gui::OpenVideoAudioPreviewDialog::tr("Video preview not possible"), Homer::Gui::OpenVideoAudioPreviewDialog::tr("The preview of the incoming video stream at local port") + " \"" + QString("%1").arg(mSbPortVideo->value()) + "\" " + Homer::Gui::OpenVideoAudioPreviewDialog::tr("with transport") + " \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportVideo->currentIndex()).c_str()) + "\" " + Homer::Gui::OpenVideoAudioPreviewDialog::tr("and codec") + "\"" + mCbCodecVideo->currentText() + "\" " + Homer::Gui::OpenVideoAudioPreviewDialog::tr("is not possible!"));
                delete tNetSource;
                return NULL;
            }
            tNetSource->SetPreBufferingActivation(mGrpPreBuffering->isChecked());
            tNetSource->SetPreBufferingAutoRestartActivation(mCbRestartPreBuffering->isChecked());
			// leave some seconds for high system load situations so that this part of the input queue can be used for compensating it
            if (mGrpPreBuffering->isChecked())
            	tNetSource->SetFrameBufferPreBufferingTime(mDSpPreBufferingTime->value());
            tNetSource->SetInputStreamPreferences(mCbCodecVideo->currentText().toStdString(), mCbRtpVideo->isChecked(), false);
            return tNetSource;
            break;
        default:
            LOG(LOG_ERROR, "Invalid index");
            break;
    }
    return NULL;
}

bool OpenVideoAudioPreviewDialog::FileSourceSelected()
{
    return (mSwPreviewPages->currentIndex() == 1);
}

bool OpenVideoAudioPreviewDialog::AVPreBuffering()
{
    switch(mSwPreviewPages->currentIndex())
    {
        case 0: // devices
            return false; // devices are not buffered by us, but maybe they are buffered by the hardware abstraction libraries which we use
        case 1: // file
            return true; // files are always pre-buffered with auto-restart;
        case 2: // network streaming
            return mGrpPreBuffering->isChecked();
    }
    return false;
}

bool OpenVideoAudioPreviewDialog::AVPreBufferingAutoRestart()
{
	if (!AVPreBuffering())
		return false;

    switch(mSwPreviewPages->currentIndex())
    {
        case 0: // devices
            return false; // devices are not buffered by us, but maybe they are buffered by the hardware abstraction libraries which we use
        case 1: // file
            return true; // files are always pre-buffered with auto-restart;
        case 2: // network streaming
            return mCbRestartPreBuffering->isChecked();
    }
    return false;
}

bool OpenVideoAudioPreviewDialog::AVSynchronization()
{
	if (!AVPreBuffering())
		return false;

    switch(mSwPreviewPages->currentIndex())
    {
        case 0: // devices
            return false; // devices are not synchronized by us, but maybe they are buffered by the hardware abstraction libraries which we use
        case 1: // file
            return true; // files are always presented with A/V synch.
        case 2: // network streaming
            return mCbAVSynch->isChecked();
    }
    return false;
}

MediaSource* OpenVideoAudioPreviewDialog::GetMediaSourceAudio()
{
    MediaSourceNet *tNetSource = NULL;
    string tOldNAPIImpl;
    QString tHost = mLeHostAudio->text();
    QString tPort = QString("%1").arg(mSbPortAudio->value());
    enum TransportType tTransport = (enum TransportType)mCbTransportAudio->currentIndex();

    Requirements *tRequs = new Requirements();
    RequirementTransmitBitErrors *tReqBitErr = new RequirementTransmitBitErrors(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
    RequirementTransmitStream *tReqStream = new RequirementTransmitStream();
    RequirementTargetPort *tReqPort = new RequirementTargetPort(tPort.toInt());

    if(!mCbAudioEnabled->isChecked())
        return NULL;

    switch(mSwPreviewPages->currentIndex())
    {
        case 0: // devices
            #if defined(LINUX) && FEATURE_PULSEAUDIO
        		if (!(WaveOutPulseAudio::PulseAudioAvailable()))
        			return new MediaSourcePortAudio(mCbDeviceAudio->currentText().toStdString());
        		else
        			return new MediaSourcePulseAudio(mCbDeviceAudio->currentText().toStdString());
			#else
            	return new MediaSourcePortAudio(mCbDeviceAudio->currentText().toStdString());
			#endif
            break;
        case 1: // file
            if (mLbFile->text() != "")
                return new MediaSourceFile(mLbFile->text().toStdString());
            else
                return NULL;
            break;
        case 2: // network streaming
            #ifdef USE_NAPI
                // add transport details depending on transport protocol selection
                switch(tTransport)
                {
                    case SOCKET_UDP_LITE:
                        tRequs->add(tReqBitErr);
                    case SOCKET_UDP:
                        tRequs->add(tReqChunks);
                        break;
                    case SOCKET_TCP:
                        tRequs->add(tReqStream);
                        break;
                    default:
                        LOG(LOG_WARN, "Unsupported transport protocol selected");
                        break;
                }

                // add local port
                tRequs->add(tReqPort);

                tOldNAPIImpl = NAPI.getCurrentImplName();
                NAPI.selectImpl(mCbNAPIImplAudio->currentText().toStdString());
                tNetSource = new MediaSourceNet(tHost.toStdString(), tRequs);
                NAPI.selectImpl(tOldNAPIImpl);
            #else
                MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortAudio->value(), (enum TransportType)mCbTransportAudio->currentIndex(), mCbRtpAudio->isChecked());
            #endif
            if (tNetSource->GetListenerPort() == 0)
            {
                ShowError(Homer::Gui::OpenVideoAudioPreviewDialog::tr("Audio preview not possible"), Homer::Gui::OpenVideoAudioPreviewDialog::tr("The preview of the incoming audio stream at local port") + " \"" + QString("%1").arg(mSbPortAudio->value()) + "\" with transport \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportAudio->currentIndex()).c_str()) + "\" " + Homer::Gui::OpenVideoAudioPreviewDialog::tr("and codec") + " \"" + mCbCodecAudio->currentText() + "\" " + Homer::Gui::OpenVideoAudioPreviewDialog::tr("is not possible!"));
                delete tNetSource;
                return NULL;
            }
            tNetSource->SetPreBufferingActivation(mGrpPreBuffering->isChecked());
            tNetSource->SetPreBufferingAutoRestartActivation(mCbRestartPreBuffering->isChecked());
			// leave some seconds for high system load situations so that this part of the input queue can be used for compensating it
            if (mGrpPreBuffering->isChecked())
            	tNetSource->SetFrameBufferPreBufferingTime(mDSpPreBufferingTime->value());
            tNetSource->SetInputStreamPreferences(mCbCodecAudio->currentText().toStdString(), mCbRtpAudio->isChecked(), false);
            return tNetSource;
            break;
        default:
            LOG(LOG_ERROR, "Invalid index");
            break;
    }
    return NULL;
}

void OpenVideoAudioPreviewDialog::SaveConfiguration()
{
    CONF.SetVideoRtp(mCbRtpVideo->isChecked());
    CONF.SetVideoTransport(Socket::String2TransportType(mCbTransportVideo->currentText().toStdString()));
    CONF.SetAudioRtp(mCbRtpAudio->isChecked());
    CONF.SetAudioTransport(Socket::String2TransportType(mCbTransportAudio->currentText().toStdString()));

    CONF.SetVideoStreamingNAPIImpl(mCbNAPIImplVideo->currentText());
    CONF.SetAudioStreamingNAPIImpl(mCbNAPIImplAudio->currentText());

    CONF.SetPreviewSelectionAudio(mCbAudioEnabled->isChecked());
    CONF.SetPreviewSelectionVideo(mCbVideoEnabled->isChecked());

    CONF.SetPreviewPreBufferingActivation(mGrpPreBuffering->isChecked());

    CONF.SetPreviewSelection(mLwSelectionList->currentRow());
}

void OpenVideoAudioPreviewDialog::NAPIVideoSelectionChanged(QString pSelection)
{
    if (pSelection == BERKEYLEY_SOCKETS)
    {
        mLeHostVideo->setText(QString(MEETING.GetHostAdr().c_str()));
    }else
    {
        mLeHostVideo->setText("Destination");
    }
}

void OpenVideoAudioPreviewDialog::NAPIAudioSelectionChanged(QString pSelection)
{
    if (pSelection == BERKEYLEY_SOCKETS)
    {
        mLeHostAudio->setText(QString(MEETING.GetHostAdr().c_str()));
    }else
    {
        mLeHostAudio->setText("Destination");
    }
}

void OpenVideoAudioPreviewDialog::LoadConfiguration()
{
	MediaSource *tVSource = NULL;
	MediaSource *tASource = NULL;

    //########################
    //### capture source
    //########################
    #ifdef WIN32
		tVSource = new MediaSourceDShow("");
    #endif
	#ifdef WIN64
		//TODO
	#endif
    #ifdef APPLE
        tVSource = new MediaSourceCoreVideo("");
    #endif
	#if (defined BSD) && (not defined APPLE)
        //TODO: replace with a specialized implementation
	#endif
    #ifdef LINUX
        tVSource = new MediaSourceV4L2("");
    #endif
	if (tVSource != NULL)
	{
		tVSource->getVideoDevices(mVideoDevicesList);
		delete tVSource;
	}

	if (CONF.AudioCaptureEnabled())
    {
        #if defined(LINUX) && FEATURE_PULSEAUDIO
    		if (!(WaveOutPulseAudio::PulseAudioAvailable()))
				tASource = new MediaSourcePortAudio("");
    		else
    			tASource = new MediaSourcePulseAudio("");
		#else
        	tASource = new MediaSourcePortAudio("");
		#endif
    }
    if (tASource != NULL)
    {
        tASource->getAudioDevices(mAudioDevicesList);
        delete tASource;
    }

    mCbDeviceVideo->clear();
    VideoDevices::iterator tItVideo;
    for (tItVideo = mVideoDevicesList.begin(); tItVideo != mVideoDevicesList.end(); tItVideo++)
    {
        mCbDeviceVideo->addItem(QString(tItVideo->Name.c_str()));
    }

    mCbDeviceAudio->clear();
    AudioDevices::iterator tItAudio;
    for (tItAudio = mAudioDevicesList.begin(); tItAudio != mAudioDevicesList.end(); tItAudio++)
    {
        mCbDeviceAudio->addItem(QString(tItAudio->Name.c_str()));
    }

    //########################
    //### stream codec
    //########################
    QString tVideoStreamCodec = CONF.GetVideoCodec();
    for (int i = 0; i < mCbCodecVideo->count(); i++)
    {
        if (tVideoStreamCodec == mCbCodecVideo->itemText(i))
        {
        	mCbCodecVideo->setCurrentIndex(i);
            break;
        }
    }

    QString tAudioStreamCodec = CONF.GetAudioCodec();
    for (int i = 0; i < mCbCodecAudio->count(); i++)
    {
        if (tAudioStreamCodec == mCbCodecAudio->itemText(i))
        {
        	mCbCodecAudio->setCurrentIndex(i);
            break;
        }
    }


    //########################
    //### host
    //########################
    mLeHostVideo->setText(QString(MEETING.GetHostAdr().c_str()));
    mLeHostAudio->setText(QString(MEETING.GetHostAdr().c_str()));

    //########################
    //### transport type
    //########################
    // remove SCTP from comboBox if is not supported
    if(!Socket::IsTransportSupported(SOCKET_SCTP))
    {
        mCbTransportVideo->removeItem(3);
        mCbTransportAudio->removeItem(3);
    }
    // remove UDP-Lite from comboBox if is not supported
    if(!Socket::IsTransportSupported(SOCKET_UDP_LITE))
    {
        mCbTransportVideo->removeItem(2);
        mCbTransportAudio->removeItem(2);
    }

    QString tVTransport = QString(Socket::TransportType2String(CONF.GetVideoTransportType()).c_str());
    for (int i = 0; i < mCbTransportVideo->count(); i++)
    {
        if (tVTransport == mCbTransportVideo->itemText(i))
        {
            mCbTransportVideo->setCurrentIndex(i);
            break;
        }
    }

    QString tATransport = QString(Socket::TransportType2String(CONF.GetAudioTransportType()).c_str());
    for (int i = 0; i < mCbTransportAudio->count(); i++)
    {
        if (tATransport == mCbTransportAudio->itemText(i))
        {
            mCbTransportAudio->setCurrentIndex(i);
            break;
        }
    }

    //########################
    //### network interface
    //########################
    list<string> tNAPIImpls = NAPI.getAllImplNames();
    list<string>::iterator tNAPIImplsIt;
    mCbNAPIImplVideo->clear();
    mCbNAPIImplAudio->clear();
    for (tNAPIImplsIt = tNAPIImpls.begin(); tNAPIImplsIt != tNAPIImpls.end(); tNAPIImplsIt++)
    {
        mCbNAPIImplVideo->addItem(QString(tNAPIImplsIt->c_str()));
        mCbNAPIImplAudio->addItem(QString(tNAPIImplsIt->c_str()));
    }
    QString tNAPIImplVideo = CONF.GetVideoStreamingNAPIImpl();
    QString tNAPIImplAudio = CONF.GetAudioStreamingNAPIImpl();
    for (int i = 0; i < mCbNAPIImplVideo->count(); i++)
    {
        QString tCurNAPIImpl = mCbNAPIImplVideo->itemText(i);
        if (tNAPIImplVideo == tCurNAPIImpl)
            mCbNAPIImplVideo->setCurrentIndex(i);
        if (tNAPIImplAudio == tCurNAPIImpl)
            mCbNAPIImplAudio->setCurrentIndex(i);
    }

    //########################
    //### RTP encapsulation
    //########################
    if (CONF.GetVideoRtp())
        mCbRtpVideo->setChecked(true);
    else
        mCbRtpVideo->setChecked(false);

    if (CONF.GetAudioRtp())
        mCbRtpAudio->setChecked(true);
    else
        mCbRtpAudio->setChecked(false);

    //########################
    //### PORTS
    //########################
    int tParticipantSessions = MEETING.CountParticipantSessions() -1;
    if (tParticipantSessions < 0)
        tParticipantSessions = 0;
    mSbPortVideo->setValue(5000 + tParticipantSessions * 4);
    mSbPortAudio->setValue(5002 + tParticipantSessions * 4);

    mCbAudioEnabled->setChecked(CONF.GetPreviewSelectionAudio());
    mCbVideoEnabled->setChecked(CONF.GetPreviewSelectionVideo());

    //########################
    //### Pre-buffering
    //########################
    mGrpPreBuffering->setChecked(CONF.GetPreviewPreBufferingActivation());

    //########################
    //### Source type selection
    //########################
    mLwSelectionList->setCurrentRow(CONF.GetPreviewSelection());
}

void OpenVideoAudioPreviewDialog::ActionGetFile()
{
    QString tFileName;

    QStringList tUserSelection = OverviewPlaylistWidget::LetUserSelectMediaFile(this, "Select file for preview", false);

    if (tUserSelection.isEmpty())
        return ;

    tFileName = tUserSelection.first();

    mLbFile->setText(tFileName);
}

}} //namespace
