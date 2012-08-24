/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a modified QApplication
 * Author:  Thomas Volkert
 * Since:   2012-08-24
 */

#include <QFileOpenEvent>
#include <QApplication>

#include <Widgets/OverviewPlaylistWidget.h>
#include <HomerApplication.h>
#include <Logger.h>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

HomerApplication::HomerApplication(int &pArgc, char **pArgv):
		QApplication(pArgc, pArgv)
{
    LOG(LOG_VERBOSE, "Created");
}

HomerApplication::~HomerApplication()
{

    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

bool HomerApplication::event(QEvent *pEvent)
{
	//LOG(LOG_VERBOSE, "Received an event of type %d", (int)pEvent->type());
	QFileOpenEvent *tFileOpenEvent;
    switch (pEvent->type())
    {
    	// OSX sends a "fileOpen" request if Homer was started via "open with"
    	case QEvent::FileOpen:
    		tFileOpenEvent = static_cast<QFileOpenEvent*>(pEvent);
    		LOG(LOG_ERROR, "Received FileOpen event to open file: %s\n", tFileOpenEvent->file().toStdString().c_str());
    		printf("Received FileOpen event to open file: %s\n", tFileOpenEvent->file().toStdString().c_str());
    		PLAYLISTWIDGET.AddEntry(tFileOpenEvent->file(), true);
        	return true;
    	default:
    		return QApplication::event(pEvent);
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
