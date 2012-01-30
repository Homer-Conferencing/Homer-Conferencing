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
 * Purpose: modified QPushButton for call button
 * Author:  Thomas Volkert
 * Since:   2008-12-15
 */

#ifndef _CALL_BUTTON_
#define _CALL_BUTTON_

#include <QPushButton>
#include <QString>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

/*
 * this modified button supports 4 states:
 *      0 - "Call" (unpressed) => standby
 *      1 - "Call" (pressed) => currently ringing
 *      2 - "Hang up" (unpressed) => call is running
 *      3 - "Hang up" (pressed) => currently aborting call
 */

class CallButton :
    public QPushButton
{
    Q_OBJECT;
public:
    /// The default constructor
    CallButton(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~CallButton();
    void ShowNewState();
    void SetPartner(QString pPartner);

public slots:
    void HandleClick();

private:
    QString     mPartner;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
