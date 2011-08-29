/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Name:    Logger.h
 * Purpose: header file for logger
 * Author:  Thomas Volkert
 * Since:   2009-05-19
 * Version: $Id$
 */

#ifndef _LOGGER_LOGGER_
#define _LOGGER_LOGGER_

#include <LogSinkConsole.h>
#include <string>
#include <list>
#include <sys/types.h>
#include <sstream>
#include <HBReflection.h>
#include <HBMutex.h>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

inline std::string GetShortFileName(std::string pLongFileName)
{
	std::string tResult = "";

	tResult = pLongFileName.substr(0, pLongFileName.length() - 4 /* ".cpp" */);

	int tPos = tResult.rfind('/');
	if (tPos != (int)std::string::npos)
		tResult = tResult.substr(tPos + 1, tResult.length() - tPos - 1);

	return tResult;
}

#define         LOG_OFF                         0
#define         LOG_ERROR                       1
#define         LOG_WARN                        2
#define         LOG_INFO                        3
#define         LOG_VERBOSE                     4

#define         LOGGER                          Logger::GetInstance()
#define         LOG(Level, ...)                 LOGGER.AddMessage(Level, GetObjectNameStr(this).c_str(), __LINE__, __VA_ARGS__)
#define         LOGEX(FromWhere, Level, ...)    LOGGER.AddMessage(Level, ("static:" + GetObjectNameStr(FromWhere) + ":" + GetShortFileName(__FILE__)).c_str(), __LINE__, __VA_ARGS__)

template <typename T>
inline std::string toString(T const& value_)
{
    std::stringstream ss;
    ss << value_;
    return ss.str();
}

typedef std::list<LogSink*>        LogSinksList;

///////////////////////////////////////////////////////////////////////////////

class Logger
{
public:

    Logger();

    /// The destructor
    virtual ~Logger();

    static Logger& GetInstance();

    void Init(int pLevel);
    void Deinit();
    void AddMessage(int pLevel, const char *pSource, int pLine, const char* pFormat, ...);
    void SetLogLevel(int pLevel);
    int GetLogLevel();

    void RegisterLogSink(LogSink *pLogSink);
    void UnregisterLogSink(LogSink *pLogSink);

private:
    void RelayMessageToLogSinks(int pLevel, std::string pTime, std::string pSource, int pLine, std::string pMessage);

    Mutex       mLoggerMutex, mLogSinksMutex;
    LogSinksList mLogSinks;
    int         mLogLevel;
    int         mLastMessageLogLevel;
    std::string mLastMessage;
    std::string	mLastSource;
    int         mLastLine;
    int         mRepetitionCount;
    LogSinkConsole *mLogSinkConsole;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
