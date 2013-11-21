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
 * Purpose: Implementation of a dialog for showing program version
 * Author:  Thomas Volkert
 * Since:   2008-11-25
 */

#include <Dialogs/VersionDialog.h>
#include <Configuration.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

VersionDialog::VersionDialog(QWidget* pParent) :
    QDialog(pParent)
{

    initializeGUI();
    mLbVersion->setText(RELEASE_VERSION_STRING);
}

VersionDialog::~VersionDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void VersionDialog::initializeGUI()
{
    setupUi(this);
    mLbFfmpeg->Init("http://ffmpeg.org");
    mLbSofiaSip->Init("http://sofia-sip.sourceforge.net/");
    mLbQt->Init("http://www.qt-project.org/");
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
