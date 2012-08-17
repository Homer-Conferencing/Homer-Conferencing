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
#include <MediaSourceVFW.h>
#include <MediaSourcePortAudio.h>
#include <MediaSourceAlsa.h>
#include <MediaSourceMMSys.h>
#include <MediaSourceNet.h>
#include <MediaSourceFile.h>
#include <Snippets.h>
#include <HBSocket.h>
#include <GAPI.h>
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
        resize(0, 0);
    }

    connect(mPbFile, SIGNAL(clicked()), this, SLOT(actionGetFile()));

    connect(mCbVideoEnabled, SIGNAL(clicked(bool)), this, SLOT(actionVideoEnabled(bool)));
    connect(mCbAudioEnabled, SIGNAL(clicked(bool)), this, SLOT(actionAudioEnabled(bool)));

    connect(mCbGAPIImplVideo, SIGNAL(currentIndexChanged(QString)), this, SLOT(GAPIVideoSelectionChanged(QString)));
    connect(mCbGAPIImplAudio, SIGNAL(currentIndexChanged(QString)), this, SLOT(GAPIAudioSelectionChanged(QString)));

    LoadConfiguration();
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
    string tOldGAPIImpl;
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
                return new MediaSourceVFW(mCbDeviceVideo->currentText().toStdString());
            #endif
            #ifdef APPLE
                return new MediaSourceCoreVideo(mCbDeviceVideo->currentText().toStdString());
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
            #ifdef USE_GAPI
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

                tOldGAPIImpl = GAPI.getCurrentImplName();
                GAPI.selectImpl(mCbGAPIImplVideo->currentText().toStdString());
                tNetSource = new MediaSourceNet(tHost.toStdString(), tRequs, mCbRtpVideo->isChecked());
                GAPI.selectImpl(tOldGAPIImpl);
            #else
                MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortVideo->value(), (enum TransportType)mCbTransportVideo->currentIndex(), mCbRtpVideo->isChecked());
            #endif
            if (tNetSource->GetListenerPort() == 0)
            {
                ShowError("Video preview not possible", "The preview of the incoming video stream at local port \"" + QString("%1").arg(mSbPortVideo->value()) + "\" with transport \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportVideo->currentIndex()).c_str()) + "\" and codec \"" + mCbCodecVideo->currentText() + "\" is not possible!");
                delete tNetSource;
                return NULL;
            }
            tNetSource->SetInputStreamPreferences(mCbCodecVideo->currentText().toStdString(), false, mCbRtpVideo->isChecked());
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

MediaSource* OpenVideoAudioPreviewDialog::GetMediaSourceAudio()
{
    MediaSourceNet *tNetSource = NULL;
    string tOldGAPIImpl;
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
            return new MediaSourcePortAudio(mCbDeviceAudio->currentText().toStdString());
            break;
        case 1: // file
            if (mLbFile->text() != "")
                return new MediaSourceFile(mLbFile->text().toStdString());
            else
                return NULL;
            break;
        case 2: // network streaming
            #ifdef USE_GAPI
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

                tOldGAPIImpl = GAPI.getCurrentImplName();
                GAPI.selectImpl(mCbGAPIImplAudio->currentText().toStdString());
                tNetSource = new MediaSourceNet(tHost.toStdString(), tRequs, mCbRtpAudio->isChecked());
                GAPI.selectImpl(tOldGAPIImpl);
            #else
                MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortAudio->value(), (enum TransportType)mCbTransportAudio->currentIndex(), mCbRtpAudio->isChecked());
            #endif
            if (tNetSource->GetListenerPort() == 0)
            {
                ShowError("Audio preview not possible", "The preview of the incoming audio stream at local port \"" + QString("%1").arg(mSbPortAudio->value()) + "\" with transport \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportAudio->currentIndex()).c_str()) + "\" and codec \"" + mCbCodecAudio->currentText() + "\" is not possible!");
                delete tNetSource;
                return NULL;
            }
            tNetSource->SetInputStreamPreferences(mCbCodecAudio->currentText().toStdString(), false, mCbRtpAudio->isChecked());
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

    CONF.SetVideoStreamingGAPIImpl(mCbGAPIImplVideo->currentText());
    CONF.SetAudioStreamingGAPIImpl(mCbGAPIImplAudio->currentText());

    CONF.SetPreviewSelectionAudio(mCbAudioEnabled->isChecked());
    CONF.SetPreviewSelectionVideo(mCbVideoEnabled->isChecked());
}

void OpenVideoAudioPreviewDialog::GAPIVideoSelectionChanged(QString pSelection)
{
    if (pSelection == BERKEYLEY_SOCKETS)
    {
        mLeHostVideo->setText(QString(MEETING.GetHostAdr().c_str()));
    }else
    {
        mLeHostVideo->setText("Destination");
    }
}

void OpenVideoAudioPreviewDialog::GAPIAudioSelectionChanged(QString pSelection)
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

    //########################
    //### capture source
    //########################
    #ifdef WIN32
        MediaSourceVFW *tVSource = new MediaSourceVFW("");
    #endif
    #ifdef APPLE
        MediaSourceCoreVideo *tVSource = new MediaSourceCoreVideo("");
    #endif
	#if (defined BSD) && (not defined APPLE)
        MediaSource *tVSource = NULL; //TODO: replace with a specialized implementation
	#endif
    #ifdef LINUX
        MediaSourceV4L2 *tVSource = new MediaSourceV4L2("");
    #endif
    if (CONF.AudioCaptureEnabled())
    {
        MediaSourcePortAudio *tASource = new MediaSourcePortAudio("");
        if (tASource != NULL)
        {
            tASource->getAudioDevices(mAudioDevicesList);
            delete tASource;
        }
    }
    if (tVSource != NULL)
    {
        tVSource->getVideoDevices(mVideoDevicesList);
        delete tVSource;
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
    if (tVideoStreamCodec == "H.261")
        mCbCodecVideo->setCurrentIndex(0);
    if (tVideoStreamCodec == "H.263")
        mCbCodecVideo->setCurrentIndex(1);
    if (tVideoStreamCodec == "H.263+")
        mCbCodecVideo->setCurrentIndex(2);
    if (tVideoStreamCodec == "H.264")
        mCbCodecVideo->setCurrentIndex(3);
    if (tVideoStreamCodec == "MPEG1")
        mCbCodecVideo->setCurrentIndex(4);
    if (tVideoStreamCodec == "MPEG2")
        mCbCodecVideo->setCurrentIndex(5);
    if (tVideoStreamCodec == "MPEG4")
        mCbCodecVideo->setCurrentIndex(6);
    if (tVideoStreamCodec == "THEORA")
        mCbCodecVideo->setCurrentIndex(7);
    if (tVideoStreamCodec == "VP8")
        mCbCodecVideo->setCurrentIndex(8);

    QString tAudioStreamCodec = CONF.GetAudioCodec();
    if (tAudioStreamCodec == "MP3 (MPA)")
        mCbCodecAudio->setCurrentIndex(0);
    if (tAudioStreamCodec == "G711 A-law (PCMA)")
        mCbCodecAudio->setCurrentIndex(1);
    if (tAudioStreamCodec == "G711 µ-law (PCMU)")
        mCbCodecAudio->setCurrentIndex(2);
    if (tAudioStreamCodec == "AAC")
        mCbCodecAudio->setCurrentIndex(3);
    if (tAudioStreamCodec == "PCM_S16_LE")
        mCbCodecAudio->setCurrentIndex(4);
    if (tAudioStreamCodec == "GSM")
        mCbCodecAudio->setCurrentIndex(5);
    if (tAudioStreamCodec == "AMR")
        mCbCodecAudio->setCurrentIndex(6);


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
    list<string> tGAPIImpls = GAPI.getAllImplNames();
    list<string>::iterator tGAPIImplsIt;
    mCbGAPIImplVideo->clear();
    mCbGAPIImplAudio->clear();
    for (tGAPIImplsIt = tGAPIImpls.begin(); tGAPIImplsIt != tGAPIImpls.end(); tGAPIImplsIt++)
    {
        mCbGAPIImplVideo->addItem(QString(tGAPIImplsIt->c_str()));
        mCbGAPIImplAudio->addItem(QString(tGAPIImplsIt->c_str()));
    }
    QString tGAPIImplVideo = CONF.GetVideoStreamingGAPIImpl();
    QString tGAPIImplAudio = CONF.GetAudioStreamingGAPIImpl();
    for (int i = 0; i < mCbGAPIImplVideo->count(); i++)
    {
        QString tCurGAPIImpl = mCbGAPIImplVideo->itemText(i);
        if (tGAPIImplVideo == tCurGAPIImpl)
            mCbGAPIImplVideo->setCurrentIndex(i);
        if (tGAPIImplAudio == tCurGAPIImpl)
            mCbGAPIImplAudio->setCurrentIndex(i);
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
    mSbPortVideo->setValue(5000 + tParticipantSessions * 4);
    mSbPortAudio->setValue(5002 + tParticipantSessions * 4);

    mCbAudioEnabled->setChecked(CONF.GetPreviewSelectionAudio());
    mCbVideoEnabled->setChecked(CONF.GetPreviewSelectionVideo());
}

void OpenVideoAudioPreviewDialog::actionGetFile()
{
    QString tFileName;

    if((mCbVideoEnabled->isChecked()) && (mCbAudioEnabled->isChecked()))
    {
        tFileName = OverviewPlaylistWidget::LetUserSelectMovieFile(this, "Select movie file for preview", false).first();
    }else
    {
        if(mCbVideoEnabled->isChecked())
            tFileName = OverviewPlaylistWidget::LetUserSelectVideoFile(this, "Select video file for preview", false).first();
        if(mCbAudioEnabled->isChecked())
            tFileName = OverviewPlaylistWidget::LetUserSelectAudioFile(this, "Select audio file for preview", false).first();
    }

    if (tFileName.isEmpty())
        return;

    mLbFile->setText(tFileName);
}

void OpenVideoAudioPreviewDialog::actionVideoEnabled(bool pState)
{

    if(pState)
    {

    }else
    {

    }
    mLbFile->setText("");
}

void OpenVideoAudioPreviewDialog::actionAudioEnabled(bool pState)
{
    if(pState)
    {

    }else
    {

    }
    mLbFile->setText("");
}

}} //namespace
