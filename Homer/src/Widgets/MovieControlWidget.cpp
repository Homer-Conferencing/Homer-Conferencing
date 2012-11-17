/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a movie control widget
 * Author:  Thomas Volkert
 * Since:   2012-11-16
 */

//#include <Dialogs/OpenVideoAudioPreviewDialog.h>
//#include <Widgets/StreamingControlWidget.h>
//#include <Widgets/OverviewPlaylistWidget.h>
#include <Widgets/MovieControlWidget.h>
//#include <Widgets/VideoWidget.h>
//#include <Widgets/OverviewPlaylistWidget.h>
//#include <MediaSourceNet.h>
//#include <WaveOutPortAudio.h>
//#include <WaveOutSdl.h>
//#include <Widgets/AudioWidget.h>
//#include <MediaSourceFile.h>
//#include <MediaSourceNet.h>
//#include <Widgets/MessageWidget.h>
//#include <Widgets/SessionInfoWidget.h>
//#include <MainWindow.h>
//#include <Configuration.h>
//#include <Meeting.h>
//#include <MeetingEvents.h>
#include <Logger.h>
//#include <Snippets.h>

//#include <QInputDialog>
//#include <QMenu>
//#include <QEvent>
//#include <QAction>
//#include <QString>
//#include <QWidget>
//#include <QFrame>
//#include <QDockWidget>
//#include <QLabel>
//#include <QMessageBox>
//#include <QMainWindow>
//#include <QSettings>
//#include <QSplitter>
//#include <QCoreApplication>
//#include <QHostInfo>
//
//#include <stdlib.h>

using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MovieControlWidget::MovieControlWidget(QWidget *pParent):
    QWidget(NULL)
{
    LOG(LOG_VERBOSE, "Created");

    //setParent(NULL);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);

    setupUi(this);
    show();
}

MovieControlWidget::~MovieControlWidget()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

}} //namespace
