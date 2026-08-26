#include "NodeGraphicsObject.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtCore/QJsonObject>
#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QtWidgets>
#include <QtCore/QJsonDocument>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "AbstractNodePainter.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeDelegateModel.hpp"
#include "NodeStyle.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"
#include "GroupGraphicsObject.hpp"

namespace QtNodes {

namespace {

QList<QColor> titleColorPresets()
{
    return {
        QColor(255, 140, 0),   // orange
        QColor(0, 170, 255),   // bright blue
        QColor(255, 105, 180), // pink
        QColor(160, 120, 220), // purple
        QColor(0, 180, 170),   // teal
        QColor(220, 50, 60),   // red
        QColor(240, 190, 40),  // yellow
        QColor(100, 180, 70),  // green
    };
}

QString makeFullOscAddressForNode(AbstractGraphModel const &model,
                                  NodeId nodeId,
                                  QString const &relative)
{
    QString const norm = relative.startsWith(QLatin1Char('/')) ? relative
                                                              : (QLatin1Char('/') + relative);
    QString prefix = QStringLiteral("/dataflow/");
    QString const alias = model.nodeData(nodeId, NodeRole::ModelAlias).toString().trimmed();
    if (!alias.isEmpty())
        prefix += alias + QLatin1Char('/');
    prefix += QString::number(nodeId);
    return prefix + norm;
}

QString relativePathForAddress(AbstractGraphModel const &model,
                               NodeId nodeId,
                               QString const &fullAddress)
{
    QString prefix = QStringLiteral("/dataflow/");
    QString const alias = model.nodeData(nodeId, NodeRole::ModelAlias).toString().trimmed();
    if (!alias.isEmpty())
        prefix += alias + QLatin1Char('/');
    prefix += QString::number(nodeId);
    if (fullAddress.startsWith(prefix))
        return fullAddress.mid(prefix.size());
    return fullAddress;
}

QStringList titleColorNames()
{
    return {
        QStringLiteral("橙色"),
        QStringLiteral("蓝色"),
        QStringLiteral("粉色"),
        QStringLiteral("紫色"),
        QStringLiteral("青色"),
        QStringLiteral("红色"),
        QStringLiteral("黄色"),
        QStringLiteral("绿色"),
    };
}

QIcon colorSwatchIcon(QColor const &color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 0, 0, 60), 1));
    painter.setBrush(color);
    painter.drawRoundedRect(1, 1, 14, 14, 3, 3);
    return QIcon(pixmap);
}

bool sameRgb(QColor const &a, QColor const &b)
{
    return a.red() == b.red() && a.green() == b.green() && a.blue() == b.blue();
}

} // namespace

NodeGraphicsObject::NodeGraphicsObject(BasicGraphicsScene &scene, NodeId nodeId)
    : _nodeId(nodeId)
    , _graphModel(scene.graphModel())
    , _nodeState(*this)
    , _proxyWidget(nullptr)
{
    scene.addItem(this);

    setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);

    setCacheMode(QGraphicsItem::DeviceCoordinateCache);

    QJsonObject nodeStyleJson = _graphModel.nodeData(_nodeId, NodeRole::Style).toJsonObject();

    NodeStyle nodeStyle(nodeStyleJson);
    // 不显示阴影效果
    if(nodeStyle.ShadowEnabled)
     {
         auto effect = new QGraphicsDropShadowEffect;
         effect->setOffset(4, 4);
         effect->setBlurRadius(20);
         effect->setColor(nodeStyle.ShadowColor);

         setGraphicsEffect(effect);
     }

    // Applies Locked / 屏蔽变量输入 interaction + opacity.
    setLockedState();

    setAcceptHoverEvents(true);

    setZValue(0);

    embedQWidget();

    nodeScene()->nodeGeometry().recomputeSize(_nodeId);

    QPointF const pos = _graphModel.nodeData<QPointF>(_nodeId, NodeRole::Position);

    setPos(pos);

    connect(&_graphModel,
            &AbstractGraphModel::nodeFlagsUpdated,
            this,
            &NodeGraphicsObject::onLockedState);

}

NodeGraphicsObject::~NodeGraphicsObject()
{
//    disconnect(&_graphModel,
//               &AbstractGraphModel::nodeFlagsUpdated,
//               this,
//               &NodeGraphicsObject::onLockedState);
}

AbstractGraphModel &NodeGraphicsObject::graphModel() const
{
    return _graphModel;
}

BasicGraphicsScene *NodeGraphicsObject::nodeScene() const
{
    return dynamic_cast<BasicGraphicsScene *>(scene());
}

void NodeGraphicsObject::syncEmbeddedWidgetSize(QWidget *w)
{
    if (!w || !_proxyWidget)
        return;

    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

    // Port stack sets a floor on the real QWidget (proxy min alone is not enough).
    unsigned int const minWidgetH = geometry.minimumEmbeddedWidgetHeight(_nodeId);
    if (minWidgetH > 0) {
        w->setMinimumHeight(static_cast<int>(minWidgetH));
        if (w->height() < static_cast<int>(minWidgetH)) {
            w->resize(w->width(), static_cast<int>(minWidgetH));
            geometry.recomputeSize(_nodeId);
        }
    }

    // Available height = nodeHeight - topOffset - bottomGap (same as widgetPosition).
    if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
        auto const nodeH = geometry.size(_nodeId).height();
        auto const overhead = static_cast<int>(geometry.embeddedWidgetTopOffset(_nodeId)
                                               + geometry.embeddedWidgetBottomGap(_nodeId));
        unsigned int widgetHeight = nodeH > overhead ? static_cast<unsigned int>(nodeH - overhead)
                                                     : minWidgetH;
        widgetHeight = std::max(widgetHeight, minWidgetH);

        w->setMinimumHeight(static_cast<int>(minWidgetH));
        w->resize(w->width(), static_cast<int>(widgetHeight));
        _proxyWidget->setMinimumHeight(widgetHeight);
        geometry.recomputeSize(_nodeId);
    }
}

void NodeGraphicsObject::updateQWidgetEmbedPos()
{
  if (_proxyWidget) {
      if(_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).value<bool>()) {
          AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
          // 端口增减后节点已 recomputeSize，这里必须同步拉高/压矮嵌入控件
          if (auto w = _proxyWidget->widget()) {
              prepareGeometryChange();
              syncEmbeddedWidgetSize(w);
          }
          _proxyWidget->setPos(geometry.widgetPosition(_nodeId));
      }else {

          scene()->removeItem(_proxyWidget);
           _proxyWidget->setWidget(nullptr);
          _proxyWidget->setParentItem(nullptr);
          _proxyWidget->deleteLater();  // 删除小部件
          _proxyWidget=nullptr;

      }
  }else {
      if(_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).value<bool>()) {
          embedQWidget();

      }
  }

}

void NodeGraphicsObject::embedQWidget()
{
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    geometry.recomputeSize(_nodeId);
    if(_proxyWidget) {
        scene()->removeItem(_proxyWidget);
        _proxyWidget->setWidget(nullptr);
        _proxyWidget->setParentItem(nullptr);
        _proxyWidget->deleteLater();  // 删除小部件
        _proxyWidget=nullptr;
    }
    if (!_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).value<bool>())
        return;

    if (auto w = _graphModel.nodeData(_nodeId, NodeRole::Widget).value<QWidget *>()) {

        _proxyWidget = new QGraphicsProxyWidget(this);

        _proxyWidget->setWidget(w);

        _proxyWidget->setPreferredWidth(5);

        geometry.recomputeSize(_nodeId);

        syncEmbeddedWidgetSize(w);

        _proxyWidget->setPos(geometry.widgetPosition(_nodeId));

        // update();

        _proxyWidget->setOpacity(1.0);
        _proxyWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
    }
}

void NodeGraphicsObject::setPortEditing(bool enabled)
{
    if (!_graphModel.nodeData(_nodeId, NodeRole::PortEditable).toBool())
        return;

    auto const currentType = static_cast<NodeWidgetType>(
        _graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toInt());
    bool const isEditing = (currentType == NodeWidgetType::PortEditWidget);

    if (enabled) {
        // 首次进入时记录展开/折叠，结束时原样恢复
        if (!isEditing) {
            _embeddableBeforePortEdit
                = _graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).toBool();
            _hasEmbeddableBeforePortEdit = true;
            // 先切到端口编辑控件，再展开，避免收起态先闪一下内部控件
            _graphModel.setNodeData(_nodeId,
                                    NodeRole::EmbeddWidgetType,
                                    static_cast<int>(NodeWidgetType::PortEditWidget));
        }
        if (!_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).toBool()) {
            _graphModel.setNodeData(_nodeId, NodeRole::WidgetEmbeddable, true);
        }
    } else if (isEditing) {
        bool const restoreEmbeddable = _hasEmbeddableBeforePortEdit ? _embeddableBeforePortEdit
                                                                    : true;
        _hasEmbeddableBeforePortEdit = false;

        if (!restoreEmbeddable) {
            // 原先折叠：先收起再切回内部控件，避免闪一下属性面板
            _graphModel.setNodeData(_nodeId, NodeRole::WidgetEmbeddable, false);
            _graphModel.setNodeData(_nodeId,
                                    NodeRole::EmbeddWidgetType,
                                    static_cast<int>(NodeWidgetType::InternalWidget));
        } else {
            _graphModel.setNodeData(_nodeId,
                                    NodeRole::EmbeddWidgetType,
                                    static_cast<int>(NodeWidgetType::InternalWidget));
            if (!_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).toBool()) {
                _graphModel.setNodeData(_nodeId, NodeRole::WidgetEmbeddable, true);
            }
        }
    }
}

void NodeGraphicsObject::setLockedState()
{
    NodeFlags flags = _graphModel.nodeFlags(_nodeId);

    bool const locked = flags.testFlag(NodeFlag::Locked);
    bool const muted = flags.testFlag(NodeFlag::Muted);
    // 选中 / hover 时不降低透明度，保持原来的高亮观感
    bool const highlighted = isSelected() || _nodeState.hovered();

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);

    // 只用全局样式 Opacity，避免 hover 时反复解析节点 Style JSON
    double const baseOpacity = StyleCollection::nodeStyle().Opacity;
    setOpacity((muted && !highlighted) ? baseOpacity * 0.4 : baseOpacity);
    if (_proxyWidget) {
        _proxyWidget->setOpacity((muted && !highlighted) ? 0.45 : 1.0);
    }
}

void NodeGraphicsObject::onLockedState(NodeId id)
{
    if (_nodeId != id) {
        return;
    }
    setLockedState();
    update();
}

QRectF NodeGraphicsObject::boundingRect() const
{
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    return geometry.boundingRect(_nodeId);
    // return NodeGeometry(_nodeId, _graphModel, nodeScene()).boundingRect();
}

void NodeGraphicsObject::setGeometryChanged()
{
    prepareGeometryChange();
}

void NodeGraphicsObject::moveConnections() const
{
    auto const &connected = _graphModel.allConnectionIds(_nodeId);

    for (auto &cnId : connected) {
        auto cgo = nodeScene()->connectionGraphicsObject(cnId);

        if (cgo)
            cgo->move();
    }
}

void NodeGraphicsObject::reactToConnection(ConnectionGraphicsObject const *cgo)
{
    _nodeState.storeConnectionForReaction(cgo);

    // 拖线靠近本节点时：找最近的可连接端口，用 QToolTip 显示端口名
    PortType requiredPort = cgo->connectionState().requiredPort();
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    QPointF local = mapFromScene(cgo->sceneTransform().map(cgo->endPoint(requiredPort)));

    PortIndex bestIndex = InvalidPortIndex;
    double bestDist = 40.0; // 与绘制端端口吸附阈值一致
    unsigned int n = _graphModel
                         .nodeData(_nodeId,
                                   (requiredPort == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                         .toUInt();
    for (PortIndex i = 0; i < n; ++i) {
        ConnectionId id = makeCompleteConnectionId(cgo->connectionId(), _nodeId, i);
        if (!_graphModel.connectionPossible(id))
            continue;
        QPointF d = local - geometry.portPosition(_nodeId, requiredPort, i);
        double dist = std::sqrt(QPointF::dotProduct(d, d));
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }

    if (bestIndex != InvalidPortIndex && scene() && !scene()->views().isEmpty()) {
        // 以目标端口的场景位置换算为屏幕坐标，再弹出 tip
        QPointF scenePos = mapToScene(geometry.portPosition(_nodeId, requiredPort, bestIndex));
        QPoint globalPos = scene()->views().first()->mapToGlobal(
            scene()->views().first()->mapFromScene(scenePos));
        showCompactPortToolTip(requiredPort, bestIndex, globalPos);
    } else {
        hideCompactPortToolTip();
    }

    update();
}

void NodeGraphicsObject::showCompactPortToolTip(PortType portType,
                                                PortIndex portIndex,
                                                QPoint globalPos)
{
    // 展开模式已常驻绘制端口名，无需 tip
    if (_graphModel.portData<bool>(_nodeId, portType, portIndex, PortRole::CaptionVisible))
        return;

    if (!scene() || scene()->views().isEmpty())
        return;

    QString text = _graphModel.portData<QString>(_nodeId, portType, portIndex, PortRole::Caption);
    if (text.isEmpty()) {
        text = (portType == PortType::In) ? QStringLiteral("IN %1").arg(portIndex)
                                          : QStringLiteral("OUT %1").arg(portIndex);
    }

    // 输入口向左/上偏移，输出口向右/下偏移，减少与节点本体重叠
    constexpr int gap = 10;
    if (nodeScene()->orientation() == Qt::Vertical) {
        globalPos += (portType == PortType::In) ? QPoint(0, -gap) : QPoint(0, gap);
    } else {
        globalPos += (portType == PortType::In) ? QPoint(-gap, 0) : QPoint(gap, 0);
    }

    QToolTip::showText(globalPos, text, scene()->views().first());
}

void NodeGraphicsObject::hideCompactPortToolTip()
{
    QToolTip::hideText();
}

void NodeGraphicsObject::paint(QPainter *painter, QStyleOptionGraphicsItem const *option, QWidget *)
{
    QString tooltip;
    QVariant var = _graphModel.nodeData(_nodeId, NodeRole::ValidationState);
    if (var.canConvert<NodeValidationState>()) {
        auto state = var.value<NodeValidationState>();
        if (state._state != NodeValidationState::State::Valid) {
            tooltip = state._stateMessage;
        }
    }
    setToolTip(tooltip);

    painter->setClipRect(option->exposedRect);

    nodeScene()->nodePainter().paint(painter, *this);
}

QVariant NodeGraphicsObject::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemScenePositionHasChanged && scene()) {
        moveConnections();
    } else if (change == ItemSelectedHasChanged) {
        setLockedState();
        // 取消选中时关闭端口编辑
        if (!value.toBool()) {
            setPortEditing(false);
        }
    }

    return QGraphicsObject::itemChange(change, value);
}

void NodeGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (graphModel().nodeFlags(_nodeId) & NodeFlag::Locked) {
        return;
    }

    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

    for (PortType portToCheck : {PortType::In, PortType::Out}) {
        QPointF nodeCoord = sceneTransform().inverted().map(event->scenePos());

        PortIndex const portIndex = geometry.checkPortHit(_nodeId, portToCheck, nodeCoord);

        if (portIndex == InvalidPortIndex)
            continue;

        auto const &connected = _graphModel.connections(_nodeId, portToCheck, portIndex);

        // Start dragging existing connection.
        if (!connected.empty() && portToCheck == PortType::In) {
            auto const &cnId = *connected.begin();

            // Need ConnectionGraphicsObject

            NodeConnectionInteraction interaction(*this,
                                                  *nodeScene()->connectionGraphicsObject(cnId),
                                                  *nodeScene());

            if (_graphModel.detachPossible(cnId))
                interaction.disconnect(portToCheck);
        } else // initialize new Connection
        {
            if (portToCheck == PortType::Out) {
                auto const outPolicy = _graphModel
                                           .portData(_nodeId,
                                                     portToCheck,
                                                     portIndex,
                                                     PortRole::ConnectionPolicyRole)
                                           .value<ConnectionPolicy>();

                if (!connected.empty() && outPolicy == ConnectionPolicy::One) {
                    for (auto &cnId : connected) {
                        _graphModel.deleteConnection(cnId);
                    }
                }
            } // if port == out

            ConnectionId const incompleteConnectionId = makeIncompleteConnectionId(_nodeId,
                                                                                   portToCheck,
                                                                                   portIndex);
            if (!_graphModel.detachPossible(incompleteConnectionId))
                continue;
            nodeScene()->makeDraftConnection(incompleteConnectionId);
        }
    }

    if (_graphModel.nodeFlags(_nodeId) & NodeFlag::Resizable) {
        auto pos = event->pos();
        bool const hit = geometry.resizeHandleRect(_nodeId).contains(QPoint(pos.x(), pos.y()));
        _nodeState.setResizing(hit);
    }

    if (!event->isAccepted()) {
        QGraphicsObject::mousePressEvent(event);
    }

    if (isSelected()) {
        Q_EMIT nodeScene()->nodeSelected(_nodeId);
    }
}

void NodeGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Deselect all other items after this one is selected.
    // Unless we press a CTRL button to add the item to the selected group before
    // starting moving.
    if (!isSelected()) {
        if (!event->modifiers().testFlag(Qt::ControlModifier))
            scene()->clearSelection();

        setSelected(true);
    }

    if (_nodeState.resizing() &&
        _graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).value<bool>()) {
        auto diff = event->pos() - event->lastPos();

        if (auto w = _graphModel.nodeData<QWidget *>(_nodeId, NodeRole::Widget)) {
            prepareGeometryChange();

            auto oldSize = w->size();

            oldSize += QSize(diff.x(), diff.y());

            AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

            // Cannot shrink below the port-driven content height (applies to QWidget, not only proxy).
            unsigned int const minWidgetH = geometry.minimumEmbeddedWidgetHeight(_nodeId);
            oldSize.setWidth(std::max(1, oldSize.width()));
            oldSize.setHeight(std::max(static_cast<int>(minWidgetH), oldSize.height()));

            w->setMinimumHeight(static_cast<int>(minWidgetH));
            w->resize(oldSize);

            // Passes the new size to the model.
            geometry.recomputeSize(_nodeId);

            if (_proxyWidget) {
                if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
                    auto const nodeH = geometry.size(_nodeId).height();
                    auto const overhead = static_cast<int>(geometry.embeddedWidgetTopOffset(_nodeId)
                                                           + geometry.embeddedWidgetBottomGap(_nodeId));
                    unsigned int widgetHeight = nodeH > overhead
                                                    ? static_cast<unsigned int>(nodeH - overhead)
                                                    : minWidgetH;
                    widgetHeight = std::max(widgetHeight, minWidgetH);

                    w->resize(w->width(), static_cast<int>(widgetHeight));
                    _proxyWidget->setMinimumHeight(widgetHeight);
                    geometry.recomputeSize(_nodeId);
                }
                _proxyWidget->setPos(geometry.widgetPosition(_nodeId));
            }

            update();

            moveConnections();

            event->accept();
        }
    } else {
        auto diff = event->pos() - event->lastPos();

        nodeScene()->undoStack().push(new MoveNodeCommand(nodeScene(), diff));

        event->accept();
    }

    QRectF r = nodeScene()->sceneRect();

    r = r.united(mapToScene(boundingRect()).boundingRect());

    nodeScene()->setSceneRect(r);
}

void NodeGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    _nodeState.setResizing(false);

    if (!event->isAccepted()) {
        QGraphicsObject::mouseReleaseEvent(event);
    }

    // position connections precisely after fast node move
    moveConnections();

    nodeScene()->nodeClicked(_nodeId);
}

void NodeGraphicsObject::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    // 提到最前即可；原先 collidingItems() 在 NoIndex/多节点下是 O(N) 扫描
    setZValue(1.0);

    _nodeState.setHovered(true);
    setLockedState();

    update();

    Q_EMIT nodeScene()->nodeHovered(_nodeId, event->screenPos());

    event->accept();
}

void NodeGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    _nodeState.setHovered(false);
    _nodeState.clearHoveredPort();
    hideCompactPortToolTip(); // 离开节点时收起端口 tip

    setZValue(0.0);
    setLockedState();

    update();

    Q_EMIT nodeScene()->nodeHoverLeft(_nodeId);

    event->accept();
}

void NodeGraphicsObject::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto pos = event->pos();

    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

    // 检测鼠标下是否命中端口
    PortType hoveredType = PortType::None;
    PortIndex hoveredIndex = InvalidPortIndex;

    for (PortType portToCheck : {PortType::In, PortType::Out}) {
        PortIndex const portIndex = geometry.checkPortHit(_nodeId, portToCheck, pos);
        if (portIndex != InvalidPortIndex) {
            hoveredType = portToCheck;
            hoveredIndex = portIndex;
            break;
        }
    }

    // 悬停端口变化时刷新绘制（端口圆点高亮）
    if (hoveredType != _nodeState.hoveredPortType()
        || hoveredIndex != _nodeState.hoveredPortIndex()) {
        _nodeState.setHoveredPort(hoveredType, hoveredIndex);
        update();
    }

    // 收起模式下用 QToolTip 显示端口名；未命中端口则隐藏
    if (hoveredType != PortType::None)
        showCompactPortToolTip(hoveredType, hoveredIndex, event->screenPos());
    else
        hideCompactPortToolTip();

    if ((_graphModel.nodeFlags(_nodeId) | NodeFlag::Resizable)
        && geometry.resizeHandleRect(_nodeId).contains(QPoint(pos.x(), pos.y()))) {
        setCursor(QCursor(Qt::SizeFDiagCursor));
    } else {
        setCursor(QCursor());
    }

    event->accept();
}

void NodeGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseDoubleClickEvent(event);
    _graphModel.setNodeData(_nodeId,NodeRole::WidgetEmbeddable,!_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).toBool());

    Q_EMIT nodeScene()->nodeDoubleClicked(_nodeId);

}

void NodeGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (_graphModel.nodeFlags(_nodeId).testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }

    // Keep multi-selection when right-clicking an already selected node;
    // otherwise select only this node.
    if (!isSelected()) {
        if (scene()) {
            scene()->clearSelection();
        }
        setSelected(true);
    }

    QMenu m_Menu;
    QAction* renameAction = m_Menu.addAction(QStringLiteral("编辑备注"));
    renameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));  // 添加快捷键
    connect(renameAction, &QAction::triggered, [this]() {
        startEditingRemarks(); // 假设这是重命名功能
    });

    if (_graphModel.nodeData<bool>(_nodeId, NodeRole::PortEditable)) {
        bool const isEditing = static_cast<NodeWidgetType>(
                                   _graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toInt())
                               == NodeWidgetType::PortEditWidget;
        if (!isEditing) {
            QAction* editPortAction = m_Menu.addAction(QStringLiteral("编辑端口"));
            editPortAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
            connect(editPortAction, &QAction::triggered, [this]() {
                setPortEditing(true);
            });
        }
    }
    QAction* helpAction = m_Menu.addAction(QStringLiteral("节点帮助"));
    helpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));  // 添加快捷键
    connect(helpAction, &QAction::triggered, [this]() {
        openNodeHelp();
    });

    // Search Node 快捷键已在 GraphicsView 注册；共享菜单项由 appendContextMenuActions 注入

    // Collect selected nodes for 屏蔽变量输入 toggle (same selection set as Color menu).
    QList<NodeId> muteTargetIds;
    if (auto *sc = scene()) {
        for (QGraphicsItem *item : sc->selectedItems()) {
            if (auto *ngo = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
                muteTargetIds.append(ngo->nodeId());
            }
        }
    }
    if (!muteTargetIds.contains(_nodeId)) {
        muteTargetIds.prepend(_nodeId);
    }

    bool allMuted = true;
    for (NodeId const id : muteTargetIds) {
        if (!_graphModel.nodeFlags(id).testFlag(NodeFlag::Muted)) {
            allMuted = false;
            break;
        }
    }

    auto pushMuteCommand = [this](QList<NodeId> const &ids, bool muted) {
        if (!nodeScene() || ids.isEmpty())
            return;

        std::vector<SetNodeDataCommand::Change> changes;
        changes.reserve(static_cast<size_t>(ids.size()));

        for (NodeId const id : ids) {
            bool const wasMuted = _graphModel.nodeFlags(id).testFlag(NodeFlag::Muted);
            if (wasMuted == muted)
                continue;

            QVariant newValue;
            if (muted) {
                newValue = true;
            } else {
                QVariantMap payload;
                payload.insert(QStringLiteral("muted"), false);
                payload.insert(QStringLiteral("sync"), true);
                newValue = payload;
            }
            changes.push_back({id, wasMuted, newValue});
        }

        if (changes.empty())
            return;

        nodeScene()->undoStack().push(
            new SetNodeDataCommand(nodeScene(),
                                   NodeRole::Muted,
                                   std::move(changes),
                                   muted ? QStringLiteral("屏蔽变量输入")
                                         : QStringLiteral("取消屏蔽输入")));
    };

    if (allMuted) {
        QAction *unmuteAction = m_Menu.addAction(QStringLiteral("取消屏蔽输入"));
        unmuteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        connect(unmuteAction, &QAction::triggered, [pushMuteCommand, muteTargetIds]() {
            pushMuteCommand(muteTargetIds, false);
        });
    } else {
        QAction *muteAction = m_Menu.addAction(QStringLiteral("屏蔽变量输入"));
        muteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        connect(muteAction, &QAction::triggered, [pushMuteCommand, muteTargetIds]() {
            pushMuteCommand(muteTargetIds, true);
        });
    }

    m_Menu.addSeparator();
    addTitleColorMenu(m_Menu);
    addExternalControlMenu(m_Menu);

    if (auto *bs = nodeScene())
        bs->appendContextMenuActions(m_Menu, ContextMenuKind::Node);

    // 显示菜单并等待用户选择
     m_Menu.exec(event->screenPos());

    event->accept(); // 确保事件被处理
}

void NodeGraphicsObject::addExternalControlMenu(QMenu &menu)
{
    QVariant const oscVar = _graphModel.nodeData(_nodeId, NodeRole::OSCAddress);
    if (!oscVar.isValid())
        return;

    using BindingMap = ExternalBindingMap;
    if (!oscVar.canConvert<BindingMap>())
        return;

    BindingMap const mapping = oscVar.value<BindingMap>();
    if (mapping.empty())
        return;

    QStringList keys;
    keys.reserve(static_cast<int>(mapping.size()));
    for (auto const &kv : mapping)
        keys.append(kv.first);
    keys.sort();

    QMenu *oscMenu = menu.addMenu(QStringLiteral("外部控制"));

    QString const nodeName = _graphModel.nodeData(_nodeId, NodeRole::Remarks).toString();
    QString const nodeType = _graphModel.nodeData(_nodeId, NodeRole::Type).toString();
    QString const displayName = nodeName.isEmpty() ? nodeType : nodeName;

    for (QString const &rel : keys) {
        QString const full = makeFullOscAddressForNode(_graphModel, _nodeId, rel);

        QMenu *addrMenu = oscMenu->addMenu(full);

        QAction *copyAct = addrMenu->addAction(QStringLiteral("复制控制地址"));
        connect(copyAct, &QAction::triggered, [full]() {
            QApplication::clipboard()->setText(full);
        });

        QAction *webAct = addrMenu->addAction(QStringLiteral("添加网页控制"));
        connect(webAct, &QAction::triggered, [this, full, nodeName, nodeType, displayName]() {
            if (auto *bs = nodeScene()) {
                QJsonObject item;
                item[QStringLiteral("entity")] = full;
                item[QStringLiteral("nodeId")] = static_cast<int>(_nodeId);
                item[QStringLiteral("nodeName")] = nodeName;
                item[QStringLiteral("nodeType")] = nodeType;
                item[QStringLiteral("relative")] = relativePathForAddress(_graphModel, _nodeId, full);
                item[QStringLiteral("suggestedName")] = displayName;
                bs->requestSendOscBindingToWebPanel(item);
            }
        });
    }
}

void NodeGraphicsObject::addTitleColorMenu(QMenu &menu)
{
    // Collect all selected nodes; fall back to the node under the cursor.
    QList<NodeId> targetNodeIds;
    if (auto *sc = scene()) {
        for (QGraphicsItem *item : sc->selectedItems()) {
            if (auto *ngo = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
                targetNodeIds.append(ngo->nodeId());
            }
        }
    }
    if (!targetNodeIds.contains(_nodeId)) {
        targetNodeIds.prepend(_nodeId);
    }

    // Checkmark: only if every selected node shares the same title color.
    QColor sharedColor;
    bool allSame = true;
    {
        NodeStyle firstStyle(
            QJsonDocument::fromVariant(_graphModel.nodeData(targetNodeIds.first(), NodeRole::Style))
                .object());
        sharedColor = firstStyle.TitleColor;
        for (int i = 1; i < targetNodeIds.size(); ++i) {
            NodeStyle style(
                QJsonDocument::fromVariant(_graphModel.nodeData(targetNodeIds.at(i), NodeRole::Style))
                    .object());
            if (!sameRgb(style.TitleColor, sharedColor)) {
                allSame = false;
                break;
            }
        }
    }

    QMenu *colorMenu = menu.addMenu(QStringLiteral("颜色"));
    QActionGroup *group = new QActionGroup(colorMenu);
    group->setExclusive(true);

    QList<QColor> const colors = titleColorPresets();
    QStringList const names = titleColorNames();

    for (int i = 0; i < colors.size(); ++i) {
        QColor const color = colors.at(i);
        QAction *action = colorMenu->addAction(colorSwatchIcon(color), names.at(i));
        action->setCheckable(true);
        action->setChecked(allSame && sameRgb(sharedColor, color));
        group->addAction(action);

        QObject::connect(action, &QAction::triggered, &menu, [this, &menu, color, targetNodeIds]() {
            std::vector<SetNodeDataCommand::Change> changes;
            changes.reserve(static_cast<size_t>(targetNodeIds.size()));

            for (NodeId const nodeId : targetNodeIds) {
                QVariant const oldStyle = _graphModel.nodeData(nodeId, NodeRole::Style);
                NodeStyle style(QJsonDocument::fromVariant(oldStyle).object());
                if (sameRgb(style.TitleColor, color))
                    continue;

                style.TitleColor = color;
                style.SelectedBoundaryColor = color;
                changes.push_back({nodeId, oldStyle, QVariant(style.toJson().toVariantMap())});
            }

            if (!changes.empty() && nodeScene()) {
                nodeScene()->undoStack().push(
                    new SetNodeDataCommand(nodeScene(),
                                           NodeRole::Style,
                                           std::move(changes),
                                           QStringLiteral("更改节点颜色")));
            }
            menu.close();
        });
    }
}

void NodeGraphicsObject::openNodeHelp() const
{
    // "Audio Analysis" -> "AudioAnalysis.html" under <exe>/html/nodes/
    QString typeName = _graphModel.nodeData(_nodeId, NodeRole::Type).toString().trimmed();
    if (typeName.isEmpty()) {
        qDebug() << "Node Help: empty node type";
        return;
    }

    // 文件名去掉所有空白字符（空格、制表符等）
    QString fileStem = typeName;
    fileStem.remove(QRegularExpression(QStringLiteral("\\s+")));
    fileStem.remove(QLatin1Char('/'));
    fileStem.remove(QLatin1Char('\\'));

    QString const helpPath = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("html/nodes/%1.html").arg(fileStem));

    QFileInfo const info(helpPath);
    if (!info.exists() || !info.isFile()) {
        qDebug() << "Node Help: page not found for type" << typeName << "expected" << helpPath;
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()))) {
        qDebug() << "Node Help: failed to open" << info.absoluteFilePath();
    }
}

void NodeGraphicsObject::keyPressEvent(QKeyEvent* event)
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
    if ((event->key() == Qt::Key_P) && (event->modifiers() & Qt::ControlModifier)
        && _graphModel.nodeData(_nodeId, NodeRole::PortEditable).toBool()) {
        bool const isEditing = static_cast<NodeWidgetType>(
                                   _graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toInt())
                               == NodeWidgetType::PortEditWidget;
        setPortEditing(!isEditing);
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_M) && (event->modifiers() & Qt::ControlModifier)) {
        QList<NodeId> targetIds;
        if (auto *sc = scene()) {
            for (QGraphicsItem *item : sc->selectedItems()) {
                if (auto *ngo = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
                    targetIds.append(ngo->nodeId());
                }
            }
        }
        if (!targetIds.contains(_nodeId)) {
            targetIds.prepend(_nodeId);
        }

        bool allMuted = true;
        for (NodeId const id : targetIds) {
            if (!_graphModel.nodeFlags(id).testFlag(NodeFlag::Muted)) {
                allMuted = false;
                break;
            }
        }

        std::vector<SetNodeDataCommand::Change> changes;
        changes.reserve(static_cast<size_t>(targetIds.size()));
        for (NodeId const id : targetIds) {
            bool const wasMuted = _graphModel.nodeFlags(id).testFlag(NodeFlag::Muted);
            if (allMuted) {
                if (!wasMuted)
                    continue;
                QVariantMap payload;
                payload.insert(QStringLiteral("muted"), false);
                payload.insert(QStringLiteral("sync"), true);
                changes.push_back({id, true, payload});
            } else {
                if (wasMuted)
                    continue;
                changes.push_back({id, false, true});
            }
        }

        if (!changes.empty() && nodeScene()) {
            nodeScene()->undoStack().push(
                new SetNodeDataCommand(nodeScene(),
                                       NodeRole::Muted,
                                       std::move(changes),
                                       allMuted ? QStringLiteral("取消屏蔽输入")
                                                : QStringLiteral("屏蔽变量输入")));
        }
        event->accept();
        return;
    }
    QGraphicsObject::keyPressEvent(event);

}

bool NodeGraphicsObject::isEditingRemarks() const
{
    return _remarksProxy && _remarksProxy->isVisible();
}

void NodeGraphicsObject::initRemarksEditor()
{
    if (_remarksProxy)
        return;

    _remarksEditor = new QLineEdit();
    _remarksEditor->setFrame(false);
    _remarksEditor->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _remarksEditor->setAttribute(Qt::WA_TranslucentBackground, false);

    connect(_remarksEditor, &QLineEdit::editingFinished, this, [this]() {
        finishEditingRemarks();
    });
    _remarksEditor->installEventFilter(this);

    _remarksProxy = new QGraphicsProxyWidget(this);
    _remarksProxy->setWidget(_remarksEditor);
    _remarksProxy->setZValue(10.0);
    _remarksProxy->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    _remarksProxy->hide();
}

void NodeGraphicsObject::syncRemarksEditorGeometry()
{
    if (!_remarksProxy || !_remarksEditor || !nodeScene())
        return;

    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
    NodeStyle nodeStyle(
        QJsonDocument::fromVariant(_graphModel.nodeData(_nodeId, NodeRole::Style)).object());

    qreal const offset = (_nodeState.hovered() || isSelected()) ? nodeStyle.HoveredPenWidth
                                                               : nodeStyle.PenWidth;
    qreal const titleH = geometry.captionPosition(_nodeId).y() * 2.0
                         - geometry.captionRect(_nodeId).height() - offset;
    QRectF const captionRect(offset,
                             offset,
                             qMax<qreal>(1.0, geometry.size(_nodeId).width() - 2.0 * offset),
                             qMax<qreal>(1.0, titleH));

    _remarksProxy->setPos(captionRect.topLeft());
    _remarksProxy->resize(captionRect.size());
    _remarksEditor->resize(captionRect.size().toSize());
}

void NodeGraphicsObject::startEditingRemarks()
{
    initRemarksEditor();

    NodeStyle nodeStyle(
        QJsonDocument::fromVariant(_graphModel.nodeData(_nodeId, NodeRole::Style)).object());

    QString const bg = nodeStyle.TitleColor.name(QColor::HexRgb);
    QString const fg = nodeStyle.FontColor.name(QColor::HexRgb);
    _remarksEditor->setStyleSheet(
        QStringLiteral(
            "QLineEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid rgba(255,255,255,0.35);"
            "  border-radius: 3px;"
            "  font-weight: bold;"
            "  padding: 0px 4px;"
            "  selection-background-color: rgba(0,0,0,0.35);"
            "}")
            .arg(bg, fg));

    syncRemarksEditorGeometry();
    _remarksEditor->setText(_graphModel.nodeData(_nodeId, NodeRole::Remarks).toString());
    _remarksProxy->show();
    _remarksEditor->setFocus(Qt::OtherFocusReason);
    _remarksEditor->selectAll();
    update();
}

void NodeGraphicsObject::finishEditingRemarks()
{
    if (!_remarksEditor || !_remarksProxy || _finishingRemarksEdit)
        return;
    if (!_remarksProxy->isVisible())
        return;

    _finishingRemarksEdit = true;

    bool const discard = _discardRemarksEdit;
    _discardRemarksEdit = false;

    QString const newRemarks = _remarksEditor->text();
    _remarksProxy->hide();

    if (!discard) {
        QString const oldRemarks = _graphModel.nodeData(_nodeId, NodeRole::Remarks).toString();
        if (oldRemarks != newRemarks && nodeScene()) {
            nodeScene()->undoStack().push(
                new SetNodeDataCommand(nodeScene(),
                                       NodeRole::Remarks,
                                       {{_nodeId, oldRemarks, newRemarks}},
                                       QStringLiteral("编辑节点备注")));
        }
    }

    setFocus();
    update();

    _finishingRemarksEdit = false;
}

bool NodeGraphicsObject::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _remarksEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                _discardRemarksEdit = true;
                finishEditingRemarks();
                return true;
            }
        }
    }
    return QGraphicsObject::eventFilter(watched, event);
}

} // namespace QtNodes
