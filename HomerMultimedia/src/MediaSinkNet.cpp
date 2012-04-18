/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a network based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2009-12-27
 */

#include <Header_Ffmpeg.h>
#include <MediaSinkNet.h>
#include <MediaSourceMem.h>
#include <MediaSourceNet.h>
#include <PacketStatistic.h>
#include <RTP.h>
#include <HBSocket.h>
#include <HBTime.h>
#include <Logger.h>
#include <Berkeley/SocketName.h>
#include <RequirementTargetPort.h>

#include <string>

#include <Requirements.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////
void MediaSinkNet::BasicInit(string pTargetHost, unsigned int pTargetPort, enum MediaSinkType pType, bool pRtpActivated)
{
	mStreamFragmentCopyBuffer = NULL;
    mGAPIDataSocket = NULL;
    mDataSocket = NULL;
    mCodec = "unknown";
    mStreamerOpened = false;
    mBrokenPipe = false;
    mMaxNetworkPacketSize = 1280;
    mCurrentStream = NULL;
    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
    mRtpActivated = pRtpActivated;
    mWaitUntillFirstKeyFrame = (pType == MEDIA_SINK_VIDEO) ? true : false;
}

MediaSinkNet::MediaSinkNet(string pTarget, Requirements *pTransportRequirements, enum MediaSinkType pType, bool pRtpActivated):
    MediaSink(pType), RTP()
{
    BasicInit(pTarget, 0, pType, pRtpActivated);
    mGAPIUsed = true;

    // get target port
    unsigned int tTargetPort = 0;
    RequirementTargetPort *tRequPort = (RequirementTargetPort*)pTransportRequirements->get(RequirementTargetPort::type());
    if (tRequPort != NULL)
    {
        tTargetPort = tRequPort->getPort();

    }else
    {
        LOG(LOG_WARN, "No target port given within requirement set, falling back to port 0");
        tTargetPort = 0;
    }
    mTargetPort = tTargetPort;

    // get target host
    mTargetHost = pTarget;

    // get transport type
    mStreamedTransport = (pTransportRequirements->contains(RequirementTransmitLossless::type()) || pTransportRequirements->contains(RequirementTransmitStream::type()));
    enum TransportType tTransportType = (mStreamedTransport ? SOCKET_TCP : (pTransportRequirements->contains(RequirementTransmitBitErrors::type()) ? SOCKET_UDP_LITE : SOCKET_UDP));
    if (mStreamedTransport)
    	mStreamFragmentCopyBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    // call GAPI
    if (mTargetHost != "")
    {
        LOG(LOG_VERBOSE, "Remote media sink at: %s<%d>%s", mTargetHost.c_str(), tTargetPort, mRtpActivated ? "(RTP)" : "");

        // finally subscribe to the target server/service
        mGAPIDataSocket = GAPI.connect(new Name(pTarget), pTransportRequirements); //new Socket(IS_IPV6_ADDRESS(pTargetHost) ? SOCKET_IPv6 : SOCKET_IPv4, pSocketType);
    }

    mMediaId = CreateId(mTargetHost, toString(mTargetPort), tTransportType, pRtpActivated);
    AssignStreamName("NET-OUT: " + mMediaId);
}

MediaSinkNet::MediaSinkNet(string pTargetHost, unsigned int pTargetPort, Socket* pSocket, enum MediaSinkType pType, bool pRtpActivated):
    MediaSink(pType), RTP()
{
    BasicInit(pTargetHost, pTargetPort, pType, pRtpActivated);
    mGAPIUsed = false;
    mStreamedTransport = (pSocket->GetTransportType() == SOCKET_TCP);
    if (mStreamedTransport)
    	mStreamFragmentCopyBuffer = (char*)malloc(MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE);

    mDataSocket = pSocket;

    // define QoS settings
    QoSSettings tQoSSettings;
    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO, mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
            tQoSSettings.DataRate = 20;
            tQoSSettings.Delay = 250;
            tQoSSettings.Features = QOS_FEATURE_NONE;
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO, mDataSocket->GetTransportType(), mDataSocket->GetNetworkType());
            tQoSSettings.DataRate = 8;
            tQoSSettings.Delay = 100;
            tQoSSettings.Features = QOS_FEATURE_NONE;
            break;
        default:
            LOG(LOG_ERROR, "Undefined media type");
            break;
    }
    mDataSocket->SetQoS(tQoSSettings);

	LOG(LOG_VERBOSE, "Remote media sink at: %s<%d>%s", pTargetHost.c_str(), pTargetPort, mRtpActivated ? "(RTP)" : "");

    mMediaId = CreateId(pTargetHost, toString(pTargetPort), mDataSocket->GetTransportType(), pRtpActivated);
    AssignStreamName("NET-OUT: " + mMediaId);
}

MediaSinkNet::~MediaSinkNet()
{
    CloseStreamer();
    if(mGAPIUsed)
    {
		if (mGAPIDataSocket != NULL)
		{
			mGAPIDataSocket->cancel();
			delete mGAPIDataSocket;
		}
    }else
    {
    	//HINT: socket object has to be deleted outside
    }
    free(mStreamFragmentCopyBuffer);
}

bool MediaSinkNet::OpenStreamer(AVStream *pStream)
{
    if (mStreamerOpened)
    {
        LOG(LOG_ERROR, "Already opened");
        return false;
    }

    mCodec = pStream->codec->codec->name;
    if (mRtpActivated)
        OpenRtpEncoder(mTargetHost, mTargetPort, pStream);

    mStreamerOpened = true;
    mCurrentStream = pStream;

    return true;
}

bool MediaSinkNet::CloseStreamer()
{
    if (!mStreamerOpened)
        return false;

    if (mRtpActivated)
        CloseRtpEncoder();

    mStreamerOpened = false;

    return true;
}

void MediaSinkNet::ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream, bool pIsKeyFrame)
{
    // check for key frame if we wait for the first key frame
    if (mWaitUntillFirstKeyFrame)
    {
        if (!pIsKeyFrame)
            return;
        else
        {
            LOG(LOG_VERBOSE, "Sending frame as first key frame to network sink");
            mWaitUntillFirstKeyFrame = false;
        }
    }

    // save maximum network packet size to use it later within SendFragment() function
    mMaxNetworkPacketSize = pStream->codec->rtp_payload_size;
    if ((mMaxNetworkPacketSize == 0) && (pStream->codec->codec_id == CODEC_ID_H261))
        mMaxNetworkPacketSize = RTP::GetH261PayloadSizeMax() + RTP_HEADER_SIZE + 4 /* H.261 rtp payload header */;

    //####################################################################
    // send packet(s) with frame data to the correct target host and port
    //####################################################################
    if (mRtpActivated)
    {
        if (!mStreamerOpened)
        {
            if (pStream == NULL)
            {
                LOG(LOG_ERROR, "Tried to process packets while streaming is closed, implicit open impossible");

                return;
            }

            OpenStreamer(pStream);
        }

        // stream changed
        if (mCurrentStream != pStream)
        {
            LOG(LOG_VERBOSE, "Restarting RTP encoder");
            CloseStreamer();
            OpenStreamer(pStream);
        }

        //####################################################################
        // limit the outgoing stream to the defined maximum FPS value
        //####################################################################
        if ((!BelowMaxFps(pStream->nb_frames)) && (!pIsKeyFrame))
        {
			#ifdef MSIN_DEBUG_PACKETS
        		LOG(LOG_VERBOSE, "Max. FPS reached, packet skipped");
			#endif

        	return;
        }

        //####################################################################
        // convert new packet to a list of RTP encapsulated frame data packets
        //####################################################################
//        char buf[256];
//        size_t size = 256;
//        if (pPacketSize < 256)
//            size = pPacketSize;
//        memcpy(buf, pPacketData, size);
//        for (unsigned int i = 0; i < 64; i++)
//            if (pPacketSize >= i)
//                LOG(LOG_VERBOSE, "FRAME data (%2u): %02hx(%3d)", i, pPacketData[i] & 0xFF, pPacketData[i] & 0xFF);

        #ifdef MSIN_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Encapsulating codec packet of size %d at memory position %p", pPacketSize, pPacketData);
        #endif
        bool tRtpCreationSucceed = RtpCreate(pPacketData, pPacketSize);
        #ifdef MSIN_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Creation of RTP packets resulted in a buffer at %p with size %u", pPacketData, pPacketSize);
        #endif

//        for (unsigned int i = 0; i < 64; i++)
//            if (pPacketSize >= i)
//                LOG(LOG_VERBOSE, "stream data (%2u): FRAME %02hx(%3d)  RTP+12 %02hx(%3d)  RTP %02hx(%3d)", i, buf[i] & 0xFF, buf[i] & 0xFF, pPacketData[i + 12] & 0xFF, pPacketData[i + 12] & 0xFF, pPacketData[i] & 0xFF, pPacketData[i] & 0xFF);

        //####################################################################
        // find the RTP packets and send them to the destination
        //####################################################################
        // HINT: a packet from the RTP muxer has the following structure:
        // 0..3     4 byte big endian (network byte order!) header giving
        //          the packet size of the following packet in bytes
        // 4..n     RTP packet data (including parts of the encoded frame)
        //####################################################################
        if ((tRtpCreationSucceed) && (pPacketData != 0) && (pPacketSize > 0))
        {
            char *tRtpPacket = pPacketData + 4;
            uint32_t tRtpPacketSize = 0;
            uint32_t tRemainingRtpDataSize = pPacketSize;
            int tRtpPacketNumber = 0;

            do{
                tRtpPacketSize = ntohl(*(uint32_t*)(tRtpPacket - 4));
                #ifdef MSIN_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "Found RTP packet %d with size of %u bytes", ++tRtpPacketNumber, tRtpPacketSize);
                #endif
                // if there is no packet data we should leave the send loop
                if (tRtpPacketSize == 0)
                    break;

                // send final packet
                SendFragment(tRtpPacket, tRtpPacketSize);

                // go to the next RTP packet
                tRtpPacket = tRtpPacket + (tRtpPacketSize + 4);
                tRemainingRtpDataSize -= (tRtpPacketSize + 4);

                #ifdef MSIN_DEBUG_PACKETS
                    if (tRemainingRtpDataSize > RTP_HEADER_SIZE)
                        LOG(LOG_VERBOSE, "Remaining RTP data: %d bytes at %p(%u) ", tRemainingRtpDataSize, tRtpPacket - 4, tRtpPacket - 4);
                #endif
            }while (tRemainingRtpDataSize > RTP_HEADER_SIZE);

            // free the temporary RTP stream packets buffer
            av_free(pPacketData);
        }
    }else
    {
        // send final packet
        SendFragment(pPacketData, pPacketSize);
    }
}

string MediaSinkNet::CreateId(string pHost, string pPort, enum TransportType pSocketTransportType, bool pRtpActivated)
{
    if (pSocketTransportType == SOCKET_TRANSPORT_TYPE_INVALID)
        return pHost + "<" + toString(pPort) + ">";
    else
        return pHost + "<" + toString(pPort) + ">(" + Socket::TransportType2String(pSocketTransportType) + (pRtpActivated ? "/RTP" : "") + ")";
}

void MediaSinkNet::SendFragment(char* pData, unsigned int pSize)
{
    if ((mTargetHost == "") || (mTargetPort == 0))
    {
        LOG(LOG_ERROR, "Remote network address invalid: %s:%u", mTargetHost.c_str(), mTargetPort);
        return;
    }
    if (mBrokenPipe)
    {
        LOG(LOG_VERBOSE, "Skipped fragment transmission because of broken pipe");
        return;
    }

    #ifdef MSIN_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Sending packet number %6ld at %p with size %4u(incl. %3u bytes header) to %s:%d", ++mPacketNumber, pData, pSize, RTP_HEADER_SIZE, mTargetHost.c_str(), mTargetPort);
//        for (int i = 0; i < 10; i++)
//            LOG(LOG_VERBOSE, "Packet data[%d] = %3d", i, pPacketData[i]);

        // if RTP activated then reparse the current packet and print the content
        if (mRtpActivated)
        {
            char *tFragmentData = pData;
            unsigned int tPacketSize = pSize;
            bool tIsLastFragment = false;
            bool tIsSenderReport = false;
            RtpParse(tFragmentData, tPacketSize, tIsLastFragment, tIsSenderReport, mCurrentStream->codec->codec_id, true);
        }
    #endif

    // HINT: we limit packet size to mMaxNetworkPacketSize if RTP is inactive
    int tFragmentCount = 1;
    if (!mRtpActivated)
    {
        tFragmentCount = (pSize + mMaxNetworkPacketSize -1) / mMaxNetworkPacketSize;
        #ifdef MSIN_DEBUG_PACKETS
            if (tFragmentCount > 1)
                LOG(LOG_WARN, "RTP is inactive and current packet of %d bytes is larger than limit of %d bytes per network packet, will split data into %d packets", pSize, mMaxNetworkPacketSize, tFragmentCount);
        #endif
    }

    unsigned int tFragmentSize = pSize;
    char *tFragmentData = pData;
    while (tFragmentCount)
    {
        if (!mRtpActivated)
            tFragmentSize = (unsigned int)(((int)pSize > mMaxNetworkPacketSize)? mMaxNetworkPacketSize : pSize);

        // for TCP add an additional fragment header in front of the codec data to be able to differentiate the fragments in a received TCP packet at receiver side
        if(mStreamedTransport)
        {
            if (MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE > TCP_FRAGMENT_HEADER_SIZE + tFragmentSize)
            {
                TCPFragmentHeader *tHeader = (TCPFragmentHeader*)mStreamFragmentCopyBuffer;
                memcpy(mStreamFragmentCopyBuffer + TCP_FRAGMENT_HEADER_SIZE, tFragmentData, tFragmentSize);
                tHeader->FragmentSize = tFragmentSize;
                tFragmentSize += TCP_FRAGMENT_HEADER_SIZE;
                tFragmentData = mStreamFragmentCopyBuffer;
            }else
            {
                LOG(LOG_ERROR, "TCP copy buffer is too small for data");
            }
        }

        AnnouncePacket(tFragmentSize);
        DoSendFragment(tFragmentData, tFragmentSize);

        tFragmentData = tFragmentData + tFragmentSize;
        tFragmentCount--;
        if ((tFragmentData > (pData + pSize)) && (tFragmentCount))
        {
            LOG(LOG_ERROR, "Something went wrong, we have too many fragments and would read over the last byte of the fragment buffer");
            return;
        }
    }
}

void MediaSinkNet::DoSendFragment(char* pData, unsigned int pSize)
{
	if(mGAPIUsed)
	{
		if (mGAPIDataSocket != NULL)
		{
			mGAPIDataSocket->write(pData, (int)pSize);
			if (mGAPIDataSocket->isClosed())
			{
				LOG(LOG_ERROR, "Error when sending data through GAPI subscription to %s:%u, will skip further transmissions", mTargetHost.c_str(), mTargetPort);
				mBrokenPipe = true;
			}
		}
	}else
	{
		if (mDataSocket != NULL)
		{
			if (!mDataSocket->Send(mTargetHost, mTargetPort, pData, (ssize_t)pSize))
			{
				LOG(LOG_ERROR, "Error when sending data through %s socket to %s:%u, will skip further transmissions", GetTransportTypeStr().c_str(), mTargetHost.c_str(), mTargetPort);
				mBrokenPipe = true;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
