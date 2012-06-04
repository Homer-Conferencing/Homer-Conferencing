/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: network simulation dock widget
 * Author:  Thomas Volkert
 * Since:   2012-06-03
 */

#ifndef _OVERVIEW_NETWORK_SIMULATION_WIDGET_
#define _OVERVIEW_NETWORK_SIMULATION_WIDGET_

#include <Core/Scenario.h>

#include <QDockWidget>
#include <QStandardItemModel>

#include <ui_OverviewNetworkSimulationWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define NETWORK_SIMULATION_GUI_UPDATE_TIME              250

///////////////////////////////////////////////////////////////////////////////
class HierarchyItem;
class NetworkScene;
class GuiNode;
class GuiLink;

class OverviewNetworkSimulationWidget :
    public QDockWidget,
    public Ui_OverviewNetworkSimulationWidget
{
    Q_OBJECT;
public:
    /// The default constructor
    OverviewNetworkSimulationWidget(QAction *pAssignedAction, QMainWindow *pMainWindow, Scenario *pScenario);

    /// The destructor.
    virtual ~OverviewNetworkSimulationWidget();

public slots:
    void SetVisible(bool pVisible);
    void SelectedNewNetworkItem();

private slots:
    void SelectedCoordinator(QModelIndex pIndex);
    void SelectedStream(QModelIndex pIndex);

private:
    void initializeGUI();
    void InitNetworkView();

    /* update views */
    void UpdateHierarchyView();
    void UpdateStreamsView();
    void UpdateNetworkView();
    void UpdateRoutingView();

    /* helpers for updating views */
    void ShowHierarchyDetails(Coordinator *pCoordinator, Node* pNode);
    void AppendHierarchySubItems(HierarchyItem *pParentItem);
    void ShowStreamDetails(const struct StreamDescriptor pDesc);
    QString CreateStreamId(const struct StreamDescriptor tDesc);
    void FillRoutingTableCell(int pRow, int pCol, QString pText);
    void FillRoutingTableRow(int pRow, RibEntry* pEntry);
    GuiNode *GetGuiNode(Node *pNode);

    virtual void closeEvent(QCloseEvent* pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pEvent);
    virtual void timerEvent(QTimerEvent *pEvent);

    Scenario                *mScenario;
    QMainWindow             *mMainWindow;
    QPoint                  mWinPos;
    QAction                 *mAssignedAction;
    QStandardItemModel      *mTvHierarchyModel, *mTvStreamsModel;
    int                     mTimerId;

    NetworkScene            *mNetworkScene;
    /* manage selection in views */
    // stream view
    int                     mStreamRow;
    // hierarchy view
    void                    *mHierarchyEntry;
    int                     mHierarchyRow;
    int                     mHierarchyCol;
    // network view
    QList<GuiLink*>         mGuiLinks;
    QList<GuiNode*>         mGuiNodes;
    // routing view
    Node                    *mSelectedNode;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif

