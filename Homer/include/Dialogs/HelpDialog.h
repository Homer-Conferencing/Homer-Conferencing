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
 * Purpose: Dialog for check for updates
 * Since:   2010-12-19
 */

#ifndef _HELP_DIALOG_
#define _HELP_DIALOG_

#include <ui_HelpDialog.h>

#include <QNetworkAccessManager>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class HelpDialog :
    public QDialog,
    public Ui_HelpDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    HelpDialog(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~HelpDialog();

    static QString GetSystemInfo();

private slots:
    void GotAnswerForHelpRequest(QNetworkReply *pReply);

private:
    void initializeGUI();

    QNetworkAccessManager   *mHttpGetHelpUrl;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
