/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Widget for displaying extended session information
 * Author:  Thomas Volkert
 * Since:   2010-09-18
 */

#ifndef _SESSION_INFO_WIDGET_
#define _SESSION_INFO_WIDGET_

#include <HBSocket.h>

#include <QWidget>
#include <QPoint>

#include <ui_SessionInfoWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class SessionInfoWidget :
    public QWidget,
    public Ui_SessionInfoWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    SessionInfoWidget(QWidget* pParent = NULL);
    void Init(QString pParticipant, enum Homer::Base::TransportType pParticipantTransport, bool pVisible = true);

    /// The destructor.
    virtual ~SessionInfoWidget();
    void SetVisible(bool pVisible);
    void SetSipInterface(QString pSipInterface = "unknown");

public slots:
    void ToggleVisibility();

private:
    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);
    virtual void timerEvent(QTimerEvent *pEvent);

    void initializeGUI();
    void UpdateView();

    QPoint              mWinPos;
    QString             mMessageHistory;
    QString             mParticipant, mSipInterface;
    enum Homer::Base::TransportType  mParticipantTransport;
    int                 mTimerId;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
