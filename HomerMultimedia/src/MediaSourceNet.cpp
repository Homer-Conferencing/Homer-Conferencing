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
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

void MediaSourceNet::Init(Socket *pDataSocket, bool pRtpActivated)
{
    if (pDataSocket == NULL)
        LOG(LOG_ERROR, "Given socket is invalid");

    mPacketNumber = 0;
    mReceiveErrors = 0;
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
            mPacketStatAdditionalFragmentSize = TCP_FRAGMENT_HEADER_SIZE;
        LOG(LOG_VERBOSE, "Listen for media packets at port %u, transport %d, IP version %d", mDataSocket->GetLocalPort(), mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
        mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
    }
    AssignStreamName(mCurrentDeviceName);
}

MediaSourceNet::MediaSourceNet(Socket *pDataSocket, bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    mListenerSocketOutside = true;

    Init(pDataSocket, pRtpActivated);
}

MediaSourceNet::MediaSourceNet(unsigned int pPortNumber, enum TransportType pTransportType,  bool pRtpActivated):
    MediaSourceMem(pRtpActivated)
{
    if ((pPortNumber == 0) || (pPortNumber > 65535))
        LOG(LOG_ERROR, "Given port number is invalid");

    mListenerSocketOutside = false;

    Init(Socket::CreateServerSocket(SOCKET_IPv6, pTransportType, pPortNumber), pRtpActivated);
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
		tDataSize = MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE;
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
		    mCurrentDeviceName = "NET-IN: " + MediaSinkNet::CreateId(mDataSocket->GetLocalHost(), toString(mDataSocket->GetLocalPort()), mDataSocket->GetTransportType(), mRtpActivated);
            AssignStreamName(mCurrentDeviceName);

            // update category for packet statistics
            enum NetworkType tNetworkType = (IS_IPV6_ADDRESS(tSourceHost)) ? SOCKET_IPv6 : SOCKET_IPv4;
            ClassifyStream(GetDataType(), mDataSocket->GetTransportType(), tNetworkType);

			#ifdef MSN_DEBUG_PACKETS
				LOG(LOG_VERBOSE, "Received packet number %5d at %p with size: %5d from %s:%u", (int)++mPacketNumber, mPacketBuffer, (int)tDataSize, tSourceHost.c_str(), tSourcePort);
			#endif

            // for TCP we have to use a special fragment header!
            if (mDataSocket->GetTransportType() == SOCKET_TCP)
            {
                TCPFragmentHeader *tHeader;
                char *tData = mPacketBuffer;

                while(tDataSize > 0)
                {
                    tHeader = (TCPFragmentHeader*)tData;
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
    if (mDataSocket != NULL)
        return mDataSocket->GetLocalPort();
    else
        return 0;
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
	    ClassifyStream(DATA_TYPE_VIDEO, mDataSocket->GetTransportType());
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
	    ClassifyStream(DATA_TYPE_AUDIO, mDataSocket->GetTransportType());
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
    }else
    {
        LOG(LOG_VERBOSE, "Loopback signaling skipped, state of opening input stream: %d, state of listener thread: %d, grabbing stopped: %d", mOpenInputStream, mListenerRunning, mGrabbingStopped);
        mGrabMutex.unlock();
    }
}

string MediaSourceNet::GetCurrentDevicePeerName()
{
	return MediaSinkNet::CreateId(mDataSocket->GetPeerHost(), toString(mDataSocket->GetPeerPort()));
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
