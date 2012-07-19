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
#include <Requirements.h>
#include <HBSocket.h>
#include <Meeting.h>
#include <Dialogs/AddNetworkSinkDialog.h>

#include <GAPI.h>
#include <Berkeley/SocketSetup.h>

#include <string>
#include <list>
#ifdef BSD
#include <HomerGLimits.h>
#else
#include <limits.h>
#endif

using namespace Homer::Base;
using namespace Homer::Monitor;
using namespace Homer::Multimedia;
using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

AddNetworkSinkDialog::AddNetworkSinkDialog(QWidget* pParent, QString pTitle, DataType pDataType, MediaSource *pMediaSource) :
    QDialog(pParent)
{
    mDataType = pDataType;
    mMediaSource = pMediaSource;
	initializeGUI();
    LoadConfiguration();
    mLeHost->setFocus(Qt::TabFocusReason);
    setWindowTitle(pTitle);
}

AddNetworkSinkDialog::~AddNetworkSinkDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void AddNetworkSinkDialog::initializeGUI()
{
    setupUi(this);
    connect(mCbGAPIImpl, SIGNAL(currentIndexChanged(QString)), this, SLOT(GAPISelectionChanged(QString)));

    if (!CONF.DebuggingEnabled())
    {
        mGbRequirements->hide();
        mGbInterface->hide();
        // minimize layout
        resize(0, 0);
    }
    if ((mDataType != DATA_TYPE_VIDEO) && (mDataType != DATA_TYPE_AUDIO))
    {
        mCbRtp->hide();
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
    if ((mDataType != DATA_TYPE_VIDEO) && (mDataType != DATA_TYPE_AUDIO))
    {
        return;
    }

    QString tHost = mLeHost->text();
    QString tPort = QString("%1").arg(mSbPort->value());
    enum TransportType tTransport = (enum TransportType)mCbTransport->currentIndex();

    Requirements *tRequs = new Requirements();
    RequirementTransmitBitErrors *tReqBitErr = new RequirementTransmitBitErrors(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
    RequirementTransmitStream *tReqStream = new RequirementTransmitStream();
    RequirementTargetPort *tReqPort = new RequirementTargetPort(tPort.toInt());
    RequirementLimitDelay *tReqDelay = new RequirementLimitDelay(mSbDelay->value());
    RequirementLimitDataRate *tReqDataRate = new RequirementLimitDataRate(mSbDataRate->value(), INT_MAX);
    RequirementTransmitLossless *tReqLossless = new RequirementTransmitLossless();

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

    // add target port
    tRequs->add(tReqPort);

    // add QoS parameter
    if (mCbDelay->isChecked())
        tRequs->add(tReqDelay);
    if (mCbDataRate->isChecked())
        tRequs->add(tReqDataRate);
    if (mCbLossless->isChecked())
        tRequs->add(tReqLossless);

    string tOldGAPIImpl = GAPI.getCurrentImplName();
    GAPI.selectImpl(mCbGAPIImpl->currentText().toStdString());
    if (mMediaSource != NULL)
        mMediaSource->RegisterMediaSink(tHost.toStdString(), tRequs, mCbRtp->isChecked());
    GAPI.selectImpl(tOldGAPIImpl);
}

Requirements* AddNetworkSinkDialog::GetRequirements()
{
    Requirements *tRequs = new Requirements();

    QString tHost = mLeHost->text();
    QString tPort = QString("%1").arg(mSbPort->value());
    enum TransportType tTransport = (enum TransportType)mCbTransport->currentIndex();

    RequirementTransmitBitErrors *tReqBitErr = new RequirementTransmitBitErrors(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    RequirementTransmitChunks *tReqChunks = new RequirementTransmitChunks();
    RequirementTransmitStream *tReqStream = new RequirementTransmitStream();
    RequirementTargetPort *tReqPort = new RequirementTargetPort(tPort.toInt());
    RequirementLimitDelay *tReqDelay = new RequirementLimitDelay(mSbDelay->value());
    RequirementLimitDataRate *tReqDataRate = new RequirementLimitDataRate(mSbDataRate->value(), INT_MAX);
    RequirementTransmitLossless *tReqLossless = new RequirementTransmitLossless();

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

    // add target port
    tRequs->add(tReqPort);

    // add QoS parameter
    if (mCbDelay->isChecked())
        tRequs->add(tReqDelay);
    if (mCbDataRate->isChecked())
        tRequs->add(tReqDataRate);
    if (mCbLossless->isChecked())
        tRequs->add(tReqLossless);

    return tRequs;
}

QString AddNetworkSinkDialog::GetTarget()
{
    return mLeHost->text();
}

QString AddNetworkSinkDialog::GetGAPIImplementation()
{
    return mCbGAPIImpl->currentText();
}

void AddNetworkSinkDialog::SaveConfiguration()
{
    switch(mDataType)
    {
        case DATA_TYPE_VIDEO:
            CONF.SetVideoRtp(mCbRtp->isChecked());
            CONF.SetVideoTransport(Socket::String2TransportType(mCbTransport->currentText().toStdString()));
            CONF.SetVideoStreamingGAPIImpl(mCbGAPIImpl->currentText());
            break;
        case DATA_TYPE_AUDIO:
            CONF.SetAudioRtp(mCbRtp->isChecked());
            CONF.SetAudioTransport(Socket::String2TransportType(mCbTransport->currentText().toStdString()));
            CONF.SetAudioStreamingGAPIImpl(mCbGAPIImpl->currentText());
            break;
        case DATA_TYPE_FILE:
            CONF.SetAppDataTransport(Socket::String2TransportType(mCbTransport->currentText().toStdString()));
            CONF.SetAppDataGAPIImpl(mCbGAPIImpl->currentText());
            break;
        default:
            LOG(LOG_WARN, "Unknown data type");
    }
}

void AddNetworkSinkDialog::GAPISelectionChanged(QString pSelection)
{
    if (pSelection == BERKEYLEY_SOCKETS)
    {
        mLeHost->setText(QString(MEETING.GetHostAdr().c_str()));
    }else
    {
        mLeHost->setText("Destination");
    }
}

void AddNetworkSinkDialog::LoadConfiguration()
{
    QString tTransport;
    QString tGAPIImpl;

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

    list<string> tGAPIImpls = GAPI.getAllImplNames();
    list<string>::iterator tGAPIImplsIt;
    mCbGAPIImpl->clear();
    for (tGAPIImplsIt = tGAPIImpls.begin(); tGAPIImplsIt != tGAPIImpls.end(); tGAPIImplsIt++)
    {
        mCbGAPIImpl->addItem(QString(tGAPIImplsIt->c_str()));
    }

    switch(mDataType)
    {
        case DATA_TYPE_VIDEO:
            mGrpTarget->setTitle(" Send video to ");
            mCbRtp->setChecked(CONF.GetVideoRtp());
            tTransport = QString(Socket::TransportType2String(CONF.GetVideoTransportType()).c_str());
            tGAPIImpl = CONF.GetVideoStreamingGAPIImpl();

            mSbPort->setValue(5000);
            mSbDelay->setValue(250);
            mSbDataRate->setValue(20);
            break;
        case DATA_TYPE_AUDIO:
            mGrpTarget->setTitle(" Send audio to ");
            mCbRtp->setChecked(CONF.GetAudioRtp());
            tTransport = QString(Socket::TransportType2String(CONF.GetAudioTransportType()).c_str());
            tGAPIImpl = CONF.GetAudioStreamingGAPIImpl();

            mSbPort->setValue(5002);
            mSbDelay->setValue(100);
            mSbDataRate->setValue(8);
            break;
        case DATA_TYPE_FILE:
            mGrpTarget->setTitle(" Send file to ");
            tTransport = QString(Socket::TransportType2String(CONF.GetAppDataTransportType()).c_str());
            tGAPIImpl = CONF.GetAppDataGAPIImpl();

            mSbPort->setValue(6000);
            mSbDelay->setValue(500);
            mSbDataRate->setValue(40);
            break;
        default:
            LOG(LOG_WARN, "Unknown data type");
    }


    for (int i = 0; i < mCbGAPIImpl->count(); i++)
    {
        QString tCurGAPIImpl = mCbGAPIImpl->itemText(i);
        if (tGAPIImpl == tCurGAPIImpl)
        {
            mCbGAPIImpl->setCurrentIndex(i);
            break;
        }
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
