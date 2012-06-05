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
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QStandardItemModel>

#include <ui_OverviewNetworkSimulationWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define NETWORK_SIMULATION_GUI_UPDATE_TIME              250

///////////////////////////////////////////////////////////////////////////////

#define GUI_NODE_TYPE   (QGraphicsItem::UserType + 1)

class GuiLink;
class OverviewNetworkSimulationWidget;
class GuiNode:
    public QGraphicsPixmapItem
{
public:
    GuiNode(Node* pNode, OverviewNetworkSimulationWidget* pNetSimWidget);
    ~GuiNode();

    void AddGuiLink(GuiLink *pGuiLink);
    int GetWidth();
    int GetHeight();
    Node* GetNode();

    virtual int type() const;

private:
    virtual QVariant itemChange(GraphicsItemChange pChange, const QVariant &pValue);

    Node            *mNode;
    OverviewNetworkSimulationWidget* mNetSimWidget;
    QGraphicsTextItem *mTextItem;
    QList<GuiLink*> mGuiLinks;
};

///////////////////////////////////////////////////////////////////////////////

#define GUI_LINK_TYPE   (QGraphicsItem::UserType + 2)

class GuiLink:
    public QGraphicsLineItem
{
public:
    GuiLink(GuiNode* pGuiNode0, GuiNode* pGuiNode1, QWidget* pParent);
    ~GuiLink();

    virtual int type() const;

    void UpdatePosition();
    GuiNode* GetGuiNode0();
    GuiNode* GetGuiNode1();

private:
    GuiNode     *mGuiNode0, *mGuiNode1;
    QWidget*    mParent;
};

///////////////////////////////////////////////////////////////////////////////

class NetworkScene:
    public QGraphicsScene
{
public:
    NetworkScene(OverviewNetworkSimulationWidget *pNetSimWidget);
    ~NetworkScene();

    virtual void wheelEvent(QGraphicsSceneWheelEvent  *pEvent);

    void Scale(qreal pFactor);
private:
    qreal     mScaleFactor;
    OverviewNetworkSimulationWidget *mNetSimWidget;
};

///////////////////////////////////////////////////////////////////////////////

class HierarchyItem;
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

    void UpdateRoutingView();

public slots:
    void SetVisible(bool pVisible);
    void SelectedNewNetworkItem();

private slots:
    void SelectedCoordinator(QModelIndex pIndex);
    void SelectedStream(QModelIndex pIndex);
    void NetworkViewZoomChanged(int pZoom);

private:
    void initializeGUI();
    void InitNetworkView();

    /* update views */
    void UpdateHierarchyView();
    void UpdateStreamsView();
    void UpdateNetworkView();

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

