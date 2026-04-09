#include "GroupGraphicsObject.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

#include "AbstractGraphModel.hpp"
#include "AbstractGroupPainter.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "GroupStyle.hpp"
#include "NodeGraphicsObject.hpp"
#include "QtNodes/GroupIdUtils"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"
#include "ConnectionGraphicsObject.hpp"
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QStyleOptionGraphicsItem>

namespace QtNodes {

GroupGraphicsObject::GroupGraphicsObject(BasicGraphicsScene &scene,
                                          GroupId const groupId)
    : _groupId(groupId)
    , _graphModel(scene.graphModel())
{
    scene.addItem(this);

    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    setAcceptHoverEvents(true);

    // addGraphicsEffect();
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setZValue(-2.0);
    GroupStyle Style=StyleCollection::groupStyle();
    setOpacity(Style.Opacity);

    _boundsUpdateTimer = new QTimer(this);
    _boundsUpdateTimer->setSingleShot(true);
    //合并5ms以内的重复计算
    _boundsUpdateTimer->setInterval(5);
    connect(_boundsUpdateTimer,
            &QTimer::timeout,
            this,
            &GroupGraphicsObject::updateGroupBounds);

    // 连接节点位置更新信号
    connect(&_graphModel, &AbstractGraphModel::nodePositionUpdated,
            this, &GroupGraphicsObject::onNodePositionUpdated);
    
    // 连接节点更新信号
    connect(&_graphModel, &AbstractGraphModel::nodeUpdated,
            this, &GroupGraphicsObject::onNodeUpdated);

    // 添加节点删除信号连接
    connect(&_graphModel, &AbstractGraphModel::nodeDeleted,
            this, &GroupGraphicsObject::onNodeDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::groupFlagsUpdated,
            this,
            &GroupGraphicsObject::onLockedState);

    updatePosition();
    //默认展开
    setCollapsed(_groupId.collapsed, false);
}

void GroupGraphicsObject::updatePosition()
{
    updateGroupBounds();
}
// 设置分组内节点是否可视
void GroupGraphicsObject::setGroupItemsVisible(bool visible)
{
    auto* scene = nodeScene();
    if (!scene)
        return;

    for (const NodeId& nodeId : _groupId.nodeIds) {
        if (auto* nodeItem = scene->nodeGraphicsObject(nodeId)) {
            nodeItem->setVisible(visible);
        }
    }
}
//设置分组折叠
void GroupGraphicsObject::setCollapsed(bool collapsed, bool updateModel)
{
    if (_collapsed == collapsed)
        return;

    auto oldGroupId = _groupId;

    _collapsed = collapsed;
    _groupId.collapsed = collapsed;

    if (_boundsUpdateTimer)
        _boundsUpdateTimer->stop();

    setGroupItemsVisible(!collapsed);

    if (collapsed) {
        setZValue(-0.5);

        auto const &groupStyle = QtNodes::StyleCollection::groupStyle();

        qreal const collapsedHeight = groupStyle.CollapsedHeight > 0 ? groupStyle.CollapsedHeight : groupStyle.CaptionHeight;5.0;

        prepareGeometryChange();
        _rect = QRectF(0, 0, _rect.width(), collapsedHeight);
        update();
    } else {
        setZValue(-2.0);

        updateGroupBounds();
    }

    auto* scene = nodeScene();
    if (scene) {
        std::unordered_set<ConnectionId> affectedConnections;

        for (const NodeId& nodeId : _groupId.nodeIds) {
            auto const conns = _graphModel.allConnectionIds(nodeId);
            affectedConnections.insert(conns.begin(), conns.end());
        }

        for (auto const &connId : affectedConnections) {
            if (auto* connItem = scene->connectionGraphicsObject(connId)) {
                connItem->move();
            }
        }
    }

    if (updateModel) {
        graphModel().updateGroup(oldGroupId, _groupId);
    }
}
//折叠后代理端口显示位置
QPointF GroupGraphicsObject::collapsedPortScenePosition(PortType portType) const
{
    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();

    QRectF const r = _rect;

    qreal const x = (portType == PortType::In) ? 0.0 : r.width();

    qreal const collapsedHeight = groupStyle.CollapsedHeight > 0 ? groupStyle.CollapsedHeight : groupStyle.CaptionHeight;
    qreal const y = groupStyle.CaptionHeight + (collapsedHeight - groupStyle.CaptionHeight) * 0.5;

    return mapToScene(QPointF(x, y));
}
//应用分组ID
void GroupGraphicsObject::applyGroupId(GroupId const &groupId)
{
    auto* scene = nodeScene();
    if (!scene)
        return;

    std::unordered_set<NodeId> oldNodes(_groupId.nodeIds.begin(), _groupId.nodeIds.end());
    std::unordered_set<NodeId> newNodes(groupId.nodeIds.begin(), groupId.nodeIds.end());

    std::unordered_set<NodeId> removedNodes;
    for (auto const nid : oldNodes) {
        if (newNodes.count(nid) == 0) {
            removedNodes.insert(nid);
        }
    }

    std::unordered_set<ConnectionId> affectedConnections;
    for (auto const nid : oldNodes) {
        auto const conns = _graphModel.allConnectionIds(nid);
        affectedConnections.insert(conns.begin(), conns.end());
    }
    for (auto const nid : newNodes) {
        auto const conns = _graphModel.allConnectionIds(nid);
        affectedConnections.insert(conns.begin(), conns.end());
    }

    _groupId = groupId;

    for (auto const nid : removedNodes) {
        if (auto* nodeItem = scene->nodeGraphicsObject(nid)) {
            nodeItem->setVisible(true);
        }
    }

    setCollapsed(_groupId.collapsed, false);

    updateGroupBounds();

    for (auto const &cid : affectedConnections) {
        if (auto* cgo = scene->connectionGraphicsObject(cid)) {
            cgo->move();
        }
    }
}

AbstractGraphModel &GroupGraphicsObject::graphModel() const
{
    return _graphModel;
}

BasicGraphicsScene *GroupGraphicsObject::nodeScene() const
{
    return dynamic_cast<BasicGraphicsScene *>(scene());
}

QRectF GroupGraphicsObject::boundingRect() const
{
    if (_collapsed) {
        auto const &groupStyle = StyleCollection::groupStyle();
        auto const &nodeStyle = StyleCollection::nodeStyle();

        qreal portWidth = groupStyle.PortWidth;
        if (portWidth <= 0.0)
            portWidth = nodeStyle.ConnectionPointDiameter;

        qreal const maxBorder = std::max<qreal>(groupStyle.PenWidth, groupStyle.HoveredPenWidth);
        qreal const halfW = portWidth * 0.5 + maxBorder;
        return _rect.adjusted(-halfW, 0.0, halfW, 0.0);
    }

    return _rect;
}

void GroupGraphicsObject::move()
{}

void GroupGraphicsObject::paint(QPainter *painter,
                                     QStyleOptionGraphicsItem const *option,
                                     QWidget *)
{
    painter->setClipRect(option->exposedRect);

    nodeScene()->groupPainter().paint(painter, *this);
}

void GroupGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    _pressedOnCaption = false;

    if (graphModel().nodeFlags().testFlag(NodeFlag::Locked)) {
        // 锁定状态下忽略鼠标左键
        event->ignore();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();
    QRectF const captionRect(0, 0, _rect.width(), groupStyle.CaptionHeight);

    if (!captionRect.contains(event->pos())) {
        event->ignore();
        return;
    }

    QGraphicsItem::mousePressEvent(event);

    if (!isSelected()) {
        //分组没有被选中，事件忽略
        event->accept();
        return;
    }

    _pressedOnCaption = true;

    auto* scene = nodeScene();
    if (!scene) {
        event->ignore();
        return;
    }

    for (const NodeId& nodeId : _groupId.nodeIds) {
        if (auto* nodeItem = scene->nodeGraphicsObject(nodeId)) {
            nodeItem->setSelected(true);
        }
    }

    event->accept();
}

void GroupGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (graphModel().nodeFlags().testFlag(NodeFlag::Locked)) {
        // 锁定状态下忽略鼠标拖拽
        event->ignore();
        return;
    }

    if (!_pressedOnCaption || !isSelected()) {
        event->ignore();
        return;
    }

    auto* scene = nodeScene();
    if (!scene) {
        event->ignore();
        return;
    }

    auto diff = event->pos() - event->lastPos();

    std::unordered_set<NodeId> nodesToMove;

    for (QGraphicsItem *item : scene->selectedItems()) {
        if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            nodesToMove.insert(n->nodeId());
        } else if (auto g = qgraphicsitem_cast<GroupGraphicsObject *>(item)) {
            for (auto const nid : g->groupId().nodeIds) {
                nodesToMove.insert(nid);
            }
        }
    }

    scene->undoStack().push(new MoveNodeCommand(scene, diff, std::move(nodesToMove)));

    QGraphicsItem::mouseMoveEvent(event);
}

void GroupGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();
    QRectF const captionRect(0, 0, _rect.width(), groupStyle.CaptionHeight);

    if (!captionRect.contains(event->pos())) {
        event->ignore();
        return;
    }
    // 双击设置折叠
    setCollapsed(!_collapsed);
    event->accept();
}

void GroupGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    _pressedOnCaption = false;
    QGraphicsItem::mouseReleaseEvent(event);
}

void GroupGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverEnterEvent(event);
}

void GroupGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverLeaveEvent(event);
}

void GroupGraphicsObject::addGraphicsEffect()
{
    auto effect = new QGraphicsBlurEffect;

    effect->setBlurRadius(1);
    setGraphicsEffect(effect);

    //auto effect = new QGraphicsDropShadowEffect;
    //auto effect = new GroupBlurEffect(this);
    //effect->setOffset(4, 4);
    //effect->setColor(QColor(Qt::gray).darker(800));
}

QPainterPath GroupGraphicsObject::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}
void GroupGraphicsObject::keyPressEvent(QKeyEvent *event)
{
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }
    // 修改条件：添加Ctrl修饰键判断
    if ((event->key() == Qt::Key_E) && (event->modifiers() & Qt::ControlModifier)){
        startEditingRemarks();
        event->accept();
        return;
    }
    QGraphicsObject::keyPressEvent(event);
}
void GroupGraphicsObject::initRemarksEditor()
{
    if (!_remarksEditor) {
        _remarksEditor = new QLineEdit();
        _remarksEditor->setStyleSheet(
            "QLineEdit {"
            "  background-color: #2D2D2D;"
            "  border: 1px solid #4D4D4D;"
            "  border-radius: 3px;"
            "  color: white;"
            "  padding: 2px 6px;"
            "}"
            "QLineEdit:focus {"
            "  border: 1px solid #6D6D6D;"
            "}"
        );

        connect(_remarksEditor, &QLineEdit::editingFinished,
                this, &GroupGraphicsObject::finishEditingRemarks);

        // 按ESC取消编辑
        _remarksEditor->installEventFilter(this);
    }
}

void GroupGraphicsObject::startEditingRemarks()
{
    initRemarksEditor();

//    // 设置编辑器位置和大小
    auto* scene = static_cast<BasicGraphicsScene*>(this->scene());
//    auto& geometry = scene->nodeGeometry();
    GroupStyle Style=StyleCollection::groupStyle();
    QRectF captionRect =QRectF(0, 0, _rect.width()-1, Style.CaptionHeight-1);
    QRectF sceneRect = mapToScene(captionRect).boundingRect();
//
    _remarksEditor->setText(_groupId.groupRemarks);
    _remarksEditor->setGeometry(
        scene->views().first()->mapFromScene(sceneRect).boundingRect()
    );
//
    // 显示编辑器
    _remarksEditor->setParent(scene->views().first()->viewport());
    _remarksEditor->show();
    _remarksEditor->setFocus();
    _remarksEditor->selectAll();
}

void GroupGraphicsObject::finishEditingRemarks()
{
    if (!_remarksEditor) return;
    // 隐藏编辑器
    _remarksEditor->hide();
    _remarksEditor->setParent(nullptr);
    // 更新组的remarks
    if (_groupId.groupRemarks != _remarksEditor->text()) {
        setRemarks(_remarksEditor->text());
        //更新模型，由于groupId不匹配remarks，所以原地更新
        graphModel().updateGroup(_groupId,_groupId);
    }
    this->setFocus();
    update();
}

// bool GroupGraphicsObject::eventFilter(QObject* watched, QEvent* event)
// {
//     if (watched == _remarksEditor) {
//         if (event->type() == QEvent::KeyPress) {
//             QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
//             if (keyEvent->key() == Qt::Key_Escape) {
//                 _remarksEditor->hide();
//                 _remarksEditor->setParent(nullptr);
//                 this->setFocus();
//                 return true;
//             }
//         }
//     }
//     return QGraphicsObject::eventFilter(watched, event);
// }

// 当组被取消选中时，也取消组内节点的选中状态
QVariant GroupGraphicsObject::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        bool newSelected = value.toBool();
        // 如果组被取消选中，同时取消组内节点的选中状态
        if (!newSelected) {
            auto* scene = nodeScene();
            if (scene) {
                for (const NodeId& nodeId : _groupId.nodeIds) {
                    if (auto* nodeItem = scene->nodeGraphicsObject(nodeId)) {
                      nodeItem->setSelected(false);
                    }
                }
            }
        }
    }
    
    return QGraphicsItem::itemChange(change, value);
}

void GroupGraphicsObject::scheduleGroupBoundsUpdate()
{
    if (!_boundsUpdateTimer)
        return;

    _boundsUpdateTimer->start();
}

// 添加新的槽函数来处理节点位置更新
void GroupGraphicsObject::onNodePositionUpdated(NodeId nodeId)
{
    // 检查更新的节点是否属于此组
    if (std::find(_groupId.nodeIds.begin(), _groupId.nodeIds.end(), nodeId) != _groupId.nodeIds.end()) {
        scheduleGroupBoundsUpdate();
    }
}

void GroupGraphicsObject::onNodeUpdated(NodeId nodeId)
{
    // 检查更新的节点是否属于此组
    if (std::find(_groupId.nodeIds.begin(), _groupId.nodeIds.end(), nodeId) != _groupId.nodeIds.end()) {
        scheduleGroupBoundsUpdate();
    }

}

// 添加节点删除信号连接
void GroupGraphicsObject::onNodeDeleted(NodeId nodeId)
{

    auto oldGroupId = _groupId;

    // 检查删除的节点是否属于此组
    auto it = std::find(_groupId.nodeIds.begin(), _groupId.nodeIds.end(), nodeId);
    if (it != _groupId.nodeIds.end()) {
        _groupId.nodeIds.erase(it);
        if (_groupId.nodeIds.empty()) {
            graphModel().deleteGroup(oldGroupId); // 直接删除空分组
            return;
        }
    }
    // 如果组内没有节点，删除组
    graphModel().updateGroup(oldGroupId,_groupId);

    scheduleGroupBoundsUpdate();
}

// 更新组的边界
void GroupGraphicsObject::updateGroupBounds()
{

    if (_groupId.nodeIds.size() == 0) {
        // 如果组内没有节点，保持最后一次的位置和大小
        return;
    }

    QPointF oldPos = pos();
    QRectF oldRect = _rect;

    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();

    // 遍历所有节点计算包围盒
    for (auto nodeId : _groupId.nodeIds) {
        QPointF pos = _graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>();
        QSize size = _graphModel.nodeData(nodeId, NodeRole::Size).value<QSize>();

        minX = qMin(minX, pos.x());
        minY = qMin(minY, pos.y());
        maxX = qMax(maxX, pos.x() + size.width());
        maxY = qMax(maxY, pos.y() + size.height());
    }

    // 添加边距
    const qreal margin = 50;
    QRectF newBounds(minX - margin,
                     minY - margin,
                     (maxX - minX) + 2 * margin,
                     (maxY - minY) + 2 * margin);

    // 设置新的位置和大小
    prepareGeometryChange(); // 通知Qt即将改变图形项的几何形状
    setPos(newBounds.topLeft());

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();

    qreal height = newBounds.height();
    if (_collapsed) {
        height = groupStyle.CollapsedHeight > 0 ? groupStyle.CollapsedHeight : groupStyle.CaptionHeight;
    }

    _rect = QRectF(0, 0, newBounds.width(), height);

    // 如果大小或位置发生变化，触发更新
    if (oldPos != pos() || oldRect != _rect) {
        update();

        if (_collapsed) {
            auto* scene = nodeScene();
            if (scene) {
                std::unordered_set<ConnectionId> affectedConnections;

                for (const NodeId& nodeId : _groupId.nodeIds) {
                    auto const conns = _graphModel.allConnectionIds(nodeId);
                    affectedConnections.insert(conns.begin(), conns.end());
                }

                for (auto const &connId : affectedConnections) {
                    if (auto* connItem = scene->connectionGraphicsObject(connId)) {
                        connItem->move();
                    }
                }
            }
        }
    }
}
void GroupGraphicsObject::setLockedState() {

    NodeFlags flags = _graphModel.nodeFlags();

    bool const locked = flags.testFlag(NodeFlag::Locked);

    if (locked) {
        if (!_lockForcesCollapse) {
            _lockForcesCollapse = true;
            _collapsedBeforeLock = _groupId.collapsed;
        }

        setCollapsed(true, false);
        _groupId.collapsed = _collapsedBeforeLock;
    } else {
        if (_lockForcesCollapse) {
            _lockForcesCollapse = false;
            setCollapsed(_groupId.collapsed, false);
        }
    }

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);

}
void GroupGraphicsObject::onLockedState(GroupId groupId)
{
    if (groupId == _groupId) {
        setLockedState();
    }
}
void GroupGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (graphModel().nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }

    QMenu m_Menu;
    QAction* renameAction = m_Menu.addAction( "Edit Remarks");
    renameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));  // 添加快捷键
    connect(renameAction, &QAction::triggered, [this]() {
        startEditingRemarks(); // 假设这是重命名功能
    });
    auto* scene = this->scene();
    auto views = scene ? scene->views() : QList<QGraphicsView*>();
    if (!views.isEmpty()) {
        auto* view = views.first();
        // 遍历 view 的 actions
        for (QAction* act : view->actions()) {
            m_Menu.addAction(act);
        }
    }

    // 显示菜单并等待用户选择
     m_Menu.exec(event->screenPos());

    // 如果用户没有选择任何项，仍然传递信号给scene
    // if (!selectedAction) {
    //     Q_EMIT nodeScene()->nodeContextMenu(_nodeId, mapToScene(event->pos()));
    // }

    event->accept(); // 确保事件被处理
}
} // namespace QtNodes
