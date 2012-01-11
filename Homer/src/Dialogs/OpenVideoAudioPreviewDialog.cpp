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
 * Purpose: Implementation of a dialog to open video/audio preview
 * Author:  Thomas Volkert
 * Since:   2011-11-28
 */

#include <Dialogs/OpenVideoAudioPreviewDialog.h>
#include <Widgets/OverviewPlaylistWidget.h>
#include <MediaSourceCoreAudio.h>
#include <MediaSourceCoreVideo.h>
#include <MediaSourceV4L2.h>
#include <MediaSourceVFW.h>
#include <MediaSourceAlsa.h>
#include <MediaSourceMMSys.h>
#include <MediaSourceNet.h>
#include <MediaSourceFile.h>
#include <Snippets.h>
#include <HBSocket.h>

#include <Configuration.h>

namespace Homer { namespace Gui {

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

    connect(mPbFile, SIGNAL(clicked()), this, SLOT(actionGetFile()));
    connect(mCbVideoEnabled, SIGNAL(clicked(bool)), this, SLOT(actionVideoEnabled(bool)));
    connect(mCbAudioEnabled, SIGNAL(clicked(bool)), this, SLOT(actionAudioEnabled(bool)));

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
    if(!mCbVideoEnabled->isChecked())
        return NULL;

    if (mRbDevice->isChecked())
    {
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
    }
    if (mRbFile->isChecked())
    {
        if (mLbFile->text() != "")
            return new MediaSourceFile(mLbFile->text().toStdString());
        else
            return NULL;
    }
    if (mRbStream->isChecked())
    {
        MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortVideo->value(), (enum TransportType)mCbTransportVideo->currentIndex(), mCbRtpVideo->isChecked());
        if (tNetSource->getListenerPort() == 0)
        {
            ShowError("Video preview not possible", "The preview of the incoming video stream at local port \"" + QString("%1").arg(mSbPortVideo->value()) + "\" with transport \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportVideo->currentIndex()).c_str()) + "\" and codec \"" + mCbCodecVideo->currentText() + "\" is not possible!");
            delete tNetSource;
            return NULL;
        }
        tNetSource->SetInputStreamPreferences(mCbCodecVideo->currentText().toStdString(), false, mCbRtpVideo->isChecked());
        return tNetSource;
    }

    return NULL;
}

bool OpenVideoAudioPreviewDialog::FileSourceSelected()
{
    return mRbFile->isChecked();
}

MediaSource* OpenVideoAudioPreviewDialog::GetMediaSourceAudio()
{
    if(!mCbAudioEnabled->isChecked())
        return NULL;

    if (mRbDevice->isChecked())
    {
        #ifdef WIN32
            return new MediaSourceMMSys(mCbDeviceAudio->currentText().toStdString());
        #endif
        #ifdef APPLE
            return new MediaSourceCoreAudio(mCbDeviceAudio->currentText().toStdString());
        #endif
        #ifdef BSD
            //TODO
        #endif
        #ifdef LINUX
            return new MediaSourceAlsa(mCbDeviceAudio->currentText().toStdString());
        #endif
    }
    if (mRbFile->isChecked())
    {
        if (mLbFile->text() != "")
            return new MediaSourceFile(mLbFile->text().toStdString());
        else
            return NULL;
    }
    if (mRbStream->isChecked())
    {
        MediaSourceNet *tNetSource = new MediaSourceNet(mSbPortAudio->value(), (enum TransportType)mCbTransportAudio->currentIndex(), mCbRtpAudio->isChecked());
        if (tNetSource->getListenerPort() == 0)
        {
            ShowError("Preview not possible", "The preview of the incoming video stream at local port \"" + QString("%1").arg(mSbPortAudio->value()) + "\" with transport \"" + QString(Socket::TransportType2String((enum TransportType)mCbTransportAudio->currentIndex()).c_str()) + "\" and codec \"" + mCbCodecAudio->currentText() + "\" is not possible!");
            delete tNetSource;
            return NULL;
        }
        tNetSource->SetInputStreamPreferences(mCbCodecAudio->currentText().toStdString(), false, mCbRtpAudio->isChecked());
        return tNetSource;
    }

    return NULL;
}

void OpenVideoAudioPreviewDialog::SaveConfiguration()
{
    CONF.SetVideoRtp(mCbRtpVideo->isChecked());
    CONF.SetVideoTransport(Socket::String2TransportType(mCbTransportVideo->currentText().toStdString()));
    CONF.SetAudioRtp(mCbRtpAudio->isChecked());
    CONF.SetAudioTransport(Socket::String2TransportType(mCbTransportAudio->currentText().toStdString()));
}

void OpenVideoAudioPreviewDialog::LoadConfiguration()
{

    //########################
    //### capture source
    //########################
    #ifdef WIN32
        MediaSourceVFW *tVSource = new MediaSourceVFW("");
        MediaSourceMMSys *tASource = new MediaSourceMMSys("");
    #endif
    #ifdef APPLE
        MediaSourceCoreVideo *tVSource = new MediaSourceCoreVideo("");
        MediaSourceCoreAudio *tASource = new MediaSourceCoreAudio("");
    #endif
	#ifdef BSD
		//TODO
	#endif
    #ifdef LINUX
        MediaSourceV4L2 *tVSource = new MediaSourceV4L2("");
        MediaSourceAlsa *tASource = new MediaSourceAlsa("");
    #endif
    tVSource->getVideoDevices(mVideoDevicesList);
    tASource->getAudioDevices(mAudioDevicesList);
    delete tVSource;
    delete tASource;

    mCbDeviceVideo->clear();
    VideoDevicesList::iterator tItVideo;
    for (tItVideo = mVideoDevicesList.begin(); tItVideo != mVideoDevicesList.end(); tItVideo++)
    {
        mCbDeviceVideo->addItem(QString(tItVideo->Name.c_str()));
    }

    mCbDeviceAudio->clear();
    AudioDevicesList::iterator tItAudio;
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
    if (tVideoStreamCodec == "MPEG4")
        mCbCodecVideo->setCurrentIndex(4);
    if (tVideoStreamCodec == "MJPEG")
        mCbCodecVideo->setCurrentIndex(5);

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
    mRbFile->setChecked(true);
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
