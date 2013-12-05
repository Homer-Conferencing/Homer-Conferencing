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
    mLastPacketPts = 0;
    mMediaId = pMediaId;
	mIncomingAVStream = NULL;
	mIncomingAVStreamCodecID = AV_CODEC_ID_NONE;
	mTargetHost = "";
	mTargetPort = 0;
    mRtpStreamOpened = false;
	mIncomingAVStreamCodecContext = NULL;
    mRtpActivated = pRtpActivated;
    mWaitUntillFirstKeyFrame = (pType == MEDIA_SINK_VIDEO) ? true : false;
    if (mRtpActivated)
        mSinkFifo = new MediaFifo(MEDIA_SOURCE_MEM_FRAGMENT_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE, GetDataTypeStr() + "-MediaSinkMem");
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

void MediaSinkMem::ProcessPacket(char* pPacketData, unsigned int pPacketSize, int64_t pPacketTimestamp, AVStream *pStream, bool pIsKeyFrame)
{
    bool tResetNeeded = false;

    #ifdef MSIM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Sending %d bytes for media sink %s", pPacketSize, GetId().c_str());
    #endif

    // return immediately if the sink is stopped
	if (!mSinkIsActive)
	{
//	    LOG(LOG_WARN, "Media sink isn't active yet");
	    return;
	}

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
        #ifdef MSIM_DEBUG_TIMING
            LOG(LOG_VERBOSE, "Current stream PTS.VAL=%d, PTS.NUM=%d, PTS.DEN=%d", pStream->pts.val, pStream->pts.num, pStream->pts.den);
        #endif
        //###################################
        //### calculate the import PTS value
        //###################################
        // the PTS value is used within the RTP packetizer to calculate the resulting timestamp value for the RTP header
        int64_t tStreamPts = (pStream->pts.den != 0 ? ((float)pStream->pts.val + pStream->pts.num / pStream->pts.den) : 0); // result = val + num / den
        #ifdef MSIM_DEBUG_TIMING
            LOG(LOG_VERBOSE, "Stream PTS: %lld, packet PTS: %lld", tStreamPts, pPacketTimestamp);
        #endif

        if (pPacketTimestamp < mLastPacketPts)
            LOG(LOG_ERROR, "Current packet pts (%lld) from A/V encoder is lower than last one (%"PRId64")", pPacketTimestamp, mLastPacketPts);

        // do we have monotonously increasing PTS values
        if (mIncomingAVStreamLastPts > pPacketTimestamp)
        {
        	LOG(LOG_WARN, "Incoming AV stream PTS values are not monotonous, resetting the streamer now..");
        	tResetNeeded = true;
        }
    	mIncomingAVStreamLastPts = pPacketTimestamp;

        //####################################################################
        // check if RTP encoder is valid for the current stream
        //####################################################################
        if ((mIncomingAVStream != NULL) && (mIncomingAVStream != pStream))
        {
            LOG(LOG_WARN, "Incoming AV stream changed from %p to %p (codec %s), resetting RTP streamer..", mIncomingAVStream, pStream, pStream->codec->codec->name);
            tResetNeeded = true;
        }else if (mIncomingAVStreamCodecContext != pStream->codec)
        {
            LOG(LOG_WARN, "Incoming AV stream unchanged but stream codec context changed from %p to %p (codec %s), resetting RTP streamer..", mIncomingAVStreamCodecContext, pStream->codec, pStream->codec->codec->name);
            tResetNeeded = true;
        }else if (mIncomingAVStreamCodecID != pStream->codec->codec_id)
        {
            LOG(LOG_WARN, "Incoming AV stream and stream codec context unchanged but stream codec ID changed from %d to %d(%s), resetting RTP streamer..", mIncomingAVStreamCodecID, pStream->codec->codec_id, pStream->codec->codec->name);
            tResetNeeded = true;
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

        if (mIncomingFirstPacket)
        {
            mIncomingFirstPacket = false;
            //mIncomingAVStreamStartPts = tAVPacketPts;
        }

        // normalize the PTS values for the RTP packetizer of ffmpeg, otherwise we have synchronization problems at receiver side because of PTS offsets
        int64_t tRtpPacketPts = pPacketTimestamp - mIncomingAVStreamStartPts;

        #ifdef MSIM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Processing packet with A/V PTS: %"PRId64" and normalized PTS: %"PRId64", offset: %"PRId64, pPacketTimestamp, tRtpPacketPts, mIncomingAVStreamStartPts);
        #endif


        int64_t tTime = Time::GetTimeStamp();
        bool tRtpCreationSucceed = RtpCreate(pPacketData, pPacketSize, tRtpPacketPts);
        #ifdef MSIM_DEBUG_TIMING
            int64_t tTime2 = Time::GetTimeStamp();
            LOG(LOG_VERBOSE, "               generating RTP envelope took %"PRId64" us", tTime2 - tTime);
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
                WriteFragment(tRtpPacket, tRtpPacketSize, ++mPacketNumber);

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
                LOG(LOG_VERBOSE, "                             sending RTP packets to network took %"PRId64" us", tTime2 - tTime);
            #endif
        }
    }else
    {
        // send final packet
        WriteFragment(pPacketData, pPacketSize, ++mPacketNumber);
    }
}

void MediaSinkMem::UpdateSynchronization(int64_t pReferenceNtpTimestamp, int64_t pReferenceFrameTimestamp)
{
    if ((mRtpActivated) && (mRtpStreamOpened))
        SetSynchronizationReferenceForRTP((uint64_t)pReferenceNtpTimestamp, (uint64_t)(pReferenceFrameTimestamp- mIncomingAVStreamStartPts));
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
    if (mSinkFifo != NULL)
        return mSinkFifo->GetSize();
    else
        return 0;
}

void MediaSinkMem::ReadFragment(char *pData, int &pDataSize, int64_t &pFragmentNumber)
{
    mSinkFifo->ReadFifo(&pData[0], pDataSize, pFragmentNumber);
    if (pDataSize > 0)
    {
        #ifdef MSIM_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Delivered packet at %p with size: %5d", pData, (int)pDataSize);
        #endif
    }

    // is FIFO near overload situation?
    if (mSinkFifo->GetUsage() >= mSinkFifo->GetSize() - 4)
    {
        LOG(LOG_WARN, "Decoder FIFO is near overload situation, deleting all stored frames");

        // delete all stored frames: it is a better for the decoding!
        mSinkFifo->ClearFifo();
    }
}

void MediaSinkMem::StopProcessing()
{
    LOG(LOG_VERBOSE, "Going to stop media sink \"%s\"", GetStreamName().c_str());
    mSinkFifo->WriteFifo(NULL, 0, 0);
    mSinkFifo->WriteFifo(NULL, 0, 0);
    LOG(LOG_VERBOSE, "Media sink \"%s\" successfully stopped", GetStreamName().c_str());
}

void MediaSinkMem::WriteFragment(char* pData, unsigned int pSize, int64_t pFragmentNumber)
{
    #ifdef MSIM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Storing packet number %6ld at %p with size %4u(%3u header) in memory \"%s\"", pFragmentNumber, pData, pSize, RTP_HEADER_SIZE, mMediaId.c_str());

        // if RTP activated then reparse the current packet and print the content
        if (mRtpActivated)
        {
            char *tPacketData = pData;
            int tPacketSize = pSize;
            bool tLastFragment;
            enum RtcpType tFragmentRtcpType;
            RtpParse(tPacketData, tPacketSize, tLastFragment, tFragmentRtcpType, mIncomingAVStream->codec->codec_id, true);
        }
    #endif
    AnnouncePacket(pSize);
    if ((int)pSize <= mSinkFifo->GetEntrySize())
    {
        mSinkFifo->WriteFifo(pData, (int)pSize, pFragmentNumber);
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

    mIncomingFirstPacket = true;
    mIncomingAVStreamStartPts = 0;
    mIncomingAVStreamCodecID = pStream->codec->codec_id;
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
