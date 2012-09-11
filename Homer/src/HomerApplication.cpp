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

#include <Widgets/OverviewPlaylistWidget.h>
#include <HomerApplication.h>
#include <Logger.h>
#include <LogSinkFile.h>
#include <LogSinkNet.h>
#include <Configuration.h>

#include <QFileOpenEvent>
#include <QApplication>
#include <QSplashScreen>
#include <QResource>
#include <QDir>

namespace Homer { namespace Gui {

using namespace std;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

HomerApplication::HomerApplication(int &pArgc, char **pArgv):
		QApplication(pArgc, pArgv)
{
	LOG(LOG_VERBOSE, "Created");

	mArguments = QCoreApplication::arguments();

	mMainWindowIsAlreadyVisible = false;
	mFileToOpen = "";

	// get the absolute path to our binary
    if (pArgc > 0)
    {
    	string tArgv0 = "";
		tArgv0 = pArgv[0];

		size_t tSize;
		tSize = tArgv0.rfind('/');

		// Windows path?
		if (tSize == string::npos)
			tSize = tArgv0.rfind('\\');

		// nothing found?
		if (tSize != string::npos)
			tSize++;
		else
			tSize = 0;
		mBinaryPath = QString(tArgv0.substr(0, tSize).c_str());
    }else
    	mBinaryPath = "";

    initializeLogging();
}

HomerApplication::~HomerApplication()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void HomerApplication::initializeLogging()
{
	if (mArguments.contains("-DebugLevel=Error"))
	{
		LOGGER.Init(LOG_ERROR);
	}else
	{
		if (mArguments.contains("-DebugLevel=Info"))
		{
			LOGGER.Init(LOG_INFO);
		}else
		{
			if (mArguments.contains("-DebugLevel=Verbose"))
			{
				LOGGER.Init(LOG_VERBOSE);
			}else
			{
				#ifdef RELEASE_VERSION
					LOGGER.Init(LOG_ERROR);
				#else
					LOGGER.Init(LOG_VERBOSE);
				#endif
			}
		}
	}

    // file based log sinks
    QStringList tFiles = mArguments.filter("-DebugOutputFile=");
    if (tFiles.size())
    {
        QString tFileName;
        foreach(tFileName, tFiles)
        {
            tFileName = tFileName.remove("-DebugOutputFile=");
            LOGGER.RegisterLogSink(new LogSinkFile(tFileName.toStdString()));
        }
    }

    // delete old default log file
    if (QFile::exists(PATH_DEFAULT_LOGFILE))
    {
    	QFile tFile(PATH_DEFAULT_LOGFILE);
    	tFile.remove();
    }

    // log to default log file
    LOGGER.RegisterLogSink(new LogSinkFile(PATH_DEFAULT_LOGFILE.toStdString()));

	// network based log sinks
    QStringList tPorts = mArguments.filter("-DebugOutputNetwork=");
    if (tPorts.size())
    {
        QString tNetwork;
        foreach(tNetwork, tPorts)
        {
        	tNetwork = tNetwork.remove("-DebugOutputNetwork=");
        	int tPos = tNetwork.lastIndexOf(':');
        	if (tPos != -1)
        	{
        		QString tPortStr = tNetwork.right(tNetwork.size() - tPos -1);
        		tNetwork = tNetwork.left(tPos);
				bool tPortParsingWasOkay = false;
        		int tPort = tPortStr.toInt(&tPortParsingWasOkay);
        		if (tPortParsingWasOkay)
        		{
        			LOG(LOG_VERBOSE, "New network based log sink at %s:%d", tNetwork.toStdString().c_str(), tPort);
					LOGGER.RegisterLogSink(new LogSinkNet(tNetwork.toStdString(), tPort));
        		}else
        			LOG(LOG_ERROR, "Couldn't parse %s as network port", tPortStr.toStdString().c_str());
        	}
        }
    }

    MainWindow::removeArguments(mArguments, "-Debug");
}

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

			mFileToOpen = tFileOpenEvent->file();

			// is playlist widget already created?
    		if (mMainWindowIsAlreadyVisible)
    			PLAYLISTWIDGET.AddEntry(mFileToOpen, true);
    		return true;
    	default:
    		return QApplication::event(pEvent);
    }
}

void HomerApplication::showGUI()
{
    // make sure every icon is visible within menus: otherwise the Ubuntu-packages will have no icons visible
    setAttribute(Qt::AA_DontShowIconsInMenus, false);

    // load the icon resources
    LOG(LOG_VERBOSE, "Loading Icons.rcc from %s", (mBinaryPath.toStdString() + "Icons.rcc").c_str());
    QResource::registerResource(mBinaryPath + "Icons.rcc");

    #ifdef RELEASE_VERSION
    	LOG(LOG_VERBOSE, "Showing splash screen");
		QPixmap tLogo(":/images/SplashScreen.png");
		QSplashScreen tSplashScreen(tLogo);
		tSplashScreen.show();
	#endif

    LOG(LOG_VERBOSE, "Creating Qt main window");
    MainWindow *mMainWindow = new MainWindow(mArguments, mBinaryPath);

	LOG(LOG_VERBOSE, "Showing Qt main window");
    mMainWindow->show();

	#ifdef RELEASE_VERSION
		LOG(LOG_VERBOSE, "End of showing splash screen");
		tSplashScreen.finish(mMainWindow);
	#endif

	mMainWindowIsAlreadyVisible = true;

    // is there a file we should open immediately after startup?
	if (mFileToOpen != "")
		PLAYLISTWIDGET.AddEntry(mFileToOpen, true);
}

string HomerApplication::GetBinaryPath()
{
	return mBinaryPath.toStdString();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
