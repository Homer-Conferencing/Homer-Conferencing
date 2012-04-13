/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a dialog for adding an additional network based sink
 * Author:  Thomas Volkert
 * Since:   2010-06-20
 */

#include <MediaSource.h>
#include <HBSocket.h>
#include <Meeting.h>
#include <Configuration.h>
#include <HBSocket.h>
#include <Dialogs/AddNetworkSinkDialog.h>

using namespace Homer::Base;
using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

AddNetworkSinkDialog::AddNetworkSinkDialog(QWidget* pParent, MediaSource *pMediaSource) :
    QDialog(pParent)
{
    mMediaSource = pMediaSource;
	initializeGUI();
    LoadConfiguration();
    mLeHost->setFocus(Qt::TabFocusReason);
}

AddNetworkSinkDialog::~AddNetworkSinkDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void AddNetworkSinkDialog::initializeGUI()
{
    setupUi(this);
    if (!CONF.DebuggingEnabled())
    {
        mGbRequirements->hide();
        mGbInterface->hide();
    }
}

int AddNetworkSinkDialog::exec()
{
	int tResult = -1;

	tResult = QDialog::exec();

	if(tResult == QDialog::Accepted)
	{
		LOG(LOG_VERBOSE, "User has added a new network sink");
		SaveConfiguration();
	    CreateNewMediaSink();
	}

	return tResult;
}

void AddNetworkSinkDialog::CreateNewMediaSink()
{
    QString tHost = mLeHost->text();
    QString tPort = QString("%1").arg(mSbPort->value());
    enum TransportType tTransport = (enum TransportType)mCbTransport->currentIndex();

    //TODO: mMediaSource->RegisterMediaSink(tHost.toStdString(), tPort.toInt(), tTransport, mCbRtp->isChecked());
}

void AddNetworkSinkDialog::SaveConfiguration()
{
    if (mMediaSource->GetMediaType() == MEDIA_VIDEO)
    {// video
        CONF.SetVideoRtp(mCbRtp->isChecked());
        CONF.SetVideoTransport(Socket::String2TransportType(mCbTransport->currentText().toStdString()));
    }else
    {// audio
        CONF.SetAudioRtp(mCbRtp->isChecked());
        CONF.SetAudioTransport(Socket::String2TransportType(mCbTransport->currentText().toStdString()));
    }
}
void AddNetworkSinkDialog::LoadConfiguration()
{
    QString tTransport;

    // remove SCTP from comboBox if is not supported
    if(!Socket::IsTransportSupported(SOCKET_SCTP))
    {
        mCbTransport->removeItem(3);
    }
    // remove UDP-Lite from comboBox if is not supported
    if(!Socket::IsTransportSupported(SOCKET_UDP_LITE))
    {
        mCbTransport->removeItem(2);
    }

    mLeHost->setText(QString(MEETING.GetHostAdr().c_str()));
    if (mMediaSource->GetMediaType() == MEDIA_VIDEO)
    {// video
        mCbRtp->setChecked(CONF.GetVideoRtp());
        tTransport = QString(Socket::TransportType2String(CONF.GetVideoTransportType()).c_str());
    }else
    {// audio
        mCbRtp->setChecked(CONF.GetAudioRtp());
        tTransport = QString(Socket::TransportType2String(CONF.GetAudioTransportType()).c_str());

        mSbPort->setValue(5002);
    }

    for (int i = 0; i < mCbTransport->count(); i++)
    {
        QString tCurTransport = mCbTransport->itemText(i);
        if (tTransport == tCurTransport)
        {
            mCbTransport->setCurrentIndex(i);
            break;
        }
    }
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
