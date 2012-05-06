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
 * Purpose: Dialog for selecting the screen segment which is to be captured
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#ifndef _SEGMENT_SELECTION_DIALOG_
#define _SEGMENT_SELECTION_DIALOG_

#include <MediaSourceDesktop.h>
#include <ui_SegmentSelectionDialog.h>

#include <QAbstractButton>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPoint>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class SegmentSelectionDialog :
    public QDialog,
    public Ui_SegmentSelectionDialog
{
    Q_OBJECT;
public:
    /// The default constructor
    SegmentSelectionDialog(QWidget* pParent, MediaSourceDesktop *pMediaSourceDesktop);

    /// The destructor.
    virtual ~SegmentSelectionDialog();

private slots:
    void ClickedButton(QAbstractButton *pButton);

private:
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void mousePressEvent(QMouseEvent *pEvent);
    virtual void mouseMoveEvent(QMouseEvent *pEvent);
    virtual void moveEvent(QMoveEvent *pEvent);
    virtual void resizeEvent(QResizeEvent *pEvent);
    void initializeGUI();
    void ResetToDefaults();

    MediaSourceDesktop *mMediaSourceDesktop;
    QPoint  mDrapPosition;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
