#include "NodeGraphicsObject.hpp"

#include <cstdlib>
#include <iostream>

#include <QtWidgets/QGraphicsEffect>
#include <QtWidgets/QtWidgets>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "AbstractNodePainter.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeDelegateModel.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"

namespace QtNodes {

NodeGraphicsObject::NodeGraphicsObject(BasicGraphicsScene &scene, NodeId nodeId)
    : _nodeId(nodeId)
    , _graphModel(scene.graphModel())
    , _nodeState(*this)
    , _proxyWidget(nullptr)
{
    scene.addItem(this);

    setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);

    setLockedState();

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

    setOpacity(nodeStyle.Opacity);

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

void NodeGraphicsObject::updateQWidgetEmbedPos()
{
  if (_proxyWidget) {
      if(_graphModel.nodeData(_nodeId, NodeRole::WidgetEmbeddable).value<bool>()) {
          AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();
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
        //需要考虑节点尺寸上预留的两个端口间隙的控件
        if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag) {
            unsigned int widgetHeight = geometry.size(_nodeId).height() -
                                        geometry.captionRect(_nodeId).height()-geometry.portSpacing(_nodeId)*2;

            // If the widget wants to use as much vertical space as possible, set
            // it to have the geom's equivalentWidgetHeight.
            _proxyWidget->setMinimumHeight(widgetHeight);
        }

        _proxyWidget->setPos(geometry.widgetPosition(_nodeId));

        // update();

        _proxyWidget->setOpacity(1.0);
        _proxyWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
    }
}

void NodeGraphicsObject::setLockedState()
{
    NodeFlags flags = _graphModel.nodeFlags(_nodeId);

    bool const locked = flags.testFlag(NodeFlag::Locked);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges, !locked);
}

void NodeGraphicsObject::onLockedState(NodeId id)
{
    if (_nodeId != id) {
        return;
    }
    setLockedState();
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

    update();
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

            w->resize(oldSize);

            AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

            // Passes the new size to the model.
            geometry.recomputeSize(_nodeId);

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
    // bring all the colliding nodes to background
    QList<QGraphicsItem *> overlapItems = collidingItems();

    for (QGraphicsItem *item : overlapItems) {
        if (item->zValue() > 0.0) {
            item->setZValue(0.0);
        }
    }

    // bring this node forward
    setZValue(1.0);

    _nodeState.setHovered(true);

    update();

    Q_EMIT nodeScene()->nodeHovered(_nodeId, event->screenPos());

    event->accept();
}

void NodeGraphicsObject::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    _nodeState.setHovered(false);

    setZValue(0.0);

    update();

    Q_EMIT nodeScene()->nodeHoverLeft(_nodeId);

    event->accept();
}

void NodeGraphicsObject::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    auto pos = event->pos();

    //NodeGeometry geometry(_nodeId, _graphModel, nodeScene());
    AbstractNodeGeometry &geometry = nodeScene()->nodeGeometry();

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



    // NodeFlags flags = _graphModel.nodeFlags(_nodeId);

    // bool const locked = flags.testFlag(NodeFlag::Locked);
    // QAction* lockAction = menu.addAction(locked?"Unlock Node":"Lock Node");
    // lockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));  // 添加快捷键
    // 连接菜单项信号

    // connect(lockAction, &QAction::triggered, [this]() {
    //     setLockedState();
    // });
    // ====== 补充：显示view的actions ======
    // 获取 view
    // 添加菜单项（示例动作，可根据需要扩展）
    QMenu m_Menu;
    QAction* renameAction = m_Menu.addAction( "Edit Remarks");
    renameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));  // 添加快捷键
    connect(renameAction, &QAction::triggered, [this]() {
        startEditingRemarks(); // 假设这是重命名功能
    });

    if (_graphModel.nodeData<bool>(_nodeId, NodeRole::PortEditable)) {

        QAction* editPortAction = m_Menu.addAction( _graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toBool()?"Finished Port Edit":"Edit Port");
        editPortAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));  // 添加快捷键
        connect(editPortAction, &QAction::triggered, [this]() {
            _graphModel.setNodeData(_nodeId, NodeRole::EmbeddWidgetType, !_graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toBool());
        });

    }
    QAction* helpAction = m_Menu.addAction( "Node Help");
    helpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));  // 添加快捷键
    connect(helpAction, &QAction::triggered, [this]() {
        qDebug()<<"Node type: "<<_graphModel.nodeData(_nodeId,NodeRole::Type).toString()<<" help function not realize";
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
    if ((event->key() == Qt::Key_P) && (event->modifiers() & Qt::ControlModifier) && _graphModel.nodeData(_nodeId, NodeRole::PortEditable).toBool()) {

        _graphModel.setNodeData(_nodeId, NodeRole::EmbeddWidgetType, !_graphModel.nodeData(_nodeId, NodeRole::EmbeddWidgetType).toBool());
        // embedQWidget();
        return;
    }
    QGraphicsObject::keyPressEvent(event);

}

void NodeGraphicsObject::initRemarksEditor()
{
    QJsonObject nodeStyleJson = _graphModel.nodeData(_nodeId, NodeRole::Style).toJsonObject();

    NodeStyle nodeStyle(nodeStyleJson);
    auto fontColor= nodeStyle.FontColor;
    if (!_remarksEditor) {
        _remarksEditor = new QLineEdit();
        
        connect(_remarksEditor, &QLineEdit::editingFinished,
                this, &NodeGraphicsObject::finishEditingRemarks);
                
        // 按ESC取消编辑
        _remarksEditor->installEventFilter(this);
    }
}

void NodeGraphicsObject::startEditingRemarks()
{
    initRemarksEditor();
    
    // 获取当前remarks
    auto currentRemarks = _graphModel.nodeData(_nodeId, NodeRole::Remarks).toString();
    
    // 设置编辑器位置和大小
    auto* scene = static_cast<BasicGraphicsScene*>(this->scene());
    auto& geometry = scene->nodeGeometry();
    QJsonObject nodeStyleJson = _graphModel.nodeData(_nodeId, NodeRole::Style).toJsonObject();

    NodeStyle nodeStyle(nodeStyleJson);
    // QRectF captionRect = QRectF(0,0,geometry.size(_nodeId).width()-20, geometry.captionPosition(_nodeId).y()*2-geometry.captionRect(_nodeId).height());
    // QRectF captionRect=QRectF(10,
    //     nodeStyle.HoveredPenWidth,
    //     geometry.size(_nodeId).width()-20,
    //     geometry.captionPosition(_nodeId).y()*2-geometry.captionRect(_nodeId).height()-nodeStyle.HoveredPenWidth*2);
    QRectF captionRect = QRectF(0,-geometry.captionRect(_nodeId).height()*2,geometry.size(_nodeId).width(), geometry.captionRect(_nodeId).height()*2);
    QRectF sceneRect = mapToScene(captionRect).boundingRect();
    
    _remarksEditor->setText(currentRemarks);
    _remarksEditor->setGeometry(
        scene->views().first()->mapFromScene(sceneRect).boundingRect()
    );
    
    // 显示编辑器
    _remarksEditor->setParent(scene->views().first()->viewport());
    _remarksEditor->show();
    _remarksEditor->setFocus();
    _remarksEditor->selectAll();
}

void NodeGraphicsObject::finishEditingRemarks()
{
    if (!_remarksEditor) return;
    
    // 保存新的remarks
    QString newRemarks = _remarksEditor->text();
    _graphModel.setNodeData(_nodeId, NodeRole::Remarks, newRemarks);
    
    // 隐藏编辑器
    _remarksEditor->hide();
    _remarksEditor->setParent(nullptr);
    this->setFocus();

    update();
}


} // namespace QtNodes
