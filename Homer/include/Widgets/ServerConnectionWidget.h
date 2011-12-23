/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Purpose: modified tool button for setting the server state
 * Author:  Thomas Volkert
 * Since:   2011-10-08
 */

#ifndef _SERVER_CONNECTION_WIDGET
#define _SERVER_CONNECTION_WIDGET

#include <QWidget>

#include <ui_ServerConnectionWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class ServerConnectionWidget :
    public QWidget,
    public Ui_ServerConnectionWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    ServerConnectionWidget(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~ServerConnectionWidget();

    void InitializeRegistrationMenu(QMenu *pMenu);
    void UpdateState(bool pRegistered);

public slots:
    void Selected(QAction *pAction);

private:
    void initializeGUI();

    QMenu       *mMenu;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
