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
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QActionGroup>
#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QStyleOptionGraphicsItem>

namespace QtNodes {

namespace {

/// QPlainTextEdit 默认 minimumSizeHint 很高，会把 QGraphicsProxyWidget 撑破标题栏
class GroupRemarksEdit : public QPlainTextEdit
{
public:
    QSize sizeHint() const override
    {
        return _fixed.isValid() ? _fixed : QPlainTextEdit::sizeHint();
    }

    QSize minimumSizeHint() const override
    {
        return _fixed.isValid() ? _fixed : QSize(0, 0);
    }

    void setCaptionSize(QSize const &size)
    {
        _fixed = size;
        setFixedSize(size);
        updateGeometry();
    }

private:
    QSize _fixed;
};

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
    // Opacity 仅作用于主体填充（见 DefaultGroupPainter），整图元保持不透明以免标题变淡
    setOpacity(1.0);

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
        applyCollapsedGeometry();
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

    // 折叠时隐藏节点会取消其选中；展开后若分组仍选中则补全选
    if (isSelected())
        selectMemberNodes();
}
//折叠后代理端口显示位置
QPointF GroupGraphicsObject::collapsedPortScenePosition(PortType portType) const
{
    QRectF const r = _rect;

    qreal const x = (portType == PortType::In) ? 0.0 : r.width();

    qreal const headerH = headerHeight();
    qreal const y = headerH + (r.height() - headerH) * 0.5;

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

QColor GroupGraphicsObject::titleColor() const
{
    if (!_groupId.titleColor.isEmpty()) {
        QColor const c(_groupId.titleColor);
        if (c.isValid())
            return c;
    }
    return StyleCollection::groupStyle().CaptionColor;
}

QColor GroupGraphicsObject::selectedBoundaryColor() const
{
    if (!_groupId.selectedBoundaryColor.isEmpty()) {
        QColor const c(_groupId.selectedBoundaryColor);
        if (c.isValid())
            return c;
    }
    // 与节点一致：未单独设置时跟随 TitleColor
    if (!_groupId.titleColor.isEmpty()) {
        QColor const c(_groupId.titleColor);
        if (c.isValid())
            return c;
    }
    return StyleCollection::groupStyle().SelectedColor;
}

void GroupGraphicsObject::setTitleColor(QColor const &color)
{
    if (!color.isValid())
        return;

    QString const hex = color.name(QColor::HexRgb);
    if (_groupId.titleColor == hex && _groupId.selectedBoundaryColor == hex)
        return;

    auto *scene = nodeScene();
    if (!scene)
        return;

    GroupId const oldGroupId = _groupId;
    GroupId newGroupId = _groupId;
    newGroupId.titleColor = hex;
    newGroupId.selectedBoundaryColor = hex; // 对应节点 SelectedBoundaryColor = TitleColor

    scene->undoStack().push(
        new UpdateGroupCommand(scene, oldGroupId, newGroupId, QStringLiteral("更改分组颜色")));
}

qreal GroupGraphicsObject::captionHeightForText(qreal width, QString const &text) const
{
    auto const &style = StyleCollection::groupStyle();
    qreal const minH = style.CaptionHeight > 0.0f ? style.CaptionHeight : 20.0;
    // 布局用稳定 PenWidth，避免 hover 时文字换行抖动
    qreal const inset = style.PenWidth;
    qreal const textWidth = width - 2.0 * inset - 12.0;

    if (text.isEmpty() || textWidth <= 1.0)
        return minH;

    QFont f;
    f.setBold(true);
    QFontMetricsF const fm(f);
    QRectF const bound = fm.boundingRect(QRectF(0, 0, textWidth, 10000.0),
                                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                         text);
    constexpr qreal vPad = 10.0; // 与绘制 textRect.adjusted(..., 5, ..., -5) 一致
    return qMax(minH, bound.height() + vPad);
}

qreal GroupGraphicsObject::captionHeightForWidth(qreal width) const
{
    return captionHeightForText(width, _groupId.groupRemarks);
}

qreal GroupGraphicsObject::captionHeight() const
{
    // 编辑中标题高度跟随草稿，避免绘制高度与编辑框不一致
    if (isEditingRemarks() && _remarksEditor)
        return captionHeightForText(_rect.width(), _remarksEditor->toPlainText());
    return captionHeightForWidth(_rect.width());
}

qreal GroupGraphicsObject::captionInset() const
{
    auto const &style = StyleCollection::groupStyle();
    return (isUnderMouse() || isSelected()) ? style.HoveredPenWidth : style.PenWidth;
}

QRectF GroupGraphicsObject::captionBarRect() const
{
    qreal const o = captionInset();
    qreal const w = _rect.width();
    qreal const h = captionHeight();
    return QRectF(o, o, qMax<qreal>(0.0, w - 2.0 * o), h);
}

qreal GroupGraphicsObject::headerHeight() const
{
    return captionInset() + captionHeight();
}

qreal GroupGraphicsObject::collapsedWidthForRemarks(QString const &text) const
{
    QString measure = text;
    if (measure.isEmpty())
        measure = QStringLiteral("Group");

    QFont f;
    f.setBold(true);
    QFontMetricsF const fm(f);

    qreal textW = 0.0;
    for (QString const &line : measure.split(QChar(u'\n')))
        textW = qMax(textW, fm.horizontalAdvance(line));

    // 与 drawGroupCaption 的 textRect.adjusted(6, ..., -6, ...) 一致
    constexpr qreal hPad = 12.0;
    // 左右代理端口各占一点内侧空间，避免文字贴边
    auto const &style = StyleCollection::groupStyle();
    qreal portPad = style.PortWidth > 0.0f ? static_cast<qreal>(style.PortWidth) : 8.0;
    constexpr qreal minW = 72.0;

    return qMax(minW, textW + hPad + portPad);
}

void GroupGraphicsObject::applyCollapsedGeometry(QString const &remarksText)
{
    QString const text = remarksText.isNull() ? _groupId.groupRemarks : remarksText;
    qreal const w = collapsedWidthForRemarks(text);
    auto const &style = StyleCollection::groupStyle();
    qreal const inset = style.PenWidth;
    qreal const h = inset + captionHeightForText(w, text) + collapsedBodyHeight();

    prepareGeometryChange();
    _rect = QRectF(0, 0, w, h);
    update();

    if (auto *scene = nodeScene()) {
        std::unordered_set<ConnectionId> affectedConnections;
        for (NodeId const nodeId : _groupId.nodeIds) {
            auto const conns = _graphModel.allConnectionIds(nodeId);
            affectedConnections.insert(conns.begin(), conns.end());
        }
        for (auto const &connId : affectedConnections) {
            if (auto *connItem = scene->connectionGraphicsObject(connId))
                connItem->move();
        }
    }
}

qreal GroupGraphicsObject::collapsedBodyHeight() const
{
    auto const &style = StyleCollection::groupStyle();
    qreal const baseCap = style.CaptionHeight > 0.0f ? style.CaptionHeight : 20.0;
    if (style.CollapsedHeight > baseCap)
        return style.CollapsedHeight - baseCap;
    return 0.0;
}

qreal GroupGraphicsObject::collapsedTotalHeight() const
{
    return headerHeight() + collapsedBodyHeight();
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

    QRectF const captionRect(0, 0, _rect.width(), headerHeight());

    if (!captionRect.contains(event->pos())) {
        event->ignore();
        return;
    }

    auto *scene = nodeScene();
    if (!scene) {
        event->ignore();
        return;
    }

    // 点击标题栏：选中分组，并选中组内全部节点。
    // 注意：不要只依赖 QGraphicsItem::mousePressEvent 的选中结果——
    // 松开时场景可能 clearSelection 再只选中分组，会触发 itemChange 清掉节点。
    bool const ctrl = event->modifiers() & Qt::ControlModifier;
    if (!ctrl)
        scene->clearSelection();

    setSelected(true);
    selectMemberNodes();

    _pressedOnCaption = true;
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

    QRectF const captionRect(0, 0, _rect.width(), headerHeight());

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

    // 单击/双击松开时，Qt 常会 clearSelection 再只选中分组，导致组内节点被清掉。
    // 约定：分组选中时可见成员必须保持全选。
    // 双击场景下，视图可能在本函数返回后才完成选中重置，故再延迟一拍同步。
    if (isSelected()) {
        selectMemberNodes();
        QTimer::singleShot(0, this, [this]() {
            if (isSelected())
                selectMemberNodes();
        });
    }

    event->accept();
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
bool GroupGraphicsObject::isEditingRemarks() const
{
    return _remarksProxy && _remarksProxy->isVisible();
}

void GroupGraphicsObject::initRemarksEditor()
{
    if (_remarksProxy)
        return;

    auto *edit = new GroupRemarksEdit();
    edit->setTabChangesFocus(false);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setFrameShape(QFrame::NoFrame);
    edit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    if (edit->document())
        edit->document()->setDocumentMargin(0);

    _remarksEditor = edit;

    connect(_remarksEditor, &QPlainTextEdit::textChanged, this, [this]() {
        if (_remarksProxy && _remarksProxy->isVisible())
            syncRemarksEditorGeometry();
    });
    _remarksEditor->installEventFilter(this);

    _remarksProxy = new QGraphicsProxyWidget(this);
    _remarksProxy->setWidget(_remarksEditor);
    _remarksProxy->setZValue(10.0);
    _remarksProxy->setMinimumSize(0.0, 0.0);
    _remarksProxy->hide();
}

void GroupGraphicsObject::syncRemarksEditorGeometry()
{
    if (!_remarksProxy || !_remarksEditor || !_remarksProxy->isVisible())
        return;

    QString const draft = _remarksEditor->toPlainText();

    // 折叠编辑时随草稿文字伸缩宽度
    if (_collapsed)
        applyCollapsedGeometry(draft);

    QFont f;
    f.setBold(true);
    _remarksEditor->setFont(f);
    if (_remarksEditor->document())
        _remarksEditor->document()->setDocumentMargin(0);

    QFontMetricsF const fm(f);
    qreal const width = qMax<qreal>(1.0, _rect.width());
    qreal const height = captionHeightForText(width, draft);
    qreal const inset = captionInset();
    QRectF const barRect(inset,
                         inset,
                         qMax<qreal>(1.0, width - 2.0 * inset),
                         height);

    // 展开态：标题高度随草稿变化时向上伸缩，避免盖住组内节点
    if (!_collapsed) {
        if (_editingCaptionHeight <= 0.0)
            _editingCaptionHeight = height;
        qreal const delta = height - _editingCaptionHeight;
        if (!qFuzzyIsNull(delta)) {
            prepareGeometryChange();
            setPos(pos() + QPointF(0.0, -delta));
            _rect.setHeight(_rect.height() + delta);
            _editingCaptionHeight = height;
        }
    }

    // 单行（含折叠）垂直居中；多行顶对齐 + 上边距 5
    constexpr int hPad = 6;
    int topPad = 5;
    QRectF const bound = fm.boundingRect(QRectF(0, 0, qMax<qreal>(1.0, barRect.width() - 12.0), 10000.0),
                                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                                         draft.isEmpty() ? QStringLiteral(" ") : draft);
    bool const singleLine = !draft.contains(QChar(u'\n'))
                            && bound.height() <= fm.lineSpacing() * 1.5;
    if (_collapsed || singleLine) {
        qreal const contentH = fm.height();
        topPad = qMax(0, qRound((height - contentH) * 0.5));
    }

    auto const &style = StyleCollection::groupStyle();
    QColor const bg = titleColor();
    QString const fg = style.FontColor.name(QColor::HexRgb);
    qreal const radius = qMax<qreal>(0.0, style.BoundaryRadius - inset);
    _remarksEditor->setStyleSheet(
        QStringLiteral(
            "QPlainTextEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: %5px;"
            "  font-weight: bold;"
            "  padding: %3px %4px 0px %4px;"
            "  selection-background-color: rgba(0,0,0,0.35);"
            "}")
            .arg(bg.name(QColor::HexRgb), fg)
            .arg(topPad)
            .arg(hPad)
            .arg(radius, 0, 'f', 1));

    QSize const fixed(qMax(1, qRound(barRect.width())), qMax(1, qRound(barRect.height())));
    // GroupRemarksEdit 无 Q_OBJECT，不能 qobject_cast
    static_cast<GroupRemarksEdit *>(_remarksEditor)->setCaptionSize(fixed);

    _remarksProxy->setMinimumSize(0.0, 0.0);
    _remarksProxy->setMaximumSize(fixed.width(), fixed.height());
    _remarksProxy->setPos(barRect.topLeft());
    _remarksProxy->resize(fixed.width(), fixed.height());
    update();
}

void GroupGraphicsObject::startEditingRemarks()
{
    initRemarksEditor();

    if (_remarksEditor->document())
        _remarksEditor->document()->setDocumentMargin(0);

    _editingCaptionHeight = captionHeightForWidth(_rect.width());
    _remarksEditor->setPlainText(_groupId.groupRemarks);
    _remarksProxy->show();
    syncRemarksEditorGeometry();
    _remarksEditor->setFocus(Qt::OtherFocusReason);
    _remarksEditor->selectAll();
    update();
}

void GroupGraphicsObject::finishEditingRemarks()
{
    if (!_remarksEditor || !_remarksProxy || _finishingRemarksEdit)
        return;
    if (!_remarksProxy->isVisible())
        return;

    _finishingRemarksEdit = true;

    bool const discard = _discardRemarksEdit;
    _discardRemarksEdit = false;

    QString const newRemarks = _remarksEditor->toPlainText();
    _remarksProxy->hide();
    _editingCaptionHeight = 0.0;

    if (!discard && _groupId.groupRemarks != newRemarks) {
        auto *scene = nodeScene();
        if (scene) {
            GroupId const oldGroupId = _groupId;
            GroupId newGroupId = _groupId;
            newGroupId.groupRemarks = newRemarks;
            scene->undoStack().push(
                new UpdateGroupCommand(scene,
                                       oldGroupId,
                                       newGroupId,
                                       QStringLiteral("编辑分组备注")));
        }
    } else if (_collapsed) {
        applyCollapsedGeometry();
    } else {
        // 放弃编辑或未改动时，按节点包围盒恢复展开几何
        updateGroupBounds();
    }

    setFocus();
    update();

    _finishingRemarksEdit = false;
}

bool GroupGraphicsObject::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _remarksEditor) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                _discardRemarksEdit = true;
                finishEditingRemarks();
                return true;
            }
            // 与节点一致：Enter 保存；Ctrl+Enter 换行
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (keyEvent->modifiers() & Qt::ControlModifier) {
                    _remarksEditor->insertPlainText(QStringLiteral("\n"));
                    return true;
                }
                finishEditingRemarks();
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            finishEditingRemarks();
            return false;
        }
    }
    return QGraphicsObject::eventFilter(watched, event);
}

// 分组选中 ⇔ 组内可见节点全选；取消选中则清空成员选中
QVariant GroupGraphicsObject::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        bool newSelected = value.toBool();
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
    } else if (change == QGraphicsItem::ItemSelectedHasChanged) {
        if (isSelected())
            selectMemberNodes();
    }
    
    return QGraphicsItem::itemChange(change, value);
}

void GroupGraphicsObject::selectMemberNodes()
{
    auto *scene = nodeScene();
    if (!scene)
        return;

    for (NodeId const nodeId : _groupId.nodeIds) {
        if (auto *nodeItem = scene->nodeGraphicsObject(nodeId)) {
            // Qt：不可见图元无法保持 selected，折叠隐藏时跳过
            if (nodeItem->isVisible())
                nodeItem->setSelected(true);
        }
    }
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

    // 顶边 = 动态标题高度 + 与节点间距（避免贴住标题栏）
    constexpr qreal margin = 50;
    constexpr qreal belowCaption = 30;

    auto const &groupStyle = StyleCollection::groupStyle();
    qreal const layoutInset = groupStyle.PenWidth;

    // 折叠：宽度贴合备注；位置仍锚定节点包围盒左上，便于与成员同步拖拽
    if (_collapsed) {
        qreal const w = collapsedWidthForRemarks();
        qreal const capH = captionHeightForWidth(w);
        qreal const h = layoutInset + capH + collapsedBodyHeight();
        qreal const topMargin = layoutInset + capH + belowCaption;

        prepareGeometryChange();
        setPos(QPointF(minX - margin, minY - topMargin));
        _rect = QRectF(0, 0, w, h);

        if (oldPos != pos() || oldRect != _rect) {
            update();

            auto *scene = nodeScene();
            if (scene) {
                std::unordered_set<ConnectionId> affectedConnections;
                for (NodeId const nodeId : _groupId.nodeIds) {
                    auto const conns = _graphModel.allConnectionIds(nodeId);
                    affectedConnections.insert(conns.begin(), conns.end());
                }
                for (auto const &connId : affectedConnections) {
                    if (auto *connItem = scene->connectionGraphicsObject(connId))
                        connItem->move();
                }
            }
        }
        return;
    }

    qreal const width = (maxX - minX) + 2 * margin;
    qreal const capH = captionHeightForWidth(width);
    qreal const topMargin = layoutInset + capH + belowCaption;
    QRectF newBounds(minX - margin,
                     minY - topMargin,
                     width,
                     (maxY - minY) + topMargin + margin);

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
void GroupGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (graphModel().nodeFlags().testFlag(NodeFlag::Locked)) {
        event->ignore();
        return;
    }

    QMenu m_Menu;
    QAction* renameAction = m_Menu.addAction(QStringLiteral("编辑备注"));
    renameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));  // 添加快捷键
    connect(renameAction, &QAction::triggered, [this]() {
        startEditingRemarks(); // 假设这是重命名功能
    });

    m_Menu.addSeparator();
    addTitleColorMenu(m_Menu);

    if (auto *bs = nodeScene())
        bs->appendContextMenuActions(m_Menu, ContextMenuKind::Group);

    // 显示菜单并等待用户选择
     m_Menu.exec(event->screenPos());

    event->accept(); // 确保事件被处理
}

void GroupGraphicsObject::addTitleColorMenu(QMenu &menu)
{
    // 收集所有选中的分组；若当前分组不在选中集中则补上
    QList<GroupGraphicsObject *> targetGroups;
    if (auto *sc = scene()) {
        for (QGraphicsItem *item : sc->selectedItems()) {
            if (auto *ggo = qgraphicsitem_cast<GroupGraphicsObject *>(item))
                targetGroups.append(ggo);
        }
    }
    if (!targetGroups.contains(this))
        targetGroups.prepend(this);

    QColor sharedColor = targetGroups.first()->titleColor();
    bool allSame = true;
    for (int i = 1; i < targetGroups.size(); ++i) {
        if (!sameRgb(targetGroups.at(i)->titleColor(), sharedColor)) {
            allSame = false;
            break;
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

        QObject::connect(action, &QAction::triggered, &menu, [color, targetGroups, &menu]() {
            auto *scene = targetGroups.isEmpty() ? nullptr : targetGroups.first()->nodeScene();
            if (!scene) {
                menu.close();
                return;
            }

            std::vector<UpdateGroupCommand::Change> changes;
            changes.reserve(static_cast<size_t>(targetGroups.size()));

            for (GroupGraphicsObject *ggo : targetGroups) {
                if (!ggo)
                    continue;
                QString const hex = color.name(QColor::HexRgb);
                GroupId const &oldGroup = ggo->groupId();
                if (oldGroup.titleColor == hex && oldGroup.selectedBoundaryColor == hex)
                    continue;

                GroupId newGroup = oldGroup;
                newGroup.titleColor = hex;
                newGroup.selectedBoundaryColor = hex;
                changes.push_back({oldGroup, newGroup});
            }

            if (!changes.empty()) {
                scene->undoStack().push(
                    new UpdateGroupCommand(scene,
                                           std::move(changes),
                                           QStringLiteral("更改分组颜色")));
            }
            menu.close();
        });
    }
}

} // namespace QtNodes
