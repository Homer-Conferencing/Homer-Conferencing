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
 * Purpose: Implementation of a modified QSlider for file playback
 * Since:   2012-07-29
 */

#include <Widgets/PlaybackSlider.h>
#include <Widgets/ParticipantWidget.h>
#include <Logger.h>

using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

PlaybackSlider::PlaybackSlider(QWidget* pParent) :
	QSlider(pParent)
{
    mParticipantWidget = NULL;
}

PlaybackSlider::~PlaybackSlider()
{
}

///////////////////////////////////////////////////////////////////////////////

void PlaybackSlider::Init(ParticipantWidget *pParticipantWidget)
{
    mParticipantWidget = pParticipantWidget;
}

void PlaybackSlider::mousePressEvent(QMouseEvent *pEvent)
{
  	LOG(LOG_VERBOSE, "User triggered mouse press event at %d*%d", pEvent->x(), pEvent->y());
  	int tNewValue = 0;
  	if (pEvent->button() == Qt::LeftButton)
	{
		if (orientation() == Qt::Vertical)
			tNewValue = minimum() + ((maximum() - minimum()) * (height() - pEvent->y())) / height();
		else
			tNewValue = minimum() + ((maximum()-minimum()) * pEvent->x()) / width();
		LOG(LOG_VERBOSE, "User triggers direct jump to position: %d", tNewValue);
		if (!isSliderDown())
		    setValue(tNewValue);
		if (mParticipantWidget != NULL)
		    mParticipantWidget->ActionSeekMovieFile(tNewValue);
		pEvent->accept();
	}
	QSlider::mousePressEvent(pEvent);
}

void PlaybackSlider::contextMenuEvent(QContextMenuEvent *pContextMenuEvent)
{
    QAction *tAction;

    // return if participant widget is not valid, in this case we cannot change the A/V drift anyway
    if (mParticipantWidget == NULL)
        return;

    QMenu tMenu(this);

    tAction = tMenu.addAction(Homer::Gui::PlaybackSlider::tr("Adjust A/V drift"));
    QIcon tIcon2;
    tIcon2.addPixmap(QPixmap(":/images/22_22/Configuration_Video.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon2);
    tAction->setCheckable(true);
    if (mParticipantWidget-> mAVDriftFrame != NULL)
        tAction->setChecked(mParticipantWidget->mAVDriftFrame->isVisible());

    QAction* tPopupRes = tMenu.exec(pContextMenuEvent->globalPos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare(Homer::Gui::PlaybackSlider::tr("Adjust A/V drift")) == 0)
        {
            mParticipantWidget->ActionToggleUserAVDriftWidget();
            return;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
