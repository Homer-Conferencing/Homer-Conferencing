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
 * Since:   2011-05-05
 */

#include <Header_Ffmpeg.h>
#include <MediaFifo.h>
#include <Logger.h>

#include <string.h> // memcpy

namespace Homer { namespace Multimedia {

using namespace Homer::Base;
using namespace std;

///////////////////////////////////////////////////////////////////////////////

MediaFifo::MediaFifo(std::string pName)
{
    mName = pName;
    mFifoSize = 0;
    mFifoEntrySize = 0;
    mFifoWritePtr = 0;
    mFifoReadPtr = 0;
    mFifoAvailableEntries = 0;
    mFifo = NULL;
    LOG(LOG_VERBOSE, "Created abstract FIFO for %s with %d entries of %d bytes", pName.c_str(), mFifoSize, mFifoEntrySize);
}

MediaFifo::MediaFifo(int pFifoSize, int pFifoEntrySize, string pName)
{
    LOG(LOG_VERBOSE, "Creating FIFO for %s with %d entries of %d bytes", pName.c_str(), pFifoSize, pFifoEntrySize);

    mName = pName;
    mFifoSize = pFifoSize;
    mFifoEntrySize = pFifoEntrySize;
    mFifoWritePtr = 0;
    mFifoReadPtr = 0;
    mFifoAvailableEntries = 0;
    mFifo = new MediaFifoEntry[mFifoSize];
    for (int i = 0; i < mFifoSize; i++)
    {
        mFifo[i].Size = 0;
        mFifo[i].Data = (char*)av_malloc(mFifoEntrySize);
        if (mFifo[i].Data == NULL)
            LOG(LOG_ERROR, "Unable to allocate %d bytes of memory for FIFO %s", mFifoEntrySize, pName.c_str());
    }
    LOG(LOG_VERBOSE, "Created FIFO for %s with %d entries of %d bytes", pName.c_str(), mFifoSize, mFifoEntrySize);
}

MediaFifo::~MediaFifo()
{
    LOG(LOG_VERBOSE, "Destroying FIFO %s with size of %d", mName.c_str(), mFifoSize);

    if (mFifo != NULL)
    {
        for (int i = 0; i < mFifoSize; i++)
        {
            mFifo[i].Size = 0;
            av_free(mFifo[i].Data);
        }
        delete[] mFifo;
        mFifo = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////

void MediaFifo::ReadFifo(char *pBuffer, int &pBufferSize, int64_t &pBufferTimestamp)
{
    int tCurrentFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: ReadFifo() START", mName.c_str());
    #endif

    // make sure there is some pending data in the input Fifo
    mFifoMutex.lock();
    while(mFifoAvailableEntries < 1)
    {
        #ifdef MF_DEBUG
            LOG(LOG_VERBOSE, "%s-FIFO: waiting for new input", mName.c_str());
        #endif

        while(!mFifoDataInputCondition.Wait(&mFifoMutex))
        {
            LOG(LOG_ERROR, "%s-FIFO: error when waiting for new input", mName.c_str());
        }

        #ifdef MF_DEBUG
            LOG(LOG_VERBOSE, "%s-FIFO: woke up from waiting on new data", mName.c_str());
        #endif

        if (mFifoAvailableEntries < 0)
            LOG(LOG_ERROR, "%s-FIFO: negative amount of entries: %d", mName.c_str(), mFifoAvailableEntries);
    }

    tCurrentFifoReadPtr = mFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: reading entry %d", mName.c_str(), tCurrentFifoReadPtr);
    #endif

    // update FIFO read pointer
    mFifoReadPtr++;
    if (mFifoReadPtr >= mFifoSize)
        mFifoReadPtr = mFifoReadPtr - mFifoSize;

    // update FIFO counter
    mFifoAvailableEntries--;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoReadPtr].EntryMutex.lock();
    mFifoMutex.unlock();

    if (pBufferSize >= mFifo[tCurrentFifoReadPtr].Size)
    {// input buffer is okay
        // get the number from Fifo
        pBufferTimestamp = mFifo[tCurrentFifoReadPtr].Number;
        // get captured data from Fifo
        pBufferSize = mFifo[tCurrentFifoReadPtr].Size;
        memcpy((void*)pBuffer, mFifo[tCurrentFifoReadPtr].Data, (size_t)pBufferSize);
    }else
    {// input buffer is too small
        LOG(LOG_ERROR, "Given read buffer is too small (%d bytes) for the current chunk of %d bytes from FIFO %s, dropping data", pBufferSize, mFifo[tCurrentFifoReadPtr].Size, mName.c_str());
        pBufferSize = 0;
    }
    // unlock fine grained mutex again
    mFifo[tCurrentFifoReadPtr].EntryMutex.unlock();

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: erased front element of size %d, size afterwards: %d", mName.c_str(), (int)pBufferSize, (int)mFifoAvailableEntries);
    #endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "%s-FIFO: data chunk with size 0 read", mName.c_str());
}

void MediaFifo::ClearFifo()
{
    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: going to clear entire buffer", mName.c_str());
    #endif

    // make sure there is some pending data in the input Fifo
    mFifoMutex.lock();

    mFifoWritePtr = 0;
    mFifoReadPtr = 0;
    mFifoAvailableEntries = 0;
    for (int i = 0; i < mFifoSize; i++)
    {
        mFifo[i].Size = 0;
    }

    // unlock
    mFifoMutex.unlock();
}

int MediaFifo::GetEntrySize()
{
    return mFifoEntrySize;
}

int MediaFifo::GetUsage()
{
    int tResult = 0;
    mFifoMutex.lock();

    tResult = mFifoAvailableEntries;

    mFifoMutex.unlock();

    return tResult;
}

int MediaFifo::GetSize()
{
    int tResult = 0;
    mFifoMutex.lock();

    tResult = mFifoSize;

    mFifoMutex.unlock();

    return tResult;
}

int MediaFifo::ReadFifoExclusive(char **pBuffer, int &pBufferSize, int64_t &pBufferTimestamp)
{
    int tCurrentFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: ReadFifoExclusive() START", mName.c_str());
    #endif

    // make sure there is some pending data in the input Fifo
    mFifoMutex.lock();
    int tRounds = 0;
    while (mFifoAvailableEntries < 1)
    {
        if (tRounds > 0)
            LOG(LOG_VERBOSE, "%s-FIFO: woke up but no new data found, already passed rounds: %d", mName.c_str(), tRounds);

        while(!mFifoDataInputCondition.Wait(&mFifoMutex))
        {
            LOG(LOG_ERROR, "%s-FIFO: error when waiting for new input", mName.c_str());
        }

        tRounds++;
    }

    tCurrentFifoReadPtr = mFifoReadPtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: reading exclusively entry %d with %d bytes", mName.c_str(), tCurrentFifoReadPtr, mFifo[tCurrentFifoReadPtr].Size);
    #endif

    // update FIFO read pointer
    mFifoReadPtr++;
    if (mFifoReadPtr >= mFifoSize)
        mFifoReadPtr = mFifoReadPtr - mFifoSize;

    // update FIFO counter
    mFifoAvailableEntries--;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoReadPtr].EntryMutex.lock();
    mFifoMutex.unlock();

    // get number
    pBufferTimestamp = mFifo[tCurrentFifoReadPtr].Number;
    // get captured data from Fifo
    pBufferSize = mFifo[tCurrentFifoReadPtr].Size;
    // don't copy, use pointer to data instead
    *pBuffer = mFifo[tCurrentFifoReadPtr].Data;

    // NO unlock of fine grained mutex again -> has to be triggered by caller via separated function: mFifo[tCurrentFifoReadPtr].EntryMutex->unlock();

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: erased front element of size %d, size afterwards: %d", mName.c_str(), (int)pBufferSize, (int)mFifoAvailableEntries);
    #endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "%s-FIFO: data chunk with size 0 read", mName.c_str());

    return tCurrentFifoReadPtr;
}

void MediaFifo::ReadFifoExclusiveFinished(int pEntryPointer)
{
    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: finishing exclusive entry access to %d", mName.c_str(), pEntryPointer);
    #endif

    mFifo[pEntryPointer].EntryMutex.unlock();
}

void MediaFifo::WriteFifo(char* pBuffer, int pBufferSize, int64_t pBufferTimestamp)
{
    int tCurrentFifoWritePtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: WriteFifo() START", mName.c_str());
    #endif

    if (pBufferSize > mFifoEntrySize)
    {
        LOG(LOG_ERROR, "%s-FIFO: entries are limited to %d bytes, current write request of %d bytes will be ignored, current FIFO size: %d", mName.c_str(), mFifoEntrySize, pBufferSize, mFifoSize);
        return;
    }

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "%s-FIFO: writing empty chunk", mName.c_str());

    mFifoMutex.lock();
    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "%s-FIFO: got lock for empty chunk", mName.c_str());

    if (mFifoAvailableEntries >= mFifoSize)
    {
        LOG(LOG_WARN, "%s-FIFO: buffer full (size is %d, read: %d, write %d) - dropping oldest (%d) data chunk", mName.c_str(), mFifoSize, mFifoReadPtr, mFifoWritePtr, mFifoReadPtr);

        // update FIFO read pointer
        mFifoReadPtr++;
        if (mFifoReadPtr >= mFifoSize)
            mFifoReadPtr = mFifoReadPtr - mFifoSize;
    }else
    {
        // update FIFO counter
        mFifoAvailableEntries++;
    }

    tCurrentFifoWritePtr = mFifoWritePtr;

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: writing entry %d", mName.c_str(), tCurrentFifoWritePtr);
    #endif

    // update FIFO write pointer
    mFifoWritePtr++;
    if (mFifoWritePtr >= mFifoSize)
        mFifoWritePtr = mFifoWritePtr - mFifoSize;

    // release FIFO mutex and use fine grained mutex of corresponding FIFO entry instead for protecting memcpy
    mFifo[tCurrentFifoWritePtr].EntryMutex.lock();

    // add the new entry
    mFifo[tCurrentFifoWritePtr].Size = pBufferSize;
    if ((pBuffer != NULL) && (pBufferSize > 0))
        memcpy((void*)mFifo[tCurrentFifoWritePtr].Data, (const void*)pBuffer, (size_t)pBufferSize);
    mFifo[tCurrentFifoWritePtr].Number = pBufferTimestamp;

    // unlock fine grained mutex again
    mFifo[tCurrentFifoWritePtr].EntryMutex.unlock();

    #ifdef MF_DEBUG
        LOG(LOG_VERBOSE, "%s-FIFO: buffer size now: %d", mName.c_str(), mFifoAvailableEntries);
    #endif

    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "Send wake up signal for empty chunk");

    mFifoDataInputCondition.Signal();
    mFifoMutex.unlock();
    if (pBufferSize == 0)
        LOG(LOG_VERBOSE, "%s-FIFO: released lock after writing empty chunk", mName.c_str());
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
