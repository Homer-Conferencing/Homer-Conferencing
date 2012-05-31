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
 * Purpose: Dialog for adding an additional network based media sink
 * Author:  Thomas Volkert
 * Since:   2010-06-20
 */

#ifndef _ADD_NETWORK_SINK_DIALOG_
#define _ADD_NETWORK_SINK_DIALOG_

#include <MediaSource.h>

#include <ui_AddNetworkSinkDialog.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class AddNetworkSinkDialog :
    public QDialog,
    public Ui_AddNetworkSinkDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    AddNetworkSinkDialog(QWidget* pParent, Homer::Multimedia::MediaSource *pMediaSource);

    /// The destructor.
    virtual ~AddNetworkSinkDialog();

    int exec();

private slots:
    void GAPISelectionChanged(QString pSelection);

private:
    void initializeGUI();
    void SaveConfiguration();
    void LoadConfiguration();
    void CreateNewMediaSink();

    Homer::Multimedia::MediaSource *mMediaSource;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
