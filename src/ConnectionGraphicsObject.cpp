#include "ConnectionGraphicsObject.hpp"

#include "AbstractConnectionPainter.hpp"
#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionIdUtils.hpp"
#include "ConnectionState.hpp"
#include "ConnectionStyle.hpp"
#include "GroupGraphicsObject.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeGraphicsObject.hpp"
#include "StyleCollection.hpp"
#include "locateNode.hpp"

#include <algorithm>

#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <QtCore/QDebug>

#include <stdexcept>

namespace QtNodes {

ConnectionGraphicsObject::ConnectionGraphicsObject(BasicGraphicsScene &scene,
                                                   ConnectionId const connectionId)
    : _connectionId(connectionId)
    , _graphModel(scene.graphModel())
    , _connectionState(*this)
    , _out{0, 0}
    , _in{0, 0}
{
    scene.addItem(this);

    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    setLockedState();

    connect(&_graphModel,
            &AbstractGraphModel::nodeFlagsUpdated,
            this,
            &ConnectionGraphicsObject::onLockedState);

    setAcceptHoverEvents(true);

    //addGraphicsEffect();

    setZValue(-1.0);

    initializePosition();
}

void ConnectionGraphicsObject::initializePosition()
{
    // This function is only called when the ConnectionGraphicsObject
    // is newly created. At this moment both end coordinates are (0, 0)
    // in Connection G.O. coordinates. The position of the whole
    // Connection G. O. in scene coordinate system is also (0, 0).
    // By moving the whole object to the Node Port position
    // we position both connection ends correctly.

    if (_connectionState.requiredPort() != PortType::None) {
        PortType attachedPort = oppositePort(_connectionState.requiredPort());

        PortIndex portIndex = getPortIndex(attachedPort, _connectionId);
        NodeId nodeId = getNodeId(attachedPort, _connectionId);

        GroupGraphicsObject* collapsedGroup = nullptr;
        auto const groups = _graphModel.allGroupIds();
        for (auto const &gid : groups) {
            if (std::find(gid.nodeIds.begin(), gid.nodeIds.end(), nodeId) != gid.nodeIds.end()) {
                if (auto* ggo = nodeScene()->groupGraphicsObject(gid)) {
                    if (ggo->isCollapsed()) {
                        //折叠状态下端口连接位置设置为分组所持有的代理端口
                        collapsedGroup = ggo;
                    }
                }
                break;
            }
        }

        if (collapsedGroup) {
            //折叠状态下端口连接位置设置为分组所持有的代理端口
            this->setPos(collapsedGroup->collapsedPortScenePosition(attachedPort));
        } else {
            //否则设置为普通节点所持有的端口
            NodeGraphicsObject *ngo = nodeScene()->nodeGraphicsObject(nodeId);

            if (ngo) {
                QTransform nodeSceneTransform = ngo->sceneTransform();
                AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
                QPointF pos = geometry.portScenePosition(nodeId,
                                                         attachedPort,
                                                         portIndex,
                                                         nodeSceneTransform);
                this->setPos(pos);
            }
        }
    }

    move();
}

AbstractGraphModel &ConnectionGraphicsObject::graphModel() const
{
    return _graphModel;
}

BasicGraphicsScene *ConnectionGraphicsObject::nodeScene() const
{
    return dynamic_cast<BasicGraphicsScene *>(scene());
}

ConnectionId const &ConnectionGraphicsObject::connectionId() const
{
    return _connectionId;
}

QRectF ConnectionGraphicsObject::boundingRect() const
{
    auto points = pointsC1C2();

    // `normalized()` fixes inverted rects.
    QRectF basicRect = QRectF(_out, _in).normalized();

    QRectF c1c2Rect = QRectF(points.first, points.second).normalized();

    QRectF commonRect = basicRect.united(c1c2Rect);

    auto const &connectionStyle = StyleCollection::connectionStyle();
    float const diam = connectionStyle.pointDiameter();
    QPointF const cornerOffset(diam, diam);

    // Expand rect by port circle diameter
    commonRect.setTopLeft(commonRect.topLeft() - cornerOffset);
    commonRect.setBottomRight(commonRect.bottomRight() + 2 * cornerOffset);

    return commonRect;
}

QPainterPath ConnectionGraphicsObject::shape() const
{
#ifdef DEBUG_DRAWING

    //QPainterPath path;

    //path.addRect(boundingRect());
    //return path;

#else
    return nodeScene()->connectionPainter().getPainterStroke(*this);
#endif
}

QPointF const &ConnectionGraphicsObject::endPoint(PortType portType) const
{
    Q_ASSERT(portType != PortType::None);

    return (portType == PortType::Out ? _out : _in);
}

void ConnectionGraphicsObject::setEndPoint(PortType portType, QPointF const &point)
{
    if (portType == PortType::In)
        _in = point;
    else
        _out = point;
}

void ConnectionGraphicsObject::move()
{
    // 根据节点ID查找其所属的折叠分组
    auto collapsedGroupForNode = [this](NodeId nodeId) -> GroupGraphicsObject* {
        if (nodeId == InvalidNodeId)
            return nullptr;

        auto const groups = _graphModel.allGroupIds();
        for (auto const &gid : groups) {
            // 若分组中包含该节点
            if (std::find(gid.nodeIds.begin(), gid.nodeIds.end(), nodeId) != gid.nodeIds.end()) {
                if (auto* ggo = nodeScene()->groupGraphicsObject(gid)) {
                    if (ggo->isCollapsed()) {
                        return ggo; // 返回折叠的分组
                    }
                }
                return nullptr;
            }
        }
        return nullptr;
    };

    // 分别获取起点和终点节点所在的折叠分组
    GroupGraphicsObject* outGroup = collapsedGroupForNode(_connectionId.outNodeId);
    GroupGraphicsObject* inGroup = collapsedGroupForNode(_connectionId.inNodeId);

    // 若两端都在同一折叠分组内，则隐藏连线
    if (outGroup && inGroup && outGroup == inGroup) {
        setVisible(false);
        return;
    }

    setVisible(true);

    // 移动连线端点到对应端口位置
    auto moveEnd = [this, outGroup, inGroup](ConnectionId cId, PortType portType) {
        NodeId nodeId = getNodeId(portType, cId);

        if (nodeId == InvalidNodeId)
            return;

        // 若端口属于折叠分组，则使用分组的代理端口位置
        GroupGraphicsObject* ggo = (portType == PortType::Out) ? outGroup : inGroup;

        if (ggo) {
            QPointF scenePos = ggo->collapsedPortScenePosition(portType);
            QPointF connectionPos = sceneTransform().inverted().map(scenePos);
            setEndPoint(portType, connectionPos);
            return;
        }

        // 否则使用普通节点的端口位置
        NodeGraphicsObject *ngo = nodeScene()->nodeGraphicsObject(nodeId);

        if (ngo) {
            AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

            QPointF scenePos = geometry.portScenePosition(nodeId,
                                                          portType,
                                                          getPortIndex(portType, cId),
                                                          ngo->sceneTransform());

            QPointF connectionPos = sceneTransform().inverted().map(scenePos);

            setEndPoint(portType, connectionPos);
        }
    };

    // 更新起点和终点
    moveEnd(_connectionId, PortType::Out);
    moveEnd(_connectionId, PortType::In);

    prepareGeometryChange();

    update();
}

ConnectionState const &ConnectionGraphicsObject::connectionState() const
{
    return _connectionState;
}

ConnectionState &ConnectionGraphicsObject::connectionState()
{
    return _connectionState;
}

void ConnectionGraphicsObject::setLockedState()
{
    NodeFlags flags = _graphModel.nodeFlags();

    bool const locked = flags.testFlag(NodeFlag::Locked);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);
}

void ConnectionGraphicsObject::onLockedState(NodeId nodeId)
{
    if (nodeId == 0 || nodeId == _connectionId.outNodeId || nodeId == _connectionId.inNodeId) {
        setLockedState();
    }
}

void ConnectionGraphicsObject::paint(QPainter *painter,
                                     QStyleOptionGraphicsItem const *option,
                                     QWidget *)
{
    if (!scene())
        return;

    painter->setClipRect(option->exposedRect);

    nodeScene()->connectionPainter().paint(painter, *this);
}

void ConnectionGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

void ConnectionGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    prepareGeometryChange();

    auto view = static_cast<QGraphicsView *>(event->widget());
    auto ngo = locateNodeAt(event->scenePos(), *nodeScene(), view->transform());
    if (ngo) {
        ngo->reactToConnection(this);

        _connectionState.setLastHoveredNode(ngo->nodeId());
    } else {
        _connectionState.resetLastHoveredNode();
    }

    //-------------------

    auto requiredPort = _connectionState.requiredPort();

    if (requiredPort != PortType::None) {
        setEndPoint(requiredPort, event->pos());
    }

    //-------------------

    update();

    event->accept();
}

void ConnectionGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    ungrabMouse();
    event->accept();

    auto view = static_cast<QGraphicsView *>(event->widget());

    Q_ASSERT(view);

    auto ngo = locateNodeAt(event->scenePos(), *nodeScene(), view->transform());

    bool wasConnected = false;

    if (ngo) {
        NodeConnectionInteraction interaction(*ngo, *this, *nodeScene());

        wasConnected = interaction.tryConnect();
    }

    // If connection attempt was unsuccessful
    if (!wasConnected) {
        // Resulting unique_ptr is not used and automatically deleted.
        nodeScene()->resetDraftConnection();
    }
}

void ConnectionGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    _connectionState.setHovered(true);

    update();

    // Signal
    nodeScene()->connectionHovered(connectionId(), event->screenPos());

    event->accept();
}

void ConnectionGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    _connectionState.setHovered(false);

    update();

    // Signal
    nodeScene()->connectionHoverLeft(connectionId());

    event->accept();
}

std::pair<QPointF, QPointF> ConnectionGraphicsObject::pointsC1C2() const
{
    switch (nodeScene()->orientation()) {
    case Qt::Horizontal:
        return pointsC1C2Horizontal();
        break;

    case Qt::Vertical:
        return pointsC1C2Vertical();
        break;
    }

    throw std::logic_error("Unreachable code after switch statement");
}

void ConnectionGraphicsObject::addGraphicsEffect()
{
    auto effect = new QGraphicsBlurEffect;

    effect->setBlurRadius(5);
    setGraphicsEffect(effect);

    //auto effect = new QGraphicsDropShadowEffect;
    //auto effect = new ConnectionBlurEffect(this);
    //effect->setOffset(4, 4);
    //effect->setColor(QColor(Qt::gray).darker(800));
}

std::pair<QPointF, QPointF> ConnectionGraphicsObject::pointsC1C2Horizontal() const
{
    double const defaultOffset = 200;

    double xDistance = _in.x() - _out.x();

    double horizontalOffset = qMin(defaultOffset, std::abs(xDistance));

    double verticalOffset = 0;

    double ratioX = 0.5;

    if (xDistance <= 0) {
        double yDistance = _in.y() - _out.y() + 20;

        double vector = yDistance < 0 ? -1.0 : 1.0;

        verticalOffset = qMin(defaultOffset, std::abs(yDistance)) * vector;

        ratioX = 1.0;
    }

    horizontalOffset *= ratioX;

    QPointF c1(_out.x() + horizontalOffset, _out.y() + verticalOffset);

    QPointF c2(_in.x() - horizontalOffset, _in.y() - verticalOffset);

    return std::make_pair(c1, c2);
}

std::pair<QPointF, QPointF> ConnectionGraphicsObject::pointsC1C2Vertical() const
{
    double const defaultOffset = 200;

    double yDistance = _in.y() - _out.y();

    double verticalOffset = qMin(defaultOffset, std::abs(yDistance));

    double horizontalOffset = 0;

    double ratioY = 0.5;

    if (yDistance <= 0) {
        double xDistance = _in.x() - _out.x() + 20;

        double vector = xDistance < 0 ? -1.0 : 1.0;

        horizontalOffset = qMin(defaultOffset, std::abs(xDistance)) * vector;

        ratioY = 1.0;
    }

    verticalOffset *= ratioY;

    QPointF c1(_out.x() + horizontalOffset, _out.y() + verticalOffset);

    QPointF c2(_in.x() - horizontalOffset, _in.y() - verticalOffset);

    return std::make_pair(c1, c2);
}

void ConnectionGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }

    QMenu m_Menu;
    QAction* focusInAction = m_Menu.addAction( "Focus Next Node");
    focusInAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_I));
    // focusInAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));  // 添加快捷键
    connect(focusInAction, &QAction::triggered, [this]() {
        if (auto nodeObj = nodeScene()->nodeGraphicsObject(_connectionId.inNodeId)) {
            nodeScene()->centerOnNode(_connectionId.inNodeId);
            nodeObj->setSelected(true);
        }
    });

    QAction* focusOutAction = m_Menu.addAction( "Focus Previous Node");
    focusOutAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_O));  // 添加快捷键
    connect(focusOutAction, &QAction::triggered, [this]() {
        if (auto nodeObj = nodeScene()->nodeGraphicsObject(_connectionId.outNodeId)) {
           nodeScene()->centerOnNode(_connectionId.outNodeId);
           nodeObj->setSelected(true);
       }
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

void ConnectionGraphicsObject::keyPressEvent(QKeyEvent* event) {
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }
    if ((event->key() == Qt::Key_I) && (event->modifiers() & Qt::AltModifier)) {

        nodeScene()->centerOnNode(_connectionId.inNodeId);
        nodeScene()->clearSelection();
        nodeScene()->nodeGraphicsObject(_connectionId.inNodeId)->setSelected(true);
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_O) && (event->modifiers() & Qt::AltModifier)) {

        nodeScene()->centerOnNode(_connectionId.outNodeId);
        nodeScene()->clearSelection();
        nodeScene()->nodeGraphicsObject(_connectionId.outNodeId)->setSelected(true);
        event->accept();
        return;
    }

    QGraphicsObject::keyPressEvent(event);


}
} // namespace QtNodes
