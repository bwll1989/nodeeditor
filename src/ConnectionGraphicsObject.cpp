#include "ConnectionGraphicsObject.hpp"

#include "AbstractConnectionPainter.hpp"
#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionIdHash.hpp"
#include "ConnectionIdUtils.hpp"
#include "ConnectionState.hpp"
#include "ConnectionStyle.hpp"
#include "GroupGraphicsObject.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeData.hpp"
#include "NodeGraphicsObject.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"
#include "locateNode.hpp"

#include <algorithm>
#include <tuple>
#include <unordered_set>
#include <vector>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QFontMetrics>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QToolTip>

#include <stdexcept>

namespace QtNodes {

namespace {

using VirtualPortKey = std::tuple<NodeId, PortType, PortIndex>;

constexpr int kFoldThreshold = 2;
constexpr qreal kTagHeight = 18.0;
constexpr qreal kTagTip = 7.0;
QString const kDefaultLabel = QStringLiteral("untitled");

std::unordered_set<VirtualPortKey> &expandedVirtualPorts()
{
    static std::unordered_set<VirtualPortKey> ports;
    return ports;
}

VirtualPortKey portKey(ConnectionId const &cid, PortType portType)
{
    return {getNodeId(portType, cid), portType, getPortIndex(portType, cid)};
}

qreal measureTagWidth(QString const &text)
{
    QFont font;
    font.setPointSize(9);
    return qMax<qreal>(48.0, QFontMetrics(font).horizontalAdvance(text) + 18.0);
}

QString resolvedLabel(QString const &label)
{
    return label.isEmpty() ? kDefaultLabel : label;
}

} // namespace

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

    setZValue(isVirtual() ? 1.0 : -1.0);

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

bool ConnectionGraphicsObject::isVirtual() const
{
    return _graphModel.connectionData(_connectionId, ConnectionRole::Virtual).toBool();
}

QString ConnectionGraphicsObject::virtualLabel() const
{
    return _graphModel.connectionData(_connectionId, ConnectionRole::VirtualLabel).toString();
}

std::vector<ConnectionId> ConnectionGraphicsObject::virtualConnectionsOnPort(PortType portType) const
{
    auto const conns = _graphModel.connections(getNodeId(portType, _connectionId),
                                               portType,
                                               getPortIndex(portType, _connectionId));

    std::vector<ConnectionId> virtualConns;
    virtualConns.reserve(conns.size());
    for (auto const &c : conns) {
        if (_graphModel.connectionData(c, ConnectionRole::Virtual).toBool())
            virtualConns.push_back(c);
    }
    std::sort(virtualConns.begin(), virtualConns.end(), [](ConnectionId const &a, ConnectionId const &b) {
        if (a.outNodeId != b.outNodeId)
            return a.outNodeId < b.outNodeId;
        if (a.outPortIndex != b.outPortIndex)
            return a.outPortIndex < b.outPortIndex;
        if (a.inNodeId != b.inNodeId)
            return a.inNodeId < b.inNodeId;
        return a.inPortIndex < b.inPortIndex;
    });
    return virtualConns;
}

int ConnectionGraphicsObject::virtualTagCount(PortType portType) const
{
    return static_cast<int>(virtualConnectionsOnPort(portType).size());
}

bool ConnectionGraphicsObject::isVirtualPortExpanded(PortType portType) const
{
    return expandedVirtualPorts().count(portKey(_connectionId, portType)) > 0;
}

bool ConnectionGraphicsObject::isVirtualPortFolded(PortType portType) const
{
    return virtualTagCount(portType) >= kFoldThreshold && !isVirtualPortExpanded(portType);
}

bool ConnectionGraphicsObject::isVirtualGroupHighlighted(PortType portType) const
{
    if (isSelected() || _connectionState.hovered())
        return true;

    // While still folded, inherit sibling hover/selection so peer tags light up together.
    if (!isVirtualPortFolded(portType) || !nodeScene())
        return false;

    for (auto const &cid : virtualConnectionsOnPort(portType)) {
        if (auto *cgo = nodeScene()->connectionGraphicsObject(cid)) {
            if (cgo->isSelected() || cgo->connectionState().hovered())
                return true;
        }
    }
    return false;
}

void ConnectionGraphicsObject::notifyVirtualGroupRepaint()
{
    if (!nodeScene() || !isVirtual())
        return;

    for (PortType const portType : {PortType::Out, PortType::In}) {
        if (virtualTagCount(portType) < kFoldThreshold)
            continue;
        for (auto const &cid : virtualConnectionsOnPort(portType)) {
            if (auto *cgo = nodeScene()->connectionGraphicsObject(cid))
                cgo->update();
        }
    }
}

void ConnectionGraphicsObject::expandVirtualEndsIfNeeded()
{
    if (!isVirtual() || _connectionState.requiresPort())
        return;
    if (virtualTagCount(PortType::Out) >= kFoldThreshold)
        setVirtualPortExpanded(PortType::Out, true);
    if (virtualTagCount(PortType::In) >= kFoldThreshold)
        setVirtualPortExpanded(PortType::In, true);
}

void ConnectionGraphicsObject::maybeCollapseVirtualPorts()
{
    if (!isVirtual() || !nodeScene() || _connectionState.requiresPort())
        return;

    for (PortType const portType : {PortType::Out, PortType::In}) {
        if (virtualTagCount(portType) < kFoldThreshold || !isVirtualPortExpanded(portType))
            continue;

        bool keep = false;
        for (auto const &cid : virtualConnectionsOnPort(portType)) {
            auto *cgo = nodeScene()->connectionGraphicsObject(cid);
            if (cgo
                && (cgo->isSelected() || cgo->connectionState().hovered() || cgo->isEditingLabel())) {
                keep = true;
                break;
            }
        }
        if (!keep)
            setVirtualPortExpanded(portType, false);
    }
}

void ConnectionGraphicsObject::setVirtualPortExpanded(PortType portType, bool expanded)
{
    auto &ports = expandedVirtualPorts();
    VirtualPortKey const key = portKey(_connectionId, portType);
    bool const changed = expanded ? ports.insert(key).second : (ports.erase(key) > 0);
    if (changed)
        refreshVirtualPortGraphics(portType);
}

void ConnectionGraphicsObject::refreshVirtualPortGraphics(PortType portType)
{
    if (!nodeScene())
        return;
    for (auto const &cid : virtualConnectionsOnPort(portType)) {
        if (auto *cgo = nodeScene()->connectionGraphicsObject(cid)) {
            cgo->prepareGeometryChange();
            cgo->update();
        }
    }
}

QString ConnectionGraphicsObject::virtualTagCaption(PortType portType) const
{
    if (isVirtualPortFolded(portType))
        return QStringLiteral("×%1 ▾").arg(virtualTagCount(portType));
    return resolvedLabel(virtualLabel());
}

int ConnectionGraphicsObject::virtualTagSlotIndex(PortType portType) const
{
    auto const virtualConns = virtualConnectionsOnPort(portType);
    for (int i = 0; i < static_cast<int>(virtualConns.size()); ++i) {
        if (virtualConns[static_cast<size_t>(i)] == _connectionId)
            return i;
    }
    return 0;
}

qreal ConnectionGraphicsObject::virtualTagWidth(PortType portType) const
{
    return measureTagWidth(virtualTagCaption(portType));
}

qreal ConnectionGraphicsObject::virtualTagChainOffset(PortType portType) const
{
    if (isVirtualPortFolded(portType))
        return 0;

    auto const virtualConns = virtualConnectionsOnPort(portType);
    int const slot = virtualTagSlotIndex(portType);
    bool const interlock = static_cast<int>(virtualConns.size()) >= 2;

    qreal offset = 0;
    for (int i = 0; i < slot; ++i) {
        QString const label
            = _graphModel.connectionData(virtualConns[static_cast<size_t>(i)],
                                         ConnectionRole::VirtualLabel)
                  .toString();
        qreal const wi = measureTagWidth(resolvedLabel(label));
        offset += interlock ? (wi - kTagTip) : wi;
    }
    return offset;
}

QPolygonF ConnectionGraphicsObject::virtualTagPolygon(PortType portType) const
{
    if (!isVirtual() || _connectionState.requiresPort())
        return {};

    bool const folded = isVirtualPortFolded(portType);
    int const slot = virtualTagSlotIndex(portType);
    if (folded && slot != 0)
        return {};

    qreal const w = virtualTagWidth(portType);
    qreal const portGap = StyleCollection::nodeStyle().ConnectionPointDiameter * 0.5 + 4.0;
    qreal const chain = virtualTagChainOffset(portType);
    bool const interlock = !folded && virtualTagCount(portType) >= 2;

    QPointF const origin = endPoint(portType);
    qreal const y0 = origin.y() - kTagHeight / 2.0;
    qreal const y1 = y0 + kTagHeight;
    qreal const yc = origin.y();

    QPolygonF poly;
    if (portType == PortType::Out) {
        qreal const x = origin.x() + portGap + chain;
        if (interlock) {
            poly << QPointF(x, yc) << QPointF(x + kTagTip, y0) << QPointF(x + w, y0)
                 << QPointF(x + w - kTagTip, yc) << QPointF(x + w, y1)
                 << QPointF(x + kTagTip, y1);
        } else {
            poly << QPointF(x, yc) << QPointF(x + kTagTip, y0) << QPointF(x + w, y0)
                 << QPointF(x + w, y1) << QPointF(x + kTagTip, y1);
        }
    } else {
        qreal const x = origin.x() - portGap - chain;
        if (interlock) {
            poly << QPointF(x, yc) << QPointF(x - kTagTip, y0) << QPointF(x - w, y0)
                 << QPointF(x - w + kTagTip, yc) << QPointF(x - w, y1)
                 << QPointF(x - kTagTip, y1);
        } else {
            poly << QPointF(x, yc) << QPointF(x - kTagTip, y0) << QPointF(x - w, y0)
                 << QPointF(x - w, y1) << QPointF(x - kTagTip, y1);
        }
    }
    return poly;
}

QRectF ConnectionGraphicsObject::boundingRect() const
{
    if (isVirtual() && !_connectionState.requiresPort()) {
        // 虚拟连线只绘制两端标签，包围盒不应跨整段 out→in，否则搜索/选中会居中到空白中点
        QRectF r;
        auto const outPoly = virtualTagPolygon(PortType::Out);
        auto const inPoly = virtualTagPolygon(PortType::In);
        if (!outPoly.isEmpty())
            r = r.united(outPoly.boundingRect());
        if (!inPoly.isEmpty())
            r = r.united(inPoly.boundingRect());
        if (r.isNull())
            r = QRectF(_out, _in).normalized();
        return r.adjusted(-4, -4, 4, 4);
    }

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
    if (isVirtual() && !_connectionState.requiresPort()) {
        QPainterPath path;
        auto const outPoly = virtualTagPolygon(PortType::Out);
        auto const inPoly = virtualTagPolygon(PortType::In);
        if (!outPoly.isEmpty())
            path.addPolygon(outPoly);
        if (!inPoly.isEmpty())
            path.addPolygon(inPoly);
        return path;
    }
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
    if (isVirtual()) {
        // Later slots paint above earlier ones so nested tips read clearly.
        int const zBoost = qMax(virtualTagSlotIndex(PortType::Out),
                                virtualTagSlotIndex(PortType::In));
        setZValue(1.0 + 0.01 * zBoost);
    } else {
        setZValue(-1.0);
    }

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

    if (isEditingLabel())
        syncLabelEditorGeometry();

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

    if (isVirtual() && !_connectionState.requiresPort() && event->button() == Qt::LeftButton)
        expandVirtualEndsIfNeeded();

    QGraphicsItem::mousePressEvent(event);
}

void ConnectionGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked) || !isVirtual()
        || _connectionState.requiresPort()) {
        QGraphicsItem::mouseDoubleClickEvent(event);
        return;
    }

    expandVirtualEndsIfNeeded();
    startEditingLabel(virtualTagPortAt(event->pos()));
    event->accept();
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
        QToolTip::hideText(); // 拖线离开节点时收起端口 tip
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

    QToolTip::hideText(); // 结束拖线，收起端口 tip

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
    expandVirtualEndsIfNeeded();

    update();
    notifyVirtualGroupRepaint();

    // Signal
    nodeScene()->connectionHovered(connectionId(), event->screenPos());

    event->accept();
}

void ConnectionGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    _connectionState.setHovered(false);

    update();
    notifyVirtualGroupRepaint();

    QPointer<ConnectionGraphicsObject> self(this);
    QTimer::singleShot(0, this, [self]() {
        if (self)
            self->maybeCollapseVirtualPorts();
    });

    // Signal
    nodeScene()->connectionHoverLeft(connectionId());

    event->accept();
}

QVariant ConnectionGraphicsObject::itemChange(QGraphicsItem::GraphicsItemChange change,
                                             const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged && isVirtual()) {
        if (value.toBool()) {
            // Selected tag → expand both ends so the peer tag is visible too.
            expandVirtualEndsIfNeeded();
        } else {
            QPointer<ConnectionGraphicsObject> self(this);
            QTimer::singleShot(0, this, [self]() {
                if (self)
                    self->maybeCollapseVirtualPorts();
            });
        }
        notifyVirtualGroupRepaint();
    }
    return QGraphicsObject::itemChange(change, value);
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

    double ratioX = 1.0;

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

    if (!_connectionState.requiresPort()) {
        if (isVirtual()) {
            QAction *jumpPeer = m_Menu.addAction(QStringLiteral("跳转到对端节点"));
            PortType const clickPort = virtualTagPortAt(event->pos());
            connect(jumpPeer, &QAction::triggered, [this, clickPort]() {
                NodeId const target = (clickPort == PortType::Out) ? _connectionId.inNodeId
                                                                  : _connectionId.outNodeId;
                if (auto *nodeObj = nodeScene()->nodeGraphicsObject(target)) {
                    nodeScene()->clearSelection();
                    nodeScene()->centerOnNode(target);
                    nodeObj->setSelected(true);
                }
            });

            QAction *restoreAction = m_Menu.addAction(QStringLiteral("恢复为实线"));
            connect(restoreAction, &QAction::triggered, [this]() {
                if (!nodeScene())
                    return;
                bool const wasVirtual = isVirtual();
                if (!wasVirtual)
                    return;
                nodeScene()->undoStack().push(
                    new SetConnectionDataCommand(
                        nodeScene(),
                        {{_connectionId, ConnectionRole::Virtual, true, false}},
                        QStringLiteral("恢复为实线")));
            });

            QAction *renameAction = m_Menu.addAction(QStringLiteral("编辑标签"));
            renameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
            PortType const editPort = clickPort;
            connect(renameAction, &QAction::triggered, [this, editPort]() {
                expandVirtualEndsIfNeeded();
                startEditingLabel(editPort);
            });

            m_Menu.addSeparator();
        } else {
            QAction *virtualAction = m_Menu.addAction(QStringLiteral("转为虚拟连线"));
            connect(virtualAction, &QAction::triggered, [this]() {
                if (!nodeScene() || isVirtual())
                    return;

                std::vector<SetConnectionDataCommand::Change> changes;
                changes.push_back({_connectionId, ConnectionRole::Virtual, false, true});
                if (virtualLabel().isEmpty()) {
                    changes.push_back({_connectionId,
                                       ConnectionRole::VirtualLabel,
                                       QString(),
                                       kDefaultLabel});
                }
                nodeScene()->undoStack().push(
                    new SetConnectionDataCommand(nodeScene(),
                                                 std::move(changes),
                                                 QStringLiteral("转为虚拟连线")));
            });
            m_Menu.addSeparator();

            QAction* focusInAction = m_Menu.addAction(QStringLiteral("聚焦结束节点"));
            focusInAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_I));
            connect(focusInAction, &QAction::triggered, [this]() {
                if (auto nodeObj = nodeScene()->nodeGraphicsObject(_connectionId.inNodeId)) {
                    nodeScene()->centerOnNode(_connectionId.inNodeId);
                    nodeObj->setSelected(true);
                }
            });

            QAction* focusOutAction = m_Menu.addAction(QStringLiteral("聚焦起始节点"));
            focusOutAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_O));
            connect(focusOutAction, &QAction::triggered, [this]() {
                if (auto nodeObj = nodeScene()->nodeGraphicsObject(_connectionId.outNodeId)) {
                    nodeScene()->centerOnNode(_connectionId.outNodeId);
                    nodeObj->setSelected(true);
                }
            });
        }
    }

    if (auto *bs = nodeScene())
        bs->appendContextMenuActions(m_Menu, ContextMenuKind::Connection);

    m_Menu.exec(event->screenPos());

    event->accept();
}

void ConnectionGraphicsObject::keyPressEvent(QKeyEvent* event) {
    if (_graphModel.nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }
    if ((event->key() == Qt::Key_E) && (event->modifiers() & Qt::ControlModifier)) {
        if (isVirtual() && !_connectionState.requiresPort()) {
            expandVirtualEndsIfNeeded();
            startEditingLabel(PortType::Out);
            event->accept();
            return;
        }
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

bool ConnectionGraphicsObject::isEditingLabel() const
{
    return _labelProxy && _labelProxy->isVisible();
}

void ConnectionGraphicsObject::initLabelEditor()
{
    if (_labelProxy)
        return;

    _labelEditor = new QLineEdit();
    _labelEditor->setFrame(false);
    _labelEditor->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _labelEditor->setAttribute(Qt::WA_TranslucentBackground, false);
    _labelEditor->setContentsMargins(0, 0, 0, 0);
    _labelEditor->setTextMargins(2, 0, 2, 0);
    QFont font = _labelEditor->font();
    font.setPointSize(9);
    font.setBold(true);
    _labelEditor->setFont(font);

    connect(_labelEditor, &QLineEdit::editingFinished, this, [this]() {
        finishEditingLabel();
    });
    _labelEditor->installEventFilter(this);

    _labelProxy = new QGraphicsProxyWidget(this);
    _labelProxy->setWidget(_labelEditor);
    _labelProxy->setZValue(10.0);
    _labelProxy->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    _labelProxy->setContentsMargins(0, 0, 0, 0);
    _labelProxy->hide();
}

QRectF ConnectionGraphicsObject::virtualTagTextRect(PortType portType) const
{
    QPolygonF const poly = virtualTagPolygon(portType);
    if (poly.isEmpty())
        return {};

    QRectF br = poly.boundingRect();
    // Keep text clear of the arrow tip and (when interlocked) the outer notch.
    bool const interlock = !isVirtualPortFolded(portType) && virtualTagCount(portType) >= 2;
    qreal const pad = interlock ? (kTagTip + 2.0) : 9.0;
    if (portType == PortType::Out)
        br.adjust(pad, 1, interlock ? -pad : -2, -1);
    else
        br.adjust(interlock ? pad : 2, 1, -pad, -1);
    return br;
}

PortType ConnectionGraphicsObject::virtualTagPortAt(QPointF const &localPos) const
{
    auto const outPoly = virtualTagPolygon(PortType::Out);
    auto const inPoly = virtualTagPolygon(PortType::In);

    if (!outPoly.isEmpty() && outPoly.containsPoint(localPos, Qt::OddEvenFill))
        return PortType::Out;
    if (!inPoly.isEmpty() && inPoly.containsPoint(localPos, Qt::OddEvenFill))
        return PortType::In;

    qreal const dOut = outPoly.isEmpty()
                           ? 1e9
                           : QLineF(localPos, outPoly.boundingRect().center()).length();
    qreal const dIn = inPoly.isEmpty()
                          ? 1e9
                          : QLineF(localPos, inPoly.boundingRect().center()).length();
    return (dIn < dOut) ? PortType::In : PortType::Out;
}

void ConnectionGraphicsObject::syncLabelEditorGeometry()
{
    if (!_labelProxy || !_labelEditor || !isVirtual())
        return;

    QRectF br = virtualTagTextRect(_labelEditPort);
    if (br.isEmpty())
        return;

    qreal const h = qMax<qreal>(14.0, kTagHeight - 2.0);
    br.setTop(br.center().y() - h * 0.5);
    br.setHeight(h);
    if (br.width() < 20.0)
        br.setWidth(20.0);

    QSize const size(qMax(1, qRound(br.width())), qMax(1, qRound(br.height())));
    _labelEditor->setFixedSize(size);
    _labelProxy->setMinimumSize(0, 0);
    _labelProxy->setMaximumSize(size.width(), size.height());
    _labelProxy->resize(size.width(), size.height());
    _labelProxy->setPos(br.topLeft());
}

void ConnectionGraphicsObject::startEditingLabel(PortType preferPort)
{
    if (!isVirtual() || _connectionState.requiresPort())
        return;

    initLabelEditor();
    _labelEditPort = (preferPort == PortType::In) ? PortType::In : PortType::Out;

    auto const &connectionStyle = StyleCollection::connectionStyle();
    QColor bg = connectionStyle.normalColor();
    if (connectionStyle.useDataDefinedColors()) {
        auto dataTypeOut = _graphModel
                               .portData(_connectionId.outNodeId,
                                         PortType::Out,
                                         _connectionId.outPortIndex,
                                         PortRole::DataType)
                               .value<NodeDataType>();
        bg = connectionStyle.normalColor(dataTypeOut.id);
    }

    QColor const fg = connectionStyle.fontColor();
    _labelEditor->setStyleSheet(
        QStringLiteral(
            "QLineEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: none;"
            "  padding: 0px;"
            "  margin: 0px;"
            "  font-weight: bold;"
            "  font-size: 9pt;"
            "  selection-background-color: rgba(0,0,0,0.35);"
            "}")
            .arg(bg.name(QColor::HexRgb), fg.name(QColor::HexRgb)));

    syncLabelEditorGeometry();
    _labelEditor->setText(resolvedLabel(virtualLabel()));
    _labelProxy->show();
    _labelEditor->setFocus(Qt::OtherFocusReason);
    _labelEditor->selectAll();
    update();
}

void ConnectionGraphicsObject::finishEditingLabel()
{
    if (!_labelEditor || !_labelProxy || _finishingLabelEdit)
        return;
    if (!_labelProxy->isVisible())
        return;

    _finishingLabelEdit = true;

    bool const discard = _discardLabelEdit;
    _discardLabelEdit = false;

    QString const newLabel = _labelEditor->text().trimmed();
    _labelProxy->hide();

    if (!discard && !newLabel.isEmpty() && nodeScene()) {
        QString const oldLabel = virtualLabel();
        if (oldLabel != newLabel) {
            nodeScene()->undoStack().push(
                new SetConnectionDataCommand(
                    nodeScene(),
                    {{_connectionId, ConnectionRole::VirtualLabel, oldLabel, newLabel}},
                    QStringLiteral("编辑虚拟标签")));
        }
    }

    setFocus();
    update();

    _finishingLabelEdit = false;
}

bool ConnectionGraphicsObject::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _labelEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                _discardLabelEdit = true;
                finishEditingLabel();
                return true;
            }
        }
    }
    return QGraphicsObject::eventFilter(watched, event);
}
} // namespace QtNodes
