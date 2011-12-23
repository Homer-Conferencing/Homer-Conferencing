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
 * Purpose: Dialog for check for updates
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#ifndef _UPDATE_CHECK_DIALOG_
#define _UPDATE_CHECK_DIALOG_

#include <ui_UpdateCheckDialog.h>

#include <QHttp>

#define TriggerVersionCheck(pReqObject, pAnswerHandleFunction) {                                                                                         \
                                                                connect(pReqObject, SIGNAL(done(bool)), this, SLOT(pAnswerHandleFunction(bool)));        \
                                                                pReqObject->setHost(RELEASE_SERVER);                                                     \
                                                                pReqObject->get(PATH_VERSION_TXT);                                                       \
                                                                }

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class UpdateCheckDialog :
    public QDialog,
    public Ui_UpdateCheckDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    UpdateCheckDialog(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~UpdateCheckDialog();

public slots:
    int exec();

private slots:
    void DownloadUpdate();
    void GotAnswerForVersionRequest(bool pError);
    void GotAnswerForChangelogRequest(bool pError);

private:
    void initializeGUI();

    QHttp           *mHttpGetVersionServer;
    QHttp           *mHttpGetChangelogUrl;
    QString         mServerVersion;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
