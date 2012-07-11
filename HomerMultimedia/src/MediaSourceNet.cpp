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
 * Purpose: Implementation of a ffmpeg based network media source
 * Author:  Thomas Volkert
 * Since:   2008-12-16
 */

// HINT: for audio streams the RTP support remains unused!

#include <MediaSourceNet.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <RequirementTransmitBitErrors.h>
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

void MediaSourceNet::Init(Socket *pDataSocket, unsigned int pLocalPort, bool pRtpActivated)
{
    mListenerPort = pLocalPort;
    mPacketNumber = 0;
    mReceiveErrors = 0;
    mPeerHost = "";
    mPeerPort = 0;
    mOpenInputStream = false;
    mListenerRunning = false;
    mRtpActivated = pRtpActivated;
    mPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    mStreamCodecId = CODEC_ID_NONE;

    mDataSocket = pDataSocket;

    if(mDataSocket != NULL)
    {
        // set receive buffer size
        mDataSocket->SetReceiveBufferSize(SOCKET_RECEIVE_BUFFER_SIZE);

        // check the UDPLite and the RTP header
        if (mDataSocket->GetTransportType() == SOCKET_UDP_LITE)
            mDataSocket->UDPLiteSetCheckLength(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);

        if (mDataSocket->GetTransportType() == SOCKET_TCP)
        {
            mStreamedTransport = true;
            mPacketStatAdditionalFragmentSize = TCP_FRAGMENT_HEADER_SIZE;
        }
        LOG(LOG_VERBOSE, "Listen for media packets at port %u, transport %d, IP version %d", mDataSocket->GetLocalPort(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
        mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

        mListenerPort = mDataSocket->GetLocalPort();
    }else if (mGAPIDataSocket != NULL)
    {
        if (mGAPIDataSocket->isClosed())
        {
            LOG(LOG_WARN, "GAPI association is closed, reseting local port to 0");
            mListenerPort = 0;
        }

        LOG(LOG_VERBOSE, "Listen for media packets at GAPI interface: %s, local port specified as: %d, TCP-like transport: %d", mGAPIDataSocket->getName()->toString().c_str(), getListenerPort(), mStreamedTransport);
        // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
        mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements().contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";
    }else
    {
        mListenerPort = 0;
        LOG(LOG_ERROR, "No valid transport socket");
    }

    AssignStreamName(mCurrentDeviceName);
}

MediaSourceNet::MediaSourceNet(Socket *pDataSocket, bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    mListenerSocketCreatedOutside = true;
    mStreamedTransport = (pDataSocket->GetTransportType() == SOCKET_TCP);

    Init(pDataSocket, 0, pRtpActivated);

    mGAPIUsed = false;
}

MediaSourceNet::MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType,  bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    if ((pPortNumber == 0) || (pPortNumber > 65535))
        LOG(LOG_ERROR, "Given port number is invalid");

    mListenerSocketCreatedOutside = false;
    mStreamedTransport = (pTransportType == SOCKET_TCP);

    Init(Socket::CreateServerSocket(SOCKET_IPv6, pTransportType, pPortNumber), 0, pRtpActivated);

    mGAPIUsed = false;

    LOG(LOG_VERBOSE, "Local media listener at: <%d>%s", pPortNumber, mRtpActivated ? "(RTP)" : "");
}

MediaSourceNet::MediaSourceNet(string pLocalName, Requirements *pTransportRequirements, bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    mListenerSocketCreatedOutside = false;
    mGAPIBinding = NULL;

    unsigned int tLocalPort = 0;
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pTransportRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        if (tLocalPort < 65536)
            tLocalPort = tRequPort->getPort();
        else
            LOG(LOG_ERROR, "Given local port number is invalid");
    }else
    {
        LOG(LOG_WARN, "No local port given within requirement set, falling back to port 0");
        tLocalPort = 0;
    }
    LOG(LOG_VERBOSE, "Local GAPI port determined as %u", tLocalPort);

    // finally offer server
    Name tName(pLocalName);
    mGAPIBinding = GAPI.bind(&tName, pTransportRequirements); //new Socket(IS_IPV6_ADDRESS(pTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, pSocketType);
    if (mGAPIBinding == NULL)
        LOG(LOG_ERROR, "Invalid GAPI setup interface");
    mGAPIDataSocket = mGAPIBinding->readConnection();
    if (mGAPIDataSocket == NULL)
        LOG(LOG_ERROR, "Invalid GAPI association");

    mStreamedTransport = pTransportRequirements->contains(pTransportRequirements->contains(RequirementTransmitStream::type()));

    Init(NULL, tLocalPort, pRtpActivated);

    mGAPIUsed = true;
}

MediaSourceNet::~MediaSourceNet()
{
	LOG(LOG_VERBOSE, "Going to destroy network based media source");

	LOG(LOG_VERBOSE, "..stopping grabbing");
    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    // check every 100 ms if listener thread is still running
	LOG(LOG_VERBOSE, "..wait for end of listener thread");
    while(mListenerRunning)
    	Suspend(100000);
	LOG(LOG_VERBOSE, "..end of listener thread reached");

    free(mPacketBuffer);

    if(mGAPIUsed)
    {
        if (mGAPIBinding != NULL)
        {
        	LOG(LOG_VERBOSE, "..destroying GAPI bind object");
        	delete mGAPIBinding; //HINT: this stops all listeners automatically
        }
    }else
    {
        if (!mListenerSocketCreatedOutside)
        {
        	LOG(LOG_VERBOSE, "..destroying socket object");
            delete mDataSocket;
        }
    }
	LOG(LOG_VERBOSE, "Destroyed");
}

bool MediaSourceNet::DoReceiveFragment(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize)
{
    bool tResult = false;

    if(mGAPIUsed)
    {
        if (mGAPIDataSocket != NULL)
        {
            mGAPIDataSocket->read(pData, pSize);
            if (!mGAPIDataSocket->isClosed())
            {// success
                tResult = true;
                pSourceHost = mGAPIDataSocket->getRemoteName()->toString();
                pSourcePort = 1;
            }
        }else
            LOG(LOG_ERROR, "Invalid GAPI association");
    }else
    {
        if (mDataSocket != NULL)
        {
            ssize_t tBufferSize = (ssize_t)pSize;
            tResult =  mDataSocket->Receive(pSourceHost, pSourcePort, (void*)pData, tBufferSize);
            pSize = (int) tBufferSize;
        }else
            LOG(LOG_ERROR, "Invalid socket association");
    }

    return tResult;
}

void* MediaSourceNet::Run(void* pArgs)
{
    string tSourceHost = "";
    unsigned int tSourcePort = 0;
    int tDataSize;

    LOG(LOG_VERBOSE, "Socket-Listener for port %u started, media type is \"%s\"", getListenerPort(), GetMediaTypeStr().c_str());
    if (mGAPIUsed)
    {
        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(GAPI," + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(GAPI," + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }else
    {
        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(NET," + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(NET," + FfmpegId2FfmpegFormat(mStreamCodecId) + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }

    do
    {
        mListenerRunning = true;

        //####################################################################
		// receive packet from network socket
		// ###################################################################
		tDataSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE;
		tSourceHost = "";
		if (!DoReceiveFragment(tSourceHost, tSourcePort, mPacketBuffer, tDataSize))
		{
		    if (mReceiveErrors == MAX_RECEIVE_ERRORS)
		    {
		        LOG(LOG_ERROR, "Maximum number of continuous receive errors(%d) is exceeded, will stop network listener", MAX_RECEIVE_ERRORS);
		        break;
		    }else
		        mReceiveErrors++;
		}else
		    mReceiveErrors = 0;

//		LOG(LOG_ERROR, "Data Size: %d", (int)tDataSize);
//		LOG(LOG_ERROR, "Port: %u", tSourcePort);
//		LOG(LOG_ERROR, "Host: %s", tSourceHost.c_str());

		if ((tDataSize > 0) && (tSourceHost != "") && (tSourcePort != 0))
		{
		    // some news about the peer?
		    if ((mPeerHost != tSourceHost) || (mPeerPort != tSourcePort))
		    {
                if (mGAPIUsed)
                {
                    // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
                    mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements().contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

                    enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements().contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
                    // update category for packet statistics
                    enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(tSourceHost)) ? SOCKET_IPv6 : SOCKET_IPv4;
                    ClassifyStream(GetDataType(), tTransportType, tNetworkType);
                }else
                {
                    mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
                    // update category for packet statistics
                    enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(tSourceHost)) ? SOCKET_IPv6 : SOCKET_IPv4;
                    ClassifyStream(GetDataType(), mDataSocket->GetTransportType(), tNetworkType);
                }
                AssignStreamName(mCurrentDeviceName);
                mPeerHost = tSourceHost;
                mPeerPort = tSourcePort;
		    }

			#ifdef MSN_DEBUG_PACKETS
				LOG(LOG_VERBOSE, "Received packet number %5d at %p with size: %5d from %s:%u", (int)++mPacketNumber, mPacketBuffer, (int)tDataSize, tSourceHost.c_str(), tSourcePort);
			#endif

            // for TCP-like transport we have to use a special fragment header!
            if (mStreamedTransport)
            {
                TCPFragmentHeader *tHeader;
                char *tData = mPacketBuffer;
                char *tDataEnd = mPacketBuffer + tDataSize;

                while(tDataSize > 0)
                {
                    if (tData > tDataEnd)
                    {
                        LOG(LOG_ERROR, "Have found an unplausible data position at %p while the data ends at %p", tData, tDataEnd);
                        break;
                    }
                    #ifdef MSN_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "Extracting a fragment from TCP stream");
                    #endif

                    tHeader = (TCPFragmentHeader*)tData;

                    if (tData + tHeader->FragmentSize > tDataEnd)
                    {
                        LOG(LOG_ERROR, "Have found an unplausible fragment size of %u bytes which is beyond the reported packet reception size", tHeader->FragmentSize);
                        break;
                    }
					//TODO: detect packet boaundaries: maybe we get the last part of a former packet and the first part of the next packet -> this results in an error message at the moment, however, we could compensate this by a fragment buffer
					//       -> picture errors occur if the video quality is high enough and causes a high data rate
                    tData += TCP_FRAGMENT_HEADER_SIZE;
                    tDataSize -= TCP_FRAGMENT_HEADER_SIZE;
                    WriteFragment(tData, (int)tHeader->FragmentSize);
                    tData += tHeader->FragmentSize;
                    tDataSize -= tHeader->FragmentSize;
                }
            }else
            {
                WriteFragment(mPacketBuffer, (int)tDataSize);
            }
		}else
		{
			if (tDataSize == 0)
			{
				LOG(LOG_VERBOSE, "Zero byte packet received, media type is \"%s\"", GetMediaTypeStr().c_str());

				// add also a zero byte packet to enable early thread termination
				WriteFragment(mPacketBuffer, 0);
			}else
			{
				LOG(LOG_VERBOSE, "Got faulty packet, media type is \"%s\"", GetMediaTypeStr().c_str());
				tDataSize = -1;
			}
		}
    }while(!mGrabbingStopped);

    mListenerRunning = false;
    LOG(LOG_VERBOSE, "Socket-Listener for port %u stopped, media type is \"%s\"", getListenerPort(), GetMediaTypeStr().c_str());

    return NULL;
}

unsigned int MediaSourceNet::getListenerPort()
{
    return mListenerPort;
}

bool MediaSourceNet::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_VERBOSE, "Trying to open the video source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_VIDEO;

	// start socket listener
    StartThread();

    bool tResult = MediaSourceMem::OpenVideoGrabDevice(pResX, pResY, pFps);

	if (tResult)
	{
        SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(NET)");
        enum TransportType tTransportType;
	    // set category for packet statistics
        if (mGAPIUsed)
            tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements().contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
        else
            tTransportType = mDataSocket->GetTransportType();
        ClassifyStream(DATA_TYPE_VIDEO, tTransportType);
	}

    return tResult;
}

bool MediaSourceNet::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    LOG(LOG_VERBOSE, "Trying to open the audio source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_AUDIO;

	// start socket listener
    StartThread();

    bool tResult = MediaSourceMem::OpenAudioGrabDevice(pSampleRate, pStereo);

	if (tResult)
	{
        SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(NET)");
        enum TransportType tTransportType;
	    // set category for packet statistics
        if (mGAPIUsed)
            tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements().contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
        else
            tTransportType = mDataSocket->GetTransportType();
        ClassifyStream(DATA_TYPE_AUDIO, tTransportType);
	}

    return tResult;
}

void MediaSourceNet::StopGrabbing()
{
    if (mGrabbingStopped)
    {
        LOG(LOG_VERBOSE, "Grabbing is already stopped");
        return;
    }

    // mark as stopped
    MediaSourceMem::StopGrabbing();

    if ((((mOpenInputStream) && (mListenerRunning)) || (mListenerRunning) || ((!mOpenInputStream) && (!mGrabMutex.tryLock(100)))))
    {
        if (mGAPIUsed)
        {
            mGAPIDataSocket->cancel();
        }else
        {
            if (mDataSocket != NULL)
            {
                LOG(LOG_VERBOSE, "Try to do loopback signaling to local IPv%d listener at port %u, transport %d", mDataSocket->GetNetworkType(), 0xFFFF & mDataSocket->GetLocalPort(), mDataSocket->GetTransportType());
                Socket  *tSocket = Socket::CreateClientSocket(mDataSocket->GetNetworkType(), mDataSocket->GetTransportType());
                char    tData[8];
                switch(tSocket->GetNetworkType())
                {
                    case SOCKET_IPv4:
                        LOG(LOG_VERBOSE, "Doing loopback signaling to IPv4 listener to port %u", getListenerPort());
                        if (!tSocket->Send("127.0.0.1", mDataSocket->GetLocalPort(), tData, 0))
                            LOG(LOG_ERROR, "Error when sending data through loopback IPv4-UDP socket");
                        break;
                    case SOCKET_IPv6:
                        LOG(LOG_VERBOSE, "Doing loopback signaling to IPv6 listener to port %u", getListenerPort());
                        if (!tSocket->Send("::1", mDataSocket->GetLocalPort(), tData, 0))
                            LOG(LOG_ERROR, "Error when sending data through loopback IPv6-UDP socket");
                        break;
                    default:
                        LOG(LOG_ERROR, "Unknown network type");
                        break;
                }
                delete tSocket;
            }
        }
    }else
    {
        LOG(LOG_VERBOSE, "Loopback signaling skipped, state of opening input stream: %d, state of listener thread: %d, grabbing stopped: %d", mOpenInputStream, mListenerRunning, mGrabbingStopped);
        mGrabMutex.unlock();
    }
}

string MediaSourceNet::GetCurrentDevicePeerName()
{
    if (mGAPIUsed)
    {
        if (mGAPIDataSocket != NULL)
            return mGAPIDataSocket->getRemoteName()->toString();
        else
            return NULL;
    }else
    {
        if (mDataSocket != NULL)
            return MediaSinkNet::CreateId(mDataSocket->GetPeerHost(), toString(mDataSocket->GetPeerPort()));
        else
            return "";
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
