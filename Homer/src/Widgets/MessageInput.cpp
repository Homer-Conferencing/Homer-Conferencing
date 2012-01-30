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
 * Purpose: Implementation of a modified QTextEdit for message input
 * Author:  Thomas Volkert
 * Since:   2009-07-21
 */

#include <Widgets/MessageInput.h>
#include <QTextEdit>
#include <QKeyEvent>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

MessageInput::MessageInput(QWidget* pParent) :
    QTextEdit(pParent)
{
	mLastKeyWasEnter = false;
}

MessageInput::~MessageInput()
{
}

///////////////////////////////////////////////////////////////////////////////

void MessageInput::keyPressEvent(QKeyEvent *pKeyEvent)
{
    if (pKeyEvent == NULL)
    	return;

    //printf("native scan code: %u Qt key: %d modifier: %u\n", (unsigned int)pKeyEvent->nativeScanCode(), pKeyEvent->key(), (unsigned int)pKeyEvent->modifiers());

    // ENTER && ALT + S for sending
    if ((((pKeyEvent->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier)) == Qt::NoModifier) && (pKeyEvent->key() == Qt::Key_Return)) ||
        (((pKeyEvent->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::ShiftModifier)) == Qt::AltModifier) && (pKeyEvent->key() == Qt::Key_S)))
    {
        emit SendTrigger();
        return;
    }

    QTextEdit::keyPressEvent(pKeyEvent);
}

void MessageInput::keyReleaseEvent(QKeyEvent *pKeyEvent)
{
    QTextEdit::keyReleaseEvent(pKeyEvent);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
