/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: FIFO memory for buffering media data in a preallocated data storage
 * Author:  Thomas Volkert
 * Since:   2011-11-13
 */

#ifndef _MULTIMEDIA_MEDIA_FIFO_
#define _MULTIMEDIA_MEDIA_FIFO_

#include <HBCondition.h>
#include <HBMutex.h>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MF_DEBUG

// maximum packet size, including encoded data and RTP/TS
#define MEDIA_SOURCE_MEM_STREAM_PACKET_BUFFER_SIZE          256*1024 // 256 kB
#define MEDIA_SOURCE_MEM_PACKET_BUFFER_SIZE              	 16*1024 // 16 kB

#define MEDIA_SOURCE_MEM_INPUT_QUEUE_SIZE_LIMIT 	             256 // 256 entries -> 4 MB

///////////////////////////////////////////////////////////////////////////////

struct MediaFifoEntry
{
	char	*Data;
	int		Size;
	Mutex   *EntryMutex;
};

///////////////////////////////////////////////////////////////////////////////

class MediaFifo
{
public:
    /// The default constructor
    MediaFifo(int pFifoSize, int pFifoEntrySize);
    /// The destructor.
    virtual ~MediaFifo();

    void WriteFifo(char* pBuffer, int pBufferSize);
    void ReadFifo(char *pBuffer, int &pBufferSize);

    int ReadFifoExclusive(char **pBuffer, int &pBufferSize);
    void ReadFifoExclusiveFinished(int pEntryPointer);

private:
    MediaFifoEntry      *mFifo;
	int					mFifoWritePtr, mFifoReadPtr, mFifoAvailableEntries, mFifoSize;
	int 				mFifoEntrySize;
    Mutex				mFifoMutex;
    Condition			mFifoDataInputCondition;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
