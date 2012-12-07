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
 * Purpose: Implementation of a dialog for selecting the screen segment which is to be captured
 * Author:  Thomas Volkert
 * Since:   2010-12-19
 */

#include <Dialogs/SegmentSelectionDialog.h>

#include <QDesktopWidget>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QMenu>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define MIN_WIDTH       352
#define MIN_HEIGHT      288

///////////////////////////////////////////////////////////////////////////////

SegmentSelectionDialog::SegmentSelectionDialog(QWidget* pParent, MediaSourceDesktop *pMediaSourceDesktop):
    QDialog(pParent)
{
    mMediaSourceDesktop = pMediaSourceDesktop;

    initializeGUI();
}

SegmentSelectionDialog::~SegmentSelectionDialog()
{
}

///////////////////////////////////////////////////////////////////////////////

void SegmentSelectionDialog::initializeGUI()
{
    LOG(LOG_VERBOSE, "Found current segment resolution of %d*%d starting at %d*%d", mMediaSourceDesktop->mSourceResX, mMediaSourceDesktop->mSourceResY, mMediaSourceDesktop->mGrabOffsetX, mMediaSourceDesktop->mGrabOffsetY);

    setupUi(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    mLbOffsetX->setText(QString("%1").arg(mMediaSourceDesktop->mGrabOffsetX));
    mLbOffsetY->setText(QString("%1").arg(mMediaSourceDesktop->mGrabOffsetY));
    mLbResX->setText(QString("%1").arg(mMediaSourceDesktop->mSourceResX));
    mLbResY->setText(QString("%1").arg(mMediaSourceDesktop->mSourceResY));

    connect(mTbDefaults, SIGNAL(clicked(bool)), this, SLOT(ResetToDefaults()));
    connect(mTbDesktop, SIGNAL(clicked(bool)), this, SLOT(ResetToDesktop()));
    connect(mTbDesktopAuto, SIGNAL(clicked(bool)), this, SLOT(ResetToDesktopAuto(bool)));

    mTbDesktopAuto->setChecked(mMediaSourceDesktop->GetAutoDesktop());
    mCbMouse->setChecked(mMediaSourceDesktop->GetMouseVisualization());

    //setSizeGripEnabled(false);
    resize(mMediaSourceDesktop->mSourceResX - (frameGeometry().width() - width()),
           mMediaSourceDesktop->mSourceResY - (frameGeometry().height() - height()) - (frameGeometry().width() - width()) / 2);
    //setSizeGripEnabled(true);
    move(mMediaSourceDesktop->mGrabOffsetX, mMediaSourceDesktop->mGrabOffsetY);
}

int SegmentSelectionDialog::exec()
{
	int tResult;

	tResult = QDialog::exec();

	if (tResult == QDialog::Accepted)
	{
		mMediaSourceDesktop->SetAutoDesktop(mTbDesktopAuto->isChecked());
		mMediaSourceDesktop->SetMouseVisualization(mCbMouse->isChecked());
	}

	return tResult;
}

void SegmentSelectionDialog::ConfigureDesktopCapturing(int pOffsetX, int pOffsetY, int pWidth, int pHeight)
{
    LOG(LOG_VERBOSE, "Configuring desktop capturing to offset=(%d, %d), dimension=%d * %d", pOffsetX, pOffsetY, pWidth, pHeight);

    mMediaSourceDesktop->SetScreenshotSize(pWidth, pHeight);
    mMediaSourceDesktop->mGrabOffsetX = pOffsetX;
    mMediaSourceDesktop->mGrabOffsetY = pOffsetY;

    mLbOffsetX->setText(QString("%1").arg(pOffsetX));
    mLbOffsetY->setText(QString("%1").arg(pOffsetY));
    mLbResX->setText(QString("%1").arg(pWidth));
    mLbResY->setText(QString("%1").arg(pHeight));

    resize(pWidth, pHeight);
    move(pOffsetX, pOffsetY);
}

void SegmentSelectionDialog::ResetToDesktop()
{
    LOG(LOG_VERBOSE, "Resetting to desktop size");

    ConfigureDesktopCapturing(0, 0, QApplication::desktop()->width(), QApplication::desktop()->height());
}

void SegmentSelectionDialog::ResetToDesktopAuto(bool pActive)
{
	if (pActive)
		ResetToDesktop();
}

void SegmentSelectionDialog::ResetToDefaults()
{
    LOG(LOG_VERBOSE, "Resetting to defaults");
    mTbDesktopAuto->setChecked(false);
    ConfigureDesktopCapturing(0, 0, MIN_WIDTH, MIN_HEIGHT);
}

void SegmentSelectionDialog::ClickedButton(QAbstractButton *pButton)
{
    if (mBb->standardButton(pButton) == QDialogButtonBox::Reset)
    {
    	ResetToDefaults();
    }
}

void SegmentSelectionDialog::contextMenuEvent(QContextMenuEvent *event)
{
    QAction *tAction;

    QMenu tMenu(this);

    tAction = tMenu.addAction("Reset to defaults");
    QIcon tIcon;
    tIcon.addPixmap(QPixmap(":/images/22_22/Reload.png"), QIcon::Normal, QIcon::Off);
    tAction->setIcon(tIcon);

    QAction* tPopupRes = tMenu.exec(QCursor::pos());
    if (tPopupRes != NULL)
    {
        if (tPopupRes->text().compare("Reset to defaults") == 0)
        {
            ResetToDefaults();
            return;
        }
    }
}

void SegmentSelectionDialog::mousePressEvent(QMouseEvent *pEvent)
{
    if (pEvent->button() == Qt::LeftButton)
    {
        mDrapPosition = pEvent->globalPos() - frameGeometry().topLeft();
        pEvent->accept();
    }
}

void SegmentSelectionDialog::mouseMoveEvent(QMouseEvent *pEvent)
{
    if (pEvent->buttons() & Qt::LeftButton)
    {
        QPoint tNewPos = pEvent->globalPos() - mDrapPosition;
        if (tNewPos.x() < 0)
            tNewPos.setX(0);
        if (tNewPos.y() < 0)
            tNewPos.setY(0);

        move(tNewPos);

        int tNewOffsetX = x();// + (frameGeometry().width() - width()) / 2;
        int tNewOffsetY = y();// + (frameGeometry().height() - height()) - (frameGeometry().width() - width()) / 2;
        mMediaSourceDesktop->mGrabOffsetX = tNewOffsetX;
        mMediaSourceDesktop->mGrabOffsetY = tNewOffsetY;
        mLbOffsetX->setText(QString("%1").arg(tNewOffsetX));
        mLbOffsetY->setText(QString("%1").arg(tNewOffsetY));

        pEvent->accept();
    }
}

void SegmentSelectionDialog::moveEvent(QMoveEvent *pEvent)
{
    QPoint tNewPos = pEvent->pos();

    bool tCorrectMove = false;

    if (tNewPos.x() < 0)
    {
        tNewPos.setX(0);
        tCorrectMove = true;
    }
    if (tNewPos.y() < 0)
    {
        tNewPos.setY(0);
        tCorrectMove = true;
    }
    if (tCorrectMove)
        move(tNewPos);

    int tNewOffsetX = x();// + (frameGeometry().width() - width()) / 2;
    int tNewOffsetY = y();// + (frameGeometry().height() - height()) - (frameGeometry().width() - width()) / 2;
    mMediaSourceDesktop->mGrabOffsetX = tNewOffsetX;
    mMediaSourceDesktop->mGrabOffsetY = tNewOffsetY;
    mLbOffsetX->setText(QString("%1").arg(tNewOffsetX));
    mLbOffsetY->setText(QString("%1").arg(tNewOffsetY));

    pEvent->accept();
}

void SegmentSelectionDialog::resizeEvent(QResizeEvent *pEvent)
{
    int tNewWidth = pEvent->size().width() + (frameGeometry().width() - width());
    if (tNewWidth < MIN_WIDTH)
        tNewWidth = MIN_WIDTH;
    mLbResX->setText(QString("%1").arg(tNewWidth));


    int tNewHeight = pEvent->size().height() + (frameGeometry().height() - height()) + (frameGeometry().width() - width()) / 2;
    if (tNewHeight < MIN_HEIGHT)
        tNewHeight = MIN_HEIGHT;
    mLbResY->setText(QString("%1").arg(tNewHeight));

    mMediaSourceDesktop->SetScreenshotSize(tNewWidth, tNewHeight);

    QDialog::resizeEvent(pEvent);
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
