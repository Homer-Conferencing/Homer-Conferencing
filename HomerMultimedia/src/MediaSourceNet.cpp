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

// maximum number of acceptable continuous receive errors
#define MEDIA_SOURCE_NET_MAX_RECEIVE_ERRORS                           3

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class NetworkListener :
    public Thread
{
public:
    NetworkListener(MediaSourceNet *pMediaSourceNet, Socket *pDataSocket, bool pRtpActivated = true);
    NetworkListener(MediaSourceNet *pMediaSourceNet, unsigned int pPortNumber, enum TransportType pTransportType, bool pRtpActivated = true);
    NetworkListener(MediaSourceNet *pMediaSourceNet, std::string pLocalName, Requirements *pTransportRequirements, bool pRtpActivated = true);

    virtual ~NetworkListener();

    void StartListener();
    void StopListener();

    unsigned int GetListenerPort();
    enum TransportType GetTransportType();
    enum NetworkType GetNetworkType();
    std::string GetListenerName();
    std::string GetCurrentDevicePeerName();

private:
    friend class MediaSourceNet;

    void Init(Socket *pDataSocket, unsigned int pLocalPort, bool pRtpActivated = true);
    bool ReceivePacket(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize);

    /* network listener */
    virtual void* Run(void* pArgs = NULL);

    MediaSourceNet      *mMediaSourceNet;
    bool                mRtpActivated;

    /* general transport */
    int                 mReceiveErrors;
    int                 mPacketNumber;
    bool                mListenerNeeded;
    bool				mListenerStopped;
    bool                mListenerSocketCreatedOutside;
    bool                mStreamedTransport;
    /* Berkeley sockets based transport */
    std::string         mPeerHost;
    unsigned int        mPeerPort;
    Socket              *mDataSocket;
    unsigned int        mListenerPort;
    /* NAPI based transport */
    IConnection         *mNAPIDataSocket;
    IBinding            *mNAPIBinding;
    bool                mNAPIUsed;
};

///////////////////////////////////////////////////////////////////////////////

void NetworkListener::Init(Socket *pDataSocket, unsigned int pLocalPort, bool pRtpActivated)
{
    mRtpActivated = pRtpActivated;
    mPacketNumber = 0;
    mPeerHost = "";
    mPeerPort = 0;
    mReceiveErrors = 0;
    mListenerPort = pLocalPort;
    mRtpActivated = pRtpActivated;

    mDataSocket = pDataSocket;

    if(mDataSocket != NULL)
    {
        // check the UDP-Lite and the RTP header
        if (mDataSocket->GetTransportType() == SOCKET_UDP_LITE)
            mDataSocket->UDPLiteSetCheckLength(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);

        if (mDataSocket->GetTransportType() == SOCKET_TCP)
        {
            mStreamedTransport = true;
            mMediaSourceNet->mPacketStatAdditionalFragmentSize = TCP_FRAGMENT_HEADER_SIZE;
        }
        LOG(LOG_VERBOSE, "Listen for media packets at port %u, transport %s, IP version %d", mDataSocket->GetLocalPort(), Socket::TransportType2String(mDataSocket->GetTransportType()).c_str(), mDataSocket->GetNetworkType());
        mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

        mListenerPort = mDataSocket->GetLocalPort();
    }else if (mNAPIDataSocket != NULL)
    {
        if (mNAPIDataSocket->isClosed())
        {
            LOG(LOG_WARN, "NAPI association is closed, reseting local port to 0");
            mListenerPort = 0;
        }

        LOG(LOG_VERBOSE, "Listen for media packets at NAPI interface: %s, local port specified as: %d, TCP-like transport: %d, requirements: %s", mNAPIDataSocket->getName()->toString().c_str(), GetListenerPort(), mStreamedTransport, (mNAPIDataSocket->getRequirements() != NULL) ? mNAPIDataSocket->getRequirements()->getDescription().c_str() : "");
        // assume Berkeley-Socket implementation behind NAPI interface => therefore we can easily conclude on "UDP/TCP/UDP-Lite"
        mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + mNAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDP-Lite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";
    }else
    {
        mListenerPort = 0;
        LOG(LOG_ERROR, "No valid transport socket");
    }

    mMediaSourceNet->AssignStreamName(mMediaSourceNet->mCurrentDeviceName);
}

NetworkListener::NetworkListener(MediaSourceNet *pMediaSourceNet, Socket *pDataSocket, bool pRtpActivated)
{
    mMediaSourceNet = pMediaSourceNet;
    LOG(LOG_VERBOSE, "Created with pre-defined socket object");
    mListenerSocketCreatedOutside = true;
    mStreamedTransport = (pDataSocket->GetTransportType() == SOCKET_TCP);

    mNAPIUsed = false;

    Init(pDataSocket, 0, pRtpActivated);
}

NetworkListener::NetworkListener(MediaSourceNet *pMediaSourceNet, unsigned int pPortNumber, enum TransportType pTransportType,  bool pRtpActivated)
{
    mMediaSourceNet = pMediaSourceNet;
    LOG(LOG_VERBOSE, "Created, have to create socket");
    if ((pPortNumber == 0) || (pPortNumber > 65535))
        LOG(LOG_ERROR, "Given port number is invalid");

    mListenerSocketCreatedOutside = false;
    mStreamedTransport = (pTransportType == SOCKET_TCP);

    mNAPIUsed = false;

    LOG(LOG_VERBOSE, "Local media listener at: <%d>%s", pPortNumber, mRtpActivated ? "(RTP)" : "");

    Init(Socket::CreateServerSocket(SOCKET_IPv6, pTransportType, pPortNumber), 0, pRtpActivated);
}

NetworkListener::NetworkListener(MediaSourceNet *pMediaSourceNet, string pLocalName, Requirements *pTransportRequirements, bool pRtpActivated)
{
    mMediaSourceNet = pMediaSourceNet;
    LOG(LOG_VERBOSE, "Created, using NAPI");
    mListenerSocketCreatedOutside = false;
    mNAPIBinding = NULL;

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
    LOG(LOG_VERBOSE, "Local NAPI port determined as %u", tLocalPort);

    // finally offer server
    Name tName(pLocalName);
    mNAPIBinding = NAPI.bind(&tName, pTransportRequirements); //new Socket(IS_IPV6_ADDRESS(pTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, pSocketType);
    if (mNAPIBinding == NULL)
        LOG(LOG_ERROR, "Invalid NAPI setup interface");
    LOG(LOG_VERBOSE, "NAP binding created with requirements: %s", (mNAPIBinding->getRequirements() != NULL) ? mNAPIBinding->getRequirements()->getDescription().c_str() : "");
    mNAPIDataSocket = mNAPIBinding->readConnection();
    if (mNAPIDataSocket == NULL)
        LOG(LOG_ERROR, "Invalid NAPI association");

    mStreamedTransport = pTransportRequirements->contains(pTransportRequirements->contains(RequirementTransmitStream::type()));

    mNAPIUsed = true;

    Init(NULL, tLocalPort, pRtpActivated);
}

NetworkListener::~NetworkListener()
{
    if(mNAPIUsed)
    {
        if (mNAPIBinding != NULL)
        {
            LOG(LOG_VERBOSE, "..destroying NAPI bind object");
            delete mNAPIBinding; //HINT: this stops all listeners automatically
        }
    }else
    {
        if (!mListenerSocketCreatedOutside)
        {
            LOG(LOG_VERBOSE, "..destroying socket object");
            delete mDataSocket;
        }
    }
}

unsigned int NetworkListener::GetListenerPort()
{
    return mListenerPort;
}

void NetworkListener::StartListener()
{
    LOG(LOG_VERBOSE, "Starting network listener for local port %u", GetListenerPort());

    int tRemainingFragments = mMediaSourceNet->mDecoderFragmentFifo->GetUsage();
    if (tRemainingFragments)
    {
        LOG(LOG_WARN, "Detected %d pending %s fragments, dropping these fragments..", tRemainingFragments, mMediaSourceNet->GetMediaTypeStr().c_str());
        mMediaSourceNet->mDecoderFragmentFifo->ClearFifo();
    }

    if (!IsRunning())
    {
        // start decoder main loop
        StartThread();

        int tLoops = 0;

        // wait until thread is running
        while ((!IsRunning() /* wait until thread is started */) || (!mListenerNeeded /* wait until thread has finished the init. process */))
        {
            if (tLoops % 10 == 0)
                LOG(LOG_VERBOSE, "Waiting for start of %s network listener thread, loop count: %d", mMediaSourceNet->GetMediaTypeStr().c_str(), ++tLoops);
            Thread::Suspend(25 * 1000);
        }
    }
}

void NetworkListener::StopListener()
{
    int tSignalingRound = 0;

    LOG(LOG_VERBOSE, "Stopping %s network listener", mMediaSourceNet->GetMediaTypeStr().c_str());

	// tell network listener thread: it isn't needed anymore
	mListenerNeeded = false;

	if (mNAPIUsed)
	{
		mNAPIDataSocket->cancel();
	}else
	{
		if (mDataSocket != NULL)
			mDataSocket->StopReceiving();
	}

	// wait for termination of decoder thread
	do
	{
		if(tSignalingRound > 0)
			LOG(LOG_WARN, "Signaling attempt %d to stop %s network listener", tSignalingRound, mMediaSourceNet->GetMediaTypeStr().c_str());
		tSignalingRound++;

		Suspend(25 * 1000);
	}while(IsRunning());
    
    LOG(LOG_VERBOSE, "%s network listener stopped", mMediaSourceNet->GetMediaTypeStr().c_str());
}

enum TransportType NetworkListener::GetTransportType()
{
    enum TransportType tTransportType;

    if (mNAPIUsed)
    {
        tTransportType = (mStreamedTransport ? SOCKET_TCP : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
    }else
    {
        tTransportType = mDataSocket->GetTransportType();
    }

    return tTransportType;
}

enum NetworkType NetworkListener::GetNetworkType()
{
    enum NetworkType tNetworkType;

    if (mNAPIUsed)
    {
        tNetworkType = (IS_IPV6_ADDRESS(mNAPIDataSocket->getName()->toString()) ? SOCKET_IPv6 : SOCKET_IPv4); //TODO: this is not correct for unknown NAPI implementations which use some different type of network
    }else
    {
        tNetworkType = mDataSocket->GetNetworkType();
    }

    return tNetworkType;
}

bool NetworkListener::ReceivePacket(std::string &pSourceHost, unsigned int &pSourcePort, char* pData, int &pSize)
{
    bool tResult = false;

    if(mNAPIUsed)
    {
        if (mNAPIDataSocket != NULL)
        {
            mNAPIDataSocket->read(pData, pSize);
            if (!mNAPIDataSocket->isClosed())
            {// success
                tResult = true;
                string tRemoteName = mNAPIDataSocket->getRemoteName()->toString();
                int tPos = tRemoteName.find('<');
                if (tPos != (int)string::npos)
                    tRemoteName = tRemoteName.substr(0, tPos);
                pSourceHost = tRemoteName;
                pSourcePort = 1;
            }
        }else
            LOG(LOG_ERROR, "Invalid NAPI association");
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

string NetworkListener::GetListenerName()
{
    string tResult = "";

    if (mNAPIUsed)
    {
        // assume Berkeley-Socket implementation behind NAPI interface => therefore we can easily conclude on "UDP/TCP/UDP-Lite"
        tResult = "NET-IN: " + mNAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDP-Lite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

    }else
    {
        tResult = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
    }

    return tResult;

}

string NetworkListener::GetCurrentDevicePeerName()
{
    if (mNAPIUsed)
    {
        if (mNAPIDataSocket != NULL)
            return mNAPIDataSocket->getRemoteName()->toString();
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

void* NetworkListener::Run(void* pArgs)
{
    char                *tPacketBuffer = NULL;
    string              tSourceHost = "";
    unsigned int        tSourcePort = 0;
    int                 tDataSize;

    LOG(LOG_VERBOSE, "%s Socket-Listener for port %u started", mMediaSourceNet->GetMediaTypeStr().c_str(), GetListenerPort());
    mListenerStopped = false;

    tPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    if (mNAPIUsed)
    {
        switch(mMediaSourceNet->mMediaType)
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(NAPI," + mMediaSourceNet->GetFormatName(mMediaSourceNet->GetCodecID()) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(NAPI," + mMediaSourceNet->GetFormatName(mMediaSourceNet->GetCodecID()) + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }else
    {
        switch(mMediaSourceNet->mMediaType)
        {
            case MEDIA_VIDEO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(NET," + mMediaSourceNet->GetFormatName(mMediaSourceNet->GetCodecID()) + ")");
                break;
            case MEDIA_AUDIO:
                SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(NET," + mMediaSourceNet->GetFormatName(mMediaSourceNet->GetCodecID()) + ")");
                break;
            default:
                LOG(LOG_ERROR, "Unknown media type");
                break;
        }
    }

    if (mNAPIUsed)
    {
        // assume Berkeley-Socket implementation behind NAPI interface => therefore we can easily conclude on "UDP/TCP/UDP-Lite"
        mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + mNAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDP-Lite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

        enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
        // update category for packet statistics
        enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(mNAPIDataSocket->getName()->toString())) ? SOCKET_IPv6 : SOCKET_IPv4;
        mMediaSourceNet->ClassifyStream(mMediaSourceNet->GetDataType(), tTransportType, tNetworkType);
    }else
    {
        mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
        // update category for packet statistics
        mMediaSourceNet->ClassifyStream(mMediaSourceNet->GetDataType(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
    }
    mMediaSourceNet->AssignStreamName(mMediaSourceNet->mCurrentDeviceName);

    // set marker to "active"
    mListenerNeeded = true;

    while ((mListenerNeeded) && (!mMediaSourceNet->mGrabbingStopped))
    {
        //####################################################################
        // receive packet from network socket
        // ###################################################################
        tDataSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE;
        tSourceHost = "";
        if (!ReceivePacket(tSourceHost, tSourcePort, tPacketBuffer, tDataSize))
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

        // stop loop if listener isn't needed anymore
        if (!mListenerNeeded)
        {
        	LOG(LOG_WARN, "Leaving %s network listener immediately", mMediaSourceNet->GetMediaTypeStr().c_str());
        	break;
        }
//      LOG(LOG_ERROR, "Data Size: %d", (int)tDataSize);
//      LOG(LOG_ERROR, "Port: %u", tSourcePort);
//      LOG(LOG_ERROR, "Host: %s", tSourceHost.c_str());

        if ((tDataSize > 0) && (tSourceHost != "") && (tSourcePort != 0))
        {
            // some news about the peer?
            if ((mPeerHost != tSourceHost) || (mPeerPort != tSourcePort))
            {
                if (mNAPIUsed)
                {
                    // assume Berkeley-Socket implementation behind NAPI interface => therefore we can easily conclude on "UDP/TCP/UDP-Lite"
                    mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + mNAPIDataSocket->getName()->toString() + "(" + (mStreamedTransport ? "TCP" : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? "UDP-Lite" : "UDP")) + (mRtpActivated ? "/RTP" : "") + ")";

                    enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (mNAPIDataSocket->getRequirements()->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
                    // update category for packet statistics
                    enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(mNAPIDataSocket->getName()->toString())) ? SOCKET_IPv6 : SOCKET_IPv4;
                    mMediaSourceNet->ClassifyStream(mMediaSourceNet->GetDataType(), tTransportType, tNetworkType);
                }else
                {
                    mMediaSourceNet->mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);

                    // update category for packet statistics
                    mMediaSourceNet->ClassifyStream(mMediaSourceNet->GetDataType(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
                }
                LOG(LOG_VERBOSE, "Setting device name to %s", mMediaSourceNet->mCurrentDeviceName.c_str());
                mPeerHost = tSourceHost;
                mPeerPort = tSourcePort;
            }

            #ifdef MSN_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Received packet number %5d at %p with size: %5d from %s:%u", (int)++mPacketNumber, tPacketBuffer, (int)tDataSize, tSourceHost.c_str(), tSourcePort);
            #endif

            // for TCP-like transport we have to use a special fragment header!
            if (mStreamedTransport)
            {
                TCPFragmentHeader *tHeader;
                char *tData = tPacketBuffer;
                char *tDataEnd = tPacketBuffer + tDataSize;

                while(tDataSize > 0)
                {
                    if (tData > tDataEnd)
                    {
                        LOG(LOG_ERROR, "Have found an invalid data position at %p while the data ends at %p", tData, tDataEnd);
                        break;
                    }
                    #ifdef MSN_DEBUG_PACKETS
                        LOG(LOG_VERBOSE, "Extracting a fragment from TCP stream");
                    #endif

                    tHeader = (TCPFragmentHeader*)tData;

                    if (tData + tHeader->FragmentSize > tDataEnd)
                    {
                        LOG(LOG_ERROR, "Have found an invalid fragment size of %u bytes which is beyond the reported packet reception size", tHeader->FragmentSize);
                        break;
                    }
                    //TODO: detect packet boundaries: maybe we get the last part of a former packet and the first part of the next packet -> this results in an error message at the moment, however, we could compensate this by a fragment buffer
                    //       -> picture errors occur if the video quality is high enough and causes a high data rate
                    tData += TCP_FRAGMENT_HEADER_SIZE;
                    tDataSize -= TCP_FRAGMENT_HEADER_SIZE;
                    mMediaSourceNet->WriteFragment(tData, (int)tHeader->FragmentSize);
                    tData += tHeader->FragmentSize;
                    tDataSize -= tHeader->FragmentSize;
                }
            }else
            {
                mMediaSourceNet->WriteFragment(tPacketBuffer, (int)tDataSize);
            }
        }else
        {
            if (tDataSize == 0)
            {
                LOG(LOG_VERBOSE, "Zero byte %s packet received", mMediaSourceNet->GetMediaTypeStr().c_str());

                // add also a zero byte packet to enable early thread termination
                mMediaSourceNet->WriteFragment(tPacketBuffer, 0);
            }else
            {
                LOG(LOG_VERBOSE, "Got faulty %s packet", mMediaSourceNet->GetMediaTypeStr().c_str());
                tDataSize = -1;
            }
        }
    }

    LOG(LOG_VERBOSE, "%s Socket-Listener for port %u finished", mMediaSourceNet->GetMediaTypeStr().c_str(), GetListenerPort());

    free(tPacketBuffer);
    mListenerStopped = true;

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void MediaSourceNet::Init(bool pRtpActivated)
{
    mSourceType = SOURCE_NETWORK;
    mOpenInputStream = false;
    mRtpActivated = pRtpActivated;

    mSourceCodecId = CODEC_ID_NONE;
}

MediaSourceNet::MediaSourceNet(Socket *pDataSocket, bool pRtpActivated):
    MediaSourceMem("NET-IN:", pRtpActivated)
{
	LOG(LOG_VERBOSE, "Created with pre-defined socket object");

    mNetworkListener = new NetworkListener(this, pDataSocket, pRtpActivated);

    Init(pRtpActivated);
}

MediaSourceNet::MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType,  bool pRtpActivated):
    MediaSourceMem("NET-IN:", pRtpActivated)
{
	LOG(LOG_VERBOSE, "Created, have to create socket");
    if ((pPortNumber == 0) || (pPortNumber > 65535))
        LOG(LOG_ERROR, "Given port number is invalid");

    LOG(LOG_VERBOSE, "Local media listener at: <%d>%s", pPortNumber, mRtpActivated ? "(RTP)" : "");

    mNetworkListener = new NetworkListener(this, pPortNumber, pTransportType, pRtpActivated);

    Init(pRtpActivated);
}

MediaSourceNet::MediaSourceNet(string pLocalName, Requirements *pTransportRequirements, bool pRtpActivated):
    MediaSourceMem("NET-IN:", pRtpActivated)
{
	LOG(LOG_VERBOSE, "Created via NAPI, local name: &s, requirements: %s", pLocalName.c_str(), (pTransportRequirements != NULL) ? pTransportRequirements->getDescription().c_str() : "");

    mNetworkListener = new NetworkListener(this, pLocalName, pTransportRequirements, pRtpActivated);

    Init(pRtpActivated);
}

MediaSourceNet::~MediaSourceNet()
{
	LOG(LOG_VERBOSE, "Going to destroy network based media source");

    LOG(LOG_VERBOSE, "..stopping grabbing");
    StopGrabbing();

    LOG(LOG_VERBOSE, "..stopping network listener");
    mNetworkListener->StopListener();

    LOG(LOG_VERBOSE, "..closing media source");
    if (mMediaSourceOpened)
        CloseGrabDevice();

    delete mNetworkListener;

    LOG(LOG_VERBOSE, "Destroyed");
}

string MediaSourceNet::GetCurrentDevicePeerName()
{
    if (mNetworkListener != NULL)
        return mNetworkListener->GetCurrentDevicePeerName();
    else
        return "";
}

unsigned int MediaSourceNet::GetListenerPort()
{
    if (mNetworkListener != NULL)
        return mNetworkListener->GetListenerPort();
    else
        return 0;
}

bool MediaSourceNet::OpenVideoGrabDevice(int pResX, int pResY, float pFps)
{
    LOG(LOG_VERBOSE, "Trying to open the video source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_VIDEO;

	// start socket listener
	mNetworkListener->StartListener();

    bool tResult = MediaSourceMem::OpenVideoGrabDevice(pResX, pResY, pFps);

	if (tResult)
	{
        mCurrentDeviceName = mNetworkListener->GetListenerName();
        SVC_PROCESS_STATISTIC.AssignThreadName("Video-Grabber(NET)");
        ClassifyStream(DATA_TYPE_VIDEO, mNetworkListener->GetTransportType(), mNetworkListener->GetNetworkType());
	}

    return tResult;
}

bool MediaSourceNet::OpenAudioGrabDevice(int pSampleRate, int pChannels)
{
    LOG(LOG_VERBOSE, "Trying to open the audio source");

    // setting this explicitly, otherwise the Run-method won't assign the thread name correctly
	mMediaType = MEDIA_AUDIO;

	// start socket listener
	mNetworkListener->StartListener();

    bool tResult = MediaSourceMem::OpenAudioGrabDevice(pSampleRate, pChannels);

	if (tResult)
	{
        mCurrentDeviceName = mNetworkListener->GetListenerName();
        SVC_PROCESS_STATISTIC.AssignThreadName("Audio-Grabber(NET)");
        ClassifyStream(DATA_TYPE_AUDIO, mNetworkListener->GetTransportType(), mNetworkListener->GetNetworkType());
	}

    return tResult;
}

bool MediaSourceNet::CloseGrabDevice()
{
    bool tResult = false;

    LOG(LOG_VERBOSE, "Going to close %s stream from network", GetMediaTypeStr().c_str());

    tResult = MediaSourceMem::CloseGrabDevice();

    LOG(LOG_VERBOSE, "...%s stream from network closed", GetMediaTypeStr().c_str());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
