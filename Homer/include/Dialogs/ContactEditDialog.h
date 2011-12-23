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
 * Purpose: Dialog for editing a contact
 * Author:  Thomas Volkert
 * Since:   2009-04-06
 */

#ifndef _CONTACT_EDIT_DIALOG_
#define _CONTACT_EDIT_DIALOG_

#include <ui_ContactEditDialog.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class ContactEditDialog :
    public QDialog,
    public Ui_ContactEditDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    ContactEditDialog(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~ContactEditDialog();

private:
    void initializeGUI();
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
