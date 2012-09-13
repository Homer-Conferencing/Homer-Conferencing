/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: modified QTextEdit for message history
 * Author:  Thomas Volkert
 * Since:   2008-12-15
 */

#ifndef _MESSAGE_HISTORY_
#define _MESSAGE_HISTORY_

#include <QTextBrowser>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define URL_SMILE                   "Smile.gif"
#define PATH_SMILE                   ":/images/30_30/Smile.gif"

///////////////////////////////////////////////////////////////////////////////

class MessageHistory :
    public QTextBrowser
{
    Q_OBJECT;
public:
    /// The default constructor
    MessageHistory(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~MessageHistory();
    void Save();
    void Update(QString pNewHistory);

public slots:
    void textSelected(bool pAvail);

private slots:
    void Animate();

private:
    void AddAnimation(const QUrl &pUrl, const QString &pFile);
    void contextMenuEvent(QContextMenuEvent *event);

    bool                    mSomeTextSelected;
    QMap<QMovie*, QUrl>     mUrls;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
