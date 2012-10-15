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
 * Purpose: Implementation of a memory based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2012-06-07
 */

#include <Header_Ffmpeg.h>
#include <MediaSinkMem.h>
#include <MediaSourceMem.h>
#include <MediaSourceMuxer.h>
#include <MediaFifo.h>
#include <MediaSinkNet.h>
#include <MediaSourceNet.h>
#include <PacketStatistic.h>
#include <RTP.h>
#include <Logger.h>

#include <string>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

#define MEDIA_SINK_MEM_PLAIN_FRAGMENT_BUFFER_SIZE    MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE

///////////////////////////////////////////////////////////////////////////////

MediaSinkMem::MediaSinkMem(string pMediaId, enum MediaSinkType pType, bool pRtpActivated):
    MediaSink(pType), RTP()
{
    mMediaId = pMediaId;
	mIncomingAVStream = NULL;
	mTargetHost = "";
	mTargetPort = 0;
    mRtpStreamOpened = false;
	mIncomingAVStreamCodecContext = NULL;
    mRtpActivated = pRtpActivated;
    mWaitUntillFirstKeyFrame = (pType == MEDIA_SINK_VIDEO) ? true : false;
    if (mRtpActivated)
        mSinkFifo = new MediaFifo(MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE, GetDataTypeStr() + "-MediaSinkMem");
    else
        mSinkFifo = new MediaFifo(MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SINK_MEM_PLAIN_FRAGMENT_BUFFER_SIZE, GetDataTypeStr() + "-MediaSinkMem");
    AssignStreamName("MEM-OUT: " + mMediaId);
    switch(pType)
    {
        case MEDIA_SINK_VIDEO:
            ClassifyStream(DATA_TYPE_VIDEO, SOCKET_RAW);
            break;
        case MEDIA_SINK_AUDIO:
            ClassifyStream(DATA_TYPE_AUDIO, SOCKET_RAW);
            break;
        default:
            break;
    }
}

MediaSinkMem::~MediaSinkMem()
{
    CloseStreamer();
    delete mSinkFifo;
}

///////////////////////////////////////////////////////////////////////////////

void MediaSinkMem::ProcessPacket(char* pPacketData, unsigned int pPacketSize, AVStream *pStream, bool pIsKeyFrame)
{
	bool tResetNeeded = false;

	// check for key frame if we wait for the first key frame
    if (mWaitUntillFirstKeyFrame)
    {
        if (!pIsKeyFrame)
        {
            #ifdef MSIM_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Still waiting for first key frame");
            #endif
            return;
        }else
        {
            LOG(LOG_VERBOSE, "Sending frame as first key frame to network sink");
            mWaitUntillFirstKeyFrame = false;
        }
    }

    if (mRtpActivated)
    {
        //####################################################################
        // check if RTP encoder is valid for the current stream
        //####################################################################
        if ((mIncomingAVStream != NULL) && (mIncomingAVStream != pStream))
        {
            LOG(LOG_VERBOSE, "Incoming AV stream changed from %p to %p (codec %s), resetting RTP streamer..", mIncomingAVStream, pStream, pStream->codec->codec->name);
            tResetNeeded = true;
        }else
        {
            if (mIncomingAVStreamCodecContext != pStream->codec)
            {
                LOG(LOG_WARN, "Incoming AV stream unchanged but stream codec context changed from %p to %p (codec %s), resetting RTP streamer..", mIncomingAVStreamCodecContext, pStream->codec, pStream->codec->codec->name);
                tResetNeeded = true;
            }
        }

        //####################################################################
        // send packet(s) with frame data to the correct target host and port
        //####################################################################
        if (!mRtpStreamOpened)
        {
            if (pStream == NULL)
            {
                LOG(LOG_ERROR, "Tried to process packets while streaming is closed, implicit open impossible");

                return;
            }

            OpenStreamer(pStream);
            tResetNeeded = false;
        }

        // stream changed
        if (tResetNeeded)
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
			#ifdef MSIM_DEBUG_PACKETS
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

        #ifdef MSIM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Encapsulating codec packet of size %d at memory position %p", pPacketSize, pPacketData);
        #endif
        int64_t tTime = Time::GetTimeStamp();
        bool tRtpCreationSucceed = RtpCreate(pPacketData, pPacketSize);
        #ifdef MSIM_DEBUG_TIMING
            int64_t tTime2 = Time::GetTimeStamp();
            LOG(LOG_VERBOSE, "               generating RTP envelope took %ld us", tTime2 - tTime);
        #endif
        #ifdef MSIM_DEBUG_PACKETS
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
            tTime = Time::GetTimeStamp();
            char *tRtpPacket = pPacketData + 4;
            uint32_t tRtpPacketSize = 0;
            uint32_t tRemainingRtpDataSize = pPacketSize;
            int tRtpPacketNumber = 0;

            do{
                tRtpPacketSize = ntohl(*(uint32_t*)(tRtpPacket - 4));
                #ifdef MSIM_DEBUG_PACKETS
                    LOG(LOG_VERBOSE, "Found RTP packet %d with size of %u bytes", ++tRtpPacketNumber, tRtpPacketSize);
                #endif
                // if there is no packet data we should leave the send loop
                if (tRtpPacketSize == 0)
                    break;

                // send final packet
                WriteFragment(tRtpPacket, tRtpPacketSize);

                // go to the next RTP packet
                tRtpPacket = tRtpPacket + (tRtpPacketSize + 4);
                tRemainingRtpDataSize -= (tRtpPacketSize + 4);

                #ifdef MSIM_DEBUG_PACKETS
                    if (tRemainingRtpDataSize > RTP_HEADER_SIZE)
                        LOG(LOG_VERBOSE, "Remaining RTP data: %d bytes at %p(%u) ", tRemainingRtpDataSize, tRtpPacket - 4, tRtpPacket - 4);
                #endif
            }while (tRemainingRtpDataSize > RTP_HEADER_SIZE);
            #ifdef MSIM_DEBUG_TIMING
                tTime2 = Time::GetTimeStamp();
                LOG(LOG_VERBOSE, "                             sending RTP packets to network took %ld us", tTime2 - tTime);
            #endif
        }
    }else
    {
        // send final packet
        WriteFragment(pPacketData, pPacketSize);
    }
}

int MediaSinkMem::GetFragmentBufferCounter()
{
    if (mSinkFifo != NULL)
        return mSinkFifo->GetUsage();
    else
        return 0;
}

int MediaSinkMem::GetFragmentBufferSize()
{
    return MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT;
}

void MediaSinkMem::ReadFragment(char *pData, int &pDataSize)
{
    mSinkFifo->ReadFifo(&pData[0], pDataSize);
    if (pDataSize > 0)
    {
        #ifdef MSIM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Delivered packet at %p with size: %5d", pData, (int)pDataSize);
        #endif
    }

    // is FIFO near overload situation?
    if (mSinkFifo->GetUsage() >= MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT - 4)
    {
        LOG(LOG_WARN, "Decoder FIFO is near overload situation, deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mSinkFifo->ClearFifo();
    }
}

void MediaSinkMem::StopProcessing()
{
    LOG(LOG_VERBOSE, "Going to stop media sink \"%s\"", GetStreamName().c_str());
    char tData[4];
    mSinkFifo->WriteFifo(tData, 0);
    mSinkFifo->WriteFifo(tData, 0);
    LOG(LOG_VERBOSE, "Media sink \"%s\" successfully stopped", GetStreamName().c_str());
}

void MediaSinkMem::WriteFragment(char* pData, unsigned int pSize)
{
    #ifdef MSIM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Storing packet number %6ld at %p with size %4u(%3u header) in memory \"%s\"", ++mPacketNumber, pData, pSize, RTP_HEADER_SIZE, mMediaId.c_str());

        // if RTP activated then reparse the current packet and print the content
        if (mRtpActivated)
        {
            char *tPacketData = pData;
            unsigned int tPacketSize = pSize;
            bool tLastFragment;
            bool tFragmentIsSenderReport;
            RtpParse(tPacketData, tPacketSize, tLastFragment, tFragmentIsSenderReport, mIncomingAVStream->codec->codec_id, true);
        }
    #endif
    AnnouncePacket(pSize);
    if ((int)pSize <= mSinkFifo->GetEntrySize())
    {
        mSinkFifo->WriteFifo(pData, (int)pSize);
    }else
        LOG(LOG_ERROR, "Packet for media sink of %u bytes is too big for FIFO with entries of %d bytes", pSize, mSinkFifo->GetEntrySize());
}

bool MediaSinkMem::OpenStreamer(AVStream *pStream)
{
    if (mRtpStreamOpened)
    {
        LOG(LOG_ERROR, "Already opened");
        return false;
    }

    mCodec = pStream->codec->codec->name;
    if (mRtpActivated)
        OpenRtpEncoder(mTargetHost, mTargetPort, pStream);

    mRtpStreamOpened = true;

    mIncomingAVStream = pStream;
	mIncomingAVStreamCodecContext = pStream->codec;

    return true;
}

bool MediaSinkMem::CloseStreamer()
{
    if (!mRtpStreamOpened)
        return false;

    if (mRtpActivated)
        CloseRtpEncoder();

    mRtpStreamOpened = false;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
