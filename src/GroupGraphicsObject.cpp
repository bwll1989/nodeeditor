#include "GroupGraphicsObject.hpp"

#include <stdexcept>

#include "AbstractGraphModel.hpp"
#include "AbstractGroupPainter.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "GroupStyle.hpp"
#include "NodeGraphicsObject.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"

#include <QtCore/QDebug>
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

//    addGraphicsEffect();
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setZValue(-10.0);
    GroupStyle Style=StyleCollection::groupStyle();
    setOpacity(Style.Opacity);

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
}

void GroupGraphicsObject::updatePosition()
{
    updateGroupBounds();
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
    return _rect;
}

void GroupGraphicsObject::move()
{
   
    

 
}

void GroupGraphicsObject::paint(QPainter *painter,
                                     QStyleOptionGraphicsItem const *option,
                                     QWidget *)
{
    painter->setClipRect(option->exposedRect);

    nodeScene()->groupPainter().paint(painter, *this);
}

void GroupGraphicsObject::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // 先调用基类的事件处理
    QGraphicsItem::mousePressEvent(event);

    // 如果是左键点击，处理选中逻辑
    if (event->button() == Qt::LeftButton) {
        // 获取场景
        auto* scene = nodeScene();
        if (!scene) return;

        // 如果按住Ctrl键，不清除其他选择
        if (!(event->modifiers() & Qt::ControlModifier)) {
            // 清除场景中其他项的选择状态
            scene->clearSelection();
        }

        // 选中组
        setSelected(true);

        // 选中组内所有节点
        for (const NodeId& nodeId : _groupId.nodeIds) {
            if (auto* nodeItem = scene->nodeGraphicsObject(nodeId)) {
                nodeItem->setSelected(true);
            }
        }

        event->accept();
    }
}

void GroupGraphicsObject::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    auto diff = event->pos() - event->lastPos();

    nodeScene()->undoStack().push(new MoveNodeCommand(nodeScene(), diff));
    QGraphicsItem::mouseMoveEvent(event);
}

void GroupGraphicsObject::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseDoubleClickEvent(event);
}

void GroupGraphicsObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
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
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        startEditingRemarks();
        event->accept();
    } else {
        QGraphicsObject::keyPressEvent(event);
    }
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
    QRectF captionRect =QRectF(0, 0, _rect.width(), Style.CaptionHeight);
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

bool GroupGraphicsObject::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _remarksEditor) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                _remarksEditor->hide();
                _remarksEditor->setParent(nullptr);
                this->setFocus();
                return true;
            }
        }
    }
    return QGraphicsObject::eventFilter(watched, event);
}

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

// 添加新的槽函数来处理节点位置更新
void GroupGraphicsObject::onNodePositionUpdated(NodeId nodeId)
{
    // 检查更新的节点是否属于此组
    if (std::find(_groupId.nodeIds.begin(), _groupId.nodeIds.end(), nodeId) != _groupId.nodeIds.end()) {
        updateGroupBounds();
    }
}

void GroupGraphicsObject::onNodeUpdated(NodeId nodeId)
{
    // 检查更新的节点是否属于此组
    if (std::find(_groupId.nodeIds.begin(), _groupId.nodeIds.end(), nodeId) != _groupId.nodeIds.end()) {
        updateGroupBounds();
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
    }
    graphModel().updateGroup(oldGroupId,_groupId);
    updateGroupBounds();
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
    _rect = QRectF(0, 0, newBounds.width(), newBounds.height());

    // 如果大小或位置发生变化，触发更新
    if (oldPos != pos() || oldRect != _rect) {
        update();
    }
}
void GroupGraphicsObject::setLockedState() {

    NodeFlags flags = _graphModel.nodeFlags();

    bool const locked = flags.testFlag(NodeFlag::Locked);

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
} // namespace QtNodes
