#pragma once

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "ConnectionIdHash.hpp"
#include "Definitions.hpp"
#include "Export.hpp"

#include "QUuidStdHash.hpp"

#include <QtCore/QPointer>
#include <QtCore/QUuid>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QMenu>

#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>


#include "GroupIdHash.hpp"
#include "GroupGraphicsObject.hpp"
#include "AbstractGroupPainter.hpp"
class QUndoStack;

namespace QtNodes {

class AbstractConnectionPainter;
class AbstractGraphModel;
class AbstractNodePainter;
class AbstractGroupPainter;
class ConnectionGraphicsObject;
class GroupGraphicsObject;
class NodeGraphicsObject;
class NodeStyle;

/// Context for right-click menus (shared actions from GraphicsView).
enum class ContextMenuKind {
    Node,
    Connection,
    Group,
    Scene, ///< Blank canvas (no item under cursor)
};

/// An instance of QGraphicsScene, holds connections and nodes.
class NODE_EDITOR_PUBLIC BasicGraphicsScene : public QGraphicsScene
{
    Q_OBJECT
public:
    BasicGraphicsScene(AbstractGraphModel &graphModel, QObject *parent = nullptr);

    // Scenes without models are not supported
    BasicGraphicsScene() = delete;

    ~BasicGraphicsScene();

public:
    /// @returns associated AbstractGraphModel.
    AbstractGraphModel const &graphModel() const;

    AbstractGraphModel &graphModel();

    AbstractNodeGeometry const &nodeGeometry() const;

    AbstractNodeGeometry &nodeGeometry();

    AbstractNodePainter &nodePainter();

    AbstractConnectionPainter &connectionPainter();

    AbstractGroupPainter &groupPainter();

    void setNodePainter(std::unique_ptr<AbstractNodePainter> newPainter);

    void setConnectionPainter(std::unique_ptr<AbstractConnectionPainter> newPainter);

    void setGroupPainter(std::unique_ptr<AbstractGroupPainter> newPainter);

    QUndoStack &undoStack();

public:
    /// Creates a "draft" instance of ConnectionGraphicsObject.
    /**
   * The scene caches a "draft" connection which has one loose end.
   * After attachment the "draft" instance is deleted and instead a
   * normal "full" connection is created.
   * Function @returns the "draft" instance for further geometry
   * manipulations.
   */
    std::unique_ptr<ConnectionGraphicsObject> const &makeDraftConnection(
        ConnectionId const newConnectionId);

    /// Deletes "draft" connection.
    /**
   * The function is called when user releases the mouse button during
   * the construction of the new connection without attaching it to any
   * node.
   */
    void resetDraftConnection();

    /// Deletes all the nodes. Connections are removed automatically.
    void clearScene();

public:
    /// @returns NodeGraphicsObject associated with the given nodeId.
    /**
   * @returns nullptr when the object is not found.
   */
    NodeGraphicsObject *nodeGraphicsObject(NodeId nodeId);

    /// @returns ConnectionGraphicsObject corresponding to `connectionId`.
    /**
   * @returns `nullptr` when the object is not found.
   */
    ConnectionGraphicsObject *connectionGraphicsObject(ConnectionId connectionId);

    GroupGraphicsObject *groupGraphicsObject(GroupId groupId);

    Qt::Orientation orientation() const { return _orientation; }

    void setOrientation(Qt::Orientation const orientation);

public:
    /// Can @return an instance of the scene context menu in subclass.
    /**
   * Used for blank-canvas create-node UI (examples: right-click; app: often double-click).
   * Default implementation returns `nullptr`.
   */
    virtual QMenu *createSceneMenu(QPointF const scenePos);

    /**
     * Append shared/edit actions into a context menu.
     * Default for Node/Connection/Group: all GraphicsView actions (examples).
     * Default for Scene: no-op (GraphicsView then falls back to createSceneMenu).
     */
    virtual void appendContextMenuActions(QMenu &menu, ContextMenuKind kind);

Q_SIGNALS:
    void modified(BasicGraphicsScene *);

    void nodeMoved(NodeId const nodeId, QPointF const &newLocation);

    void nodeClicked(NodeId const nodeId);

    void nodeSelected(NodeId const nodeId);

    void nodeDoubleClicked(NodeId const nodeId);

    void nodeHovered(NodeId const nodeId, QPoint const screenPos);

    void nodeHoverLeft(NodeId const nodeId);

    void connectionHovered(ConnectionId const connectionId, QPoint const screenPos);

    void connectionHoverLeft(ConnectionId const connectionId);

    /// Signal allows showing custom context menu upon clicking a node.
    void nodeContextMenu(NodeId const nodeId, QPointF const pos);

private:
    /// @brief Creates Node and Connection graphics objects.
    /**
   * Function is used to populate an empty scene in the constructor. We
   * perform depth-first AbstractGraphModel traversal. The connections are
   * created by checking non-empty node `Out` ports.
   */
    void traverseGraphAndPopulateGraphicsObjects();

    /// Redraws adjacent nodes for given `connectionId`
    void updateAttachedNodes(ConnectionId const connectionId, PortType const portType);

public Q_SLOTS:
    /// Slot called when the `connectionId` is erased form the AbstractGraphModel.
    void onConnectionDeleted(ConnectionId const connectionId);

    /// Slot called when the `connectionId` is created in the AbstractGraphModel.
    void onConnectionCreated(ConnectionId const connectionId);

    /// Slot called when connection display data changes.
    void onConnectionUpdated(ConnectionId const connectionId);

    void onNodeDeleted(NodeId const nodeId);

    void onNodeCreated(NodeId const nodeId);

    void onNodePositionUpdated(NodeId const nodeId);

    void onNodeWidgetUpdated(NodeId const nodeId);

    void onNodeUpdated(NodeId const nodeId);

    void onNodeClicked(NodeId const nodeId);

    void onGroupCreated(GroupId const groupId);

    void onGroupDeleted(GroupId const groupId);

    void onGroupUpdate(GroupId const groupId);

    void onModelReset();

    void centerOnNode(NodeId nodeId);

    /**
     * @brief 弹出节点搜索条（横向：搜索框 | ← → | 当前索引/总数）
     *
     * 按 Remarks / Type / Caption / NodeId 以及虚拟连线 tag 标签过滤；
     * ←→ 与 Enter 在匹配结果间选中并居中。每个场景最多一个搜索条；
     * 重复调用会复用并聚焦输入框。
     */
    void showSearchNodeBar();

private:
    /// 选中节点并居中到视图
    void selectAndCenterNode(NodeId nodeId);

    /// 选中连线并居中到视图（虚拟连线会展开 tag）
    void selectAndCenterConnection(ConnectionId connectionId);

    AbstractGraphModel &_graphModel;

    using UniqueNodeGraphicsObject = std::unique_ptr<NodeGraphicsObject>;

    using UniqueConnectionGraphicsObject = std::unique_ptr<ConnectionGraphicsObject>;

    using UniqueGroupGraphicsObject = std::unique_ptr<GroupGraphicsObject>;

    std::unordered_map<NodeId, UniqueNodeGraphicsObject> _nodeGraphicsObjects;

    std::unordered_map<ConnectionId, UniqueConnectionGraphicsObject> _connectionGraphicsObjects;

    std::unordered_map<GroupId, UniqueGroupGraphicsObject> _groupGraphicsObjects;

    std::unique_ptr<ConnectionGraphicsObject> _draftConnection;

    std::unique_ptr<GroupGraphicsObject> _draftGroup;

    std::unique_ptr<AbstractNodeGeometry> _nodeGeometry;

    std::unique_ptr<AbstractNodePainter> _nodePainter;

    std::unique_ptr<AbstractGroupPainter> _groupPainter;

    std::unique_ptr<AbstractConnectionPainter> _connectionPainter;

    bool _nodeDrag;

    QUndoStack *_undoStack;

    Qt::Orientation _orientation;

    /// 当前场景的节点搜索条（无边框浮动条；关闭后指针自动清空）
    QPointer<QDialog> _searchNodeBar;
};

} // namespace QtNodes
