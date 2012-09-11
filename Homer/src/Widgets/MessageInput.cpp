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
#include <Logger.h>

using namespace Homer::Base;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define MESSAGE_HISTORY_SIZE                    32 // entries

///////////////////////////////////////////////////////////////////////////////

MessageInput::MessageInput(QWidget* pParent) :
    QTextEdit(pParent)
{
	mLastKeyWasEnter = false;
	mMessageHistoryPos = -1;
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
        if (toPlainText().size() > 0)
            Send();
        return;
    }

    if (textCursor().atStart())
    {
        if (pKeyEvent->key() == Qt::Key_Up)
        {
            if (mMessageHistoryPos == -1)
                mMessageHistoryPos = mMessageHistory.size() -1;
            else
                if(mMessageHistoryPos > 0)
                    mMessageHistoryPos--;
            if ((mMessageHistoryPos >= 0) && (mMessageHistoryPos < mMessageHistory.size()))
            {
                setPlainText(mMessageHistory[mMessageHistoryPos]);
            }
            LOG(LOG_VERBOSE, "User wants previous message [UP], got history index: %d, history size: %d", mMessageHistoryPos, mMessageHistory.size());
            return;
        }
        if (pKeyEvent->key() == Qt::Key_Down)
        {
            if (mMessageHistoryPos == -1)
                mMessageHistoryPos = 0;
            else
                if(mMessageHistoryPos < mMessageHistory.size() - 1)
                    mMessageHistoryPos++;
            if ((mMessageHistoryPos >= 0) && (mMessageHistoryPos < mMessageHistory.size()))
            {
                setPlainText(mMessageHistory[mMessageHistoryPos]);
            }
            LOG(LOG_VERBOSE, "User wants previous message [DOWN], got history index: %d, history size: %d", mMessageHistoryPos, mMessageHistory.size());
            return;
        }
    }
    QTextEdit::keyPressEvent(pKeyEvent);
}

void MessageInput::Send()
{
    LOG(LOG_VERBOSE, "Sending text to participant");

    // limit size of message history
    while ((mMessageHistory.size()) && (mMessageHistory.size() > MESSAGE_HISTORY_SIZE))
        mMessageHistory.erase(mMessageHistory.begin());

    // add current message to history
    mMessageHistory.push_back(toPlainText());
    mMessageHistoryPos = -1;

    // filter and replace URLs
    QString tInputMessage = toPlainText();
    QString tOutputMessage = "";
    int tStartPos = 0;
    int tEndPos = -1;

    while (tStartPos < tInputMessage.size())
    {
        tEndPos = tInputMessage.indexOf(' ', tStartPos);
        if (tEndPos == -1)
        {
            if (tStartPos == 0)
                tEndPos = tInputMessage.size() -1;
            else
                break;
        }

        QString tWord = tInputMessage.mid(tStartPos, tEndPos - tStartPos + 1);
        if ((tWord.startsWith("http://")) && (tWord.size() > 7))
        {
            LOG(LOG_VERBOSE, "Found http reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\">" + tWord + "</a>");
        }else if ((tWord.startsWith("ftp://")) && (tWord.size() > 6))
        {
            LOG(LOG_VERBOSE, "Found ftp reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\">" + tWord + "</a>");
        }else if ((tWord.startsWith("mailto://")) && (tWord.size() > 9))
        {
            LOG(LOG_VERBOSE, "Found mailto reference: %s", tWord.toStdString().c_str());
            tOutputMessage.append("<a href=\"" + tWord + "\">" + tWord + "</a>");
        }else
            tOutputMessage.append(tWord);

        if (tEndPos < tInputMessage.size() -1)
            tOutputMessage.append(' ');
        tStartPos = tEndPos + 1;
    }
    if (tOutputMessage.size() > 0)
        setPlainText(tOutputMessage);

    emit SendTrigger();
}

void MessageInput::Clear()
{
    mMessageHistoryPos = -1;
    setPlainText("");
    setFocus(Qt::TabFocusReason);
}

void MessageInput::keyReleaseEvent(QKeyEvent *pKeyEvent)
{
    QTextEdit::keyReleaseEvent(pKeyEvent);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
