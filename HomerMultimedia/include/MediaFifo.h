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
 * Since:   2011-11-13
 */

#ifndef _MULTIMEDIA_MEDIA_FIFO_
#define _MULTIMEDIA_MEDIA_FIFO_

#include <HBCondition.h>
#include <HBMutex.h>

#include <string>
#include <stdint.h>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MF_DEBUG

///////////////////////////////////////////////////////////////////////////////

struct MediaFifoEntry
{
	char	*Data;
	int		Size;
	int64_t Number;
	Mutex   EntryMutex;
};

///////////////////////////////////////////////////////////////////////////////

class MediaFifo
{
public:
    MediaFifo(std::string pName = "");
    MediaFifo(int pFifoSize, int pFifoEntrySize, std::string pName = "");
    virtual ~MediaFifo();

    virtual void WriteFifo(char* pBuffer, int pBufferSize, int64_t pBufferTimestamp);
    virtual void ReadFifo(char *pBuffer, int &pBufferSize, int64_t &pBufferTimestamp); // memory copy, returns entire memory
    virtual void ClearFifo();

    virtual int ReadFifoExclusive(char **pBuffer, int &pBufferSize, int64_t &pBufferTimestamp); // avoids memory copy, returns a pointer to memory
    virtual void ReadFifoExclusiveFinished(int pEntryPointer);

    virtual int GetEntrySize();
    virtual int GetUsage();
    virtual int GetSize();

protected:
    std::string			mName;
    MediaFifoEntry      *mFifo;
	int					mFifoWritePtr;
	int                 mFifoReadPtr;
	int                 mFifoAvailableEntries;
	int                 mFifoSize;
	int 				mFifoEntrySize;
    Mutex				mFifoMutex;
    Condition			mFifoDataInputCondition;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
