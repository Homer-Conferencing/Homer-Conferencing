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
 * Purpose: header file for log sink for file outputs
 * Since:   2011-07-27
 */

#ifndef _LOGGER_LOG_SINK_FILE_
#define _LOGGER_LOG_SINK_FILE_

#include <string>
#include <LogSink.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class LogSinkFile:
    public LogSink
{
public:

    LogSinkFile(std::string pFileName);

    /// The destructor
    virtual ~LogSinkFile();

    virtual void ProcessMessage(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

private:
    std::string         mFileName;
    FILE                *mFile;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
