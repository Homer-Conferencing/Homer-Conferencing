/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation of a dialog for acknowledging the files to transfer
 * Author:  Thomas Volkert
 * Since:   2011-02-04
 */

#include <Dialogs/FileTransferAckDialog.h>

namespace Homer { namespace Gui {


///////////////////////////////////////////////////////////////////////////////

FileTransferAckDialog::FileTransferAckDialog(QWidget* pParent, QString pParticipant, QStringList pFiles) :
    QDialog(pParent)
{
    initializeGUI(pParticipant, pFiles);
}

FileTransferAckDialog::~FileTransferAckDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void FileTransferAckDialog::initializeGUI(QString pParticipant, QStringList pFiles)
{
    setupUi(this);
    setWindowTitle("Transfer these files to \"" + pParticipant + "\"?");
    for (int i = 0; i < pFiles.size(); i++)
        mTbFiles->setPlainText(mTbFiles->toPlainText() + pFiles[i] + "\n");
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
