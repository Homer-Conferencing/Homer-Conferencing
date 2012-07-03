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

MediaSinkMem::MediaSinkMem(string pMemoryId, enum MediaSinkType pType, bool pRtpActivated):
    MediaSinkNet("memory", 0, NULL, pType, pRtpActivated)
{
    // overwrite id from MediaSinkNet
    mMemoryId = pMemoryId;
    mMediaId = mMemoryId;
    mSinkFifo = new MediaFifo(MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT, MEDIA_SOURCE_MEM_FRAGMENT_BUFFER_SIZE, "MediaSinkMem");
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
    delete mSinkFifo;
}

void MediaSinkMem::SendFragment(char* pData, unsigned int pSize)
{
    #ifdef MSIM_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Storing packet number %6ld at %p with size %4u(%3u header) in memory \"%s\"", ++mPacketNumber, pData, pSize, RTP_HEADER_SIZE, mMemoryId.c_str());

        // if RTP activated then reparse the current packet and print the content
        if (mRtpActivated)
        {
            char *tPacketData = pData;
            unsigned int tPacketSize = pSize;
            bool tLastFragment;
            bool tFragmentIsSenderReport;
            RtpParse(tPacketData, tPacketSize, tLastFragment, tFragmentIsSenderReport, mCurrentStream->codec->codec_id, true);
        }
    #endif
    AnnouncePacket(pSize);

    mSinkFifo->WriteFifo(pData, (int)pSize);
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

}

void MediaSinkMem::StopReading()
{
    char tData[4];
    mSinkFifo->WriteFifo(tData, 0);
    mSinkFifo->WriteFifo(tData, 0);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
