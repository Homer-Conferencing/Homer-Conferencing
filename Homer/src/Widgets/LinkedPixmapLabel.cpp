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
 * Purpose: Implementation of a modified QLabel for showing a linked pixmap
 * Since:   2012-08-21
 */

#include <Widgets/LinkedPixmapLabel.h>
#include <Logger.h>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>

using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

LinkedPixmapLabel::LinkedPixmapLabel(QWidget* pParent) :
	QLabel(pParent)
{

}

LinkedPixmapLabel::~LinkedPixmapLabel()
{
}

///////////////////////////////////////////////////////////////////////////////

void LinkedPixmapLabel::Init(QString pLink)
{
	mLink = pLink;
}

void LinkedPixmapLabel::mousePressEvent(QMouseEvent *pEvent)
{
  	LOG(LOG_VERBOSE, "User triggered mouse press event at %d*%d", pEvent->x(), pEvent->y());
  	if (pEvent->button() == Qt::LeftButton)
  	{
  		QDesktopServices::openUrl(QUrl(mLink));
  	}
  	QLabel::mousePressEvent(pEvent);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
