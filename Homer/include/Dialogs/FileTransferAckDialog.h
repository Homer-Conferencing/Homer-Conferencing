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
 * Purpose: Dialog for acknowledging the files to transfer
 * Author:  Thomas Volkert
 * Since:   2011-02-04
 */

#ifndef _FILE_TRANSFER_ACK_DIALOG_
#define _FILE_TRANSFER_ACK_DIALOG_

#include <ui_FileTransferAckDialog.h>

#include <QString>
#include <QStringList>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class FileTransferAckDialog :
    public QDialog,
    public Ui_FileTransferAckDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    FileTransferAckDialog(QWidget* pParent, QString pParticipant, QStringList pFiles);

    /// The destructor.
    virtual ~FileTransferAckDialog();

private:
    void initializeGUI(QString pParticipant, QStringList pFiles);
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
