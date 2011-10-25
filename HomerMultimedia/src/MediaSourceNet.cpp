/*
 * Name:    MediaSourceNet.C
 * Purpose: Implementation of a ffmpeg based network media source
 * Author:  Thomas Volkert
 * Since:   2008-12-16
 * Version: $Id: MediaSourceNet.cpp,v 1.60 2011/09/07 13:29:45 chaos Exp $
 */

// HINT: for audio streams the RTP support remains unused!

#include <MediaSourceNet.h>
#include <MediaSource.h>
#include <ProcessStatisticService.h>
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

MediaSourceNet::MediaSourceNet(Socket *pDataSocket, bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    if (pDataSocket == NULL)
        LOG(LOG_ERROR, "Given socket is invalid");

    mPacketNumber = 0;
    mReceiveErrors = 0;
    mOpenInputStream = false;
    mListenerRunning = false;
    mListenerSocketOutside = true;
    mRtpActivated = pRtpActivated;
    mPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE);

    mStreamCodecId = CODEC_ID_NONE;

    mDataSocket = pDataSocket;
    // check the UDPLite and the RTP header
    if ((mDataSocket != NULL) && (mDataSocket->GetTransportType() == SOCKET_UDP_LITE))
        mDataSocket->SetUdpLiteChecksumCoverage(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    LOG(LOG_VERBOSE, "Listen for media packets at port %u, transport %d, IP version %d", mDataSocket->getLocalPort(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
    AssignStreamName("NET-IN: <" + toString(mDataSocket->getLocalPort()) + ">");
    mCurrentDeviceName = "NET-IN: <" + toString(mDataSocket->getLocalPort()) + ">";
}

MediaSourceNet::MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType,  bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    if ((pPortNumber == 0) || (pPortNumber > 65535))
        LOG(LOG_ERROR, "Given socket is invalid");

    mPacketNumber = 0;
    mReceiveErrors = 0;
    mOpenInputStream = false;
    mListenerRunning = false;
    mRtpActivated = pRtpActivated;
    mPacketBuffer = (char*)malloc(MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE);

    mStreamCodecId = CODEC_ID_NONE;

    mDataSocket = new Socket(pPortNumber, pTransportType, 0);
    mListenerSocketOutside = false;
    // check the UDPLite and the RTP header
    if ((mDataSocket != NULL) && (mDataSocket->GetTransportType() == SOCKET_UDP_LITE))
        mDataSocket->SetUdpLiteChecksumCoverage(UDP_LITE_HEADER_SIZE + RTP_HEADER_SIZE);
    LOG(LOG_VERBOSE, "Listen for media packets at port %u, transport %d, IP version %d", mDataSocket->getLocalPort(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
    AssignStreamName("NET-IN: <" + toString(mDataSocket->getLocalPort()) + ">");
    mCurrentDeviceName = "NET-IN: <" + toString(mDataSocket->getLocalPort()) + ">";
}

MediaSourceNet::~MediaSourceNet()
{
    StopGrabbing();

    if (mMediaSourceOpened)
        CloseGrabDevice();

    // check every 100 ms if listener thread is still running
    while(mListenerRunning)
    	Suspend(100000);

    free(mPacketBuffer);

    if (!mListenerSocketOutside)
        delete mDataSocket;
}

void* MediaSourceNet::Run(void* pArgs)
{
    string tSourceHost = "";
    unsigned int tSourcePort = 0;
    ssize_t tDataSize;

    LOG(LOG_VERBOSE, "Socket-Listener for port %u started, media type is \"%s\"", getListenerPort(), GetMediaTypeStr().c_str());
    switch(mMediaType)
    {
		case MEDIA_VIDEO:
			SVC_PROCESS_STATISTIC.AssignThreadName("Video-InputListener(NET)");
			break;
		case MEDIA_AUDIO:
			SVC_PROCESS_STATISTIC.AssignThreadName("Audio-InputListener(NET)");
			break;
		default:
			LOG(LOG_ERROR, "Unknown media type");
			break;
    }

    do
    {
        mListenerRunning = true;

        //####################################################################
		// receive packet from network socket
		// ###################################################################
		tDataSize = MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE;
		tSourceHost = "";
		if (!mDataSocket->Receive(tSourceHost, tSourcePort, mPacketBuffer, tDataSize))
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
			mCurrentDeviceName = "NET: " + tSourceHost + "<" + toString(tSourcePort) + ">";

			#ifdef MSN_DEBUG_PACKETS
				LOG(LOG_VERBOSE, "Received packet number %5d at %p with size: %5d from %s:%u", (int)++mPacketNumber, mPacketBuffer, (int)tDataSize, tSourceHost.c_str(), tSourcePort);
			#endif

            AddDataInput(mPacketBuffer, tDataSize);
		}else
		{
			if (tDataSize == 0)
			{
				LOG(LOG_VERBOSE, "Zero byte packet received, media type is \"%s\"", GetMediaTypeStr().c_str());

				// add also a zero byte packet to enable early thread termination
                AddDataInput(mPacketBuffer, 0);
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
    return mDataSocket->getLocalPort();
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

	    // set category for packet statistics
	    ClassifyStream(DATA_TYPE_VIDEO, (enum PacketType)mDataSocket->GetTransportType());
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

	    // set category for packet statistics
	    ClassifyStream(DATA_TYPE_AUDIO, (enum PacketType)mDataSocket->GetTransportType());
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

    if ((mDataSocket != NULL) && (((mOpenInputStream) && (mListenerRunning)) || (mListenerRunning) || ((!mOpenInputStream) && (!mGrabMutex.tryLock(100)))))
    {
        LOG(LOG_VERBOSE, "Try to do loopback signaling to local IPv%d listener at port %u, transport %d", mDataSocket->GetNetworkType(), 0xFFFF & mDataSocket->getLocalPort(), mDataSocket->GetTransportType());
        Socket  *tSocket = new Socket(mDataSocket->GetNetworkType(), mDataSocket->GetTransportType());
        char    tData[8];
        switch(tSocket->GetNetworkType())
        {
            case SOCKET_IPv4:
                LOG(LOG_VERBOSE, "Doing loopback signaling to IPv4 listener to port %u", getListenerPort());
                if (!tSocket->Send("127.0.0.1", mDataSocket->getLocalPort(), tData, 0))
                    LOG(LOG_ERROR, "Error when sending data through loopback IPv4-UDP socket");
                break;
            case SOCKET_IPv6:
                LOG(LOG_VERBOSE, "Doing loopback signaling to IPv6 listener to port %u", getListenerPort());
                if (!tSocket->Send("::1", mDataSocket->getLocalPort(), tData, 0))
                    LOG(LOG_ERROR, "Error when sending data through loopback IPv6-UDP socket");
                break;
            default:
                LOG(LOG_ERROR, "Unknown network type");
                break;
        }
        delete tSocket;
    }else
    {
        LOG(LOG_VERBOSE, "Loopback signaling skipped, state of opening input stream: %d, state of listener thread: %d, grabbing stopped: %d", mOpenInputStream, mListenerRunning, mGrabbingStopped);
        mGrabMutex.unlock();
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
