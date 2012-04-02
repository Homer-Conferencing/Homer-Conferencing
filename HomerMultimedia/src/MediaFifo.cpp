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
 * Purpose: Implementation of a ffmpeg based memory media source
 * Author:  Thomas Volkert
 * Since:   2011-05-05
 */

#include <MediaFifo.h>
#include <Logger.h>

#include <string.h>

namespace Homer { namespace Multimedia {

using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

MediaFifo::MediaFifo(int pFifoSize, int pFifoEntrySize)
{
    mFifoSize = pFifoSize;
    mFifoEntrySize = pFifoEntrySize;
    mFifoWritePtr = 0;
    mFifoReadPtr = 0;
    mFifoAvailableEntries = 0;
    mFifo = (MediaFifoEntry*)malloc(sizeof(MediaFifoEntry) * pFifoSize);
	for (int i = 0; i < mFifoSize; i++)
	{
		mFifo[i].Size = 0;
		mFifo[i].Data = (char*)malloc(pFifoEntrySize);
		if (mFifo[i].Data == NULL)
			LOG(LOG_ERROR, "Unable to allocate memory for FIFO");
		mFifo[i].EntryMutex = new Mutex();
	}
	LOG(LOG_VERBOSE, "Created FIFO with %d entries of %d Kb", mFifoSize, pFifoEntrySize / 1024);
}

MediaFifo::~MediaFifo()
{
    for (int i = 0; i < mFifoSize; i++)
	{
		mFifo[i].Size = 0;
		free(mFifo[i].Data);
		delete mFifo[i].EntryMutex;
	}
    free(mFifo);
}

///////////////////////////////////////////////////////////////////////////////

void MediaFifo::ReadFifo(char *pBuffer, int &pBufferSize)
{
    int tCurrentFifoReadPtr;

    #ifdef MF_DEBUG
		LOG(LOG_VERBOSE, "Going to read data chunk from FIFO");
	#endif

    // make sure there is some pending data in the input Fifo
	mFifoMutex.lock();
	while(mFifoAvailableEntries < 1)
	{
		#ifdef MF_DEBUG
			LOG(LOG_VERBOSE, "Waiting for a new FIFO input");
		#endif

		mFifoDataInputCondition.Reset();
		mFifoMutex.unlock();

		while(!mFifoDataInputCondition.Wait())
			LOG(LOG_ERROR, "Error when waiting for new FIFO input");

		mFifoMutex.lock();

		if (mFifoAvailableEntries < 0)
		    LOG(LOG_ERROR, "FIFO has negative amount of entries: %d", mFifoAvailableEntries);
	}

	tCurrentFifoReadPtr = mFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Reading FIFO entry %d", tCurrentFifoReadPtr);
    #endif

    // update FIFO read pointer
    mFifoReadPtr++;
    if (mFifoReadPtr >= mFifoSize)
        mFifoReadPtr = mFifoReadPtr - mFifoSize;

    // update FIFO counter
    mFifoAvailableEntries--;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoReadPtr].EntryMutex->lock();
    mFifoMutex.unlock();

    // get captured data from Fifo
	pBufferSize = mFifo[tCurrentFifoReadPtr].Size;
    memcpy((void*)pBuffer, mFifo[tCurrentFifoReadPtr].Data, (size_t)pBufferSize);

    // unlock fine grained mutex again
    mFifo[tCurrentFifoReadPtr].EntryMutex->unlock();

	#ifdef MF_DEBUG
		LOG(LOG_VERBOSE, "Erased front element of size %d from FIFO, size afterwards: %d", (int)pBufferSize, (int)mFifoAvailableEntries);
	#endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "Data chunk with size 0 read from FIFO");
}

int MediaFifo::UsedFifoSize()
{
    int tResult = 0;
    mFifoMutex.lock();

    tResult = mFifoAvailableEntries;

    mFifoMutex.unlock();

    return tResult;
}

int MediaFifo::ReadFifoExclusive(char **pBuffer, int &pBufferSize)
{
    int tCurrentFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Going to read data chunk from FIFO");
    #endif

    // make sure there is some pending data in the input Fifo
    mFifoMutex.lock();
    int tRounds = 0;
    while (mFifoAvailableEntries < 1)
    {
        if (tRounds > 0)
            LOG(LOG_VERBOSE, "Woke up but no new data found, already passed rounds: %d", tRounds);

        mFifoDataInputCondition.Reset();
        mFifoMutex.unlock();

        while(!mFifoDataInputCondition.Wait())
            LOG(LOG_ERROR, "Error when waiting for new FIFO input");

        mFifoMutex.lock();

        tRounds++;
    }

    tCurrentFifoReadPtr = mFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Reading exclusivle FIFO entry %d", tCurrentFifoReadPtr);
    #endif

    // update FIFO read pointer
    mFifoReadPtr++;
    if (mFifoReadPtr >= mFifoSize)
        mFifoReadPtr = mFifoReadPtr - mFifoSize;

    // update FIFO counter
    mFifoAvailableEntries--;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoReadPtr].EntryMutex->lock();
    mFifoMutex.unlock();

    // get captured data from Fifo
    pBufferSize = mFifo[tCurrentFifoReadPtr].Size;
    // don't copy, use pointer to data instead
    *pBuffer = mFifo[tCurrentFifoReadPtr].Data;

    // NO unlock of fine grained mutex again -> has to be triggered by caller via separated function: mFifo[tCurrentFifoReadPtr].EntryMutex->unlock();

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Erased front element of size %d from FIFO, size afterwards: %d", (int)pBufferSize, (int)mFifoAvailableEntries);
    #endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "Data chunk with size 0 read from FIFO");

    return tCurrentFifoReadPtr;
}

void MediaFifo::ReadFifoExclusiveFinished(int pEntryPointer)
{
    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Finishing exclusive FIFO entry access to %d", pEntryPointer);
    #endif

    mFifo[pEntryPointer].EntryMutex->unlock();
}

void MediaFifo::WriteFifo(char* pBuffer, int pBufferSize)
{
    int tCurrentFifoWritePtr;

    #ifdef MF_DEBUG
		LOG(LOG_VERBOSE, "Going to write data chunk to FIFO");
	#endif

	if (pBufferSize > mFifoEntrySize)
	{
		LOG(LOG_ERROR, "FIFO entries are limited to %d bytes, current write request of %s bytes will be ignored", mFifoEntrySize, pBufferSize);
		return;
	}

	if (pBufferSize == 0)
	    LOG(LOG_VERBOSE, "Writing empty chunk to FIFO");

	mFifoMutex.lock();
	if (mFifoAvailableEntries >= mFifoSize -1)
	{
	    LOG(LOG_WARN, "FIFO full (size is %d, read: %d, write %d) - dropping oldest (%d) data chunk", mFifoSize, mFifoReadPtr, mFifoWritePtr, mFifoReadPtr);

	    // update FIFO read pointer
        mFifoReadPtr++;
        if (mFifoReadPtr >= mFifoSize)
            mFifoReadPtr = mFifoReadPtr - mFifoSize;
	}

	tCurrentFifoWritePtr = mFifoWritePtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "Writing FIFO entry %d", tCurrentFifoWritePtr);
    #endif

    // update FIFO write pointer
    mFifoWritePtr++;
    if (mFifoWritePtr >= mFifoSize)
        mFifoWritePtr = mFifoWritePtr - mFifoSize;

    // update FIFO counter
    mFifoAvailableEntries++;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoWritePtr].EntryMutex->lock();
    mFifoMutex.unlock();

    // add the new entry
    mFifo[tCurrentFifoWritePtr].Size = pBufferSize;
    memcpy((void*)mFifo[tCurrentFifoWritePtr].Data, (const void*)pBuffer, (size_t)pBufferSize);

    // unlock fine grained mutex again
    mFifo[tCurrentFifoWritePtr].EntryMutex->unlock();

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "FIFO length now: %d", mFifoAvailableEntries);
    #endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "Send wake up signal for empty chunk");

    mFifoDataInputCondition.SignalAll();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
