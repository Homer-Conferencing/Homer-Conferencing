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
#include <QGraphicsSceneContextMenuEvent>

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

    virtual int type() const;
    Node* GetNode();

    void AddGuiLink(GuiLink *pGuiLink);
    int GetWidth();
    int GetHeight();

private:
    virtual QVariant itemChange(GraphicsItemChange pChange, const QVariant &pValue);
    void UpdateText(bool pSelected);

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
    GuiLink(Link *pLink, OverviewNetworkSimulationWidget* pNetSimWidget);
    ~GuiLink();

    virtual int type() const;
    Link* GetLink();

    void UpdatePosition();
    void UpdateColoring();
    GuiNode* GetGuiNode0();
    GuiNode* GetGuiNode1();

    void ShowContextMenu(QGraphicsSceneContextMenuEvent *pEvent);
    void ShowCapSettings();

private:
    GuiNode     *mGuiNode0, *mGuiNode1;
    OverviewNetworkSimulationWidget* mNetSimWidget;
    Link        *mLink;
};

///////////////////////////////////////////////////////////////////////////////

class NetworkScene:
    public QGraphicsScene
{
public:
    NetworkScene(OverviewNetworkSimulationWidget *pNetSimWidget);
    ~NetworkScene();

    virtual void wheelEvent(QGraphicsSceneWheelEvent  *pEvent);
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *pEvent);

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
    void NetworkViewZoomChanged(int pZoom);

    GuiNode *GetGuiNode(Node *pNode);

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

    /* helpers for updating views */
    void ShowHierarchyDetails(Coordinator *pCoordinator, Node* pNode);
    void AppendHierarchySubItems(HierarchyItem *pParentItem);
    void ShowStreamDetails(const struct StreamDescriptor pDesc);
    QString CreateStreamId(const struct StreamDescriptor tDesc);
    void FillRoutingTableCell(int pRow, int pCol, QString pText);
    void FillRoutingTableRow(int pRow, RibEntry* pEntry);

    virtual void closeEvent(QCloseEvent* pEvent);
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

