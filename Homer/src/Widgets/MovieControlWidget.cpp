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

#include <Widgets/MovieControlWidget.h>
#include <Logger.h>

using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MovieControlWidget::MovieControlWidget(VideoWidget *pVideoWidget):
    QWidget(NULL)
{
    LOG(LOG_VERBOSE, "Created");

    mVideoWidget = pVideoWidget;

    //setParent(NULL);
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint | Qt::FramelessWindowHint );

    setupUi(this);
    show();
}

MovieControlWidget::~MovieControlWidget()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////
void MovieControlWidget::keyPressEvent(QKeyEvent *pEvent)
{
	//LOG(LOG_VERBOSE, "Got movie control window key press event with key %s(0x%x, mod: 0x%x)", pEvent->text().toStdString().c_str(), pEvent->key(), (int)pEvent->modifiers());

	// forward the event to the parent widget
	QCoreApplication::postEvent(mVideoWidget, new QKeyEvent(QEvent::KeyPress, pEvent->key(), pEvent->modifiers(), pEvent->text()));

	pEvent->accept();
}

void MovieControlWidget::keyReleaseEvent(QKeyEvent *pEvent)
{
	// forward the event to the parent widget
	QCoreApplication::postEvent(mVideoWidget, new QKeyEvent(QEvent::KeyRelease, pEvent->key(), pEvent->modifiers(), pEvent->text()));

	pEvent->accept();
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
