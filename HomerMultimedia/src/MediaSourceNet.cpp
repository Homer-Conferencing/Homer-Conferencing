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

#define SOCKET_RECEIVE_BUFFER_SIZE                      2 * 1024 * 1024

///////////////////////////////////////////////////////////////////////////////

void MediaSourceNet::Init(Socket *pDataSocket, unsigned int pLocalPort, bool pRtpActivated)
{
    mSourceType = SOURCE_NETWORK;
    mListenerPort = pLocalPort;
    mPacketNumber = 0;
    mReceiveErrors = 0;
    mPeerHost = "";
    mPeerPort = 0;
    mOpenInputStream = false;
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

        LOG(LOG_VERBOSE, "Listen for media packets at GAPI interface: %s, local port specified as: %d, TCP-like transport: %d", mGAPIDataSocket->getName()->toString().c_str(), GetListenerPort(), mStreamedTransport);
        // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
        mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";
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

    LOG(LOG_VERBOSE, "..stopping network listener");
    StopListener();

    LOG(LOG_VERBOSE, "..closing media source");
    if (mMediaSourceOpened)
        CloseGrabDevice();

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
    free(mPacketBuffer);
	LOG(LOG_VERBOSE, "Destroyed");
}

bool MediaSourceNet::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_VERBOSE, "Trying to open the video source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_VIDEO;

	// start socket listener
    StartListener();

    bool tResult = MediaSourceMem::OpenVideoGrabDevice(pResX, pResY, pFps);

	if (tResult)
	{
        SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(NET)");
        enum TransportType tTransportType;
        enum NetworkType tNetworkType;

		// set category for packet statistics
        if (mGAPIUsed)
        {
            // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
            mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

            tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
            tNetworkType = (IS_IPV6_ADDRESS(mGAPIDataSocket->getName()->toString()) ? SOCKET_IPv6 : SOCKET_IPv4); //TODO: this is not correct for unknown GAPI implementations which use some different type of network
        }else
        {
            mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

            tTransportType = mDataSocket->GetTransportType();
            tNetworkType = mDataSocket->GetNetworkType();
        }
        ClassifyStream(DATA_TYPE_VIDEO, tTransportType, tNetworkType);
	}

    return tResult;
}

bool MediaSourceNet::OpenAudioGrabDevice(int pSampleRate, bool pStereo)
{
    LOG(LOG_VERBOSE, "Trying to open the audio source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_AUDIO;

	// start socket listener
    StartListener();

    bool tResult = MediaSourceMem::OpenAudioGrabDevice(pSampleRate, pStereo);

	if (tResult)
	{
        SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(NET)");
        enum TransportType tTransportType;
        enum NetworkType tNetworkType;

	    // set category for packet statistics
        if (mGAPIUsed)
        {
            // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
            mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

            tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
            tNetworkType = IS_IPV6_ADDRESS(mGAPIDataSocket->getName()->toString()) ? SOCKET_IPv6 : SOCKET_IPv4; //TODO: this is not correct for unknown GAPI implementations which use some different type of network
        }else
        {
            mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

            tTransportType = mDataSocket->GetTransportType();
            tNetworkType = mDataSocket->GetNetworkType();
        }
        ClassifyStream(DATA_TYPE_AUDIO, tTransportType, tNetworkType);
	}

    return tResult;
}

bool MediaSourceNet::ReceivePacket(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize)
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
                string tRemoteName = mGAPIDataSocket->getRemoteName()->toString();
                int tPos = tRemoteName.find('<');
                if (tPos != (int)string::npos)
                    tRemoteName = tRemoteName.substr(0, tPos);
                pSourceHost = tRemoteName;
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

void MediaSourceNet::StartListener()
{
    LOG(LOG_VERBOSE, "Starting network listener for local port %u", GetListenerPort());

    mListenerNeeded = true;

    // start socket listener
    StartThread();
}

void MediaSourceNet::StopListener()
{
    int tSignalingRound = 0;

    LOG(LOG_VERBOSE, "Stopping network listener");

    if (mDecoderFifo != NULL)
    {
        // tell transcoder thread it isn't needed anymore
        mListenerNeeded = false;

		if (mGAPIUsed)
		{
			mGAPIDataSocket->cancel();
		}else
		{
			if (mDataSocket != NULL)
				mDataSocket->Close();
		}

		if (!StopThread(3000))
			LOG(LOG_ERROR, "Failed to stop %s network listener", GetMediaTypeStr().c_str());
    }

    LOG(LOG_VERBOSE, "Network listener stopped");
}

void* MediaSourceNet::Run(void* pArgs)
{
    string tSourceHost = "";
    unsigned int tSourcePort = 0;
    int tDataSize;

    LOG(LOG_VERBOSE, "%s Socket-Listener for port %u started", GetMediaTypeStr().c_str(), GetListenerPort());
    if (mGAPIUsed)
    {
        switch(mMediaType)
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(GAPI," + GetFormatName(mStreamCodecId) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(GAPI," + GetFormatName(mStreamCodecId) + ")");
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
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(NET," + GetFormatName(mStreamCodecId) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(NET," + GetFormatName(mStreamCodecId) + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }

    if (mGAPIUsed)
    {
        // assume Berkeley-Socket implementation behind GAPI interface => therefore we can easily conclude on "UDP/TCP/UDPlite"
        mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

        enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
        // update category for packet statistics
        enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(mGAPIDataSocket->getName()->toString())) ? SOCKET_IPv6 : SOCKET_IPv4;
        ClassifyStream(GetDataType(), tTransportType, tNetworkType);
    }else
    {
        mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
        // update category for packet statistics
        ClassifyStream(GetDataType(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
    }
    AssignStreamName(mCurrentDeviceName);

    while ((mListenerNeeded) && (!mGrabbingStopped))
    {
        //####################################################################
		// receive packet from network socket
		// ###################################################################
		tDataSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE;
		tSourceHost = "";
		if (!ReceivePacket(tSourceHost, tSourcePort, mPacketBuffer, tDataSize))
		{
		    if (mReceiveErrors == MEDIA_SOURCE_NET_MAX_RECEIVE_ERRORS)
		    {
		        LOG(LOG_ERROR, "Maximum number of continuous receive errors(%d) is exceeded, will stop network listener", MEDIA_SOURCE_NET_MAX_RECEIVE_ERRORS);
		        mListenerNeeded = false;
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
                    mCurrentDeviceName = "NET-IN: " + mGAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDPlite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

                    enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (mGAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
                    // update category for packet statistics
                    enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(mGAPIDataSocket->getName()->toString())) ? SOCKET_IPv6 : SOCKET_IPv4;
                    ClassifyStream(GetDataType(), tTransportType, tNetworkType);
                }else
                {
                    mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

                    // update category for packet statistics
                    ClassifyStream(GetDataType(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
                }
				LOG(LOG_VERBOSE, "Setting device name to %s", mCurrentDeviceName.c_str());
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
					//TODO: detect packet boundaries: maybe we get the last part of a former packet and the first part of the next packet -> this results in an error message at the moment, however, we could compensate this by a fragment buffer
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
				LOG(LOG_VERBOSE, "Zero byte %s packet received", GetMediaTypeStr().c_str());

				// add also a zero byte packet to enable early thread termination
				WriteFragment(mPacketBuffer, 0);
			}else
			{
				LOG(LOG_VERBOSE, "Got faulty %s packet", GetMediaTypeStr().c_str());
				tDataSize = -1;
			}
		}
    }

    LOG(LOG_VERBOSE, "%s Socket-Listener for port %u finished", GetMediaTypeStr().c_str(), GetListenerPort());

    return NULL;
}

unsigned int MediaSourceNet::GetListenerPort()
{
    return mListenerPort;
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
