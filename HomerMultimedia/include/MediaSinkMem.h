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
 * Purpose: memory based media sink which supports RTP
 * Author:  Thomas Volkert
 * Since:   2012-06-07
 */

#ifndef _MULTIMEDIA_MEDIA_SINK_MEM_
#define _MULTIMEDIA_MEDIA_SINK_MEM_

#include <string>

#include <MediaFifo.h>
#include <MediaSinkNet.h>
#include <RTP.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSIM_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSinkMem:
    public MediaSinkNet
{

public:
    MediaSinkMem(std::string pMemoryId, enum MediaSinkType pType= MEDIA_SINK_UNKNOWN);

    virtual ~MediaSinkMem();

    virtual void ReadFragment(char *pData, ssize_t &pDataSize);
    virtual void StopReading();

protected:
    virtual void SendFragment(char* pData, unsigned int pSize);

private:
    std::string     mMemoryId;
    MediaFifo       *mSinkFifo;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
